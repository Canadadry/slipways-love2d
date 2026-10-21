#!/bin/sh
# usage: love_e2e_run.sh <hash command> <fake> <game dir> <replay> <capture> <hash> <error>
# Runs `<fake> <game dir> <replay> <capture>` after removing the three
# outputs. Exit 0 of the fake: <hash> holds the capture's sha256. Otherwise
# <error> holds `fake_love exited <status>` (or `killed by signal <n>`) and
# the fake's stderr, and no capture nor hash is left. Exit 0 in both cases,
# so that make runs every replay; non-zero only on a broken argument list or
# hash command.

set -u

if [ $# -ne 7 ]; then
    echo "love_e2e_run: usage: love_e2e_run.sh <hash command> <fake> <game dir> <replay> <capture> <hash> <error>" >&2
    exit 2
fi
hash_command=$1
fake=$2
game=$3
replay=$4
capture=$5
hash=$6
error=$7

rm -f "$capture" "$hash" "$error"
stderr_log="$error.stderr"

"$fake" "$game" "$replay" "$capture" 2> "$stderr_log"
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
exit 0
