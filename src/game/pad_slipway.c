#include "game/pad_slipway.h"

#include "game/planet.h"
#include "game/planet_family_trade.h"
#include "game/slipway.h"

#include <assert.h>

void pad_slipway_init(pad_slipway_t *slipway) {
    *slipway = (pad_slipway_t){.start = ENTITY_ID_NONE, .target = ENTITY_ID_NONE, .phase = PAD_SLIPWAY_NONE};
}

int pad_slipway_can_start(const entity_t *e) {
    if (e == NULL || e->is_dead || !planet_is_family(e) || e->state != ENTITY_STATE_SETTLED) {
        return 0;
    }
    return !(e->class->tags & TAG(TAG_SLIPGATE)) || slipgate_takes_slipway((const planet_t *)e);
}

static int is_not_start(entity_t *e, void *start) {
    return e != start;
}

entity_list_t pad_slipway_candidates(const allocator_t *zone, const entity_list_t *candidates, const entity_t *start) {
    return entity_list_filter(zone, candidates, is_not_start, (void *)start);
}

entity_t *pad_slipway_between(const entity_t *start, const entity_t *target) {
    assert(start != NULL && planet_is_family(start) && "pad_slipway_between: only a planet holds a slipways list");
    entity_id_list_iter_t it = entity_id_list_all(&((const planet_t *)start)->slipways);
    entity_t *slipway;
    while (entity_id_list_iter_next(&it, &slipway)) {
        assert(!slipway->is_dead &&
               "pad_slipway_between: a slipway dies only when its drop is refused, before it joins a planet's list");
        if (slipway_other_end((const slipway_t *)slipway, start) == target) {
            return slipway;
        }
    }
    return NULL;
}

pad_slipway_outcome_t pad_slipway_outcome(const entity_t *start, const entity_t *target, int linked_before) {
    if (linked_before || target == NULL) {
        return PAD_SLIPWAY_REFUSED;
    }
    return pad_slipway_between(start, target) != NULL ? PAD_SLIPWAY_BUILT : PAD_SLIPWAY_REFUSED;
}

void pad_slipway_release(pad_slipway_t *slipway, int linked_before) {
    assert(slipway->phase == PAD_SLIPWAY_HELD && !entity_id_is_none(slipway->target) &&
           "pad_slipway_release: only a held slipway aimed at a target is released");
    slipway->linked_before = linked_before;
    slipway->phase = PAD_SLIPWAY_RELEASED;
}

void pad_slipway_rebase(pad_slipway_t *slipway, pad_slipway_outcome_t outcome) {
    assert(slipway->phase == PAD_SLIPWAY_RELEASED && "pad_slipway_rebase: only the frame after a release rebases");
    if (outcome == PAD_SLIPWAY_BUILT) {
        slipway->start = slipway->target;
    }
    slipway->target = ENTITY_ID_NONE;
    slipway->phase = PAD_SLIPWAY_PRESSED;
}
