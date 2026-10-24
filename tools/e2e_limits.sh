#!/bin/sh
# usage: e2e_limits.sh <exceptions> [<events>...]
# Exit 1 when the exception list is missing, or when an e2e is missing,
# lacks its <name>.txt or its <name>.json.sha256, runs past 180 frames
# (600 when it is listed in <exceptions>, as `<events> reason`: only the
# first field counts), does not declare "input": "pad", or carries mouse_x,
# mouse_y, mouse_buttons or dpad, or when the first paragraph of its .txt,
# lines joined by single spaces, does not hold exactly one "<N> frames." with
# N its frame count; or when a line of <exceptions> names no given e2e,
# does not state "<N> frames:" as its second and third fields with N the
# e2e's frame count, or names an e2e of 180 frames or fewer.
# Every file is examined before concluding.
# A frame is an object of the "frames" array: with strings ignored, each "{"
# at depth 2 (root object, array, object), whatever the frame holds.

set -u

exceptions=$1
shift
if [ ! -f "$exceptions" ]; then
    echo "e2e: missing exceptions list: $exceptions" >&2
    exit 1
fi

failed=0
listed=$#
given=""
for events in "$@"; do
    if [ ! -f "$events" ]; then
        echo "e2e: $events missing" >&2
        failed=1
        continue
    fi
    for companion in "${events%.json}.txt" "$events.sha256"; do
        if [ ! -f "$companion" ]; then
            echo "e2e: $events: $companion missing" >&2
            failed=1
        fi
    done

    given="$given $events"
    set -- "$@" "reading=json" "$events"
    if [ -f "${events%.json}.txt" ] && [ ! -s "${events%.json}.txt" ]; then
        echo "e2e: $events: .txt states no \"N frames.\"" >&2
        failed=1
    elif [ -f "${events%.json}.txt" ]; then
        set -- "$@" "reading=txt" "events=$events" "${events%.json}.txt"
    fi
done

# One awk for every replay and .txt,
# so make e2e pays one process, not one per replay. An empty file never
# reaches FNR == 1, hence the -s test above. A JSON string never spans lines.
shift "$listed"
if ! awk -v given="$given" -v exceptions="$exceptions" '
    function judge(    rest, count, said) {
        rest = " " text
        while (match(rest, /[^0-9A-Za-z_][0-9]+ frames\./)) {
            said = substr(rest, RSTART + 1, RLENGTH - 9)
            count++
            rest = substr(rest, RSTART + RLENGTH)
        }
        if (count == 0) printf "e2e: %s: .txt states no \"N frames.\"\n", name
        else if (count > 1) printf "e2e: %s: .txt states \"N frames.\" %d times\n", name, count
        else if (said != frames_of[name]) printf "e2e: %s: .txt says %s frames, the file has %s\n", name, said, frames_of[name]
        else return
        bad = 1
    }
    BEGIN {
        replays = split(given, replay, " ")
        for (i = 1; i <= replays; i++) frames_of[replay[i]] = 0
        keys = split("mouse_x mouse_y mouse_buttons dpad", key, " ")
    }
    FILENAME == exceptions {
        if ($0 !~ /^[ \t]*$/) { lines++; first[lines] = $1; second[lines] = $2; third[lines] = $3; exception[$1] = 1 }
        next
    }
    reading == "json" {
        if (FNR == 1) depth = 0
        if ($0 ~ /"input": *"pad"/) padded[FILENAME] = 1
        for (k = 1; k <= keys; k++) if (index($0, "\"" key[k] "\"")) carries[FILENAME, key[k]] = 1
        line = $0
        gsub(/"([^"\\]|\\.)*"/, "", line)
        gsub(/[^][{}]/, "", line)
        for (i = 1; i <= length(line); i++) {
            c = substr(line, i, 1)
            if (c == "{" || c == "[") {
                if (c == "{" && depth == 2) frames_of[FILENAME]++
                depth++
            } else depth--
        }
        next
    }
    FNR == 1 { if (judged) judge(); judged = 1; name = events; text = ""; done = 0 }
    done { next }
    /^[ \t]*$/ { if (text != "") done = 1; next }
    { sub(/^[ \t]+/, ""); sub(/[ \t]+$/, ""); text = text (text == "" ? "" : " ") $0 }
    END {
        if (judged) judge()
        for (i = 1; i <= replays; i++) {
            if (!(replay[i] in padded)) {
                printf "e2e: %s: does not declare \"input\": \"pad\"\n", replay[i]
                bad = 1
            }
            for (k = 1; k <= keys; k++) if ((replay[i], key[k]) in carries) {
                printf "e2e: %s: carries the mouse field \"%s\"\n", replay[i], key[k]
                bad = 1
            }
            limit = replay[i] in exception ? 600 : 180
            if (frames_of[replay[i]] > limit) {
                printf "e2e: %s: %s frames, more than %s\n", replay[i], frames_of[replay[i]], limit
                bad = 1
            }
        }
        for (i = 1; i <= lines; i++) {
            if (!(first[i] in frames_of)) {
                printf "e2e: %s: listed in %s but not given\n", first[i], exceptions
                bad = 1
                continue
            }
            if (second[i] !~ /^[0-9]+$/ || third[i] !~ /^frames:/) {
                printf "e2e: %s: %s states no \"N frames:\"\n", first[i], exceptions
                bad = 1
            } else if (second[i] + 0 != frames_of[first[i]]) {
                printf "e2e: %s: %s says %s frames, the file has %s\n", first[i], exceptions, second[i], frames_of[first[i]]
                bad = 1
            }
            if (frames_of[first[i]] <= 180) {
                printf "e2e: %s: listed in %s but runs %s frames, not over 180\n", first[i], exceptions, frames_of[first[i]]
                bad = 1
            }
        }
        exit bad
    }' "$exceptions" "$@" >&2; then
    failed=1
fi

exit $failed
