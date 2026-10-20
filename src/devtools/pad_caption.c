#include "devtools/pad_caption.h"

#include "p8/gfx.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *first;
    size_t length;
} caption_line_t;

static caption_line_t trimmed_line(const char *start, const char *end) {
    while (start < end && *start == ' ') {
        start++;
    }
    while (end > start && end[-1] == ' ') {
        end--;
    }
    caption_line_t line = {start, (size_t)(end - start)};
    return line;
}

static int lines_contain_either(const caption_line_t *lines, int line_count, char a, char b) {
    for (int i = 0; i < line_count; i++) {
        for (size_t k = 0; k < lines[i].length; k++) {
            if (lines[i].first[k] == a || lines[i].first[k] == b) {
                return 1;
            }
        }
    }
    return 0;
}

static int refuse_missing_glyph(const caption_line_t *lines, int line_count, char *error, size_t error_size) {
    for (int i = 0; i < line_count; i++) {
        for (size_t k = 0; k < lines[i].length; k++) {
            unsigned char byte = (unsigned char)lines[i].first[k];
            if (p8_font_has_glyph((char)byte)) {
                continue;
            }
            if (byte >= 0x20 && byte < 0x7f) {
                snprintf(error, error_size, "caption: no font glyph for '%c'", byte);
            } else {
                snprintf(error, error_size, "caption: no font glyph for byte 0x%02x", byte);
            }
            return 1;
        }
    }
    return 0;
}

int pad_caption_parse(const char *script_text, char *out, size_t out_size, char *error, size_t error_size) {
    caption_line_t lines[PAD_CAPTION_LINES_MAX];
    int line_count = 0;
    const char *start = script_text;
    for (;;) {
        const char *end = strchr(start, '/');
        if (!end) {
            end = start + strlen(start);
        }
        caption_line_t line = trimmed_line(start, end);
        if (line.length == 0) {
            snprintf(error, error_size, "caption: line %d is empty", line_count + 1);
            return 0;
        }
        if (line.length > PAD_CAPTION_LINE_CHARS) {
            snprintf(error, error_size, "caption: line %d has %d characters, at most %d", line_count + 1,
                     (int)line.length, PAD_CAPTION_LINE_CHARS);
            return 0;
        }
        if (line_count < PAD_CAPTION_LINES_MAX) {
            lines[line_count] = line;
        }
        line_count++;
        if (*end == '\0') {
            break;
        }
        start = end + 1;
    }
    if (line_count > PAD_CAPTION_LINES_MAX) {
        snprintf(error, error_size, "caption: %d lines, at most %d", line_count, PAD_CAPTION_LINES_MAX);
        return 0;
    }
    if (lines_contain_either(lines, line_count, 'v', 'V')) {
        snprintf(error, error_size, "caption: the letter v draws as u");
        return 0;
    }
    if (refuse_missing_glyph(lines, line_count, error, error_size)) {
        return 0;
    }

    size_t used = 0;
    for (int i = 0; i < line_count; i++) {
        size_t separator = i > 0 ? 1 : 0;
        assert(used + separator + lines[i].length < out_size && "out holds PAD_CAPTION_TEXT_SIZE bytes");
        if (separator) {
            out[used++] = '\n';
        }
        memcpy(out + used, lines[i].first, lines[i].length);
        used += lines[i].length;
    }
    out[used] = '\0';
    return 1;
}

void pad_caption_track_init(pad_caption_track_t *track, pad_caption_t *storage, int capacity) {
    track->captions = storage;
    track->capacity = capacity;
    track->count = 0;
    track->shown = 0;
}

static int close_shown(pad_caption_track_t *track, int frame, int checked, char *error, size_t error_size) {
    if (!track->shown) {
        return 1;
    }
    pad_caption_t *caption = &track->captions[track->count - 1];
    assert(frame >= caption->from && "frames only move forward");
    int frames_shown = frame - caption->from;
    if (checked && frames_shown < PAD_CAPTION_SHOWN_MIN) {
        snprintf(error, error_size, "caption: shown %d frames, at least %d", frames_shown, PAD_CAPTION_SHOWN_MIN);
        return 0;
    }
    caption->to = frame;
    track->shown = 0;
    return 1;
}

int pad_caption_track_show(pad_caption_track_t *track, int frame, const char *text, char *error, size_t error_size) {
    if (!close_shown(track, frame, 1, error, error_size)) {
        return 0;
    }
    assert((track->count == 0 || frame >= track->captions[track->count - 1].to) && "frames only move forward");
    assert(track->count < track->capacity &&
           "each closed caption takes PAD_CAPTION_SHOWN_MIN frames: size the storage from the frame budget");
    assert(strlen(text) < PAD_CAPTION_TEXT_SIZE && "text comes from pad_caption_parse");
    pad_caption_t *caption = &track->captions[track->count++];
    caption->from = frame;
    caption->to = frame;
    snprintf(caption->text, sizeof caption->text, "%s", text);
    track->shown = 1;
    return 1;
}

int pad_caption_track_clear(pad_caption_track_t *track, int frame, char *error, size_t error_size) {
    if (!track->shown) {
        snprintf(error, error_size, "caption -: no caption shown");
        return 0;
    }
    return close_shown(track, frame, 1, error, error_size);
}

int pad_caption_track_end(pad_caption_track_t *track, int frame, int checked, char *error, size_t error_size) {
    return close_shown(track, frame, checked, error, error_size);
}

int pad_caption_track_frame(const pad_caption_track_t *track, int frame, int buttons, char *error, size_t error_size) {
    if (!track->shown || buttons == 0) {
        return 1;
    }
    int after = frame - track->captions[track->count - 1].from;
    assert(after >= 0 && "frames only move forward");
    if (after < PAD_CAPTION_QUIET_MIN) {
        snprintf(error, error_size, "caption: a button down %d frame%s after the caption appeared, at least %d", after,
                 after == 1 ? "" : "s", PAD_CAPTION_QUIET_MIN);
        return 0;
    }
    return 1;
}

typedef struct {
    char *out;
    size_t size;
    size_t used;
} member_writer_t;

static void member_append(member_writer_t *writer, const char *text, size_t length) {
    assert(writer->used + length < writer->size && "out too small for the captions member");
    memcpy(writer->out + writer->used, text, length);
    writer->used += length;
    writer->out[writer->used] = '\0';
}

static void member_append_string(member_writer_t *writer, const char *text) {
    member_append(writer, text, strlen(text));
}

int pad_caption_write_member(const pad_caption_track_t *track, char *out, size_t out_size) {
    assert(out_size > 0);
    assert(!track->shown && "close the shown caption before writing: it has no `to` yet");
    out[0] = '\0';
    if (track->count == 0) {
        return 0;
    }
    member_writer_t writer = {out, out_size, 0};
    member_append_string(&writer, "\"captions\": [\n");
    for (int i = 0; i < track->count; i++) {
        const pad_caption_t *caption = &track->captions[i];
        char head[64];
        int head_length = snprintf(head, sizeof head, "    { \"from\": %d, \"to\": %d, \"text\": \"", caption->from, caption->to);
        member_append(&writer, head, (size_t)head_length);
        for (const char *p = caption->text; *p != '\0'; p++) {
            member_append(&writer, *p == '\n' ? "\\n" : p, *p == '\n' ? 2 : 1);
        }
        member_append_string(&writer, i + 1 < track->count ? "\" },\n" : "\" }\n");
    }
    member_append_string(&writer, "  ]");
    return (int)writer.used;
}

typedef struct {
    const char *p;
    char *error;
    size_t error_size;
} member_reader_t;

static void reader_skip_spaces(member_reader_t *reader) {
    while (*reader->p == ' ' || *reader->p == '\t' || *reader->p == '\n' || *reader->p == '\r') {
        reader->p++;
    }
}

static int reader_expect(member_reader_t *reader, const char *token, int caption) {
    reader_skip_spaces(reader);
    size_t length = strlen(token);
    if (strncmp(reader->p, token, length) != 0) {
        if (caption == 0) {
            snprintf(reader->error, reader->error_size, "\"captions\": expected %s", token);
        } else {
            snprintf(reader->error, reader->error_size, "\"captions\": caption %d: expected %s", caption, token);
        }
        return 0;
    }
    reader->p += length;
    return 1;
}

static int reader_frame(member_reader_t *reader, const char *key, int caption, int *out) {
    if (!reader_expect(reader, key, caption) || !reader_expect(reader, ":", caption)) {
        return 0;
    }
    reader_skip_spaces(reader);
    int value = 0;
    int digits = 0;
    while (*reader->p >= '0' && *reader->p <= '9') {
        if (value > (INT_MAX - (*reader->p - '0')) / 10) {
            digits = 0;
            break;
        }
        value = value * 10 + (*reader->p++ - '0');
        digits++;
    }
    if (digits == 0) {
        snprintf(reader->error, reader->error_size, "\"captions\": caption %d: %s is not a frame number", caption, key);
        return 0;
    }
    *out = value;
    return 1;
}

static int reader_text(member_reader_t *reader, int caption, char *out) {
    if (!reader_expect(reader, "\"text\"", caption) || !reader_expect(reader, ":", caption) ||
        !reader_expect(reader, "\"", caption)) {
        return 0;
    }
    size_t used = 0;
    while (*reader->p != '"') {
        char c = *reader->p;
        if (c == '\0' || c == '\n') {
            snprintf(reader->error, reader->error_size, "\"captions\": caption %d: unterminated text", caption);
            return 0;
        }
        if (c == '\\') {
            if (reader->p[1] != 'n') {
                snprintf(reader->error, reader->error_size, "\"captions\": caption %d: escape \\%c, only \\n is read",
                         caption, reader->p[1] ? reader->p[1] : ' ');
                return 0;
            }
            c = '\n';
            reader->p++;
        }
        if (used + 1 >= PAD_CAPTION_TEXT_SIZE) {
            snprintf(reader->error, reader->error_size, "\"captions\": caption %d: text longer than %d bytes", caption,
                     PAD_CAPTION_TEXT_SIZE - 1);
            return 0;
        }
        out[used++] = c;
        reader->p++;
    }
    reader->p++;
    out[used] = '\0';
    return 1;
}

int pad_caption_read_member(const char *json, pad_caption_t *captions, int capacity, char *error, size_t error_size) {
    assert(json && captions && capacity > 0 && error && error_size > 0);
    static const char key[] = "\"captions\":";
    const char *header_end = strstr(json, "\"frames\":");
    const char *start = strstr(json, key);
    if (!start || (header_end && start > header_end)) {
        return 0;
    }
    member_reader_t reader = {start + strlen(key), error, error_size};
    if (!reader_expect(&reader, "[", 0)) {
        return -1;
    }
    int count = 0;
    for (;;) {
        int caption = count + 1;
        if (count == capacity) {
            snprintf(error, error_size, "\"captions\": more than %d captions", capacity);
            return -1;
        }
        pad_caption_t *out = &captions[count];
        if (!reader_expect(&reader, "{", caption) || !reader_frame(&reader, "\"from\"", caption, &out->from) ||
            !reader_expect(&reader, ",", caption) || !reader_frame(&reader, "\"to\"", caption, &out->to) ||
            !reader_expect(&reader, ",", caption) || !reader_text(&reader, caption, out->text) ||
            !reader_expect(&reader, "}", caption)) {
            return -1;
        }
        count++;
        reader_skip_spaces(&reader);
        if (*reader.p == ']') {
            return count;
        }
        if (!reader_expect(&reader, ",", caption)) {
            return -1;
        }
    }
}
