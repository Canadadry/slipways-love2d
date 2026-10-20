#!/bin/sh
# usage: float_guard.sh <compiler> <cflags> <scratch directory> <source.c>...
# Writes the report on the standard output. Exit 1 when a file fails under
# `-mgeneral-regs-only`, 2 with no source or when the compiler, objdump or
# awk fails its probe: a guard that sees nothing would pass for green. The
# objects are never linked: on x86_64, their double arguments go on the
# stack, an ABI inconsistent with the rest. The diagnostic
# of the nth file stays in <directory>/<n>-<file>.err.
# Where the compiler accepts floating point under the flag (Apple clang
# lowers it to soft-float), the guard scans the -O0 disassembly for scalar
# floating-point registers instead: arm64 only, exit 2 elsewhere.

set -u

cc=$1
cflags=$2
work=$3
shift 3

if [ $# -eq 0 ]; then
    echo "float_guard: no source" >&2
    exit 2
fi

mkdir -p "$work"
probe=$work/probe.c
echo 'int probe_half(int x); int probe_half(int x) { return x / 2; }' > "$work/integer-probe.c"
echo 'double probe_half(int x); double probe_half(int x) { return x / 2.0; }' > "$probe"
# cflags is deliberately unquoted: it is a list of options.
if $cc $cflags -mgeneral-regs-only -c -o "$work/probe.o" "$probe" 2> /dev/null; then
    detector=disassembly
elif $cc $cflags -mgeneral-regs-only -c -o "$work/integer-probe.o" "$work/integer-probe.c" 2> "$work/integer-probe.err"; then
    detector=flag
else
    echo "float_guard: $cc $cflags -mgeneral-regs-only rejects integer code, see $work/integer-probe.err" >&2
    exit 2
fi

failed=$work/failed.txt
objects=$work/objects.txt
: > "$failed"
: > "$objects"
index=0
for src in "$@"; do
    index=$((index + 1))
    # The index keeps sub_x/y.c and sub/x_y.c from sharing an object.
    base=$work/$index-$(echo "$src" | tr '/' '_')
    if [ "$detector" = flag ]; then
        $cc $cflags -mgeneral-regs-only -c -o "$base.o" "$src" 2> "$base.err"
    else
        # -O0 keeps the floating-point code the optimizer would fold away.
        $cc $cflags -O0 -c -o "$base.o" "$src" 2> "$base.err" &&
            printf '%s\t%s\n' "$base.o" "$src" >> "$objects"
    fi || echo "$src" >> "$failed"
done

files=$#
tab=$(printf '\t')
set --
if [ "$detector" = disassembly ]; then
    $cc $cflags -O0 -c -o "$work/probe.o" "$probe" || exit 2
    printf '%s\t%s\n' "$work/probe.o" "$probe" >> "$objects"
fi
while IFS=$tab read -r object src; do
    set -- "$@" "$object"
done < "$objects"

# One objdump for every object: one per file adds 40% to make test's guard.
# q and v registers are left out: integer code copies structs through them.
# The probe goes through the same objdump and awk: a tool that sees nothing,
# or a target without s, d and h floating-point registers, must not pass for
# green.
if [ $# -gt 0 ]; then
    if ! objdump -d --no-show-raw-insn "$@" > "$work/disassembly.txt"; then
        echo "float_guard: objdump failed" >&2
        exit 2
    fi
    awk -v objects="$objects" '
        BEGIN {
            while ((getline line < objects) > 0) {
                split(line, pair, "\t")
                source[pair[1]] = pair[2]
            }
        }
        /:[[:space:]]+file format / {
            current = $0
            sub(/:[[:space:]]+file format .*/, "", current)
        }
        /^[[:space:]]*[0-9a-f]+:[[:space:]]+[a-z0-9.]+.*[[:space:],[{][sdh][0-9]+([^0-9a-zA-Z_]|$)/ {
            if (!(current in listed)) print source[current]
            listed[current] = 1
        }' "$work/disassembly.txt" > "$work/flagged.txt" || {
        echo "float_guard: awk failed" >&2
        exit 2
    }
    if ! grep -qxF "$probe" "$work/flagged.txt"; then
        echo "float_guard: the disassembly scan misses $probe; it knows arm64 only" >&2
        exit 2
    fi
    grep -vxF "$probe" "$work/flagged.txt" >> "$failed"
fi

cat <<'EOF'
# -mgeneral-regs-only guard over the core (no floating-point register),
# written by tools/float_guard.sh through make float-guard. Do not edit.
#
# One line per file of the core that does not compile without
# floating-point registers. The list must be empty.
#
# Blind spot on x86_64: a double argument passed to an external function
# compiles without error under the guard. The absence of any floating-point
# prototype in the core holds it: such a call is an implicit declaration
# there, hence an error.
#
EOF
LC_ALL=C sort "$failed" | sed 's/^/failed /'
count=$(wc -l < "$failed" | tr -d ' ')
echo "#"
echo "float-guard files $files failing $count"
[ "$count" -eq 0 ]
