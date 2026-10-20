#include "game/hud.h"

#include <assert.h>
#include <string.h>

#include "game/text_image.h"

static int to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

static const unsigned char palette_blocks[16][16] = {
    {0, 1, 2, 131, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {0, 1, 2, 131, 2, 5, 6, 7, 8, 4, 9, 11, 12, 5, 14, 9},
    {0, 1, 1, 131, 2, 1, 13, 6, 2, 2, 4, 3, 13, 5, 8, 4},
    {0, 0, 1, 129, 1, 1, 5, 13, 2, 2, 2, 3, 5, 1, 2, 2},
    {0, 0, 0, 129, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 2, 2, 130, 2, 2, 8, 8, 2, 2, 8, 8, 2, 2, 8, 8},
    {0, 1, 1, 129, 1, 1, 5, 13, 1, 1, 5, 5, 1, 1, 5, 5},
    {0, 1, 2, 131, 4, 5, 6, 7, 10, 9, 0, 4, 2, 13, 0, 0},
    {0, 1, 2, 131, 4, 5, 6, 7, 0, 10, 9, 0, 4, 13, 4, 0},
    {0, 1, 2, 131, 4, 5, 6, 7, 0, 0, 10, 4, 0, 13, 9, 4},
    {0, 1, 2, 131, 4, 5, 6, 7, 4, 0, 0, 9, 4, 13, 0, 9},
    {0, 1, 2, 131, 4, 5, 6, 7, 9, 4, 0, 0, 9, 13, 9, 0},
    {0, 1, 2, 131, 4, 5, 6, 7, 0, 9, 4, 0, 0, 13, 10, 9},
    {0, 1, 2, 131, 4, 5, 6, 7, 9, 0, 9, 2, 0, 13, 0, 10},
    {0, 1, 2, 131, 4, 5, 6, 6, 0, 0, 0, 0, 0, 13, 0, 0},
};

void hud_select_palette_block(p8_vram_t *mem, p8_fixed_t no) {
    int block = p8_fixed_flr_int(no);
    assert(block >= 0 && block < 16 && "hud_select_palette_block: only blocks 0..15 exist; the cart would read past its table");
    memcpy(mem->draw_palette, palette_blocks[block], sizeof(mem->draw_palette));
}

void hud_select_palette_block_default(p8_vram_t *mem) {
    hud_select_palette_block(mem, p8_fixed_from_int(0));
}

int hud_palette_block_default_in_force(const p8_vram_t *mem) {
    return memcmp(mem->draw_palette, palette_blocks[0], sizeof(mem->draw_palette)) == 0;
}

void hud_draw_bar(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, p8_fixed_t y2, int col) {
    assert(gfx != NULL && "hud_draw_bar: p8_rectfill dereferences the graphics state, so a NULL one is not a no-op");
    if (gfx != NULL) {
        gfx->pen_color = col;
    }
    p8_rectfill(gfx, mem, 0, to_int(y1), P8_SCREEN_WIDTH - 1, to_int(y2), col);
}

void hud_draw_bar_y1(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, int col) {
    hud_draw_bar(gfx, mem, y1, y1, col);
}

void hud_draw_bar_default(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, p8_fixed_t y2) {
    int col = (gfx != NULL) ? gfx->pen_color : 6;
    hud_draw_bar(gfx, mem, y1, y2, col);
}

void hud_draw_help_frame(const p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t x1, p8_fixed_t y1, p8_fixed_t x2) {
    int galaxy_menu_button1 = to_int(x1);
    int top_pixel = to_int(y1);
    int galaxy_menu_button2 = to_int(x2);

    p8_rect(gfx, mem, galaxy_menu_button1, top_pixel, galaxy_menu_button2 + 1, top_pixel + 12, 0);
    p8_rectfill(gfx, mem, galaxy_menu_button1 + 1, top_pixel + 1, galaxy_menu_button2, top_pixel + 11, 5);
    p8_spr(gfx, mem, 70, galaxy_menu_button1, top_pixel, 1, 2);
    p8_spr(gfx, mem, 71, galaxy_menu_button2 - 2, top_pixel, 1, 2);
}

static int aligned_x(const char *t, p8_fixed_t x, p8_fixed_t a) {
    return to_int(x) - to_int(p8_fixed_mul(p8_fixed_mul(a, p8_fixed_from_int(4)), p8_fixed_from_int((int)strlen(t))));
}

void hud_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
              p8_fixed_t a) {
    p8_print_outlined(gfx, mem, t, aligned_x(t, x, a), to_int(y), c & 0x0F);
}

void hud_print_outlined_text_image(p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
                                   p8_fixed_t a) {
    text_image_print_outlined(gfx, mem, t, aligned_x(t, x, a), to_int(y), c & 0x0F);
}

void hud_print_outlined_default(const p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y,
                       int c) {
    hud_print_outlined(gfx, mem, t, x, y, c, p8_fixed_from_int(0));
}
