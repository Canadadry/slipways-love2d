#include "game/pad_target.h"

#include "game/planet.h"
#include "p8/fixed.h"

#include <assert.h>

int pad_target_is_candidate(const entity_t *e) {
    return e != NULL && !e->is_dead && planet_is_family(e) &&
           (e->state == ENTITY_STATE_KNOWN || e->state == ENTITY_STATE_SETTLED);
}

static int is_direction(pad_direction_t direction) {
    return direction == PAD_DIRECTION_UP || direction == PAD_DIRECTION_DOWN || direction == PAD_DIRECTION_LEFT ||
           direction == PAD_DIRECTION_RIGHT;
}

typedef struct {
    p8_fixed_t along;
    p8_fixed_t perpendicular;
} offset_t;

static offset_t offset_in(pad_direction_t direction, vec_t from, vec_t to) {
    p8_fixed_t dx = p8_fixed_sub(to.x, from.x);
    p8_fixed_t dy = p8_fixed_sub(to.y, from.y);
    switch (direction) {
    case PAD_DIRECTION_RIGHT:
        return (offset_t){dx, p8_fixed_abs(dy)};
    case PAD_DIRECTION_LEFT:
        return (offset_t){-dx, p8_fixed_abs(dy)};
    case PAD_DIRECTION_DOWN:
        return (offset_t){dy, p8_fixed_abs(dx)};
    default:
        return (offset_t){-dy, p8_fixed_abs(dx)};
    }
}

entity_t *pad_target_pick(const entity_t *origin, pad_direction_t direction, const entity_list_t *candidates) {
    assert(is_direction(direction) &&
           "pad_target_pick: the direction is exactly one of PAD_DIRECTION_UP, DOWN, LEFT or RIGHT");
    assert(origin != NULL && entity_has_map_pos(origin) && "pad_target_pick: the origin is an entity with a map position");
    if (candidates == NULL) {
        return NULL;
    }
    vec_t from = entity_map_pos(origin);
    entity_t *best = NULL;
    offset_t best_offset = {0, 0};
    p8_fixed_t best_score = 0;
    for (int i = 0; i < candidates->count; i++) {
        entity_t *e = candidates->items[i];
        if (e == origin) {
            continue;
        }
        assert(e != NULL && entity_has_map_pos(e) && "pad_target_pick: every candidate is an entity with a map position");
        offset_t offset = offset_in(direction, from, entity_map_pos(e));
        if (offset.along <= 0 || offset.perpendicular > offset.along) {
            continue;
        }
        p8_fixed_t score = p8_fixed_add(offset.along, offset.perpendicular * PAD_TARGET_PERPENDICULAR_WEIGHT);
        if (best == NULL || score < best_score ||
            (score == best_score && offset.perpendicular < best_offset.perpendicular)) {
            best = e;
            best_offset = offset;
            best_score = score;
        }
    }
    return best;
}
