#!/bin/sh
# usage: float_link.sh <compiler> <cflags> <binary> <object.o>...
# Links the objects without `-lm` into a scratch binary: the core calls no
# floating-point math function of the libc.
# Prints `unresolved <symbol>` per unresolved symbol, then
# `float-link objects N unresolved M`. Exit 1 when the link fails; the full
# diagnostic stays in <binary>.err. Exit 2 with no object: a link of
# nothing would pass for green.
# The binary is never run: the measure is a link fact, not a replay.

set -u

cc=$1
cflags=$2
bin=$3
shift 3

if [ $# -eq 0 ]; then
    echo "float_link: no object" >&2
    exit 2
fi

mkdir -p "$(dirname "$bin")"
rm -f "$bin"
status=0
# cflags is deliberately unquoted: it is a list of options.
$cc $cflags -o "$bin" "$@" 2> "$bin.err" || status=1

# GNU linker: undefined reference to `sym'; lld: undefined symbol: sym
sed -n "s/.*undefined reference to [\`']\([^']*\)'.*/\1/p; s/.*undefined symbol: \([^[:space:]]*\).*/\1/p" "$bin.err" \
    | LC_ALL=C sort -u > "$bin.unresolved"
sed 's/^/unresolved /' "$bin.unresolved"
count=$(wc -l < "$bin.unresolved" | tr -d ' ')
echo "float-link objects $# unresolved $count"
if [ "$status" -ne 0 ] && [ "$count" -eq 0 ]; then
    echo "float_link: link failed, see $bin.err" >&2
fi
exit "$status"
