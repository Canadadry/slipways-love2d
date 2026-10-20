#include "game/planet_family_trade.h"

#include <assert.h>
#include <string.h>

#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/particle.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_trade.h"
#include "game/probe.h"
#include "game/slipway.h"
#include "game/trade.h"
#include "engine/frame_zone.h"

static const int_seq_t synthesizer_synthesizable_resources_spent = {0, NULL};

static int g_family_trade_up = 0;

int slipgate_takes_slipway(const planet_t *self) {
    return self->slipways.count < 3;
}

down_t slipgate_accept_drop(const planet_t *self, const entity_t *s) {
    int start_not_self;
    down_t out = {0, ENTITY_ID_NONE};

    if (self == NULL) {
        return out;
    }

    start_not_self = !(s != NULL && s->class == slipway_class() &&
                    slipway_start((const slipway_t *)s) == &self->header);

    out.stop = start_not_self && slipgate_takes_slipway(self);
    return out;
}

static down_t slipgate_accept_drop_method(entity_t *self, const entity_t *s) {
    return slipgate_accept_drop((const planet_t *)self, s);
}

static const entity_class_t slipgate_left_press_empty_class_def = {0};

typedef struct {
    entity_t header;
} slipgate_left_press_empty_t;

static const slipgate_left_press_empty_t slipgate_left_press_empty = {{.class = &slipgate_left_press_empty_class_def}};

down_t slipgate_left_press(planet_t *self) {
    down_t accept_drop_result = {0, ENTITY_ID_NONE};
    slipway_keys_t keys = {0};
    slipway_t *made;

    if (self == NULL) {
        return accept_drop_result;
    }

    accept_drop_result = slipgate_accept_drop(self, (const entity_t *)&slipgate_left_press_empty);

    if (!accept_drop_result.stop) {
        return accept_drop_result;
    }

    assert(slipway_ready() && "slipway_init() must run before slipgate_left_press builds a slipway");
    keys.start = entity_id_link(&self->header);
    made = slipway_new(&keys);
    accept_drop_result.held = entity_id_link(made != NULL ? &made->header : NULL);
    accept_drop_result.stop = made != NULL;
    return accept_drop_result;
}

static down_t slipgate_left_press_method(entity_t *self, mouse_pos_t mouse) {
    (void)mouse;
    return slipgate_left_press((planet_t *)self);
}

void slipgate_refresh(planet_t *self) {
    int slipways_count = 0;
    p8_fixed_t s_val = p8_fixed_from_int(0);

    if (self == NULL) {
        return;
    }

    slipways_count = self->slipways.count;

    if (self->archetype->link_sprites != NULL) {
        int entry;
        if (int_seq_at(self->archetype->link_sprites->items, self->archetype->link_sprites->count, slipways_count + 1, &entry)) {
            s_val = p8_fixed_from_int(entry);
        }
    }

    self->sprite = s_val;
}

static void slipgate_refresh_method(entity_t *self) {
    slipgate_refresh((planet_t *)self);
}

void slipgate_offer_trade(planet_t *self, trade_offer_t *trade_offer) {
    const trade_route_t *route;
    const entity_id_list_t *slipways = NULL;
    entity_id_list_iter_t iter;
    entity_t *slipway;

    if (self == NULL || trade_offer == NULL) {
        return;
    }
    route = &trade_offer->route;

    slipways = &self->slipways;

    iter = entity_id_list_all(slipways);
    while (entity_id_list_iter_next(&iter, &slipway)) {
        if (trade_offer->amount <= p8_fixed_from_int(0)) {
            continue;
        }

        if (entity_id_list_has(&route->slipways, entity_id_of(slipway))) {
            continue;
        }

        {
            trade_offer_t gf = *trade_offer;
            entity_t *other;

            gf.route = trade_extend(frame_zone(), route, &self->header, slipway);
            assert(slipway->class == slipway_class() && "a slipgate's slipways list holds slipways only: planet_attach_slipway adds to it");
            other = slipway_other_end((const slipway_t *)slipway, (const entity_t *)self);
            /* The copy is on the stack: no offer_trade method may store its offer. */
            if (other != NULL && other->class->offer_trade != NULL) {
                other->class->offer_trade(other, &gf);
            }
            trade_offer->amount = gf.amount;
            trade_offer->accepted_routes = gf.accepted_routes;
        }
    }
}

static void slipgate_offer_trade_method(entity_t *self, struct trade_offer *offer) {
    slipgate_offer_trade((planet_t *)self, offer);
}

static const entity_class_t slipgate_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = slipgate_refresh_method,
    .release = planet_release_method,
    .left_press = slipgate_left_press_method,
    .accept_drop = slipgate_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = slipgate_offer_trade_method,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_SLIPGATE) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

static void slipgate_init_instance(planet_t *self, const entity_class_t *cls, const planet_archetype_t *archetype) {
    structure_init_instance(self, cls, archetype);
}

static planet_t *slipgate_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_trade_up && "planet_family_trade_init() must run first");
    slipgate_init_instance(self, &slipgate_class_def, &slipgate_archetype);
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

static const entity_class_t infraspace_gate_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = slipgate_refresh_method,
    .release = planet_release_method,
    .left_press = slipgate_left_press_method,
    .accept_drop = slipgate_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = slipgate_offer_trade_method,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_SLIPGATE) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

static planet_t *infraspace_gate_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_trade_up && "planet_family_trade_init() must run first");
    slipgate_init_instance(self, &infraspace_gate_class_def, &infraspace_gate_archetype);
    self->header.has |= ENTITY_HAS_INFRASPACE;
    self->header.infraspace = self->archetype->infraspace;
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "infraspace_gate_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void synthesizer_attach_slipway(planet_t *self, entity_t *slipway) {
    entity_t *other_end = NULL;

    if (self != NULL && slipway != NULL) {
        assert(slipway->class == slipway_class() && "synthesizer_attach_slipway: the slipway is one slipway_drop registers");
        other_end = slipway_other_end((const slipway_t *)slipway, (const entity_t *)self);
    }

    if (other_end != NULL) {
        const pointer_list_t *jy;
        pointer_list_iter_t it;
        void *i_val;

        assert(planet_is_family(other_end) && "synthesizer_attach_slipway: the other end of a registered slipway is a planet");
        assert((((const planet_t *)other_end)->has & PLANET_HAS_NEEDS) && "synthesizer_attach_slipway: the other end of a slipway to a synthesizer holds needs");
        jy = &((const planet_t *)other_end)->needs;

        it = pointer_list_all(jy);
        while (pointer_list_iter_next(&it, &i_val)) {
            const need_t *i;
            p8_fixed_t gc;

            if (i_val == NULL) {
                continue;
            }
            i = i_val;

            gc = i->has_satisfied ? i->satisfied : p8_fixed_from_int(0);
            if (gc != p8_fixed_from_int(0)) {
                continue;
            }

            if (i->row.has_resource && self->synthesizable_resources != NULL && int_seq_has_fixed(self->synthesizable_resources, i->row.resource)) {
                production_t *kw = planet_production(self);
                kw->has_resource = 1;
                kw->resource = i->row.resource;
                self->synthesizable_resources = &synthesizer_synthesizable_resources_spent;
            }
        }
    }

    planet_attach_slipway(self, slipway);
}

void synthesizer_set_spent(planet_t *self, int spent) {
    assert(self != NULL && self->header.class == synthesizer_class() && "synthesizer_set_spent: self is a synthesizer");
    self->synthesizable_resources = spent ? &synthesizer_synthesizable_resources_spent : self->archetype->synthesizable_resources;
}

static void synthesizer_attach_slipway_method(entity_t *self, entity_t *slipway) {
    synthesizer_attach_slipway((planet_t *)self, slipway);
}

static const entity_class_t synthesizer_class_def = {
    .draw = ascension_draw_method,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = ascension_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = synthesizer_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .spawn_particle = probe_spawn_particle,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

static planet_t *synthesizer_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_trade_up && "planet_family_trade_init() must run first");
    ascension_init_instance(self, &synthesizer_class_def, &synthesizer_archetype);
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "synthesizer_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    emitter_init((entity_t *)self);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void planet_family_trade_init(void) {
    assert(structure_ready() && ascension_ready() &&
           "planet_family_init() must run first");
    planet_family_trade_shutdown();
    g_family_trade_up = 1;
}

void planet_family_trade_shutdown(void) {
    g_family_trade_up = 0;
}

int slipgate_ready(void) {
    return g_family_trade_up;
}

static entity_t *slipgate_kind_new(const planet_keys_t *keys) {
    return &slipgate_create(keys)->header;
}

static const entity_kind_t slipgate_kind_def = {slipgate_kind_new, &slipgate_kind_record};

const entity_kind_t *slipgate_kind(void) {
    return &slipgate_kind_def;
}

const entity_class_t *slipgate_class(void) {
    return &slipgate_class_def;
}

planet_t *slipgate_new(const planet_keys_t *keys) {
    return slipgate_create(keys);
}

int infraspace_gate_ready(void) {
    return g_family_trade_up;
}

static entity_t *infraspace_gate_kind_new(const planet_keys_t *keys) {
    return &infraspace_gate_create(keys)->header;
}

static const entity_kind_t infraspace_gate_kind_def = {infraspace_gate_kind_new, &infraspace_gate_kind_record};

const entity_kind_t *infraspace_gate_kind(void) {
    return &infraspace_gate_kind_def;
}

const entity_class_t *infraspace_gate_class(void) {
    return &infraspace_gate_class_def;
}

planet_t *infraspace_gate_new(const planet_keys_t *keys) {
    return infraspace_gate_create(keys);
}

int synthesizer_ready(void) {
    return g_family_trade_up;
}

static entity_t *synthesizer_kind_new(const planet_keys_t *keys) {
    return &synthesizer_create(keys)->header;
}

static const entity_kind_t synthesizer_kind_def = {synthesizer_kind_new, &synthesizer_kind_record};

const entity_kind_t *synthesizer_kind(void) {
    return &synthesizer_kind_def;
}

const entity_class_t *synthesizer_class(void) {
    return &synthesizer_class_def;
}

planet_t *synthesizer_new(const planet_keys_t *keys) {
    return synthesizer_create(keys);
}
