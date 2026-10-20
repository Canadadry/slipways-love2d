#include "devtools/vram_digest.h"

#include <assert.h>

uint32_t vram_digest_fnv1a(const unsigned char *bytes, size_t count) {
    assert(bytes != NULL || count == 0);
    uint32_t hash = 0x811c9dc5u;
    for (size_t i = 0; i < count; i++) {
        hash ^= bytes[i];
        hash *= 0x01000193u;
    }
    return hash;
}

int vram_digest_samples_frame(int frame, int frame_count) {
    assert(frame >= 0 && frame < frame_count);
    return frame % 32 == 0 || frame == frame_count - 1;
}

enum { LINE_CAP = 48 };

typedef struct {
    char text[LINE_CAP];
    size_t length;
} line_t;

static void line_add_char(line_t *line, char c) {
    assert(line->length + 1 < LINE_CAP && "digest line longer than LINE_CAP");
    line->text[line->length++] = c;
    line->text[line->length] = '\0';
}

static void line_add_text(line_t *line, const char *text) {
    for (; *text != '\0'; text++) line_add_char(line, *text);
}

/* By hand, not printf: the digest must not depend on a C library's
   formatter. */
static void line_add_hex(line_t *line, uint32_t value) {
    static const char digits[] = "0123456789abcdef";
    for (int shift = 28; shift >= 0; shift -= 4) {
        line_add_char(line, digits[(value >> shift) & 0xfu]);
    }
}

static void line_add_decimal(line_t *line, int value) {
    assert(value >= 0);
    char reversed[10];
    int count = 0;
    do {
        reversed[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count > 0) line_add_char(line, reversed[--count]);
}

static line_t line_start(const char *kind) {
    line_t line = { "", 0 };
    line_add_text(&line, kind);
    line_add_char(&line, ' ');
    return line;
}

static void line_end_with_hash(const vram_digest_sink_t *sink, line_t *line, const unsigned char *bytes, size_t count) {
    assert(sink != NULL && sink->write_line != NULL);
    line_add_hex(line, vram_digest_fnv1a(bytes, count));
    line_add_char(line, '\n');
    sink->write_line(sink->context, line->text);
}

void vram_digest_write_sprite_sheet(const vram_digest_sink_t *sink, const p8_vram_t *memory) {
    line_t line = line_start("sprite-sheet");
    line_end_with_hash(sink, &line, &memory->sprite_sheet[0][0], sizeof memory->sprite_sheet);
}

void vram_digest_write_screen(const vram_digest_sink_t *sink, const p8_vram_t *memory, int frame) {
    line_t line = line_start("screen");
    line_add_decimal(&line, frame);
    line_add_char(&line, ' ');
    line_end_with_hash(sink, &line, &memory->screen[0][0], sizeof memory->screen);
}

void vram_digest_write_map(const vram_digest_sink_t *sink, const p8_vram_t *memory) {
    line_t line = line_start("map");
    line_end_with_hash(sink, &line, &memory->map_cells[0][0], sizeof memory->map_cells);
}
