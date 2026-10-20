#!/bin/sh
# usage: pointer_guard.sh <exception list> <source file>...
# Counts, per source file, the data pointers declared in struct fields and
# file-scope variables, the declarations of type entity_list_t, and the local
# arrays of entity pointers (entity_t *name[...], const and static included),
# and requires strict equality with the list. Exit 1 on any difference, 2 with
# no source file or an unreadable list: a guard over nothing would pass for
# green.

set -u

list=$1
shift

if [ $# -eq 0 ]; then
    echo "pointer_guard: no source file" >&2
    exit 2
fi

if [ ! -r "$list" ]; then
    echo "pointer_guard: unreadable list: $list" >&2
    exit 2
fi

report=$(mktemp)
summary=$(mktemp)
trap 'rm -f "$report" "$summary"' EXIT

awk -v list="$list" -v summary="$summary" '
function reset_file() {
    pending = ""
    depth = 0
    in_comment = 0
    in_directive = 0
}

# A block comment left open carries over to the next line through in_comment.
function strip(text,    out, token) {
    out = ""
    for (;;) {
        if (in_comment) {
            if (!match(text, /\*\//)) return out
            text = substr(text, RSTART + 2)
            in_comment = 0
            out = out " "
        }
        if (!match(text, /\/\*|\/\/|"([^"\\]|\\.)*"|\047([^\047\\]|\\.)*\047/)) return out text
        out = out substr(text, 1, RSTART - 1)
        token = substr(text, RSTART, 2)
        text = substr(text, RSTART + RLENGTH)
        if (token == "//") return out
        if (token == "/*") in_comment = 1
        else out = out "0"
    }
}

function split_declarators(statement, decls,    count, i, c, nesting, start) {
    if (statement !~ /\(/) return split(statement, decls, ",")
    count = 0
    nesting = 0
    start = 1
    for (i = 1; i <= length(statement); i++) {
        c = substr(statement, i, 1)
        if (c == "(") nesting++
        else if (c == ")") nesting--
        else if (c == "," && nesting == 0) {
            decls[++count] = substr(statement, start, i - start)
            start = i + 1
        }
    }
    decls[++count] = substr(statement, start)
    return count
}

function examine(statement,    count, decls, i, type, is_list) {
    if (statement ~ /(^|[^A-Za-z0-9_])(typedef|extern)([^A-Za-z0-9_]|$)/) return
    gsub(/\[[^]]*\]/, "[]", statement)
    count = split_declarators(statement, decls)
    for (i = 1; i <= count; i++) sub(/=.*/, "", decls[i])
    type = " " decls[1]
    sub(/[A-Za-z_][A-Za-z0-9_]*[ \t]*(\[\][ \t]*)*$/, "", type)
    if (type ~ /[^A-Za-z0-9_](allocator_t|p8_graphics_t|p8_vram_t)([^A-Za-z0-9_]|$)/) return
    is_list = (type ~ /[^A-Za-z0-9_]entity_list_t([^A-Za-z0-9_]|$)/)
    for (i = 1; i <= count; i++) {
        if (decls[i] ~ /\(/) continue
        if (decls[i] ~ /\*/) pointers[FILENAME]++
        if (is_list) lists[FILENAME]++
    }
}

function examine_local(statement,    count, decls, i) {
    if (statement !~ /^[ \t]*((static|const|volatile)[ \t]+)*entity_t[^A-Za-z0-9_]/) return
    gsub(/\[[^]]*\]/, "[]", statement)
    sub(/^[ \t]*((static|const|volatile)[ \t]+)*entity_t/, "", statement)
    count = split_declarators(statement, decls)
    for (i = 1; i <= count; i++) {
        sub(/=.*/, "", decls[i])
        if (decls[i] ~ /^[ \t]*\*[ \t]*(const[ \t]+)?[A-Za-z_][A-Za-z0-9_]*[ \t]*\[\]/) arrays[FILENAME]++
    }
}

function open_brace() {
    depth++
    if (pending ~ /(^|[^A-Za-z0-9_])(struct|union)([ \t]+[A-Za-z_][A-Za-z0-9_]*)?[ \t]*$/) kind[depth] = "struct"
    else if (pending ~ /(^|[^A-Za-z0-9_])enum([ \t]+[A-Za-z_][A-Za-z0-9_]*)?[ \t]*$/) kind[depth] = "enum"
    else if (pending ~ /(^|[^=!<>])=([^=]|$)/) kind[depth] = "init"
    else kind[depth] = "other"
    head[depth] = pending
    pending = ""
}

# The text before a struct, enum or initializer body is the start of the
# statement that the closing brace continues.
function close_brace() {
    if (depth == 0) return
    pending = (kind[depth] == "other") ? "" : head[depth] " "
    depth--
}

# From the arguments, not from FNR == 1: an empty file has no first line.
BEGIN { for (i = 1; i < ARGC; i++) seen[ARGV[i]] = 1 }

FNR == 1 { reset_file() }

{
    line = strip($0)
    if (in_directive || line ~ /^[ \t]*#/) {
        in_directive = ($0 ~ /\\$/)
        next
    }
    while (match(line, /[;{}]/)) {
        pending = pending substr(line, 1, RSTART - 1)
        c = substr(line, RSTART, 1)
        line = substr(line, RSTART + 1)
        if (c == ";") {
            if (depth == 0 || kind[depth] == "struct") examine(pending)
            else if (kind[depth] == "other") examine_local(pending)
            pending = ""
        } else if (c == "{") {
            open_brace()
        } else {
            close_brace()
        }
    }
    pending = pending line " "
}

function compare(what, prefix, got, listed, seen,    f, n) {
    for (f in seen) {
        n = got[f] + 0
        if (n > 0 && !(f in listed))
            printf "pointer_guard: %s has %d %s, not listed; line to add: %s%s %d\n", f, n, what, prefix, f, n
        else if ((f in listed) && n == 0)
            printf "pointer_guard: %s no longer has any %s; line to remove: %s%s %s\n", f, what, prefix, f, listed[f]
        else if ((f in listed) && n != listed[f])
            printf "pointer_guard: %s has %d %s, %s listed; line to write: %s%s %d\n", f, n, what, listed[f], prefix, f, n
    }
    for (f in listed)
        if (!(f in seen))
            printf "pointer_guard: %s is not among the files given; line to remove: %s%s %s\n", f, prefix, f, listed[f]
}

END {
    while ((getline entry < list) > 0) {
        sub(/#.*/, "", entry)
        fields = split(entry, field, " ")
        if (fields == 2) listed_pointers[field[1]] = field[2]
        else if (fields == 3 && field[1] == "entity-lists") listed_lists[field[2]] = field[3]
        else if (fields == 3 && field[1] == "entity-arrays") listed_arrays[field[2]] = field[3]
    }
    compare("pointer(s)", "", pointers, listed_pointers, seen)
    compare("entity list(s)", "entity-lists ", lists, listed_lists, seen)
    compare("local entity array(s)", "entity-arrays ", arrays, listed_arrays, seen)
    for (f in seen) {
        files++
        pointers_total += pointers[f]
        lists_total += lists[f]
        arrays_total += arrays[f]
    }
    printf "pointer-guard files %d pointers %d entity-lists %d entity-arrays %d\n", files, pointers_total, lists_total,
        arrays_total > summary
}' "$@" | LC_ALL=C sort > "$report"
cat "$report" "$summary"
[ ! -s "$report" ] || { echo "pointer_guard: if intended, edit $list by hand"; exit 1; }
