#!/bin/sh
# usage: state_peaks.sh <out> <events> <state> [<events> <state>...]
# Writes <out> in every case: one `replay <events> clean|stopped ...|no state`
# line per pair, then one `<zone|cap> <name> <peak> <limit> <events>` line per
# zone and cap, the highest peak over every state (a tie keeps the first
# pair), then the first `budget zones <sum> <limit>` line, whose bytes left
# go to stdout, followed by the two GPU primitive buffers sized by the
# `zone gpu_frame` limit. Exit 1, with `state: <events> stopped`, when a state is missing or
# starts with `stopped`, and with `state: <zone|cap> <name> peak ... over margin`
# for each peak line where 9 x peak > 8 x limit (limits are sized at peak +
# 1/8); an exempt line only needs peak <= limit.

set -u

out=$1
shift

failed=0
: > "$out.tmp"
awk_args=""
while [ $# -ge 2 ]; do
    events=$1
    state=$2
    shift 2
    status="no state"
    if [ -f "$state" ]; then
        status=$(head -n 1 "$state")
        case $status in
            stopped*) ;;
            *) status=clean ;;
        esac
        awk_args="$awk_args replay=$events $state"
    fi
    echo "replay $events $status" >> "$out.tmp"
    if [ "$status" != clean ]; then
        echo "state: $events stopped" >&2
        failed=1
    fi
done

if [ -n "$awk_args" ]; then
    awk '
        ($1 == "zone" || $1 == "cap") && NF == 5 {
            key = $1 " " $2
            if (!(key in peak)) { order[++keys] = key; peak[key] = -1 }
            if ($4 + 0 > peak[key]) { peak[key] = $4 + 0; limit[key] = $5; holder[key] = replay }
        }
        $1 == "budget" && $2 == "zones" && NF == 4 && budget == "" { budget = $0 }
        END {
            for (i = 1; i <= keys; i++) print order[i], peak[order[i]], limit[order[i]], holder[order[i]]
            if (budget != "") print budget
        }
    ' $awk_args >> "$out.tmp"
fi

mv "$out.tmp" "$out"
awk '$1 == "budget" { print "state: " $4 - $3 " bytes left under ZONE_BUDGET_BYTES" }' "$out"
awk '$1 == "zone" && $2 == "gpu_frame" { print "state: GPU primitive buffers 2 x " $4 " = " 2 * $4 " bytes, outside ZONE_BUDGET_BYTES" }' "$out"

# Proven ceilings (largest-case unit tests, sized without margin):
#   bytes_button bytes_row particles_help_screen.
# Lists reserved in full at creation, their margin judged on elements_*:
#   bytes_particles bytes_particles_help_screen bytes_ephemerals.
exempt="bytes_button bytes_row particles_help_screen bytes_particles bytes_particles_help_screen bytes_ephemerals"
awk -v exempt="$exempt" '
    BEGIN { n = split(exempt, names, " "); for (i = 1; i <= n; i++) is_exempt[names[i]] = 1 }
    ($1 == "zone" || $1 == "cap") && NF == 5 {
        peak = $3 + 0; limit = $4 + 0
        fits = ($2 in is_exempt) ? peak <= limit : 9 * peak <= 8 * limit
        if (!fits) {
            printf "state: %s %s peak %d over margin %d (limit %d, %s)\n", $1, $2, peak, int(8 * limit / 9), limit, $5 > "/dev/stderr"
            bad = 1
        }
    }
    END { exit bad }
' "$out" || failed=1
exit $failed
