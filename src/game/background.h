#ifndef GAME_BACKGROUND_H
#define GAME_BACKGROUND_H

#define BACKGROUND_GRID_EDGE 262

#include "game/game_data.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

#include <stdint.h>

typedef struct {
    entity_t header;
    const background_archetype_t *archetype;
    uint32_t seed;
} background_t;

void background_init(void);

void background_shutdown(void);

int background_ready(void);

const entity_class_t *background_class(void);

/* Records the RNG state as its seed, then draws its tile map from the RNG. */
background_t *background_new(void);

/* The live background, or NULL when none is up. */
const background_t *background_live(void);

void background_construct(p8_vram_t *memory, background_t *self);

void background_draw(p8_graphics_t *gfx, p8_vram_t *mem, const background_t *self);

down_t background_left_press(background_t *self);

#endif
