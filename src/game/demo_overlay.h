#ifndef GAME_DEMO_OVERLAY_H
#define GAME_DEMO_OVERLAY_H

#include "game/demo_data.h"
#include "p8/gfx.h"

int demo_overlay_find_caption(const demo_data_caption_t *captions, int count, int frame);

void demo_overlay_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int frame);

#endif
