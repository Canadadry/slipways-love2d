#include "game/loading_screen.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static const char LOADING_TEXT[] = "loading";

enum {
    TEXT_COLOR = 7,
    MARGIN = 1,
    TEXT_WIDTH = (int)(sizeof LOADING_TEXT - 1) * P8_FONT_CELL_WIDTH - (P8_FONT_CELL_WIDTH - P8_FONT_GLYPH_WIDTH),
    TEXT_X = P8_SCREEN_WIDTH - MARGIN - TEXT_WIDTH,
    TEXT_Y = P8_SCREEN_HEIGHT - MARGIN - P8_FONT_GLYPH_HEIGHT,
};

void loading_screen_draw(p8_graphics_t *gfx, p8_vram_t *mem) {
    assert(gfx != NULL && mem != NULL && "loading_screen_draw: print dereferences both");
    p8_graphics_t found = *gfx;
    unsigned char palette[sizeof mem->draw_palette];

    memcpy(palette, mem->draw_palette, sizeof palette);
    for (int col = 0; col < (int)sizeof palette; col++) {
        mem->draw_palette[col] = (unsigned char)(col == 0 ? col | P8_DRAW_PALETTE_TRANSPARENT_BIT : col);
    }
    p8_camera_reset(gfx);
    p8_fillp_reset(gfx);

    p8_cls(mem);
    p8_print(gfx, mem, LOADING_TEXT, TEXT_X, TEXT_Y, TEXT_COLOR);

    *gfx = found;
    memcpy(mem->draw_palette, palette, sizeof palette);
}
