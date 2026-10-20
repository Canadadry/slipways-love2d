#include "game/planet_family.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/entity.h"
#include "game/particle.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/probe.h"
#include "game/visual_random.h"

static int g_family_up = 0;

void structure_update_settled_method(entity_t *self, p8_fixed_t t) {
    planet_bob(self, t);
}

static const entity_class_t structure_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = planet_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_STRUCTURE) | TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

void structure_init_instance(planet_t *self, const entity_class_t *cls, const planet_archetype_t *archetype) {
    planet_init_instance(self, cls, archetype);
    self->has |= PLANET_HAS_TARGETING;
    self->targeting.archetype = &self->archetype->targeting;
    self->targeting.cost = self->archetype->targeting.cost;
    self->targeting.has_cost = self->archetype->targeting.has_cost;
    self->sprite = self->archetype->sprite;
    targeting_set_target(&self->targeting, NULL);
}

static planet_t *structure_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    structure_init_instance(self, &structure_class_def, &structure_archetype);
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "structure_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void trading_hub_update_settled(planet_t *self, p8_fixed_t t) {
    if (self == NULL) {
        return;
    }
    planet_bob((entity_t *)self, t);

    assert(self->has_development_tier && "trading_hub_update_settled: a settled trading hub has a development tier; only ascension_draw checks for none");
    if (self->has_development_tier && self->development_tier >= p8_fixed_from_int(2)) {
        self->palette_block =
            p8_fixed_add(p8_fixed_from_int(8),
                            p8_fixed_div(p8_fixed_mod(t, p8_fixed_from_int(42)), p8_fixed_from_int(6)));
    }
}

static void trading_hub_update_settled_method(entity_t *self, p8_fixed_t t) {
    trading_hub_update_settled((planet_t *)self, t);
}

int planet_family_bobs(const entity_t *e) {
    if (e == NULL || e->state != ENTITY_STATE_SETTLED || e->class == NULL) {
        return 0;
    }
    state_fn_t settled = e->class->states[ENTITY_STATE_SETTLED];
    return settled == structure_update_settled_method || settled == trading_hub_update_settled_method;
}

static const entity_class_t trading_hub_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = planet_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = trading_hub_update_settled_method,
        },
    .tags = TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE) | LINEAGE(LINEAGE_TRADING_HUB),
};

static planet_t *trading_hub_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    structure_init_instance(self, &trading_hub_class_def, &trading_hub_archetype);
    self->palette_block = self->archetype->palette_block;
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "trading_hub_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

static const entity_class_t processor_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .draw_overlay = targeting_tooltip_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = planet_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

static planet_t *processor_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    structure_init_instance(self, &processor_class_def, &processor_archetype);
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "processor_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void ascension_draw(p8_graphics_t *gfx, p8_vram_t *mem, planet_t *self, vec_t p) {
    if (self == NULL) {
        return;
    }
    if (self->has_development_tier && self->development_tier >= p8_fixed_from_int(2)) {
        emitter_update(gfx, mem, (entity_t *)self);
    }
    planet_draw(gfx, mem, self, p);
}

void ascension_draw_method(entity_t *self, vec_t screen_pos) {
    ascension_draw(dispatch_context_gfx(), dispatch_context_mem(), (planet_t *)self, screen_pos);
}

void ascension_release_method(entity_t *self) {
    planet_release_row(self);
    emitter_release_particles(emitter_of(self));
    planet_grab_arena_released((const planet_t *)self);
}

static const entity_class_t ascension_class_def = {
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
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .spawn_particle = probe_spawn_particle,
    .states =
        {
            [ENTITY_STATE_VANISHING] = planet_update_vanishing_method,
            [ENTITY_STATE_SETTLED] = structure_update_settled_method,
        },
    .tags = TAG(TAG_ASCENSION) | TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE),
};

void ascension_init_instance(planet_t *self, const entity_class_t *cls, const planet_archetype_t *archetype) {
    structure_init_instance(self, cls, archetype);
    self->has |= PLANET_HAS_EMITTER;
    self->emitter.archetype = &self->archetype->emitter;
    self->emitter.speed_factor = self->archetype->emitter.speed_factor;
    self->emitter.angle_slices = self->archetype->emitter.angle_slices;
    self->emitter.trail_colors = self->archetype->emitter.trail_colors;
    self->emitter.base_radius = self->archetype->emitter.base_radius;
    self->emitter.matrix_xx = self->archetype->emitter.matrix_xx;
    self->emitter.matrix_xy = self->archetype->emitter.matrix_xy;
    self->emitter.matrix_yx = self->archetype->emitter.matrix_yx;
    self->emitter.matrix_yy = self->archetype->emitter.matrix_yy;
    self->emitter.has = 0;
}

static planet_t *ascension_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    ascension_init_instance(self, &ascension_class_def, &ascension_archetype);
    assert((keys == NULL || (!keys->has_map_pos && !keys->has_pos)) &&
           "ascension_create: the only construction key is planet_id (structure_icon_left_press); another key has no member "
           "-- stop, do not add one");
    planet_copy_keys(self, keys);
    emitter_init((entity_t *)self);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void protostar_construct(planet_t *self) {
    burst_keys_t keys = {0};
    planet_t *spawned;

    if (self == NULL) {
        return;
    }

    keys.planet.pos = vec_add(entity_pos(&self->header), self->archetype->spawn_offset);
    keys.planet.has_pos = 1;
    keys.construct = &protostar_burst_construct_keys;

    spawned = burst_new(&keys);
    (void)spawned;
}

static const entity_class_t protostar_class_def = {
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
    .attach_slipway = planet_attach_slipway_method,
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

static planet_t *protostar_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    ascension_init_instance(self, &protostar_class_def, &protostar_archetype);
    planet_copy_keys(self, keys);
    protostar_construct(self);
    emitter_init((entity_t *)self);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void burst_construct(planet_t *self) {
    camera_t *mo;

    if (self == NULL) {
        return;
    }
    emitter_init((entity_t *)self);
    emitter_spawn((entity_t *)self, p8_fixed_from_int(140));

    if (main_camera() != NULL) {
        mo = main_camera_typed();
        assert(mo != NULL && "burst_construct: the camera placeholder has no shake; a burst before reset_world() stops the run");
        mo->header.shake = p8_fixed_from_int(3);
    }
}

particle_t *burst_spawn_particle(entity_t *self_ptr) {
    const planet_t *self = (const planet_t *)self_ptr;
    p8_fixed_t qd;
    p8_fixed_t ms;
    p8_fixed_t iu;
    p8_fixed_t py;
    vec_t p;
    vec_t v;
    vec_t a;
    particle_t *particle;

    qd = p8_fixed_add(visual_random_draw(P8_FIXED_LIT(0x00018000, 1.5)), P8_FIXED_LIT(0x00008000, 0.5));
    if (visual_random_draw_one() < P8_FIXED_LIT(0x00009999, 0.6)) {
        qd = p8_fixed_mul(p8_fixed_flr(p8_fixed_div(qd, P8_FIXED_LIT(0x00008000, 0.5))),
                              P8_FIXED_LIT(0x00008000, 0.5));
    }
    p = vec_from_polar(qd, visual_random_draw_one());

    ms = self->speed;
    iu = self->acceleration_factor;
    py = self->emitter.speed_factor;

    v = vec_mul(p, ms);
    a = vec_mul(p, iu);

    particle = particle_take();
    particle->pos = p;
    particle->velocity = v;
    particle->has_acceleration = 1;
    particle->acceleration = a;
    particle->life = p8_fixed_add(visual_random_draw(py), py);

    return particle;
}

void burst_draw(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos) {
    const pointer_list_t *particles = NULL;
    const int_seq_t *ramp;
    int third_taken = 0;
    int index;
    int screen_x;
    int screen_y;

    if (self == NULL) {
        return;
    }
    ramp = self->emitter.trail_colors;
    assert(ramp != NULL && "a burst always holds a trail ramp: the archetype's default or its construct keys'");

    if (self->emitter.has & EMITTER_HAS_PARTICLES) {
        particles = &self->emitter.particles;
    }

    screen_x = p8_fixed_flr_int(screen_pos.x);
    screen_y = p8_fixed_flr_int(screen_pos.y);

    for (index = 1; index <= 3; index++) {
        const particle_t *pt = NULL;

        if (particles != NULL && index <= particles->count) {
            pt = particles->items[index - 1];
        }

        if (pt != NULL) {
            int idx;
            int color = 0;
            int trail_color;

            idx = p8_fixed_flr_int(p8_fixed_flr(p8_fixed_div(pt->life, p8_fixed_from_int(2))));
            if (int_seq_at(ramp->items, ramp->count, idx, &trail_color)) {
                color = trail_color;
            }
            if (!particle_pixels_hidden()) {
                p8_circ(graphics, memory, screen_x, screen_y, p8_fixed_flr_int(vec_length(pt->pos)), color);
            }
        }

        /* A flag, not the particle: emitter_update may give it back before is_dead is written. */
        third_taken = pt != NULL;
    }

    emitter_update(graphics, memory, (entity_t *)self);
    self->header.is_dead = !third_taken;
}

static void burst_draw_method(entity_t *self, vec_t screen_pos) {
    burst_draw(dispatch_context_gfx(), dispatch_context_mem(), (planet_t *)self, screen_pos);
}

/* Read the slot first: arena_release() frees the struct and the list with it. */
static void burst_release_method(entity_t *self) {
    const allocator_t *slot = ((planet_t *)self)->slipways.zone;

    ascension_release_method(self);
    arena_release(slot);
}

static const entity_class_t burst_class_def = {
    .draw = burst_draw_method,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = burst_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .spawn_particle = burst_spawn_particle,
    .states = {[ENTITY_STATE_VANISHING] = planet_update_vanishing_method},
    .tags = 0,
    .lineage = LINEAGE(LINEAGE_PLANET),
};

static planet_t *burst_create(const burst_keys_t *keys) {
    const allocator_t *slot = particle_burst_slot_take();
    planet_t *self = (planet_t *)allocator_alloc(slot, sizeof *self);

    assert(g_family_up && "planet_family_init() must run first");
    planet_init_instance(self, &burst_class_def, &burst_archetype);
    self->has |= PLANET_HAS_EMITTER;
    self->emitter.archetype = &self->archetype->emitter;
    self->emitter.matrix_xx = self->archetype->emitter.matrix_xx;
    self->emitter.matrix_xy = self->archetype->emitter.matrix_xy;
    self->emitter.matrix_yx = self->archetype->emitter.matrix_yx;
    self->emitter.matrix_yy = self->archetype->emitter.matrix_yy;
    self->emitter.trail_colors = self->archetype->emitter.trail_colors;
    self->acceleration_factor = self->archetype->acceleration_factor;
    self->emitter.speed_factor = self->archetype->emitter.speed_factor;
    self->speed = self->archetype->speed;
    self->emitter.has = 0;
    self->has |= PLANET_HAS_OUTGOING_ROUTES;
    if (keys != NULL) {
        planet_copy_keys(self, &keys->planet);
        if (keys->construct != NULL) {
            self->emitter.matrix_xx = keys->construct->matrix_xx;
            self->emitter.matrix_xy = keys->construct->matrix_xy;
            self->emitter.matrix_yx = keys->construct->matrix_yx;
            self->emitter.matrix_yy = keys->construct->matrix_yy;
            self->emitter.trail_colors = keys->construct->trail_colors;
            self->acceleration_factor = keys->construct->acceleration_factor;
            self->emitter.speed_factor = keys->construct->speed_factor;
            self->speed = keys->construct->speed;
        }
    }
    burst_construct(self);
    planet_construct_in(self, slot);
    entity_construct(&self->header);
    return self;
}

void planet_family_init(void) {
    assert(planet_ready() && "planet_init() must run first");
    planet_family_shutdown();
    g_family_up = 1;
}

void planet_family_shutdown(void) {
    g_family_up = 0;
}

int structure_ready(void) {
    return g_family_up;
}

static entity_t *structure_kind_new(const planet_keys_t *keys) {
    return &structure_create(keys)->header;
}

static const entity_kind_t structure_kind_def = {structure_kind_new, &structure_kind_record};

const entity_kind_t *structure_kind(void) {
    return &structure_kind_def;
}

const entity_class_t *structure_class(void) {
    return &structure_class_def;
}

planet_t *structure_new(const planet_keys_t *keys) {
    return structure_create(keys);
}

int trading_hub_ready(void) {
    return g_family_up;
}

static entity_t *trading_hub_kind_new(const planet_keys_t *keys) {
    return &trading_hub_create(keys)->header;
}

static const entity_kind_t trading_hub_kind_def = {trading_hub_kind_new, &trading_hub_kind_record};

const entity_kind_t *trading_hub_kind(void) {
    return &trading_hub_kind_def;
}

const entity_class_t *trading_hub_class(void) {
    return &trading_hub_class_def;
}

planet_t *trading_hub_new(const planet_keys_t *keys) {
    return trading_hub_create(keys);
}

int processor_ready(void) {
    return g_family_up;
}

static entity_t *processor_kind_new(const planet_keys_t *keys) {
    return &processor_create(keys)->header;
}

static const entity_kind_t processor_kind_def = {processor_kind_new, &processor_kind_record};

const entity_kind_t *processor_kind(void) {
    return &processor_kind_def;
}

const entity_class_t *processor_class(void) {
    return &processor_class_def;
}

planet_t *processor_new(const planet_keys_t *keys) {
    return processor_create(keys);
}

int ascension_ready(void) {
    return g_family_up;
}

static entity_t *ascension_kind_new(const planet_keys_t *keys) {
    return &ascension_create(keys)->header;
}

static const entity_kind_t ascension_kind_def = {ascension_kind_new, &ascension_kind_record};

const entity_kind_t *ascension_kind(void) {
    return &ascension_kind_def;
}

const entity_class_t *ascension_class(void) {
    return &ascension_class_def;
}

planet_t *ascension_new(const planet_keys_t *keys) {
    return ascension_create(keys);
}

int protostar_ready(void) {
    return g_family_up;
}

static entity_t *protostar_kind_new(const planet_keys_t *keys) {
    return &protostar_create(keys)->header;
}

static const entity_kind_t protostar_kind_def = {protostar_kind_new, &protostar_kind_record};

const entity_kind_t *protostar_kind(void) {
    return &protostar_kind_def;
}

const entity_class_t *protostar_class(void) {
    return &protostar_class_def;
}

planet_t *protostar_new(const planet_keys_t *keys) {
    return protostar_create(keys);
}

int burst_ready(void) {
    return g_family_up;
}

static entity_t *burst_kind_new(const planet_keys_t *keys) {
    burst_keys_t burst_keys;

    if (keys == NULL) {
        return &burst_create(NULL)->header;
    }
    burst_keys.planet = *keys;
    burst_keys.construct = NULL;
    return &burst_create(&burst_keys)->header;
}

static const entity_kind_t burst_kind_def = {burst_kind_new, &burst_kind_record};

const entity_kind_t *burst_kind(void) {
    return &burst_kind_def;
}

const entity_class_t *burst_class(void) {
    return &burst_class_def;
}

planet_t *burst_new(const burst_keys_t *keys) {
    return burst_create(keys);
}

static const struct {
    const char *class_name;
    const entity_kind_t *(*kind)(void);
} planet_kinds[] = {
    {"protostar", protostar_kind},
    {"burst", burst_kind},
};

const entity_kind_t *planet_kind_named(const char *fz) {
    size_t i;

    for (i = 0; i < sizeof planet_kinds / sizeof planet_kinds[0]; i++) {
        if (strcmp(planet_kinds[i].class_name, fz) == 0) {
            return planet_kinds[i].kind();
        }
    }
    return NULL;
}
