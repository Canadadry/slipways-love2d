#include "replay.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NO_NODE ((size_t)-1)
#define MAX_DEPTH 64

typedef enum {
    NODE_NULL,
    NODE_FALSE,
    NODE_TRUE,
    NODE_INTEGER,
    NODE_STRING,
    NODE_ARRAY,
    NODE_OBJECT
} node_kind_t;

typedef struct {
    node_kind_t kind;
    size_t offset;
    const char *key;
    const char *string;
    size_t string_length;
    long long integer;
    size_t first_child;
    size_t child_count;
    size_t next;
} node_t;

struct replay {
    const node_t *frames;
    size_t node_count;
    node_t nodes[];
};

/* With `nodes` NULL the parser only validates and counts, so that the second
 * pass fills a single allocation of the exact size. */
typedef struct {
    const char *name;
    const char *text;
    size_t length;
    size_t pos;
    node_t *nodes;
    size_t node_count;
    char *chars;
    size_t char_count;
    char *error;
    size_t error_size;
    int failed;
} parser_t;

static void fail_at(parser_t *parser, size_t offset, const char *format, ...) {
    if (parser->failed) {
        return;
    }
    parser->failed = 1;
    char problem[160];
    va_list args;
    va_start(args, format);
    vsnprintf(problem, sizeof(problem), format, args);
    va_end(args);
    if (parser->error_size > 0) {
        snprintf(parser->error, parser->error_size, "%s: offset %zu: %s", parser->name, offset, problem);
    }
}

static int at_end(const parser_t *parser) {
    return parser->pos >= parser->length;
}

static char peek(const parser_t *parser) {
    return at_end(parser) ? '\0' : parser->text[parser->pos];
}

static void skip_whitespace(parser_t *parser) {
    while (!at_end(parser)) {
        char c = parser->text[parser->pos];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return;
        }
        parser->pos++;
    }
}

static size_t new_node(parser_t *parser, node_kind_t kind) {
    size_t index = parser->node_count++;
    if (parser->nodes != NULL) {
        node_t *node = &parser->nodes[index];
        memset(node, 0, sizeof(*node));
        node->kind = kind;
        node->offset = parser->pos;
        node->first_child = NO_NODE;
        node->next = NO_NODE;
    }
    return index;
}

static void put_char(parser_t *parser, char c) {
    if (parser->chars != NULL) {
        parser->chars[parser->char_count] = c;
    }
    parser->char_count++;
}

static void put_utf8(parser_t *parser, unsigned long code) {
    if (code < 0x80) {
        put_char(parser, (char)code);
    } else if (code < 0x800) {
        put_char(parser, (char)(0xC0 | (code >> 6)));
        put_char(parser, (char)(0x80 | (code & 0x3F)));
    } else if (code < 0x10000) {
        put_char(parser, (char)(0xE0 | (code >> 12)));
        put_char(parser, (char)(0x80 | ((code >> 6) & 0x3F)));
        put_char(parser, (char)(0x80 | (code & 0x3F)));
    } else {
        put_char(parser, (char)(0xF0 | (code >> 18)));
        put_char(parser, (char)(0x80 | ((code >> 12) & 0x3F)));
        put_char(parser, (char)(0x80 | ((code >> 6) & 0x3F)));
        put_char(parser, (char)(0x80 | (code & 0x3F)));
    }
}

/* Reads the 4 hex digits after `\u`; returns ULONG_MAX on failure. */
static unsigned long read_hex4(parser_t *parser) {
    unsigned long code = 0;
    for (int i = 0; i < 4; i++) {
        char c = peek(parser);
        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            fail_at(parser, parser->pos, "bad \\u escape: 4 hex digits expected");
            return ULONG_MAX;
        }
        code = code * 16 + (unsigned long)digit;
        parser->pos++;
    }
    return code;
}

static int parse_unicode_escape(parser_t *parser, size_t escape_offset) {
    unsigned long code = read_hex4(parser);
    if (code == ULONG_MAX) {
        return 0;
    }
    if (code >= 0xDC00 && code <= 0xDFFF) {
        fail_at(parser, escape_offset, "bad \\u escape: lone low surrogate");
        return 0;
    }
    if (code >= 0xD800 && code <= 0xDBFF) {
        if (peek(parser) != '\\' || parser->pos + 1 >= parser->length || parser->text[parser->pos + 1] != 'u') {
            fail_at(parser, escape_offset, "bad \\u escape: high surrogate without its low half");
            return 0;
        }
        parser->pos += 2;
        unsigned long low = read_hex4(parser);
        if (low == ULONG_MAX) {
            return 0;
        }
        if (low < 0xDC00 || low > 0xDFFF) {
            fail_at(parser, escape_offset, "bad \\u escape: high surrogate without its low half");
            return 0;
        }
        code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
    }
    put_utf8(parser, code);
    return 1;
}

/* Returns the copy's start in `chars` (NUL-terminated), or NULL on the
 * counting pass and on failure; `failed` tells the two apart. */
static const char *parse_string(parser_t *parser, size_t *length) {
    size_t start_offset = parser->pos;
    assert(peek(parser) == '"' && "parse_string starts on the opening quote");
    parser->pos++;
    size_t start = parser->char_count;
    for (;;) {
        if (at_end(parser)) {
            fail_at(parser, start_offset, "unterminated string");
            return NULL;
        }
        unsigned char c = (unsigned char)parser->text[parser->pos];
        if (c == '"') {
            parser->pos++;
            break;
        }
        if (c < 0x20) {
            fail_at(parser, parser->pos, "control character in a string");
            return NULL;
        }
        if (c != '\\') {
            put_char(parser, (char)c);
            parser->pos++;
            continue;
        }
        size_t escape_offset = parser->pos;
        parser->pos++;
        char escaped = peek(parser);
        parser->pos++;
        switch (escaped) {
        case '"': put_char(parser, '"'); break;
        case '\\': put_char(parser, '\\'); break;
        case '/': put_char(parser, '/'); break;
        case 'b': put_char(parser, '\b'); break;
        case 'f': put_char(parser, '\f'); break;
        case 'n': put_char(parser, '\n'); break;
        case 'r': put_char(parser, '\r'); break;
        case 't': put_char(parser, '\t'); break;
        case 'u':
            if (!parse_unicode_escape(parser, escape_offset)) {
                return NULL;
            }
            break;
        default:
            fail_at(parser, escape_offset, "bad escape in a string");
            return NULL;
        }
    }
    *length = parser->char_count - start;
    put_char(parser, '\0');
    return parser->chars != NULL ? parser->chars + start : NULL;
}

static int is_digit(char c) {
    return c >= '0' && c <= '9';
}

static size_t parse_integer(parser_t *parser) {
    size_t index = new_node(parser, NODE_INTEGER);
    size_t start = parser->pos;
    int negative = 0;
    if (peek(parser) == '-') {
        negative = 1;
        parser->pos++;
    }
    if (!is_digit(peek(parser))) {
        fail_at(parser, start, "a digit is expected after '-'");
        return NO_NODE;
    }
    if (peek(parser) == '0' && parser->pos + 1 < parser->length && is_digit(parser->text[parser->pos + 1])) {
        fail_at(parser, start, "integer with a leading zero");
        return NO_NODE;
    }
    long long value = 0;
    while (is_digit(peek(parser))) {
        int digit = peek(parser) - '0';
        if (value > (LLONG_MAX - digit) / 10) {
            fail_at(parser, start, "integer out of range");
            return NO_NODE;
        }
        value = value * 10 + digit;
        parser->pos++;
    }
    if (peek(parser) == '.') {
        fail_at(parser, start, "number with a fraction: a replay holds integers only");
        return NO_NODE;
    }
    if (peek(parser) == 'e' || peek(parser) == 'E') {
        fail_at(parser, start, "number with an exponent: a replay holds integers only");
        return NO_NODE;
    }
    if (parser->nodes != NULL) {
        parser->nodes[index].integer = negative ? -value : value;
    }
    return index;
}

static size_t parse_literal(parser_t *parser, const char *word, node_kind_t kind) {
    size_t word_length = strlen(word);
    if (parser->length - parser->pos < word_length || memcmp(parser->text + parser->pos, word, word_length) != 0) {
        fail_at(parser, parser->pos, "not JSON: unexpected character '%c'", peek(parser));
        return NO_NODE;
    }
    size_t index = new_node(parser, kind);
    parser->pos += word_length;
    return index;
}

static size_t parse_value(parser_t *parser, int depth);

static void append_child(parser_t *parser, size_t parent, size_t *last, size_t child) {
    if (parser->nodes != NULL) {
        if (*last == NO_NODE) {
            parser->nodes[parent].first_child = child;
        } else {
            parser->nodes[*last].next = child;
        }
        parser->nodes[parent].child_count++;
    }
    *last = child;
}

static size_t parse_container(parser_t *parser, int depth, node_kind_t kind) {
    char close = kind == NODE_OBJECT ? '}' : ']';
    if (depth >= MAX_DEPTH) {
        fail_at(parser, parser->pos, "nesting deeper than %d", MAX_DEPTH);
        return NO_NODE;
    }
    size_t index = new_node(parser, kind);
    parser->pos++;
    skip_whitespace(parser);
    if (peek(parser) == close) {
        parser->pos++;
        return index;
    }
    size_t last = NO_NODE;
    for (;;) {
        const char *key = NULL;
        if (kind == NODE_OBJECT) {
            skip_whitespace(parser);
            if (peek(parser) != '"') {
                fail_at(parser, parser->pos, "a string key is expected in an object");
                return NO_NODE;
            }
            size_t key_length;
            key = parse_string(parser, &key_length);
            if (parser->failed) {
                return NO_NODE;
            }
            skip_whitespace(parser);
            if (peek(parser) != ':') {
                fail_at(parser, parser->pos, "':' is expected after an object key");
                return NO_NODE;
            }
            parser->pos++;
        }
        size_t child = parse_value(parser, depth + 1);
        if (child == NO_NODE) {
            return NO_NODE;
        }
        if (parser->nodes != NULL) {
            parser->nodes[child].key = key;
        }
        append_child(parser, index, &last, child);
        skip_whitespace(parser);
        if (peek(parser) == ',') {
            parser->pos++;
            continue;
        }
        if (peek(parser) == close) {
            parser->pos++;
            return index;
        }
        if (at_end(parser)) {
            fail_at(parser, parser->pos, "unexpected end of text");
        } else {
            fail_at(parser, parser->pos, "',' or '%c' is expected", close);
        }
        return NO_NODE;
    }
}

static size_t parse_value(parser_t *parser, int depth) {
    skip_whitespace(parser);
    if (at_end(parser)) {
        fail_at(parser, parser->pos, "unexpected end of text");
        return NO_NODE;
    }
    char c = peek(parser);
    switch (c) {
    case '{':
        return parse_container(parser, depth, NODE_OBJECT);
    case '[':
        return parse_container(parser, depth, NODE_ARRAY);
    case '"': {
        size_t index = new_node(parser, NODE_STRING);
        size_t length = 0;
        const char *string = parse_string(parser, &length);
        if (parser->failed) {
            return NO_NODE;
        }
        if (parser->nodes != NULL) {
            parser->nodes[index].string = string;
            parser->nodes[index].string_length = length;
        }
        return index;
    }
    case 't':
        return parse_literal(parser, "true", NODE_TRUE);
    case 'f':
        return parse_literal(parser, "false", NODE_FALSE);
    case 'n':
        return parse_literal(parser, "null", NODE_NULL);
    default:
        if (c == '-' || is_digit(c)) {
            return parse_integer(parser);
        }
        fail_at(parser, parser->pos, "not JSON: unexpected character '%c'", c);
        return NO_NODE;
    }
}

static void parse_document(parser_t *parser) {
    size_t root = parse_value(parser, 0);
    if (root == NO_NODE) {
        return;
    }
    skip_whitespace(parser);
    if (!at_end(parser)) {
        fail_at(parser, parser->pos, "trailing text after the document");
    }
}

static const node_t *object_member(const replay_t *replay, const node_t *object, const char *key) {
    for (size_t i = object->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        if (strcmp(replay->nodes[i].key, key) == 0) {
            return &replay->nodes[i];
        }
    }
    return NULL;
}

static int find_frames(replay_t *replay, parser_t *parser) {
    const node_t *root = &replay->nodes[0];
    if (root->kind != NODE_OBJECT) {
        fail_at(parser, root->offset, "the document is not an object");
        return 0;
    }
    const node_t *frames = object_member(replay, root, "frames");
    if (frames == NULL) {
        fail_at(parser, root->offset, "\"frames\" missing");
        return 0;
    }
    if (frames->kind != NODE_ARRAY) {
        fail_at(parser, frames->offset, "\"frames\" is not an array");
        return 0;
    }
    size_t frame_index = 0;
    for (size_t i = frames->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        if (replay->nodes[i].kind != NODE_OBJECT) {
            fail_at(parser, replay->nodes[i].offset, "frame %zu is not an object", frame_index);
            return 0;
        }
        frame_index++;
    }
    replay->frames = frames;
    return 1;
}

replay_t *replay_parse(const char *name, const char *text, size_t length, char *error, size_t error_size) {
    assert(name != NULL && (text != NULL || length == 0));
    parser_t parser = {
        .name = name,
        .text = text,
        .length = length,
        .error = error,
        .error_size = error_size,
    };
    parse_document(&parser);
    if (parser.failed) {
        return NULL;
    }
    size_t node_count = parser.node_count;
    size_t char_count = parser.char_count;
    replay_t *replay = malloc(sizeof(*replay) + node_count * sizeof(node_t) + char_count);
    if (replay == NULL) {
        fail_at(&parser, 0, "out of memory");
        return NULL;
    }
    replay->node_count = node_count;
    replay->frames = NULL;
    parser.pos = 0;
    parser.node_count = 0;
    parser.char_count = 0;
    parser.nodes = replay->nodes;
    parser.chars = (char *)(replay->nodes + node_count);
    parse_document(&parser);
    assert(!parser.failed && parser.node_count == node_count && parser.char_count == char_count
           && "the filling pass reads the text the counting pass accepted");
    if (!find_frames(replay, &parser)) {
        free(replay);
        return NULL;
    }
    return replay;
}

replay_t *replay_read(const char *path, char *error, size_t error_size) {
    assert(path != NULL);
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        snprintf(error, error_size, "%s: cannot open: %s", path, strerror(errno));
        return NULL;
    }
    size_t capacity = 1 << 16;
    size_t length = 0;
    char *text = malloc(capacity);
    while (text != NULL) {
        length += fread(text + length, 1, capacity - length, file);
        if (length < capacity) {
            break;
        }
        capacity *= 2;
        char *grown = realloc(text, capacity);
        if (grown == NULL) {
            free(text);
        }
        text = grown;
    }
    int read_failed = ferror(file);
    fclose(file);
    if (text == NULL) {
        snprintf(error, error_size, "%s: out of memory", path);
        return NULL;
    }
    if (read_failed) {
        free(text);
        snprintf(error, error_size, "%s: read error", path);
        return NULL;
    }
    replay_t *replay = replay_parse(path, text, length, error, error_size);
    free(text);
    return replay;
}

size_t replay_frame_count(const replay_t *replay) {
    assert(replay != NULL);
    return replay->frames->child_count;
}

void replay_free(replay_t *replay) {
    free(replay);
}
