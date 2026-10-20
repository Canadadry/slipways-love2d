#!/bin/sh
# usage: constants_check.sh <expected gaps> <reference> <p8/fixed.c> <working directory> <source.c>...
# Exit 0 when the GAP literals of constants_compare.sh are exactly those of
# the list (one per line, `#` starts a comment), printing only the summary
# line of the comparison. Exit 1 otherwise, when the list or the working
# directory is missing, when the comparator fails, or when the list names a
# literal absent from the sources; on a verdict, the whole table goes to
# stderr first.

set -u

expected=$1
shift
work=$3
table=$work/table.txt

fail() {
    echo "constants_check: $1" >&2
    exit 1
}

[ -f "$expected" ] || fail "expected-gap list $expected missing"
mkdir -p "$work" || fail "cannot create the working directory $work"
sh "$(dirname "$0")/constants_compare.sh" "$@" > "$table" || fail "the comparator stopped on an error"

verdict=$(awk -F' [|] ' '
    FILENAME == ARGV[1] { if ($0 !~ /^#/ && NF) listed[$0] = 1; next }
    NF == 5 { status[$1] = $4 }
    END {
        for (lit in listed) if (!(lit in status)) print "listed literal absent from the sources: " lit
        for (lit in listed) if (status[lit] == "ok") print "stale list: remove " lit
        for (lit in status) if (status[lit] == "GAP" && !(lit in listed)) print "regression: " lit
    }' "$expected" "$table" | sort)

if [ -n "$verdict" ]; then
    cat "$table" >&2
    echo "$verdict" | sed 's/^/constants_check: /' >&2
    fail "the GAP literals are not those of $expected"
fi
tail -n 1 "$table"
