#include "game/gen.h"

#include <assert.h>
#include <stddef.h>

#include "game/game_data.h"
#include "engine/allocator.h"

random_walk_t *random_walk_new(const allocator_t *zone, p8_fixed_t n_, p8_fixed_t b) {
    random_walk_t *state = (random_walk_t *)allocator_alloc(zone, sizeof *state);
    state->step = n_;
    state->bias = b;
    return state;
}

p8_fixed_t random_walk_next(random_walk_t *state) {
    p8_fixed_t one_minus_abs_b = p8_fixed_sub(p8_fixed_from_int(1), p8_fixed_abs(state->bias));
    p8_fixed_t b_pos = (state->bias > 0) ? state->bias : 0;
    p8_fixed_t v = p8_fixed_add(p8_fixed_rnd(one_minus_abs_b), b_pos);
    p8_fixed_t half = P8_FIXED_LIT(0x00008000, 0.5);

    state->bias = p8_fixed_add(state->bias, p8_fixed_mul(p8_fixed_sub(half, v), state->step));
    return v;
}

p8_fixed_t random_walk_draw(void *ctx) {
    return random_walk_next((random_walk_t *)ctx);
}

/* Return, not continue, at the first unwritten slot: slots fill out of order. */
static int weighted_picker_recent_contains(const weighted_picker_t *state, const planet_types_entry_t *e) {
    int i;
    for (i = 0; i < 3; i++) {
        if (!state->recent_set[i]) {
            return 0;
        }
        if (state->recent[i] == e) {
            return 1;
        }
    }
    return 0;
}

static void weighted_picker_fill(weighted_picker_t *state) {
    int i;

    state->tickets_count = 0;
    for (i = 0; i < PLANET_TYPES_COUNT; i++) {
        const planet_types_entry_t *entity = &planet_types_table[i];
        int count = p8_fixed_flr_int(p8_fixed_mul(entity->weight, state->multiplier));
        int n;

        for (n = 1; n <= count; n++) {
            assert(state->tickets_count < WEIGHTED_PICKER_TICKETS_CAP && "weighted_picker_fill: the ticket pool is sized for "
                   "planet_types_table at a multiplier of 2; a larger table or multiplier needs a larger WEIGHTED_PICKER_TICKETS_CAP");
            state->tickets[state->tickets_count++] = entity;
        }
    }
}

weighted_picker_t *weighted_picker_new(const allocator_t *zone, p8_fixed_t multiplier, weighted_picker_random_fn_t r, void *r_ctx) {
    weighted_picker_t *state = (weighted_picker_t *)allocator_alloc(zone, sizeof *state);
    int i;

    for (i = 0; i < 3; i++) {
        state->recent[i] = NULL;
        state->recent_set[i] = 0;
    }
    state->multiplier = multiplier;
    state->random = r;
    state->random_ctx = r_ctx;
    weighted_picker_fill(state);
    return state;
}

static int weighted_picker_has_drawable_ticket(const weighted_picker_t *state) {
    int i;

    for (i = 0; i < state->tickets_count; i++) {
        if (!weighted_picker_recent_contains(state, state->tickets[i])) {
            return 1;
        }
    }
    return 0;
}

static void weighted_picker_tickets_del(weighted_picker_t *state, const planet_types_entry_t *e) {
    int i = 0;

    while (state->tickets[i] != e) {
        i++;
    }
    state->tickets_count--;
    for (; i < state->tickets_count; i++) {
        state->tickets[i] = state->tickets[i + 1];
    }
}

const planet_types_entry_t *weighted_picker_next(weighted_picker_t *state) {
    const planet_types_entry_t *e;

    if (!weighted_picker_has_drawable_ticket(state)) {
        weighted_picker_fill(state);
    }

    for (;;) {
        p8_fixed_t r_val = state->random(state->random_ctx);
        p8_fixed_t len_fixed = p8_fixed_from_int(state->tickets_count);
        p8_fixed_t idx_fixed =
            p8_fixed_flr(p8_fixed_add(p8_fixed_mul(r_val, len_fixed), p8_fixed_from_int(1)));
        int idx0 = p8_fixed_flr_int(idx_fixed) - 1;

        e = state->tickets[idx0];
        if (!weighted_picker_recent_contains(state, e)) {
            break;
        }
    }

    weighted_picker_tickets_del(state, e);
    {
        int slot = 3 - (state->tickets_count % 3);
        state->recent[slot - 1] = e;
        state->recent_set[slot - 1] = 1;
    }
    return e;
}

random_walk_t *weighted_picker_walk(const weighted_picker_t *state) {
    if (state->random != random_walk_draw) {
        return NULL;
    }
    return (random_walk_t *)state->random_ctx;
}
