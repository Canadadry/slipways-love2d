#!/bin/sh
# usage: e2e_order.sh <coverage script> <replay> <object dir> <work dir> "<sources>" <output> <events>...
# Runs <coverage script> on each events file alone, into <work dir>/<name>/,
# and writes <output>: one `<name> <lines>` row per events file, <name> its
# base name, <lines> the hit rows of its lines.txt, sorted by lines then
# name. On a failed run, exits 1 naming the replay and leaves <output> as is.

set -u

coverage=$1
replay=$2
object_dir=$3
work_dir=$4
sources=$5
output=$6
shift 6

export LC_ALL=C
mkdir -p "$work_dir"
counts=$work_dir/counts.txt
: > "$counts"

# Sequential: every run shares the .gcda counters of <object dir>.
for events in "$@"; do
    name=$(basename "$events")
    if ! sh "$coverage" "$replay" "$object_dir" "$work_dir/$name" "$sources" "$events" > "$work_dir/$name.log" 2>&1; then
        cat "$work_dir/$name.log"
        echo "e2e-order: $name failed" >&2
        exit 1
    fi
    awk -v name="$name" '{ hit += $2 } END { print name, hit + 0 }' "$work_dir/$name/lines.txt" >> "$counts"
done

sort -k2,2n -k1,1 "$counts" > "$output.tmp" && mv "$output.tmp" "$output"
