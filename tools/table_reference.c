/* Host tool: writes to argv[1] the C data tables and constants that
 * tests/lua/test_modules.lua compares with their Lua copies, one unit per C
 * source of tools/table_reference/, named in the Makefile's
 * TABLE_REFERENCE_SOURCES. Each table is a header line
 * "= <module> <name> <bytes of its lines>", then its lines in the order the
 * Lua side renders them: integer keys ascending, then string keys by path
 * text, depth first. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "table_reference/table_reference.h"

#define X(unit) void table_reference_##unit(void);
TABLE_REFERENCE_UNITS
#undef X

typedef struct {
    char *text;
    size_t id_length;
    size_t key_length;
} reference_line_t;

static reference_line_t *lines;
static size_t line_count;
static size_t line_capacity;

static void fail(const char *message) {
    fprintf(stderr, "table_reference: %s\n", message);
    exit(1);
}

static void add_line(const char *module, const char *value, const char *path_format, va_list args) {
    char path[256];
    int path_length = vsnprintf(path, sizeof path, path_format, args);
    if (path_length < 0 || (size_t)path_length >= sizeof path) fail("path too long");
    size_t name_length = strspn(path, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
    if (name_length == 0) fail("path without a table name");
    size_t length = strlen(module) + 1 + (size_t)path_length + 1 + strlen(value);
    char *text = malloc(length + 1);
    if (text == NULL) fail("out of memory");
    snprintf(text, length + 1, "%s %s %s", module, path, value);
    if (line_count == line_capacity) {
        line_capacity = line_capacity == 0 ? 4096 : line_capacity * 2;
        lines = realloc(lines, line_capacity * sizeof lines[0]);
        if (lines == NULL) fail("out of memory");
    }
    lines[line_count].text = text;
    lines[line_count].id_length = strlen(module) + 1 + name_length;
    lines[line_count].key_length = strlen(module) + 1 + (size_t)path_length;
    line_count++;
}

void table_reference_int(const char *module, long long value, const char *path_format, ...) {
    char text[32];
    snprintf(text, sizeof text, "%lld", value);
    va_list args;
    va_start(args, path_format);
    add_line(module, text, path_format, args);
    va_end(args);
}

void table_reference_function(const char *module, int present, const char *path_format, ...) {
    if (!present) {
        return;
    }
    va_list args;
    va_start(args, path_format);
    add_line(module, "function", path_format, args);
    va_end(args);
}

void table_reference_pointer(const char *module, const char *name, const char *path_format, ...) {
    if (name == NULL) {
        return;
    }
    char text[128];
    snprintf(text, sizeof text, "&%s", name);
    va_list args;
    va_start(args, path_format);
    add_line(module, text, path_format, args);
    va_end(args);
}

/* The quoting of test_modules.lua: control, quote, backslash and non-ASCII bytes as \ddd. */
void table_reference_string(const char *module, const char *text, const char *path_format, ...) {
    char quoted[1024];
    size_t used = 0;
    quoted[used++] = '"';
    for (const unsigned char *c = (const unsigned char *)text; *c != '\0'; c++) {
        if (used + 6 >= sizeof quoted) fail("string too long");
        if (*c < 32 || *c == 127 || *c == '"' || *c == '\\' || *c >= 128) {
            used += (size_t)snprintf(quoted + used, sizeof quoted - used, "\\%03d", *c);
        } else {
            quoted[used++] = (char)*c;
        }
    }
    quoted[used++] = '"';
    quoted[used] = '\0';
    va_list args;
    va_start(args, path_format);
    add_line(module, quoted, path_format, args);
    va_end(args);
}

/* One step of a path: an integer key, or a string key spelled ".name" or
 * "[\"text\"]", compared by its spelling. Returns the length consumed. */
static size_t path_step(const char *path, size_t length, int *is_integer, long long *integer) {
    if (path[0] == '[' && path[1] != '"') {
        char *end;
        *is_integer = 1;
        *integer = strtoll(path + 1, &end, 10);
        return (size_t)(end - path) + 1;
    }
    *is_integer = 0;
    size_t i = 1;
    if (path[0] == '[') {
        const char *close = strstr(path + 2, "\"]");
        if (close == NULL) fail("unterminated string key");
        return (size_t)(close - path) + 2;
    }
    while (i < length && path[i] != '.' && path[i] != '[') i++;
    return i;
}

static int compare_paths(const char *a, size_t a_length, const char *b, size_t b_length) {
    while (a_length > 0 && b_length > 0) {
        int a_integer, b_integer;
        long long a_value = 0, b_value = 0;
        size_t a_step = path_step(a, a_length, &a_integer, &a_value);
        size_t b_step = path_step(b, b_length, &b_integer, &b_value);
        if (a_integer != b_integer) return a_integer ? -1 : 1;
        if (a_integer && a_value != b_value) return a_value < b_value ? -1 : 1;
        if (!a_integer) {
            int order = memcmp(a, b, a_step < b_step ? a_step : b_step);
            if (order != 0) return order;
            if (a_step != b_step) return a_step < b_step ? -1 : 1;
        }
        a += a_step;
        a_length -= a_step;
        b += b_step;
        b_length -= b_step;
    }
    return (a_length > 0) - (b_length > 0);
}

static int compare_lines(const void *left, const void *right) {
    const reference_line_t *a = left;
    const reference_line_t *b = right;
    size_t shorter = a->id_length < b->id_length ? a->id_length : b->id_length;
    int order = memcmp(a->text, b->text, shorter);
    if (order != 0) return order;
    if (a->id_length != b->id_length) return a->id_length < b->id_length ? -1 : 1;
    return compare_paths(a->text + a->id_length, a->key_length - a->id_length, b->text + b->id_length,
                         b->key_length - b->id_length);
}

static void write_lines(FILE *out) {
    qsort(lines, line_count, sizeof lines[0], compare_lines);
    for (size_t first = 0; first < line_count;) {
        size_t end = first;
        size_t bytes = 0;
        while (end < line_count && lines[end].id_length == lines[first].id_length &&
               memcmp(lines[end].text, lines[first].text, lines[first].id_length) == 0) {
            bytes += strlen(lines[end].text) + 1;
            end++;
        }
        fprintf(out, "= %.*s %zu\n", (int)lines[first].id_length, lines[first].text, bytes);
        for (size_t i = first; i < end; i++) {
            fprintf(out, "%s\n", lines[i].text);
        }
        first = end;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: table_reference <output file>\n");
        return 2;
    }
#define X(unit) table_reference_##unit();
    TABLE_REFERENCE_UNITS
#undef X
    FILE *out = fopen(argv[1], "w");
    if (out == NULL) fail("cannot write the output file");
    write_lines(out);
    if (fclose(out) != 0) fail("write failed");
    return 0;
}
