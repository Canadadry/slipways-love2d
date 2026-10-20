#!/bin/sh
# usage: coverage.sh <replay> <object dir> <out dir> "<sources>" <events>...
# <replay> is built with --coverage from the objects of <object dir>, one per
# source of "<sources>". Writes <out dir>/lines.txt: one `<path>:<line> <0|1>`
# row per executable line under src/game/ or src/engine/ (headers included),
# 1 if any translation unit hit it.

set -u

replay=$1
object_dir=$2
out_dir=$3
sources=$4
shift 4

mkdir -p "$out_dir"
find "$object_dir" -name '*.gcda' -delete

for events in "$@"; do
    if ! "$replay" "$events" /dev/null "$out_dir/dump.txt" > "$out_dir/replay.log" 2>&1; then
        cat "$out_dir/replay.log"
        echo "coverage: $events stopped"
        exit 1
    fi
done

# The `0:Source:` row switches the file the following rows belong to: without
# it, the lines of an inline function of a header land on the .c file.
: > "$out_dir/gcov.txt"
for source in $sources; do
    object=$object_dir/${source%.c}.o
    gcov -t -o "$object" "$source" 2> /dev/null >> "$out_dir/gcov.txt" || {
        echo "coverage: gcov failed on $object"
        exit 1
    }
done

awk -F: '
    {
        count = $1; gsub(/ /, "", count)
        line = $2; gsub(/ /, "", line)
    }
    line == "0" && $3 == "Source" { path = $4; sub(/^\.\//, "", path); next }
    path !~ /^src\/(game|engine)\// || count == "-" || line !~ /^[0-9]+$/ || line == "0" { next }
    {
        key = path ":" line
        hit = (count == "#####" || count == "=====") ? 0 : 1
        if (!(key in lines) || hit > lines[key]) lines[key] = hit
    }
    END { for (key in lines) print key, lines[key] }
' "$out_dir/gcov.txt" | sort -t: -k1,1 -k2,2n > "$out_dir/lines.txt"

awk -v replays=$# '
    { total++; hit += $2 }
    END { printf "coverage: %d executable lines in src/game/ and src/engine/, %d hit, %d not hit, %d replays\n", total, hit, total - hit, replays }
' "$out_dir/lines.txt"
echo "coverage: lines in $out_dir/lines.txt"
