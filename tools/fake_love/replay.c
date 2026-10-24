#include "replay.h"

#include "pad.h"

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

typedef struct {
    unsigned held;
    int resize_width;
    int resize_height;
    const replay_touch_t *touches;
    size_t touch_count;
    replay_mouse_t mouse;
} frame_input_t;

struct replay {
    const node_t *frames;
    frame_input_t *inputs;
    replay_touch_t *touches;
    size_t touch_count;
    unsigned char *initial_save;
    size_t initial_save_count;
    const char *bindings;
    const char *pad_sheet;
    int attract_after;
    int window_width;
    int window_height;
    size_t screens[REPLAY_SCREENS_MAX];
    size_t screen_count;
    size_t node_count;
    node_t nodes[];
};

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

static int check_input(const replay_t *replay, parser_t *parser) {
    const node_t *root = &replay->nodes[0];
    const node_t *input = object_member(replay, root, "input");
    if (input == NULL) {
        fail_at(parser, root->offset, "\"input\" missing: a replay holds \"input\": \"pad\"");
        return 0;
    }
    if (input->kind != NODE_STRING) {
        fail_at(parser, input->offset, "\"input\" is not a string: a replay holds \"input\": \"pad\"");
        return 0;
    }
    if (strcmp(input->string, "pad") != 0) {
        fail_at(parser, input->offset, "\"input\" is \"%s\", not \"pad\"", input->string);
        return 0;
    }
    return 1;
}

static const char *kind_name(node_kind_t kind) {
    switch (kind) {
    case NODE_NULL: return "null";
    case NODE_FALSE: return "false";
    case NODE_TRUE: return "true";
    case NODE_INTEGER: return "an integer";
    case NODE_STRING: return "a string";
    case NODE_ARRAY: return "an array";
    case NODE_OBJECT: return "an object";
    }
    return "?";
}

static int read_frame_pad(const replay_t *replay, parser_t *parser, size_t frame_index, const node_t *pad, unsigned *held) {
    if (pad->kind != NODE_ARRAY) {
        fail_at(parser, pad->offset, "f%zu: \"pad\" is %s, not an array", frame_index, kind_name(pad->kind));
        return 0;
    }
    *held = 0;
    for (size_t i = pad->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        const node_t *button = &replay->nodes[i];
        if (button->kind == NODE_STRING) {
            fail_at(parser, button->offset, "f%zu: button \"%s\" is not an integer", frame_index, button->string);
            return 0;
        }
        if (button->kind != NODE_INTEGER) {
            fail_at(parser, button->offset, "f%zu: button %s is not an integer", frame_index, kind_name(button->kind));
            return 0;
        }
        if (button->integer < 1 || button->integer > PAD_BUTTON_COUNT) {
            fail_at(parser, button->offset, "f%zu: button %lld is not 1-%d", frame_index, button->integer, PAD_BUTTON_COUNT);
            return 0;
        }
        unsigned bit = 1u << (button->integer - 1);
        if (*held & bit) {
            fail_at(parser, button->offset, "f%zu: button %lld listed twice", frame_index, button->integer);
            return 0;
        }
        *held |= bit;
    }
    return 1;
}

static int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

static const char *describe_value(const node_t *value, char *out, size_t size) {
    if (value->kind == NODE_STRING) {
        snprintf(out, size, "\"%.40s\"%s", value->string, value->string_length > 40 ? "..." : "");
    } else if (value->kind == NODE_INTEGER) {
        snprintf(out, size, "%lld", value->integer);
    } else {
        snprintf(out, size, "%s", kind_name(value->kind));
    }
    return out;
}

static int read_initial_save(replay_t *replay, parser_t *parser, const node_t *member) {
    char value[64];
    describe_value(member, value, sizeof(value));
    if (member->kind != NODE_STRING) {
        fail_at(parser, member->offset, "\"initial_save\" %s is not a string of hex digits", value);
        return 0;
    }
    if (member->string_length == 0) {
        fail_at(parser, member->offset, "\"initial_save\" %s is empty", value);
        return 0;
    }
    if (member->string_length % 2 != 0) {
        fail_at(parser, member->offset, "\"initial_save\" %s has an odd number of digits", value);
        return 0;
    }
    for (size_t i = 0; i < member->string_length; i++) {
        if (hex_digit_value(member->string[i]) < 0) {
            fail_at(parser, member->offset, "\"initial_save\" %s holds a character that is not a hex digit", value);
            return 0;
        }
    }
    size_t count = member->string_length / 2;
    replay->initial_save = malloc(count);
    if (replay->initial_save == NULL) {
        fail_at(parser, member->offset, "out of memory");
        return 0;
    }
    for (size_t i = 0; i < count; i++) {
        int high = hex_digit_value(member->string[2 * i]);
        int low = hex_digit_value(member->string[2 * i + 1]);
        replay->initial_save[i] = (unsigned char)(high * 16 + low);
    }
    replay->initial_save_count = count;
    return 1;
}

#define FIXTURE_NAME_MAX_LENGTH 64

static int is_fixture_name(const char *name, size_t length, const char *extension) {
    if (length < 5 || length > FIXTURE_NAME_MAX_LENGTH || name[0] == '.' || strcmp(name + length - 4, extension) != 0
        || strcmp(name, "bindings.cfg") == 0) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        char c = name[i];
        int allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.'
                      || c == '-';
        if (!allowed) {
            return 0;
        }
    }
    return 1;
}

static int read_bindings(replay_t *replay, parser_t *parser, const node_t *member) {
    char value[64];
    describe_value(member, value, sizeof(value));
    if (member->kind == NODE_STRING && strcmp(member->string, "personal") == 0) {
        fail_at(parser, member->offset,
                "\"bindings\" %s: recorded with a personal bindings.cfg; only a fixture next to the replay runs", value);
        return 0;
    }
    if (member->kind != NODE_STRING || !is_fixture_name(member->string, member->string_length, ".cfg")) {
        fail_at(parser, member->offset,
                "\"bindings\" %s is not a .cfg file name of 5-%d characters A-Za-z0-9_.- next to the replay", value,
                FIXTURE_NAME_MAX_LENGTH);
        return 0;
    }
    replay->bindings = member->string;
    return 1;
}

static int read_pad_sheet(replay_t *replay, parser_t *parser, const node_t *member) {
    if (member->kind != NODE_STRING || !is_fixture_name(member->string, member->string_length, ".png")) {
        char value[64];
        fail_at(parser, member->offset,
                "\"pad_sheet\" %s is not a .png file name of 5-%d characters A-Za-z0-9_.- next to the replay",
                describe_value(member, value, sizeof(value)), FIXTURE_NAME_MAX_LENGTH);
        return 0;
    }
    replay->pad_sheet = member->string;
    return 1;
}

static int read_attract_after(replay_t *replay, parser_t *parser, const node_t *member) {
    if (member->kind != NODE_INTEGER || member->integer < 1 || member->integer > INT_MAX) {
        char value[64];
        fail_at(parser, member->offset, "\"attract_after\" %s is not an integer 1-%d",
                describe_value(member, value, sizeof(value)), INT_MAX);
        return 0;
    }
    replay->attract_after = (int)member->integer;
    return 1;
}

#define WINDOW_SIZE_MAX 4096

static const char *describe_size(const replay_t *replay, const node_t *value, char *out, size_t size) {
    if (value->kind != NODE_ARRAY) {
        return describe_value(value, out, size);
    }
    size_t used = (size_t)snprintf(out, size, "[");
    size_t shown = 0;
    for (size_t i = value->first_child; i != NO_NODE && used < size; i = replay->nodes[i].next) {
        char element[64];
        const char *text = shown == 3 ? "..." : describe_value(&replay->nodes[i], element, sizeof(element));
        used += (size_t)snprintf(out + used, size - used, "%s%s", shown == 0 ? "" : ", ", text);
        if (++shown > 3) {
            break;
        }
    }
    if (used < size) {
        snprintf(out + used, size - used, "]");
    }
    return out;
}

static int read_size(const replay_t *replay, const node_t *value, int *width, int *height) {
    if (value->kind != NODE_ARRAY || value->child_count != 2) {
        return 0;
    }
    const node_t *first = &replay->nodes[value->first_child];
    const node_t *second = &replay->nodes[first->next];
    const node_t *const sides[] = {first, second};
    for (size_t i = 0; i < 2; i++) {
        if (sides[i]->kind != NODE_INTEGER || sides[i]->integer < 1 || sides[i]->integer > WINDOW_SIZE_MAX) {
            return 0;
        }
    }
    *width = (int)first->integer;
    *height = (int)second->integer;
    return 1;
}

static int read_window(replay_t *replay, parser_t *parser, const node_t *member) {
    if (!read_size(replay, member, &replay->window_width, &replay->window_height)) {
        char value[96];
        fail_at(parser, member->offset, "\"window\" %s is not [width, height] with integers 1-%d",
                describe_size(replay, member, value, sizeof(value)), WINDOW_SIZE_MAX);
        return 0;
    }
    return 1;
}

static int read_screens(replay_t *replay, parser_t *parser, const node_t *member) {
    if (member->kind != NODE_ARRAY) {
        fail_at(parser, member->offset, "\"screens\" is %s, not an array of frame numbers", kind_name(member->kind));
        return 0;
    }
    if (member->child_count > REPLAY_SCREENS_MAX) {
        fail_at(parser, member->offset, "\"screens\" lists %zu frames, more than %d", member->child_count,
                REPLAY_SCREENS_MAX);
        return 0;
    }
    size_t frame_count = replay->frames->child_count;
    for (size_t i = member->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        const node_t *entry = &replay->nodes[i];
        char value[64];
        if (entry->kind != NODE_INTEGER) {
            fail_at(parser, entry->offset, "\"screens\" entry %s is not an integer",
                    describe_value(entry, value, sizeof(value)));
            return 0;
        }
        if (entry->integer < 0 || (unsigned long long)entry->integer >= frame_count) {
            char range[48] = "the replay has no frame";
            if (frame_count > 0) {
                snprintf(range, sizeof(range), "0-%zu", frame_count - 1);
            }
            fail_at(parser, entry->offset, "\"screens\" entry %lld is not a frame of this replay (%s)", entry->integer,
                    range);
            return 0;
        }
        size_t frame = (size_t)entry->integer;
        if (replay->screen_count > 0 && frame <= replay->screens[replay->screen_count - 1]) {
            fail_at(parser, entry->offset, "\"screens\" entry %zu is not after %zu", frame,
                    replay->screens[replay->screen_count - 1]);
            return 0;
        }
        replay->screens[replay->screen_count++] = frame;
    }
    return 1;
}

typedef int (*member_reader_t)(replay_t *replay, parser_t *parser, const node_t *member);

static int read_start_state(replay_t *replay, parser_t *parser) {
    static const struct {
        const char *key;
        member_reader_t read;
    } MEMBERS[] = {
        {"initial_save", read_initial_save},
        {"bindings", read_bindings},
        {"pad_sheet", read_pad_sheet},
        {"attract_after", read_attract_after},
        {"window", read_window},
        {"screens", read_screens},
    };
    const node_t *root = &replay->nodes[0];
    const node_t *start = object_member(replay, root, "start");
    if (start != NULL) {
        fail_at(parser, start->offset, "\"start\" is not supported: the fake plays from the title screen only");
        return 0;
    }
    for (size_t k = 0; k < sizeof(MEMBERS) / sizeof(MEMBERS[0]); k++) {
        const node_t *member = object_member(replay, root, MEMBERS[k].key);
        if (member == NULL) {
            continue;
        }
        for (size_t i = member->next; i != NO_NODE; i = replay->nodes[i].next) {
            if (strcmp(replay->nodes[i].key, MEMBERS[k].key) == 0) {
                fail_at(parser, replay->nodes[i].offset, "\"%s\" given twice", MEMBERS[k].key);
                return 0;
            }
        }
        if (!MEMBERS[k].read(replay, parser, member)) {
            return 0;
        }
    }
    return 1;
}

static int read_frame_resize(const replay_t *replay, parser_t *parser, size_t frame_index, const node_t *resize,
                             int *current_width, int *current_height, frame_input_t *input) {
    if (resize == NULL) {
        return 1;
    }
    char value[96];
    describe_size(replay, resize, value, sizeof(value));
    int width = 0;
    int height = 0;
    if (!read_size(replay, resize, &width, &height)) {
        fail_at(parser, resize->offset, "f%zu: \"resize\" %s is not [width, height] with integers 1-%d", frame_index,
                value, WINDOW_SIZE_MAX);
        return 0;
    }
    if (width == *current_width && height == *current_height) {
        fail_at(parser, resize->offset,
                "f%zu: \"resize\" %s is already the window size: LÖVE calls love.resize only on a change", frame_index,
                value);
        return 0;
    }
    input->resize_width = width;
    input->resize_height = height;
    *current_width = width;
    *current_height = height;
    return 1;
}

#define TOUCH_COUNT_MAX 10
#define TOUCH_ID_MAX 2147483647

static int read_three_integers(const replay_t *replay, const node_t *value, long long out[3]) {
    if (value->kind != NODE_ARRAY || value->child_count != 3) {
        return 0;
    }
    size_t i = value->first_child;
    for (size_t k = 0; k < 3; k++, i = replay->nodes[i].next) {
        if (replay->nodes[i].kind != NODE_INTEGER) {
            return 0;
        }
        out[k] = replay->nodes[i].integer;
    }
    return 1;
}

static int is_inside(long long x, long long y, int width, int height) {
    return x >= 0 && x < width && y >= 0 && y < height;
}

static int read_frame_touches(replay_t *replay, parser_t *parser, size_t frame_index, const node_t *touches, int width,
                              int height, frame_input_t *input) {
    input->touches = replay->touches + replay->touch_count;
    input->touch_count = 0;
    if (touches == NULL) {
        return 1;
    }
    if (touches->kind != NODE_ARRAY) {
        fail_at(parser, touches->offset, "f%zu: \"touches\" is %s, not an array of [id, x, y]", frame_index,
                kind_name(touches->kind));
        return 0;
    }
    if (touches->child_count > TOUCH_COUNT_MAX) {
        fail_at(parser, touches->offset, "f%zu: %zu touches, more than %d", frame_index, touches->child_count,
                TOUCH_COUNT_MAX);
        return 0;
    }
    for (size_t i = touches->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        const node_t *touch = &replay->nodes[i];
        long long values[3];
        if (!read_three_integers(replay, touch, values)) {
            char value[96];
            fail_at(parser, touch->offset, "f%zu: touch %s is not [id, x, y] with integers", frame_index,
                    describe_size(replay, touch, value, sizeof(value)));
            return 0;
        }
        if (values[0] < 1 || values[0] > TOUCH_ID_MAX) {
            fail_at(parser, touch->offset, "f%zu: touch id %lld is not 1-%d", frame_index, values[0], TOUCH_ID_MAX);
            return 0;
        }
        for (size_t k = 0; k < input->touch_count; k++) {
            if (input->touches[k].id == values[0]) {
                fail_at(parser, touch->offset, "f%zu: touch id %lld listed twice", frame_index, values[0]);
                return 0;
            }
        }
        if (!is_inside(values[1], values[2], width, height)) {
            fail_at(parser, touch->offset, "f%zu: touch %lld at (%lld, %lld) is outside the %dx%d window", frame_index,
                    values[0], values[1], values[2], width, height);
            return 0;
        }
        replay->touches[replay->touch_count++] = (replay_touch_t){(int)values[0], (int)values[1], (int)values[2]};
        input->touch_count++;
    }
    return 1;
}

static int read_frame_mouse(const replay_t *replay, parser_t *parser, size_t frame_index, const node_t *mouse, int width,
                            int height, replay_mouse_t *current) {
    if (mouse == NULL) {
        return 1;
    }
    long long values[3];
    if (!read_three_integers(replay, mouse, values) || (values[2] != 0 && values[2] != 1)) {
        char value[96];
        fail_at(parser, mouse->offset, "f%zu: \"mouse\" %s is not [x, y, left] with integers, left 0 or 1", frame_index,
                describe_size(replay, mouse, value, sizeof(value)));
        return 0;
    }
    if (!is_inside(values[0], values[1], width, height)) {
        fail_at(parser, mouse->offset, "f%zu: mouse at (%lld, %lld) is outside the %dx%d window", frame_index, values[0],
                values[1], width, height);
        return 0;
    }
    *current = (replay_mouse_t){(int)values[0], (int)values[1], (int)values[2]};
    return 1;
}

static size_t touch_capacity(const replay_t *replay) {
    size_t capacity = 0;
    for (size_t f = replay->frames->first_child; f != NO_NODE; f = replay->nodes[f].next) {
        for (size_t i = replay->nodes[f].first_child; i != NO_NODE; i = replay->nodes[i].next) {
            if (strcmp(replay->nodes[i].key, "touches") == 0 && replay->nodes[i].kind == NODE_ARRAY) {
                capacity += replay->nodes[i].child_count;
            }
        }
    }
    return capacity;
}

enum { FRAME_PAD, FRAME_RESIZE, FRAME_TOUCHES, FRAME_MOUSE, FRAME_KEY_COUNT };

static const char *const FRAME_KEYS[FRAME_KEY_COUNT] = {"pad", "resize", "touches", "mouse"};

static int find_frame_members(const replay_t *replay, parser_t *parser, size_t frame_index, const node_t *frame,
                              const node_t *members[FRAME_KEY_COUNT]) {
    for (size_t k = 0; k < FRAME_KEY_COUNT; k++) {
        members[k] = NULL;
    }
    for (size_t i = frame->first_child; i != NO_NODE; i = replay->nodes[i].next) {
        const node_t *member = &replay->nodes[i];
        size_t k = 0;
        while (k < FRAME_KEY_COUNT && strcmp(member->key, FRAME_KEYS[k]) != 0) {
            k++;
        }
        if (k == FRAME_KEY_COUNT) {
            fail_at(parser, member->offset,
                    "f%zu: unexpected key \"%s\": a frame holds only \"pad\", \"resize\", \"touches\" and \"mouse\"",
                    frame_index, member->key);
            return 0;
        }
        if (members[k] != NULL) {
            fail_at(parser, member->offset, "f%zu: \"%s\" given twice", frame_index, member->key);
            return 0;
        }
        members[k] = member;
    }
    return 1;
}

static int read_frames(replay_t *replay, parser_t *parser) {
    replay->inputs = calloc(replay->frames->child_count + 1, sizeof(*replay->inputs));
    replay->touches = calloc(touch_capacity(replay) + 1, sizeof(*replay->touches));
    if (replay->inputs == NULL || replay->touches == NULL) {
        fail_at(parser, 0, "out of memory");
        return 0;
    }
    int width = replay->window_width;
    int height = replay->window_height;
    replay_mouse_t mouse = {0, 0, 0};
    size_t frame_index = 0;
    for (size_t f = replay->frames->first_child; f != NO_NODE; f = replay->nodes[f].next) {
        const node_t *members[FRAME_KEY_COUNT];
        frame_input_t *input = &replay->inputs[frame_index];
        if (!find_frame_members(replay, parser, frame_index, &replay->nodes[f], members)
            || (members[FRAME_PAD] != NULL && !read_frame_pad(replay, parser, frame_index, members[FRAME_PAD], &input->held))
            || !read_frame_resize(replay, parser, frame_index, members[FRAME_RESIZE], &width, &height, input)
            || !read_frame_touches(replay, parser, frame_index, members[FRAME_TOUCHES], width, height, input)
            || !read_frame_mouse(replay, parser, frame_index, members[FRAME_MOUSE], width, height, &mouse)) {
            return 0;
        }
        input->mouse = mouse;
        frame_index++;
    }
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
    replay->inputs = NULL;
    replay->touches = NULL;
    replay->touch_count = 0;
    replay->initial_save = NULL;
    replay->initial_save_count = 0;
    replay->bindings = NULL;
    replay->pad_sheet = NULL;
    replay->attract_after = 0;
    replay->window_width = 640;
    replay->window_height = 480;
    replay->screen_count = 0;
    if (!find_frames(replay, &parser) || !check_input(replay, &parser) || !read_start_state(replay, &parser)
        || !read_frames(replay, &parser)) {
        replay_free(replay);
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

unsigned replay_frame_held(const replay_t *replay, size_t frame) {
    assert(replay != NULL && frame < replay_frame_count(replay));
    return replay->inputs[frame].held;
}

int replay_frame_resize(const replay_t *replay, size_t frame, int *width, int *height) {
    assert(replay != NULL && frame < replay_frame_count(replay) && width != NULL && height != NULL);
    const frame_input_t *input = &replay->inputs[frame];
    if (input->resize_width == 0) {
        return 0;
    }
    *width = input->resize_width;
    *height = input->resize_height;
    return 1;
}

const replay_touch_t *replay_frame_touches(const replay_t *replay, size_t frame, size_t *count) {
    assert(replay != NULL && frame < replay_frame_count(replay) && count != NULL);
    *count = replay->inputs[frame].touch_count;
    return replay->inputs[frame].touches;
}

replay_mouse_t replay_frame_mouse(const replay_t *replay, size_t frame) {
    assert(replay != NULL && frame < replay_frame_count(replay));
    return replay->inputs[frame].mouse;
}

const unsigned char *replay_initial_save(const replay_t *replay, size_t *count) {
    assert(replay != NULL && count != NULL);
    *count = replay->initial_save_count;
    return replay->initial_save;
}

const char *replay_bindings(const replay_t *replay) {
    assert(replay != NULL);
    return replay->bindings;
}

const char *replay_pad_sheet(const replay_t *replay) {
    assert(replay != NULL);
    return replay->pad_sheet;
}

int replay_attract_after(const replay_t *replay) {
    assert(replay != NULL);
    return replay->attract_after;
}

void replay_window(const replay_t *replay, int *width, int *height) {
    assert(replay != NULL && width != NULL && height != NULL);
    *width = replay->window_width;
    *height = replay->window_height;
}

bool replay_frame_screen_listed(const replay_t *replay, size_t frame) {
    assert(replay != NULL && frame < replay_frame_count(replay));
    for (size_t i = 0; i < replay->screen_count; i++) {
        if (replay->screens[i] == frame) {
            return true;
        }
    }
    return false;
}

void replay_free(replay_t *replay) {
    if (replay != NULL) {
        free(replay->inputs);
        free(replay->touches);
        free(replay->initial_save);
    }
    free(replay);
}
