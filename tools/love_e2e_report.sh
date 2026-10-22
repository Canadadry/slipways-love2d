#!/bin/sh
# usage: love_e2e_report.sh [<replay> <reference .sha256> <current hash> <error file>]...
# A replay with an error file is FAILED, its lines on stderr; the others go
# to frames_check.sh in one call. Ends with the count
# line; exit 0 only when every replay matches.

set -u

if [ $(($# % 4)) -ne 0 ]; then
    echo "e2e: usage: love_e2e_report.sh [<replay> <reference .sha256> <current hash> <error file>]..." >&2
    exit 2
fi

replays=0
failures=0
compared=
while [ $# -ge 4 ]; do
    replays=$((replays + 1))
    if [ -f "$4" ]; then
        failures=$((failures + 1))
        {
            echo "e2e: FAILED $1 -- the fake stopped before the last frame."
            sed 's/^/e2e:   /' "$4"
        } >&2
    else
        compared="$compared $1 $2 $3"
    fi
    shift 4
done

case $0 in
*/*) tools=${0%/*} ;;
*) tools=. ;;
esac

work=$(mktemp -d) || exit 2
trap 'rm -rf "$work"' EXIT
check_status=0
if [ -n "$compared" ]; then
    # Word splitting of $compared: replay paths never hold a space.
    {
        sh "$tools/frames_check.sh" $compared
        echo $? > "$work/status"
    } | tee "$work/output"
    read -r check_status < "$work/status"
fi
matches=$(grep -c ': matches (' "$work/output" 2>/dev/null)
matches=${matches:-0}
diverges=$((replays - matches - failures))

echo "e2e: $matches match, $diverges diverge, $failures failed, of $replays replays in tests/e2e"
[ $failures -eq 0 ] && [ "$check_status" -eq 0 ] && [ $matches -eq $replays ]
