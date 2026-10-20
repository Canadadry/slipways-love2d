#include "game/slipway.h"

#include <assert.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "game/economy_formulas.h"
#include "engine/entity_registry.h"
#include "game/main_economy.h"
#include "engine/geom.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_id_list.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/targeting.h"
#include "game/tech_gate.h"
#include "game/ui.h"
#include "engine/vec.h"
#include "engine/frame_zone.h"
#include "game/particle_zone.h"

static int g_slipway_up = 0;

static const entity_class_t slipway_too_far_class_def = {0};

static slipway_too_far_t g_slipway_too_far = {{.class = &slipway_too_far_class_def}, "-too far-", {0, 0}};

static const entity_class_t slipway_mouse_point_class_def = {0};

vec_t slipway_end_map_pos(const entity_t *end) {
    return end != NULL ? entity_map_pos(end) : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

vec_t slipway_end_pos(const entity_t *end) {
    return end != NULL ? entity_pos(end) : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

entity_t *slipway_start(const slipway_t *self) {
    return entity_get(self->start);
}

entity_t *slipway_end(const slipway_t *self) {
    if (self->end_is_point) {
        return (entity_t *)&self->point.header;
    }
    return entity_get(self->end);
}

entity_t *slipway_too_far(void) {
    return &g_slipway_too_far.header;
}

static int slipway_copy_infraspace(slipway_t *self, const entity_t *from) {
    if (from == NULL || !entity_has(from, ENTITY_HAS_INFRASPACE)) {
        return 0;
    }
    self->header.infraspace = from->infraspace;
    self->header.has |= ENTITY_HAS_INFRASPACE;
    return 1;
}

/* The start's infraspace, else the end's, and the layer that follows. */
static void slipway_inherit_infraspace(slipway_t *self) {
    /* An infraspace of 0 still counts as one. */
    self->header.infraspace = p8_fixed_from_int(0);
    self->header.has &= (uint16_t)~ENTITY_HAS_INFRASPACE;
    if (!slipway_copy_infraspace(self, slipway_start(self))) {
        slipway_copy_infraspace(self, slipway_end(self));
    }
    entity_registry_set_layer(&self->header, entity_has(&self->header, ENTITY_HAS_INFRASPACE) ? p8_fixed_from_int(2) : p8_fixed_from_int(4));
}

void slipway_served_planets_add(slipway_served_planets_t *hg, const char *id) {
    assert(id != NULL &&
           "slipway_served_planets_add: a NULL identifier does not fit the member's type -- stop, do not widen the member");
    if (slipway_served_planets_has(hg, id)) {
        return;
    }
    assert(hg->count < SLIPWAY_SERVED_PLANETS_CAP && "slipway_served_planets_add: more planets than the galaxy holds (90) -- stop, do not widen the member");
    assert(strlen(id) < PLANET_ID_SIZE &&
           "slipway_served_planets_add: a planet id wider than any the cart makes -- stop, do not widen the copy");
    strcpy(hg->ids[hg->count], id);
    hg->count++;
}

int slipway_served_planets_has(const slipway_served_planets_t *hg, const char *id) {
    int i;

    if (hg == NULL || id == NULL) {
        return 0;
    }
    for (i = 0; i < hg->count; i++) {
        if (strcmp(hg->ids[i], id) == 0) {
            return 1;
        }
    }
    return 0;
}

void slipway_construct(slipway_t *self) {
    if (self == NULL) {
        return;
    }

    pointer_list_init_reserve(&self->transit_dots, grab_arena_construction_zone(), SLIPWAY_TRANSIT_DOTS_RESERVE);
    self->served_planets.count = 0;
}

void slipway_drag(slipway_t *self, mouse_pos_t mouse) {
    ui_t *ok;
    entity_t *hovered;

    if (self == NULL) {
        return;
    }

    ok = main_ui();
    hovered = ok != NULL ? ui_hovered(ok) : NULL;

    if (hovered != NULL && hovered->class->accept_drop != NULL) {
        self->end = entity_id_link(hovered);
        self->end_is_point = 0;
    } else {
        vec_t point_pos = mouse_pos_world(mouse);

        if (mouse_pos_has_world(mouse)) {
            entity_set_pos(&self->point.header, point_pos);
        } else {
            entity_clear_pos(&self->point.header);
        }
        entity_set_map_pos(&self->point.header, vec_div(point_pos, p8_fixed_from_int(128)));
        self->end = ENTITY_ID_NONE;
        self->end_is_point = 1;
    }

    self->targeting.cost = economy_formulas_route_base_cost(slipway_end_map_pos(slipway_start(self)), slipway_end_map_pos(slipway_end(self)));
    self->targeting.has_cost = 1;

    slipway_inherit_infraspace(self);

    targeting_set_target(&self->targeting, slipway_obstruction(self));
    if (targeting_target(&self->targeting) != NULL) {
        probe_link_target(targeting_target(&self->targeting), &self->header);
    }
}

void slipway_connect(slipway_t *self, entity_t *end) {
    assert(self != NULL && end != NULL && "slipway_connect: a resumed slipway joins two saved nodes");
    self->end = entity_id_link(end);
    self->end_is_point = 0;
    slipway_inherit_infraspace(self);
    entity_set_state(&self->header, ENTITY_STATE_NORMAL);
}

void slipway_drop(slipway_t *self, entity_t *end_planet) {
    int refused;

    if (self == NULL) {
        return;
    }

    /* The charge spends credits: keep it behind the NULL test. The clock advances once, after both attaches. */
    refused = (end_planet == NULL) || !targeting_confirm_without_clock((entity_t *)self);
    if (refused) {
        self->header.is_dead = 1;
        return;
    }

    self->fade_in_step = p8_fixed_from_int(1);
    self->end = entity_id_link(end_planet);
    self->end_is_point = 0;

    {
        entity_t *qu = slipway_start(self);
        int start_registered = qu != NULL && qu->class->attach_slipway != NULL;
        int end_registered;
        if (start_registered) {
            qu->class->attach_slipway(qu, &self->header);
        }
        end_registered = end_planet != NULL && end_planet->class->attach_slipway != NULL;
        if (end_registered) {
            end_planet->class->attach_slipway(end_planet, &self->header);
        }
        assert(start_registered && end_registered &&
               "slipway_drop: both endpoints must have attach_slipway, or the slipway never joins their slipways lists");
        (void)start_registered;
        (void)end_registered;
        economy_advance_clock(main_economy_typed(), p8_fixed_from_int(1));
    }
}

typedef struct {
    vec_t start_point;
    vec_t end_point;
} slipway_obstruction_node_ctx_t;

static int slipway_obstruction_node_predicate(entity_t *e, void *ctx_ptr) {
    const slipway_obstruction_node_ctx_t *ctx = (const slipway_obstruction_node_ctx_t *)ctx_ptr;

    if (e == NULL) {
        return 0;
    }
    return geom_near_segment(ctx->start_point, ctx->end_point, slipway_end_map_pos(e), P8_FIXED_LIT(0x000000d1, 0.0032));
}

typedef struct {
    vec_t start_point;
    vec_t end_point;
    int self_has_infraspace;
    p8_fixed_t self_infraspace;
} slipway_obstruction_slipway_ctx_t;

static int slipway_obstruction_slipway_predicate(entity_t *e, void *ctx_ptr) {
    const slipway_obstruction_slipway_ctx_t *ctx = (const slipway_obstruction_slipway_ctx_t *)ctx_ptr;
    const slipway_t *sw;

    if (e == NULL) {
        return 0;
    }
    assert(e->class == slipway_class() && "the slipway tag holds slipways only");
    sw = (const slipway_t *)e;
    if (entity_has(&sw->header, ENTITY_HAS_INFRASPACE) != ctx->self_has_infraspace ||
        (entity_has(&sw->header, ENTITY_HAS_INFRASPACE) && sw->header.infraspace != ctx->self_infraspace)) {
        return 0;
    }
    return geom_segments_cross(ctx->start_point, ctx->end_point, slipway_end_map_pos(slipway_start(sw)), slipway_end_map_pos(slipway_end(sw)));
}

entity_t *slipway_obstruction(slipway_t *self) {
    entity_t *start_node;
    entity_t *end_node;
    vec_t start_point;
    vec_t end_point;
    p8_fixed_t threshold;

    if (self == NULL) {
        return NULL;
    }

    start_node = slipway_start(self);
    end_node = slipway_end(self);
    start_point = slipway_end_map_pos(start_node);
    end_point = slipway_end_map_pos(end_node);

    threshold = tech_gate_if_unlocked("space folding", P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x0001147a, 1.08));

    if (vec_length_squared(vec_sub(start_point, end_point)) > threshold) {
        return &g_slipway_too_far.header;
    }

    {
        const entity_id_list_t *oe = NULL;

        if (start_node != NULL) {
            assert(planet_is_family(start_node) && "a slipway's start is a planet: slipway_create asserts it");
            oe = &((const planet_t *)start_node)->slipways;
        }
        if (oe != NULL) {
            entity_id_list_iter_t it = entity_id_list_all(oe);
            entity_t *w;
            while (entity_id_list_iter_next(&it, &w)) {
                assert(w->class == slipway_class() &&
                       "the start's slipways list holds slipways: only planet_attach_slipway adds to it, and only slipways");
                if (slipway_other_end((const slipway_t *)w, start_node) == end_node) {
                    return w;
                }
            }
        }
    }

    {
        slipway_obstruction_node_ctx_t node_ctx;
        slipway_obstruction_slipway_ctx_t slipway_ctx;
        entity_list_t part1;
        entity_list_t part2;
        entity_list_t ds;
        entity_t *result = NULL;

        node_ctx.start_point = start_point;
        node_ctx.end_point = end_point;
        slipway_ctx.start_point = start_point;
        slipway_ctx.end_point = end_point;
        slipway_ctx.self_has_infraspace = entity_has(&self->header, ENTITY_HAS_INFRASPACE);
        slipway_ctx.self_infraspace = self->header.infraspace;

        part1 = entity_list_filter(frame_zone(), entities_by_tag(TAG_NODE), slipway_obstruction_node_predicate, &node_ctx);
        part2 = entity_list_filter(frame_zone(), entities_by_tag(TAG_SLIPWAY), slipway_obstruction_slipway_predicate, &slipway_ctx);
        ds = entity_list_concat(frame_zone(), &part1, &part2);

        if (ds.count > 0) {
            result = ds.items[0];
        }
        return result;
    }
}

entity_t *slipway_other_end(const slipway_t *self, const entity_t *planet_instance) {
    if (self == NULL) {
        return NULL;
    }

    /* A NULL end falls through to the start even when `planet_instance` is the start. */
    if (planet_instance == slipway_start(self) && slipway_end(self) != NULL) {
        return slipway_end(self);
    }
    return slipway_start(self);
}

void slipway_draw_overlay(slipway_t *self) {
    if (self == NULL) {
        return;
    }

    if (slipway_start(self) != slipway_end(self)) {
        targeting_tooltip(dispatch_context_gfx(), dispatch_context_mem(), (entity_t *)self);
    }
}

static void slipway_drag_method(entity_t *self, mouse_pos_t mouse) {
    slipway_drag((slipway_t *)self, mouse);
}

static void slipway_drop_method(entity_t *self, entity_t *dropped) {
    slipway_drop((slipway_t *)self, dropped);
}

static void slipway_draw_overlay_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    slipway_draw_overlay((slipway_t *)self);
}

static void slipway_release_method(entity_t *self) {
    slipway_t *jc = (slipway_t *)self;
    const allocator_t *zone = jc->transit_dots.zone;

    for (int i = 0; i < jc->transit_dots.count; i++)
        allocator_release(particle_zone(PARTICLE_SLOT_TRANSIT_DOT), jc->transit_dots.items[i]);
    jc->transit_dots.count = 0;
    grab_arena_give_back_if_placing(zone, jc->header.state);
}

static int slipway_is_endpoint_class(const entity_class_t *cls) {
    return cls->states[ENTITY_STATE_SETTLED] != NULL || cls == planet_class();
}

static const entity_class_t slipway_class_def = {
    .draw = slipway_draw_method,
    .draw_overlay = slipway_draw_overlay_method,
    .drag = slipway_drag_method,
    .drop = slipway_drop_method,
    .release = slipway_release_method,
    .tags = TAG(TAG_SLIPWAY),
};

static slipway_t *slipway_create(const slipway_keys_t *keys) {
    slipway_t *self;

    grab_arena_open(SLIPWAY_CONSTRUCTION_BYTES);
    self = (slipway_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    entity_init_instance(&self->header, &slipway_class_def, slipway_archetype.state, slipway_archetype.state_time);
    self->archetype = &slipway_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->targeting.archetype = &self->archetype->targeting;
    self->fade_in_step = self->archetype->fade_in_step;
    self->start = ENTITY_ID_NONE;
    self->transit_dots = (pointer_list_t){0};
    self->served_planets.count = 0;
    self->end = ENTITY_ID_NONE;
    self->end_is_point = 0;
    self->targeting.cost = self->archetype->targeting.cost;
    self->targeting.has_cost = self->archetype->targeting.has_cost;
    self->header.infraspace = 0;
    targeting_set_target(&self->targeting, NULL);
    self->targeted_by = ENTITY_ID_NONE;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->header.is_dead = 0;
    entity_init_header(&self->point.header, &slipway_mouse_point_class_def);
    entity_set_map_pos(&self->point.header, vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0)));
    if (keys != NULL && entity_get(keys->start) != NULL) {
        assert(slipway_is_endpoint_class(entity_get(keys->start)->class) &&
               "slipway_create: a start key that is neither NULL nor a planet or a structure "
               "does not fit the member's type -- stop, do not widen the member");
        self->start = keys->start;
    }
    slipway_construct(self);
    grab_arena_close();
    entity_construct(&self->header);
    return self;
}

void slipway_init(void) {
    slipway_shutdown();
    g_slipway_up = 1;
}

void slipway_shutdown(void) {
    g_slipway_up = 0;
}

int slipway_ready(void) {
    return g_slipway_up;
}

const entity_class_t *slipway_class(void) {
    return &slipway_class_def;
}

slipway_t *slipway_new(const slipway_keys_t *keys) {
    return slipway_create(keys);
}

const entity_class_t *slipway_too_far_class(void) {
    return &slipway_too_far_class_def;
}

const entity_class_t *slipway_mouse_point_class(void) {
    return &slipway_mouse_point_class_def;
}
