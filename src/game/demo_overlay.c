#include "game/demo_overlay.h"

#include <assert.h>
#include <string.h>

#define DEMO_OVERLAY_LINE_LENGTH 32

enum {
    BAND_FILL = 1,
    TEXT_COLOR = 7,
    TAG_FILL = 8,
    PADDING = 1,
    LINE_GAP = 1,
    TOP_HUD_HEIGHT = 11,
    BOTTOM_HUD_HEIGHT = 11,
    BAND_BOTTOM = P8_SCREEN_HEIGHT - BOTTOM_HUD_HEIGHT - 1,
    BAND_HEIGHT = 2 * PADDING + 2 * P8_FONT_GLYPH_HEIGHT + LINE_GAP,
    BAND_TOP = BAND_BOTTOM - BAND_HEIGHT + 1,
};

static const char TAG_TEXT[] = "demo";

int demo_overlay_find_caption(const demo_data_caption_t *captions, int count, int frame) {
    for (int i = 0; i < count; i++) {
        if (captions[i].from <= frame && frame < captions[i].to) {
            return i;
        }
    }
    return -1;
}

static int text_width(int length) {
    return length * P8_FONT_CELL_WIDTH - 1;
}

static void print_centred(const p8_graphics_t *gfx, p8_vram_t *mem, const char *line, int y) {
    int x = (P8_SCREEN_WIDTH - text_width((int)strlen(line))) / 2;
    p8_print(gfx, mem, line, x, y, TEXT_COLOR);
}

static void draw_tag(const p8_graphics_t *gfx, p8_vram_t *mem) {
    int right = P8_SCREEN_WIDTH - 1;
    int left = right - text_width((int)strlen(TAG_TEXT)) - 2 * PADDING + 1;
    int bottom = TOP_HUD_HEIGHT + P8_FONT_GLYPH_HEIGHT + 2 * PADDING - 1;
    p8_rectfill(gfx, mem, left, TOP_HUD_HEIGHT, right, bottom, TAG_FILL);
    p8_print(gfx, mem, TAG_TEXT, left + PADDING, TOP_HUD_HEIGHT + PADDING, TEXT_COLOR);
}

static void copy_line(char line[DEMO_OVERLAY_LINE_LENGTH + 1], const char *start, size_t length) {
    assert(length <= DEMO_OVERLAY_LINE_LENGTH &&
           "a caption line over 32 characters: the generator refuses it; a hand-made unit would overflow the line buffer");
    memcpy(line, start, length);
    line[length] = '\0';
}

static void draw_caption(const p8_graphics_t *gfx, p8_vram_t *mem, const char *text) {
    char first[DEMO_OVERLAY_LINE_LENGTH + 1];
    char second[DEMO_OVERLAY_LINE_LENGTH + 1];
    p8_rectfill(gfx, mem, 0, BAND_TOP, P8_SCREEN_WIDTH - 1, BAND_BOTTOM, BAND_FILL);
    const char *newline = strchr(text, '\n');
    if (newline == NULL) {
        copy_line(first, text, strlen(text));
        print_centred(gfx, mem, first, BAND_TOP + (BAND_HEIGHT - P8_FONT_GLYPH_HEIGHT) / 2);
        return;
    }
    assert(strchr(newline + 1, '\n') == NULL && "a caption of more than 2 lines: the generator refuses it");
    copy_line(first, text, (size_t)(newline - text));
    copy_line(second, newline + 1, strlen(newline + 1));
    print_centred(gfx, mem, first, BAND_TOP + PADDING);
    print_centred(gfx, mem, second, BAND_TOP + PADDING + P8_FONT_GLYPH_HEIGHT + LINE_GAP);
}

void demo_overlay_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int frame) {
    if (frame < 0) {
        return;
    }
    p8_graphics_t own = *gfx;
    p8_camera_reset(&own);
    p8_fillp_reset(&own);
    draw_tag(&own, mem);
    int caption = demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, frame);
    if (caption >= 0) {
        draw_caption(&own, mem, demo_data_captions[caption].text);
    }
}
