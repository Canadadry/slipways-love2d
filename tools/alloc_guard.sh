#!/bin/sh
# usage: alloc_guard.sh <exception list> <object root> <object.o>...
# Counts, per object, the relocations to malloc, calloc, realloc and free
# (also in the Darwin form `_malloc`), and requires strict equality with the
# list. The object <root>/src/x.o appears there as src/x.c. Exit 1 on any
# difference, 2 with no object or an unreadable list: a guard over nothing
# would pass for green. $OBJDUMP replaces objdump.

set -u

list=$1
root=$2
shift 2

if [ $# -eq 0 ]; then
    echo "alloc_guard: no object" >&2
    exit 2
fi

if [ ! -r "$list" ]; then
    echo "alloc_guard: unreadable list: $list" >&2
    exit 2
fi

objdump=${OBJDUMP:-objdump}
actual=$(mktemp)
report=$(mktemp)
trap 'rm -f "$actual" "$report"' EXIT

for obj in "$@"; do
    src=${obj#"$root"/}
    src=${src%.o}.c
    # Only code and data sections count: the relocations of `.debug_*` and
    # `.eh_frame` are not calls.
    count=$("$objdump" -r "$obj" | awk '
        /^RELOCATION RECORDS FOR/ { skip = ($0 ~ /debug|eh_frame/); next }
        skip || NF < 3 { next }
        {
            sym = $NF
            sub(/[-+]0x[0-9a-fA-F]+$/, "", sym)
            sub(/^_/, "", sym)
            if (sym == "malloc" || sym == "calloc" || sym == "realloc" || sym == "free") n++
        }
        END { print n + 0 }')
    echo "$src $count" >> "$actual"
done

{
    awk '{ print "A", $1, $2 }' "$actual"
    sed -e 's/#.*//' "$list" | awk 'NF >= 2 { print "L", $1, $2 }'
} | awk '
    $1 == "A" { seen[$2] = 1; got[$2] = $3 }
    $1 == "L" { listed[$2] = $3 }
    END {
        for (f in got) {
            if (got[f] > 0 && !(f in listed)) {
                printf "alloc_guard: %s allocates %d time(s), not listed; line to add: %s %d\n", f, got[f], f, got[f]
            } else if ((f in listed) && got[f] == 0) {
                printf "alloc_guard: %s no longer allocates; line to remove: %s %s\n", f, f, listed[f]
            } else if ((f in listed) && got[f] != listed[f]) {
                printf "alloc_guard: %s allocates %d time(s), %s listed; line to write: %s %d\n", f, got[f], listed[f], f, got[f]
            }
        }
        for (f in listed) {
            if (!(f in seen)) {
                printf "alloc_guard: %s is not among the objects given; line to remove: %s %s\n", f, f, listed[f]
            }
        }
    }' | LC_ALL=C sort > "$report"
cat "$report"
awk '$2 > 0 { files++; calls += $2 } END { printf "alloc-guard objects %d allocating %d calls %d\n", NR, files, calls }' "$actual"
[ ! -s "$report" ] || { echo "alloc_guard: if intended, edit $list by hand"; exit 1; }
