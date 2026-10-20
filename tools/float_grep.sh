#!/bin/sh
# usage: float_grep.sh <compiler> <cflags> <exclusions> <directory> <source.c>...
# Looks for any trace of floating point in the core: the given sources plus
# the headers that `<compiler> <cflags> -MM` finds for them. Every `.c` or `.h`
# file of <directory> is either in that core or in <exclusions> (lines
# `<path> | <justification>`, a leading `#` for a comment), never both,
# never neither. Prints the matching lines, then
# `float-grep core N excluded K lines M`. Exit 1 when a line is found, 2 on
# an inconsistent partition, an invalid exclusion list, an empty core or a
# compiler failure: a measure over nothing would pass for green.

set -u

cc=$1
cflags=$2
exclusions=$3
dir=$4
shift 4

fail() {
    echo "float_grep: $1" >&2
    exit 2
}

[ $# -gt 0 ] || fail "no source"
[ -f "$exclusions" ] || fail "exclusion list not found: $exclusions"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

# cflags is deliberately unquoted: it is a list of options.
$cc $cflags -MM "$@" > "$work/deps" || fail "$cc -MM failed"
# Apple clang adds files that are not C to the `-MM` dependencies (the
# SDKSettings.json of the SDK): only `.c` and `.h` files make the core.
tr ' \\' '\n\n' < "$work/deps" | grep -v ':$' | grep -E '\.[ch]$' | LC_ALL=C sort -u > "$work/core"
[ -s "$work/core" ] || fail "empty core"

errors=0
: > "$work/excluded"
while IFS= read -r line; do
    case $line in
        '#'* | '') continue ;;
    esac
    case $line in
        *'|'*) ;;
        *) echo "float_grep: line without a justification: $line" >&2; errors=1; continue ;;
    esac
    path=$(printf '%s\n' "${line%%|*}" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
    why=$(printf '%s\n' "${line#*|}" | sed 's/^[[:space:]]*//; s/[[:space:]]*$//')
    if [ -z "$why" ]; then
        echo "float_grep: empty justification: $path" >&2; errors=1
    fi
    if [ ! -f "$path" ]; then
        echo "float_grep: exclusion of a missing file: $path" >&2; errors=1
    fi
    if grep -qxF "$path" "$work/excluded"; then
        echo "float_grep: duplicate exclusion: $path" >&2; errors=1
    fi
    echo "$path" >> "$work/excluded"
done < "$exclusions"
LC_ALL=C sort -u "$work/excluded" -o "$work/excluded"

find "$dir" \( -name '*.c' -o -name '*.h' \) -type f | LC_ALL=C sort > "$work/all"
LC_ALL=C comm -12 "$work/core" "$work/excluded" > "$work/both"
LC_ALL=C sort -u "$work/core" "$work/excluded" | LC_ALL=C comm -23 "$work/all" - > "$work/neither"
while IFS= read -r path; do
    echo "float_grep: in the core and excluded: $path" >&2; errors=1
done < "$work/both"
while IFS= read -r path; do
    echo "float_grep: neither in the core nor excluded: $path" >&2; errors=1
done < "$work/neither"
[ "$errors" -eq 0 ] || exit 2

# Comments included. grep exits 1 when nothing matches: not an error here.
status=0
grep -HnE '\b(double|float)\b' $(cat "$work/core") > "$work/found" || status=$?
[ "$status" -le 1 ] || fail "grep failed"
LC_ALL=C sort -t: -k1,1 -k2,2n "$work/found"

count=$(wc -l < "$work/found" | tr -d ' ')
echo "float-grep core $(wc -l < "$work/core" | tr -d ' ') excluded $(wc -l < "$work/excluded" | tr -d ' ') lines $count"
[ "$count" -eq 0 ]
