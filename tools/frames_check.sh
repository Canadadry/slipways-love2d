#!/bin/sh
# usage: frames_check.sh [--no-baseline-hints] [<events> <reference .sha256> <current hash>]...
# Exit 0 when no existing reference diverges and no current hash is
# missing; an events file without a reference is reported, not compared.
# Every replay is examined before concluding.

set -u

baseline_hints=1
if [ "${1:-}" = "--no-baseline-hints" ]; then
    baseline_hints=0
    shift
fi

failed=0
while [ $# -ge 3 ]; do
    events=$1
    reference=$2
    current=$3
    shift 3

    if [ ! -f "$reference" ]; then
        echo "frames: $events: no reference ($reference missing) -- not compared."
        if [ $baseline_hints -eq 1 ]; then
            echo "frames:   to set it from the current code: make e2e-baseline"
        fi
        continue
    fi
    if [ ! -f "$current" ]; then
        echo "frames: $events: current hash missing ($current)" >&2
        failed=1
        continue
    fi

    expected=$(cat "$reference")
    actual=$(cat "$current")
    if [ "$expected" = "$actual" ]; then
        echo "frames: $events: matches ($actual)"
        continue
    fi

    failed=1
    {
        echo "frames: DIVERGENT $events -- the game no longer draws the same thing."
        echo "frames:   expected ($reference): $expected"
        echo "frames:   actual   ($current): $actual"
        if [ $baseline_hints -eq 1 ]; then
            echo "frames:   rebuild the reference and find the first diverging frame: make e2e-ref"
            echo "frames:   if the divergence is intended: make e2e-baseline"
        else
            echo "frames:   the .json.sha256 is the C game's reference: fix the Lua game or the fake, never the reference"
        fi
    } >&2
done

exit $failed
