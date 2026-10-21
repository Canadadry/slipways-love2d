#!/bin/sh
# usage: fake_love_guard.sh <object root> <binary> <object.o>... [<binary> <object.o>...]... -- <source or directory>...
# Fails when the fake side uses src/: an object built from <root>/src/, a
# dependency file (<object>.d) naming a header under src/, or a source (the
# .c and .h files given or under the directories) whose #include names src/
# or one of its directories. Exit 1 naming each offender, 2 with no object,
# no source or a missing dependency file: a guard over nothing would pass
# for green.

set -u

fail() {
    echo "fake_love_guard: $1" >&2
    exit 2
}

[ $# -ge 1 ] || fail "usage: fake_love_guard.sh <object root> <binary> <object.o>... -- <source or directory>..."
root=${1%/}
shift

links=
dependencies=
binary=
while [ $# -gt 0 ] && [ "$1" != -- ]; do
    case $1 in
    *.o)
        [ -n "$binary" ] || fail "$1 follows no binary"
        [ -f "${1%.o}.d" ] || fail "dependency file not found: ${1%.o}.d"
        links="$links$binary $1
"
        dependencies="$dependencies ${1%.o}.d"
        ;;
    *) binary=$1 ;;
    esac
    shift
done
[ "${1:-}" = -- ] || fail "no -- before the sources"
shift
[ -n "$links" ] || fail "no object"
[ $# -gt 0 ] || fail "no source"
sources=$(find "$@" -type f \( -name '*.c' -o -name '*.h' \) | LC_ALL=C sort -u)
[ -n "$sources" ] || fail "no C file in: $*"

printf '%s' "$links" | awk -v root="$root" -v here="$(pwd)" -v sources=$(echo "$sources" | wc -l) '
    function sorted_print(lines, n,    i, j, t) {
        for (i = 2; i <= n; i++) {
            t = lines[i]
            for (j = i - 1; j >= 1 && lines[j] > t; j--) lines[j + 1] = lines[j]
            lines[j + 1] = t
        }
        for (i = 1; i <= n; i++) print lines[i]
    }

    function report(line) {
        if (!(line in reported)) out[++failing] = line
        reported[line] = 1
    }

    # Resolves . and .. so that a/../../src/x.h counts: the compiler writes a
    # header path as the include reached it.
    function under_src(path,    parts, n, i, depth, first) {
        if (index(path, here "/") == 1) path = substr(path, length(here) + 2)
        else if (substr(path, 1, 1) == "/") return 0
        n = split(path, parts, "/")
        depth = 0
        for (i = 1; i < n; i++) {
            if (parts[i] == "" || parts[i] == ".") continue
            if (parts[i] != "..") {
                if (++depth == 1) first = parts[i]
            } else if (--depth < 0) {
                return 0
            }
        }
        return depth >= 1 && first == "src"
    }

    FILENAME == "-" {
        objects[$2] = 1
        if (index($2, root "/src/") == 1) report("fake_love_guard: " $1 " links " $2 ", built from src/")
        next
    }

    FILENAME ~ /\.d$/ {
        object = substr(FILENAME, 1, length(FILENAME) - 2) ".o"
        for (i = 1; i <= NF; i++) {
            if ($i == "\\" || $i ~ /:$/ || (FNR == 1 && i == 2)) continue
            if (under_src($i)) report("fake_love_guard: " object " includes " $i)
        }
        next
    }

    /^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]/ {
        path = $0
        sub(/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]/, "", path)
        sub(/[>"].*/, "", path)
        if (path ~ /(^|\/)src\// || path ~ /^(p8|engine|game|platform|devtools|cmd)\//)
            report("fake_love_guard: " FILENAME ":" FNR " includes " path)
    }

    END {
        n = 0
        for (object in objects) n++
        sorted_print(out, failing)
        printf "fake-love-guard objects %d sources %d failing %d\n", n, sources, failing
        exit (failing > 0)
    }' - $dependencies $sources
