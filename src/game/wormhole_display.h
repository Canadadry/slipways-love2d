#ifndef GAME_WORMHOLE_DISPLAY_H
#define GAME_WORMHOLE_DISPLAY_H

#include "p8/gfx.h"
#include "p8/vram.h"
#include "engine/vec.h"
#include "game/wormhole.h"

void wormhole_draw(p8_graphics_t *graphics, p8_vram_t *memory, wormhole_t *self, vec_t screen_pos);

#endif
