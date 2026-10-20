#!/bin/sh

set -u

fail() {
    echo "button_guard: $1" >&2
    exit 2
}

[ $# -eq 1 ] || fail "usage: button_guard.sh <directory>"
dir=${1%/}

[ -d "$dir" ] || fail "directory not found: $dir"
[ -n "$(find "$dir" -type f \( -name '*.c' -o -name '*.h' \) -print -quit)" ] ||
    fail "no C file under: $dir"

files=$(grep -rl --include='*.c' --include='*.h' 'P8_PAD_BUTTON_' "$dir")

exec awk -v dir="$dir" -v sq="'" '
    BEGIN {
        exempt[dir "/cheat.c"] = 1
        exempt[dir "/bindings.c"] = 1
        exempt[dir "/bindings.h"] = 1
    }

    function sorted_print(lines, n,    i, j, t) {
        for (i = 2; i <= n; i++) {
            t = lines[i]
            for (j = i - 1; j >= 1 && lines[j] > t; j--) lines[j + 1] = lines[j]
            lines[j + 1] = t
        }
        for (i = 1; i <= n; i++) print lines[i]
    }

    function source_line(    line, code, i, token, word) {
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
            if (word !~ /^P8_PAD_BUTTON_/) continue
            got[FILENAME]++
            reads++
        }
    }

    { source_line() }

    END {
        n = 0
        used_files = 0
        for (path in got) {
            used_files++
            if (!(path in exempt)) out[++n] = sprintf("button_guard: %s names %d button(s) outside cheat.c and the bindings module", path, got[path])
        }
        sorted_print(out, n)
        printf "button-guard files %d reads %d\n", used_files, reads + 0
        exit (n > 0)
    }' /dev/null $files
