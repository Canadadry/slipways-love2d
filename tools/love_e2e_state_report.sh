#!/bin/sh
# usage: love_e2e_state_report.sh [<replay> <state> <reference .end-state> <error file>]...
# A replay with an error file is FAILED, its lines on stderr; the others go
# to end_state.sh check in one call. Ends with the count line; exit 0 only
# when every replay matches.

set -u

if [ $(($# % 4)) -ne 0 ]; then
    echo "love-e2e-state: usage: love_e2e_state_report.sh [<replay> <state> <reference .end-state> <error file>]..." >&2
    exit 2
fi

case $0 in
*/*) tools=${0%/*} ;;
*) tools=. ;;
esac

replays=0
failures=0
checked=
names=
while [ $# -ge 4 ]; do
    replays=$((replays + 1))
    if [ -f "$4" ]; then
        failures=$((failures + 1))
        {
            echo "love-e2e-state: FAILED $1 -- the fake stopped before the last frame."
            sed 's/^/love-e2e-state:   /' "$4"
        } >&2
    else
        checked="$checked $1 $2 $3"
        names="$names $1"
    fi
    shift 4
done

work=$(mktemp -d) || exit 2
trap 'rm -rf "$work"' EXIT
: > "$work/output"
if [ -n "$checked" ]; then
    # Word splitting of $checked: replay paths never hold a space.
    sh "$tools/end_state.sh" check $checked | tee "$work/output"
fi
differing=$(awk -v names="$names" '
    BEGIN { split(names, list, " "); for (i in list) compared[list[i]] = 1 }
    $1 == "end-state:" && ($2 in compared) && !($2 in seen) { seen[$2] = 1; count++ }
    END { print count + 0 }
' "$work/output")
matches=$((replays - failures - differing))

echo "love-e2e-state: $matches match, $differing differ, $failures failed, of $replays replays in tests/e2e"
[ $matches -eq $replays ]
