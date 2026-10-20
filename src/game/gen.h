#ifndef GAME_GEN_H
#define GAME_GEN_H

#include "p8/fixed.h"
#include "game/game_data.h"
#include "engine/allocator.h"

typedef struct {
    p8_fixed_t step;
    p8_fixed_t bias;
} random_walk_t;

random_walk_t *random_walk_new(const allocator_t *zone, p8_fixed_t n_, p8_fixed_t b);
p8_fixed_t random_walk_next(random_walk_t *state);

/* A weighted_picker_random_fn_t: ctx must be a random_walk_t *. */
p8_fixed_t random_walk_draw(void *ctx);

typedef p8_fixed_t (*weighted_picker_random_fn_t)(void *ctx);

enum { WEIGHTED_PICKER_TICKETS_CAP = 66 };

typedef struct {
    const planet_types_entry_t *tickets[WEIGHTED_PICKER_TICKETS_CAP];
    int tickets_count;
    const planet_types_entry_t *recent[3];
    int recent_set[3];
    p8_fixed_t multiplier;
    weighted_picker_random_fn_t random;
    void *random_ctx;
} weighted_picker_t;

/* Pools planet_types_table, each entry floor(weight * multiplier) times. */
weighted_picker_t *weighted_picker_new(const allocator_t *zone, p8_fixed_t multiplier, weighted_picker_random_fn_t r, void *r_ctx);
const planet_types_entry_t *weighted_picker_next(weighted_picker_t *state);

/* The live walk the picker draws from, owned by the picker's zone; NULL unless its source is random_walk_draw. */
random_walk_t *weighted_picker_walk(const weighted_picker_t *state);

#endif
