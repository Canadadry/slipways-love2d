#ifndef GAME_HUD_H
#define GAME_HUD_H

#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/vram.h"

void hud_select_palette_block(p8_vram_t *mem, p8_fixed_t no);
void hud_select_palette_block_default(p8_vram_t *mem);
int hud_palette_block_default_in_force(const p8_vram_t *mem);

void hud_draw_bar(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, p8_fixed_t y2, int col);
void hud_draw_bar_y1(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, int col);
void hud_draw_bar_default(p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t y1, p8_fixed_t y2);

void hud_draw_help_frame(const p8_graphics_t *gfx, p8_vram_t *mem, p8_fixed_t x1, p8_fixed_t y1, p8_fixed_t x2);

void hud_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
              p8_fixed_t a);
/* hud_print_outlined's contract (same pixels, same draw palette after), through
 * text_image_print_outlined. gfx's camera moves during a build and is restored. */
void hud_print_outlined_text_image(p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y, int c,
                                   p8_fixed_t a);
void hud_print_outlined_default(const p8_graphics_t *gfx, p8_vram_t *mem, const char *t, p8_fixed_t x, p8_fixed_t y,
                       int c);

#endif
