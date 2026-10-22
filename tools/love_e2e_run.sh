#!/bin/sh
# usage: love_e2e_run.sh <hash command> <fake> <game dir> <replay> <capture> <hash> <error> [<state>]
# Runs `<fake> <game dir> <replay> <capture> [<state>]` after removing the
# outputs. Exit 0 of the fake: <hash> holds the capture's sha256 and <state>
# the game's debug_state(). Otherwise <error> holds `fake_love exited
# <status>` (or `killed by signal <n>`) and the fake's stderr, and no
# capture, hash nor state is left. Exit 0 in both cases,
# so that make runs every replay; non-zero only on a broken argument list or
# hash command.

set -u

if [ $# -ne 7 ] && [ $# -ne 8 ]; then
    echo "love_e2e_run: usage: love_e2e_run.sh <hash command> <fake> <game dir> <replay> <capture> <hash> <error> [<state>]" >&2
    exit 2
fi
hash_command=$1
fake=$2
game=$3
replay=$4
capture=$5
hash=$6
error=$7
state=${8:-}

rm -f "$capture" "$hash" "$error"
[ -z "$state" ] || rm -f "$state"
stderr_log="$error.stderr"

if [ -z "$state" ]; then
    "$fake" "$game" "$replay" "$capture" 2> "$stderr_log"
else
    "$fake" "$game" "$replay" "$capture" "$state" 2> "$stderr_log"
fi
status=$?
if [ $status -eq 0 ]; then
    [ ! -s "$stderr_log" ] || cat "$stderr_log" >&2
    rm -f "$stderr_log"
    # $hash_command is split on purpose: `shasum -a 256`.
    line=$($hash_command "$capture") || exit 1
    echo "${line%% *}" > "$hash" || exit 1
    exit 0
fi

if [ $status -gt 128 ]; then
    echo "fake_love killed by signal $((status - 128))" > "$error"
else
    echo "fake_love exited $status" > "$error"
fi
cat "$stderr_log" >> "$error"
rm -f "$stderr_log" "$capture"
[ -z "$state" ] || rm -f "$state"
exit 0
