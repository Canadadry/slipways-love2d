#include "game/wormhole.h"

#include <assert.h>

#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/targeting.h"
#include "game/tech_gate.h"
#include "engine/allocator.h"

static int g_wormhole_up = 0;

static const entity_class_t wormhole_too_far_target_class_def = {0};

static wormhole_too_far_target_t g_wormhole_too_far_target_wrapper = {
    {.class = &wormhole_too_far_target_class_def},
    wormhole_too_far_target_status_text,
};

static int wormhole_is_anchor(const entity_t *p) {
    return p != NULL && entity_has(p, ENTITY_HAS_FOLD_ANCHOR) && p->fold_anchor != 0;
}

static int wormhole_anchor_count(void) {
    entity_list_iter_t it = entity_list_all(live_entities());
    entity_t *p;
    int n = 0;

    while (entity_list_iter_next(&it, &p)) {
        n += wormhole_is_anchor(p);
    }
    return n;
}

void wormhole_construct(wormhole_t *self) {
    entity_id_list_t *ox;
    entity_list_iter_t it;
    entity_t *p;
    int n;

    if (self == NULL) {
        return;
    }

    n = wormhole_anchor_count();
    ox = &self->anchors;
    entity_id_list_init_reserve(ox, grab_arena_construction_zone(), n);
    it = entity_list_all(live_entities());
    while (entity_list_iter_next(&it, &p)) {
        if (wormhole_is_anchor(p)) {
            entity_id_list_add(ox, entity_id_link(p));
        }
    }
    assert(ox->count == n && ox->capacity == n);
}

void wormhole_drag(wormhole_t *self, mouse_pos_t mouse) {
    vec_t p_vec;
    entity_t *nearest = NULL;
    p8_fixed_t nq;
    p8_fixed_t threshold;

    targeting_track((entity_t *)self, mouse);
    if (self == NULL) {
        return;
    }

    p_vec = entity_map_pos(&self->header);
    nq = entity_geom_nearest_in_ids(p_vec, &self->anchors, &nearest);
    self->anchor = entity_id_link(nearest);

    threshold = tech_gate_if_unlocked("space folding", P8_FIXED_LIT(0x000030a3, 0.19), P8_FIXED_LIT(0x0000451e, 0.27));

    if (nq > threshold) {
        targeting_set_target(&self->targeting, &wormhole_too_far_target_wrapper()->header);
    }
}

static void wormhole_drag_method(entity_t *self, mouse_pos_t mouse) {
    wormhole_drag((wormhole_t *)self, mouse);
}

void wormhole_update_placing(wormhole_t *self, p8_fixed_t t) {
    p8_fixed_t threshold;
    p8_fixed_t one;
    p8_fixed_t reach;
    p8_fixed_t min_val;
    p8_fixed_t sin_term;
    p8_fixed_t result;

    if (self == NULL) {
        return;
    }

    threshold = tech_gate_if_unlocked("space folding", p8_fixed_from_int(29), p8_fixed_from_int(35));

    one = p8_fixed_from_int(1);
    reach = p8_fixed_div(t, p8_fixed_from_int(15));
    min_val = (reach < one) ? reach : one;
    sin_term =
        p8_fixed_mul(p8_fixed_sin(p8_fixed_div(t, p8_fixed_from_int(40))), P8_FIXED_LIT(0x00001999, 0.1));

    result = p8_fixed_mul(threshold, p8_fixed_add(min_val, sin_term));
    self->radius = result;
}

static void wormhole_update_placing_method(entity_t *self, p8_fixed_t t) {
    wormhole_update_placing((wormhole_t *)self, t);
}

void wormhole_drop(wormhole_t *self) {
    p8_fixed_t t;
    int result;

    if (self == NULL) {
        return;
    }

    t = self->header.state_time;

    /* targeting_confirm's side effects must not run before tick 15: do not hoist it out of the else. */
    if (t < p8_fixed_from_int(15)) {
        result = 1;
    } else {
        result = !targeting_confirm((entity_t *)self);
    }

    self->header.is_dead = result;
}

static void wormhole_drop_method(entity_t *self, entity_t *dropped) {
    (void)dropped;
    wormhole_drop((wormhole_t *)self);
}

void wormhole_update_unfolding(wormhole_t *self, p8_fixed_t t) {
    p8_fixed_t sr;
    vec_t self_p;
    const entity_list_t *planet_list;
    int i;

    if (self == NULL) {
        return;
    }

    sr = p8_fixed_add(p8_fixed_div(wormhole_reveal_radius(self, t), p8_fixed_from_int(128)),
                          P8_FIXED_LIT(0x000007ef, 0.031));

    self_p = entity_map_pos(&self->header);
    planet_list = entities_by_tag(TAG_PLANET);
    if (planet_list != NULL) {
        for (i = 0; i < planet_list->count; i++) {
            entity_t *planet = planet_list->items[i];
            planet_t *pt;

            if (planet == NULL) {
                continue;
            }
            assert(planet_is_family(planet) && "wormhole_update_unfolding: the planet tag holds planets only");
            pt = (planet_t *)planet;

            if (pt->header.state == ENTITY_STATE_UNKNOWN) {
                vec_t planet_map_pos = entity_map_pos(&pt->header);
                vec_t diff = vec_sub(planet_map_pos, self_p);
                if (vec_length(diff) < sr) {
                    planet_roll_type(pt);
                }
            }
        }
    }

    self->header.is_dead = t > p8_fixed_from_int(25);
}

static void wormhole_update_unfolding_method(entity_t *self, p8_fixed_t t) {
    wormhole_update_unfolding((wormhole_t *)self, t);
}

p8_fixed_t wormhole_reveal_radius(wormhole_t *self, p8_fixed_t t) {
    p8_fixed_t sq = p8_fixed_sqrt(p8_fixed_div(t, p8_fixed_from_int(15)));
    p8_fixed_t one = p8_fixed_from_int(1);
    p8_fixed_t min_val = (sq < one) ? sq : one;
    p8_fixed_t oi = p8_fixed_from_int(0);

    if (self != NULL) {
        oi = self->radius;
    }
    return p8_fixed_mul(min_val, oi);
}

static void wormhole_release_method(entity_t *self) {
    grab_arena_give_back(((wormhole_t *)self)->anchors.zone);
}

static const entity_class_t wormhole_class_def = {
    .draw = wormhole_draw_method,
    .draw_overlay = targeting_tooltip_method,
    /* Not targeting_track_method: wormhole_drag adds the nearest-anchor search and the too-far target. */
    .drag = wormhole_drag_method,
    .drop = wormhole_drop_method,
    .release = wormhole_release_method,
    .states =
        {
            [ENTITY_STATE_PLACING] = wormhole_update_placing_method,
            [ENTITY_STATE_UNFOLDING] = wormhole_update_unfolding_method,
        },
    .tags = TAG(TAG_WORMHOLE),
};

static wormhole_t *wormhole_create(void) {
    wormhole_t *self;

    grab_arena_open(ZONE_BYTES(sizeof *self) + ZONE_BYTES((size_t)wormhole_anchor_count() * sizeof(entity_id_t)));
    self = (wormhole_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    entity_init_instance(&self->header, &wormhole_class_def, wormhole_archetype.state, wormhole_archetype.state_time);
    self->archetype = &wormhole_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->targeting.archetype = &self->archetype->targeting;
    self->radius = self->archetype->radius;
    self->targeting.cost = self->archetype->targeting.cost;
    self->targeting.has_cost = self->archetype->targeting.has_cost;
    self->has = WORMHOLE_HAS_TOO_FAR_TARGET;
    self->anchors = (entity_id_list_t){0};
    targeting_set_target(&self->targeting, NULL);
    self->anchor = ENTITY_ID_NONE;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->header.is_dead = 0;
    wormhole_construct(self);
    grab_arena_close();
    entity_construct(&self->header);
    return self;
}

void wormhole_init(void) {
    wormhole_shutdown();
    g_wormhole_up = 1;
}

void wormhole_shutdown(void) {
    g_wormhole_up = 0;
}

int wormhole_ready(void) {
    return g_wormhole_up;
}

const entity_class_t *wormhole_class(void) {
    return &wormhole_class_def;
}

wormhole_t *wormhole_new(void) {
    return wormhole_create();
}

int wormhole_live_count(void) {
    const entity_list_t *wormholes = entities_by_tag(TAG_WORMHOLE);
    return wormholes != NULL ? wormholes->count : 0;
}

wormhole_too_far_target_t *wormhole_too_far_target_wrapper(void) {
    return &g_wormhole_too_far_target_wrapper;
}

const entity_class_t *wormhole_too_far_target_class(void) {
    return &wormhole_too_far_target_class_def;
}
