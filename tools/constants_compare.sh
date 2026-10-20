#!/bin/sh
# usage: constants_compare.sh <probe output> <p8/fixed.c> <work dir>
#                             <source.c>...
# One line per distinct literal of the P8_FIXED_LIT(<hex>, <literal>) calls
# of the sources:
#   <literal> | <probe hex> | <C hex> | ok|GAP | <sites>
# then the counts line. A literal is a GAP if one of its values differs
# from the probe; <C hex> then lists its distinct values, separated by
# commas. The C value comes from a program generated in the work dir,
# which compiles the exact text of each call against the real p8/fixed.h.
# Exits 0 if the table is complete, even with GAPs; 1 if block A is
# missing, if a literal is not in it, if there is no constant, if a
# P8_FIXED_LIT call is not in the strict form, or if the program does not
# compile.

set -u

probe=$1
fixed_c=$2
work=$3
shift 3

fail() {
    echo "constants_compare: $1" >&2
    exit 1
}

mkdir -p "$work" || fail "cannot create the work directory $work"
sites=$work/sites.txt
reference=$work/reference.txt
gen=$work/constants_values.c
bin=$work/constants_values

[ $# -gt 0 ] || fail "no constant in the sources: no source"
awk '
{
    rest = $0
    while ((p = index(rest, "P8_FIXED_LIT")) > 0) {
        rest = substr(rest, p)
        if (!match(rest, /^P8_FIXED_LIT\(0x[0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f], [0-9]*\.?[0-9]+\)/)) {
            printf "constants_compare: %s:%d: P8_FIXED_LIT call outside the strict form\n", FILENAME, FNR > "/dev/stderr"
            bad = 1
            break
        }
        call = substr(rest, 1, RLENGTH)
        lit = call
        sub(/^[^,]*, /, "", lit)
        sub(/\)$/, "", lit)
        print lit "\t" call
        rest = substr(rest, RLENGTH + 1)
    }
}
END { exit bad }' "$@" > "$sites" || fail "malformed P8_FIXED_LIT call"
[ -s "$sites" ] || fail "no constant in the sources"

awk '/^## a literals$/ { in_a = 1; found = 1; next }
     /^## / { in_a = 0 }
     in_a && NF == 2 { print $1, tolower($2) }
     END { exit found ? 0 : 1 }' "$probe" > "$reference" \
    || fail "block A (## a literals) not found in $probe"

missing=$(awk -F'\t' 'NR == FNR { split($0, f, " "); seen[f[1]] = 1; next } !($1 in seen) && !($1 in told) { told[$1] = 1; print $1 }' "$reference" "$sites")
[ -z "$missing" ] || fail "literal missing from block A of $probe: $(echo $missing)"

{
    echo '#include <stdint.h>'
    echo '#include <stdio.h>'
    echo '#include "p8/fixed.h"'
    echo 'static void show(int call, p8_fixed_t v) {'
    echo '    uint32_t u = (uint32_t)v;'
    printf '%s\n' '    printf("%d 0x%04x.%04x\n", call, (unsigned)(u >> 16), (unsigned)(u & 0xffffu));'
    echo '}'
    echo 'int main(void) {'
    awk -F'\t' '!($2 in seen) { seen[$2] = ++k; printf "    show(%d, %s);\n", k, $2 }' "$sites"
    echo '    return 0;'
    echo '}'
} > "$gen"

${CC:-cc} -std=c99 -Wall -Wextra -Werror -I "$(dirname "$(dirname "$fixed_c")")" -o "$bin" "$gen" "$fixed_c" \
    || fail "generated program $gen does not compile"
"$bin" > "$work/values.txt" || fail "$bin exits with an error"

awk 'FILENAME == ARGV[1] { rel[$1] = $2; next }
     FILENAME == ARGV[2] { value[$1] = $2; next }
     {
         split($0, f, "\t")
         lit = f[1]
         if (!(f[2] in call)) call[f[2]] = ++k
         v = value[call[f[2]]]
         if (!(lit in n)) order[++lits] = lit
         n[lit]++
         if (!((lit, v) in has)) {
             has[lit, v] = 1
             if (lit in c) c[lit] = c[lit] "," v
             else c[lit] = v
             if (v != rel[lit]) off[lit] = 1
         }
     }
     END {
         for (i = 1; i <= lits; i++) {
             lit = order[i]
             status = (lit in off) ? "GAP" : "ok"
             print lit " | " rel[lit] " | " c[lit] " | " status " | " n[lit]
             total += n[lit]; if (status == "ok") ok++; else gaps++
         }
         printf "literals %d | sites %d | ok %d | gaps %d\n", lits, total, ok, gaps
     }' \
    "$reference" "$work/values.txt" "$sites"
