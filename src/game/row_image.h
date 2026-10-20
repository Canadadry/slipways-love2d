#ifndef GAME_ROW_IMAGE_H
#define GAME_ROW_IMAGE_H

#include "game/layout.h"

/* 1 when row_image_draw_at copies the row from an image slot: only sprite and nothing items, at
 * least one sprite, each at a whole, non-negative x offset, a y offset of 0, within a slot. */
int row_image_cacheable(const layout_row_t *row);

/* layout_row_draw_at's contract (same pixels, same draw palette after), through an image slot
 * keyed by the row's content when the row is cacheable. gfx's camera moves during a build and is
 * restored. */
void row_image_draw_at(p8_graphics_t *gfx, p8_vram_t *mem, const layout_row_t *row, vec_t p, p8_fixed_t a);

#endif
