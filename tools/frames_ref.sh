#!/bin/sh
# usage: frames_ref.sh <frames_tool> <capture directory> <worktree directory>
#                      [<events> <reference .sha256> <current hash> <capture>]...
# For each diverging replay, replays the events file at the commit that set
# its reference and leaves the capture in
# <capture directory>/<events>.ref.bin. Returns non-zero if a reference is
# not committed, if an old commit does not build or if a replay fails.

set -u
LC_ALL=C
export LC_ALL

FRAME_BYTES=9600

tool=$1
frames_dir=$2
worktrees_dir=$3
shift 3

root=$(pwd)
worktree=
# One line per diverging replay: <commit> <events> <capture>.
pending=$root/$frames_dir/frames-ref.pending

remove_worktree() {
    if [ -n "$worktree" ]; then
        git worktree remove --force "$worktree" > /dev/null 2>&1 || rm -rf "$worktree"
        git worktree prune
        worktree=
    fi
}

cleanup() {
    remove_worktree
    rm -f "$pending"
}

trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

mkdir -p "$root/$frames_dir"
: > "$pending"

while [ $# -ge 4 ]; do
    events=$1
    reference=$2
    current=$3
    capture=$4
    shift 4

    [ -f "$reference" ] || continue
    if [ ! -f "$current" ]; then
        echo "e2e-ref: $events: current hash missing ($current)" >&2
        exit 1
    fi
    [ "$(cat "$reference")" = "$(cat "$current")" ] && continue

    # Outside the repository, or not committed yet, the line has no commit:
    # git blame fails, or names it by an all-zero hash.
    commit=$(git blame --porcelain -L 1,1 -- "$reference" 2> /dev/null | sed -n '1s/ .*//p')
    case $commit in
        '' | *[!0]*) ;;
        *) commit= ;;
    esac
    if [ -z "$commit" ]; then
        echo "e2e-ref: $events: $reference is not committed -- no reference to rebuild." >&2
        echo "e2e-ref:   commit it before running e2e-ref: git status -- $reference" >&2
        exit 1
    fi
    echo "$commit $events $capture" >> "$pending"
done

if [ ! -s "$pending" ]; then
    echo "e2e-ref: no diverging replay -- nothing to rebuild."
    exit 0
fi

# Prints the first frame that differs between two captures, or nothing if
# they are identical. When one is a prefix of the other, it is the frame
# that follows the end of the shorter one.
first_divergent_frame() {
    out=$(cmp "$1" "$2" 2> /dev/null)
    status=$?
    [ $status -eq 0 ] && return 0
    if [ $status -ne 1 ]; then
        echo "e2e-ref: cmp $1 $2 failed" >&2
        return 1
    fi
    byte=$(printf '%s\n' "$out" | sed -n -e 's/.* differ: byte \([0-9][0-9]*\).*/\1/p' -e 's/.* differ: char \([0-9][0-9]*\).*/\1/p')
    if [ -n "$byte" ]; then
        echo $(( (byte - 1) / $FRAME_BYTES ))
        return 0
    fi
    size1=$(wc -c < "$1" | tr -d ' ')
    size2=$(wc -c < "$2" | tr -d ' ')
    if [ "$size1" -lt "$size2" ]; then shorter=$size1; else shorter=$size2; fi
    echo $(( shorter / $FRAME_BYTES ))
}

for commit in $(cut -d' ' -f1 "$pending" | sort -u); do
    worktree=$root/$worktrees_dir/$commit
    if [ -e "$worktree" ]; then
        git worktree remove --force "$worktree" > /dev/null 2>&1 || rm -rf "$worktree"
        git worktree prune
    fi
    mkdir -p "$(dirname "$worktree")"
    echo "e2e-ref: rebuilding the reference $commit"
    if ! git worktree add --detach "$worktree" "$commit" > /dev/null; then
        echo "e2e-ref: cannot add a worktree at $commit" >&2
        exit 1
    fi
    driver=tmp/pico8_replay
    grep -q -e '^$(TMP_DIR)/replay:' "$worktree/Makefile" && driver=tmp/replay
    if ! (cd "$worktree" && MAKEFLAGS= MFLAGS= make $driver < /dev/null > /dev/null); then
        echo "e2e-ref: the replay driver does not build at commit $commit" >&2
        exit 1
    fi

    # Before the difficulty shortcut was removed, the driver started from
    # the title screen only with this flag; every e2e starts from the title.
    flag=
    grep -q -e '--from-title' "$worktree/src/main.c" && flag=--from-title

    while read -r line_commit events capture; do
        [ "$line_commit" = "$commit" ] || continue
        ref_capture=$frames_dir/$events.ref.bin
        mkdir -p "$(dirname "$root/$ref_capture")"
        rm -f "$root/$ref_capture"
        if [ ! -f "$worktree/$events" ]; then
            echo "e2e-ref: $events does not exist at commit $commit" >&2
            exit 1
        fi
        if ! (cd "$worktree" && ./$driver $flag "$events" "$root/$ref_capture" < /dev/null > /dev/null); then
            rm -f "$root/$ref_capture"
            echo "e2e-ref: $events: the replay fails at commit $commit" >&2
            exit 1
        fi

        frame=$(first_divergent_frame "$ref_capture" "$capture") || exit 1
        if [ -z "$frame" ]; then
            echo "e2e-ref: $events: the capture rebuilt at $commit is identical to the current capture."
            continue
        fi
        echo "e2e-ref: $events: first diverging frame $frame (reference $commit)"
        echo "e2e-ref:   reference: $tool frame $ref_capture $frame"
        echo "e2e-ref:   current:   $tool frame $capture $frame"
    done < "$pending"

    remove_worktree
done

exit 0
