#!/bin/sh
# usage: random_guard.sh <list> <directory>
# Counts, per `.c` and `.h` file of <directory>, the names of each random
# family outside comments and string literals:
#   game       p8_fixed_rnd* (but p8_fixed_rnd_from_state), p8_fixed_set_rnd_state
#   generator  p8_fixed_rnd_from_state, the generator on a caller's state
#   visual     visual_random_draw*
# and requires strict equality with the list (lines
# `<path> <family> <count> | <reason>`, a leading `#` for a comment). Prints
# the differences, then `random-guard files N calls M`. Exit 1 on any
# difference, 2 on an invalid or empty list or a missing directory: a guard
# over nothing would pass for green.

set -u

list=$1
dir=$2

fail() {
    echo "random_guard: $1" >&2
    exit 2
}

[ -d "$dir" ] || fail "directory not found: $dir"
[ -f "$list" ] || fail "list not found: $list"

# Word splitting below is deliberate: one argument per file, no path of src/
# has a space.
files=$(grep -rlE --include='*.c' --include='*.h' 'p8_fixed_rnd|p8_fixed_set_rnd_state|visual_random_draw' "$dir")

exec awk -v list="$list" -v sq="'" '
    function sorted_print(lines, n,    i, j, t) {
        for (i = 2; i <= n; i++) {
            t = lines[i]
            for (j = i - 1; j >= 1 && lines[j] > t; j--) lines[j + 1] = lines[j]
            lines[j + 1] = t
        }
        for (i = 1; i <= n; i++) print lines[i]
    }

    function invalid(message) {
        print "random_guard: " message > "/dev/stderr"
        bad_list = 1
    }

    function list_line(    bar, why, n, field, path, family, count, probe) {
        if ($0 ~ /^[[:space:]]*(#|$)/) return
        bar = index($0, "|")
        if (bar == 0) { invalid("line without a reason: " $0); return }
        why = substr($0, bar + 1)
        gsub(/^[[:space:]]+|[[:space:]]+$/, "", why)
        n = split(substr($0, 1, bar - 1), field, " ")
        if (n != 3) { invalid("not `<path> <family> <count> | <reason>`: " $0); return }
        path = field[1]; family = field[2]; count = field[3]
        if (why == "") invalid("empty reason: " path)
        if (family != "game" && family != "generator" && family != "visual") invalid("unknown family " family ": " path)
        if (count !~ /^[1-9][0-9]*$/) invalid("count not a positive integer: " $0)
        if ((getline probe < path) < 0) invalid("listed file missing: " path)
        close(path)
        if ((path " " family) in want) invalid("duplicate line: " path " " family)
        want[path " " family] = count
        entries++
    }

    # Comments become a space and string or character literals empty quotes,
    # so a name in a comment or a message is not a call.
    function source_line(    line, code, i, token, word, family) {
        line = $0
        code = ""
        while (line != "") {
            if (block) {
                i = index(line, "*/")
                if (i == 0) break
                line = substr(line, i + 2)
                block = 0
                code = code " "
                continue
            }
            if (!match(line, "/\\*|//|\"|" sq)) { code = code line; break }
            code = code substr(line, 1, RSTART - 1)
            token = substr(line, RSTART, RLENGTH)
            line = substr(line, RSTART + RLENGTH)
            if (token == "//") break
            if (token == "/*") { block = 1; code = code " "; continue }
            if (match(line, "^([^" token "\\\\]|\\\\.)*" token)) line = substr(line, RLENGTH + 1)
            else line = ""
            code = code token token
        }
        while (match(code, /[A-Za-z0-9_]+/)) {
            word = substr(code, RSTART, RLENGTH)
            code = substr(code, RSTART + RLENGTH)
            if (word == "p8_fixed_rnd_from_state") family = "generator"
            else if (word ~ /^p8_fixed_rnd/ || word == "p8_fixed_set_rnd_state") family = "game"
            else if (word ~ /^visual_random_draw/) family = "visual"
            else continue
            got[FILENAME " " family]++
            used[FILENAME] = 1
            calls++
        }
    }

    FILENAME == list { list_line(); next }
    { source_line() }

    END {
        if (entries == 0 && !bad_list) invalid("empty list")
        if (bad_list) exit 2
        n = 0
        for (key in got) {
            split(key, part, " ")
            if (!(key in want)) {
                out[++n] = sprintf("random_guard: %s uses the %s family %d time(s), not listed", part[1], part[2], got[key])
            } else if (got[key] != want[key]) {
                out[++n] = sprintf("random_guard: %s uses the %s family %d time(s), listed %d", part[1], part[2], got[key], want[key])
            }
        }
        for (key in want) {
            split(key, part, " ")
            if (!(key in got)) out[++n] = sprintf("random_guard: %s no longer uses the %s family, listed %d", part[1], part[2], want[key])
        }
        sorted_print(out, n)
        used_files = 0
        for (f in used) used_files++
        printf "random-guard files %d calls %d\n", used_files, calls + 0
        if (n > 0) print "random_guard: if intended, edit " list " by hand"
        exit (n > 0)
    }' "$list" $files
