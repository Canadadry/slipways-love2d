#!/bin/sh
# usage: e2e_limits.sh <exceptions> [<events>...]
# Exit 1 when the exception list is missing, or when an e2e is missing,
# lacks its <name>.txt or its <name>.json.sha256, runs past 180 frames
# (600 when it is listed in <exceptions>, as `<events> reason`: only the
# first field counts), does not declare "input": "pad", or carries mouse_x,
# mouse_y, mouse_buttons or dpad, or when the first paragraph of its .txt,
# lines joined by single spaces, does not hold exactly one "<N> frames." with
# N its frame count; or when a line of <exceptions> names no given e2e,
# does not state "<N> frames:" as its second and third fields with N the
# e2e's frame count, or names an e2e of 180 frames or fewer.
# Every file is examined before concluding.
# A frame of the file is a game frame: the "pad" keys are counted.

set -u

exceptions=$1
shift
if [ ! -f "$exceptions" ]; then
    echo "e2e: missing exceptions list: $exceptions" >&2
    exit 1
fi

failed=0
txts=0
counts=""
for events in "$@"; do
    if [ ! -f "$events" ]; then
        echo "e2e: $events missing" >&2
        failed=1
        continue
    fi
    for companion in "${events%.json}.txt" "$events.sha256"; do
        if [ ! -f "$companion" ]; then
            echo "e2e: $events: $companion missing" >&2
            failed=1
        fi
    done

    if ! grep -q '"input": *"pad"' "$events"; then
        echo "e2e: $events: does not declare \"input\": \"pad\"" >&2
        failed=1
    fi
    for key in mouse_x mouse_y mouse_buttons dpad; do
        if grep -q "\"$key\"" "$events"; then
            echo "e2e: $events: carries the mouse field \"$key\"" >&2
            failed=1
        fi
    done
    frames=$(grep -o '"pad":' "$events" | wc -l | tr -d ' ')
    counts="$counts $events=$frames"
    limit=180
    if awk -v e="$events" '$1 == e { found = 1 } END { exit !found }' "$exceptions"; then
        limit=600
    fi
    if [ "$frames" -gt "$limit" ]; then
        echo "e2e: $events: $frames frames, more than $limit" >&2
        failed=1
    fi
    if [ -f "${events%.json}.txt" ] && [ ! -s "${events%.json}.txt" ]; then
        echo "e2e: $events: .txt states no \"N frames.\"" >&2
        failed=1
    elif [ -f "${events%.json}.txt" ]; then
        txts=$((txts + 1))
        set -- "$@" "events=$events" "frames=$frames" "${events%.json}.txt"
    fi
done

# One awk over the exceptions list, then every .txt, each preceded by its
# events=/frames= assignments, so make e2e pays one process, not one per
# replay. An empty file never reaches FNR == 1, hence the -s test above.
shift $(($# - 3 * txts))
if ! awk -v counts="$counts" -v exceptions="$exceptions" '
    function judge(    rest, count, said) {
        rest = " " text
        while (match(rest, /[^0-9A-Za-z_][0-9]+ frames\./)) {
            said = substr(rest, RSTART + 1, RLENGTH - 9)
            count++
            rest = substr(rest, RSTART + RLENGTH)
        }
        if (count == 0) printf "e2e: %s: .txt states no \"N frames.\"\n", name
        else if (count > 1) printf "e2e: %s: .txt states \"N frames.\" %d times\n", name, count
        else if (said != count_of) printf "e2e: %s: .txt says %s frames, the file has %s\n", name, said, count_of
        else return
        bad = 1
    }
    BEGIN {
        pairs = split(counts, pair, " ")
        for (i = 1; i <= pairs; i++) {
            match(pair[i], /=[0-9]+$/)
            frames_of[substr(pair[i], 1, RSTART - 1)] = substr(pair[i], RSTART + 1) + 0
        }
    }
    FILENAME == exceptions {
        if ($0 ~ /^[ \t]*$/) next
        if (!($1 in frames_of)) {
            printf "e2e: %s: listed in %s but not given\n", $1, exceptions
            bad = 1
            next
        }
        if ($2 !~ /^[0-9]+$/ || $3 !~ /^frames:/) {
            printf "e2e: %s: %s states no \"N frames:\"\n", $1, exceptions
            bad = 1
        } else if ($2 + 0 != frames_of[$1]) {
            printf "e2e: %s: %s says %s frames, the file has %s\n", $1, exceptions, $2, frames_of[$1]
            bad = 1
        }
        if (frames_of[$1] <= 180) {
            printf "e2e: %s: listed in %s but runs %s frames, not over 180\n", $1, exceptions, frames_of[$1]
            bad = 1
        }
        next
    }
    FNR == 1 { if (judged) judge(); judged = 1; name = events; count_of = frames; text = ""; done = 0 }
    done { next }
    /^[ \t]*$/ { if (text != "") done = 1; next }
    { sub(/^[ \t]+/, ""); sub(/[ \t]+$/, ""); text = text (text == "" ? "" : " ") $0 }
    END { if (judged) judge(); exit bad }' "$exceptions" "$@" >&2; then
    failed=1
fi

exit $failed
