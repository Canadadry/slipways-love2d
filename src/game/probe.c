#include "game/probe.h"

#include <assert.h>

#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "engine/geom.h"
#include "p8/gfx.h"
#include "game/hud.h"
#include "engine/entity.h"
#include "game/particle.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/slipway.h"
#include "game/ui.h"
#include "game/wormhole.h"
#include "game/visual_random.h"
#include "p8/str.h"
#include "engine/frame_zone.h"

static int g_probe_up = 0;
typedef enum { MAIN_ECONOMY_NONE, MAIN_ECONOMY_PLACEHOLDER, MAIN_ECONOMY_REAL } main_economy_state_t;

static main_economy_state_t g_main_economy_state = MAIN_ECONOMY_NONE;
static entity_id_t g_main_economy;

static const entity_class_t main_economy_placeholder_class_def = {0};

static main_economy_placeholder_t g_main_economy_placeholder = {
    {.class = &main_economy_placeholder_class_def}, 0, 0, 0, 0};

static int targeting_track_target_is_holder_class(const entity_class_t *cls) {
    return cls->drag == targeting_track_method || cls == wormhole_class();
}

static const entity_class_t targeting_track_target_class_def = {0};

static targeting_track_target_t g_targeting_track_target = {{.class = &targeting_track_target_class_def}, {0, 0}};

static int probe_to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

typedef struct {
    vec_t self_map_pos;
} targeting_track_ctx_t;

static int targeting_track_slipway_predicate(entity_t *e, void *ctx_ptr) {
    const targeting_track_ctx_t *ctx = (const targeting_track_ctx_t *)ctx_ptr;
    const slipway_t *sw;
    vec_t start_p;
    vec_t end_p;

    if (e == NULL) {
        return 0;
    }
    assert(e->class == slipway_class() && "the slipway tag holds slipways only");
    sw = (const slipway_t *)e;

    start_p = slipway_end_map_pos(slipway_start(sw));
    end_p = slipway_end_map_pos(slipway_end(sw));

    return geom_near_segment(start_p, end_p, ctx->self_map_pos, P8_FIXED_LIT(0x000000d1, 0.0032));
}

static int probe_panel_box_contains(vec_t rp) {
    const box_t *hw = ui_panel_box();

    if (hw == NULL) {
        return 0;
    }
    return box_contains(*hw, rp);
}

static int probe_is_target_class(const entity_class_t *cls) {
    return cls->states[ENTITY_STATE_SETTLED] != NULL || cls == planet_class() ||
           cls == slipway_class() || cls == targeting_track_target_class();
}

void probe_link_target(entity_t *et, entity_t *self) {
    if (planet_is_family(et)) {
        assert((self->class->drag == targeting_track_method || self->class == slipway_class()) &&
               "probe_link_target: a planet's targeted_by takes NULL, a host whose drag is targeting_track_method "
               "or a slipway, nothing else -- stop, do not widen the member");
        ((planet_t *)et)->targeted_by = entity_id_link(self);
        return;
    }
    if (et->class == slipway_class()) {
        assert((self->class->drag == targeting_track_method || self->class == slipway_class() ||
                self->class == wormhole_class()) &&
               "probe_link_target: a slipway's targeted_by takes NULL, a host whose drag is targeting_track_method, "
               "a wormhole or another slipway, nothing else -- stop");
        ((slipway_t *)et)->targeted_by = entity_id_link(self);
        return;
    }
    if (et->class == slipway_too_far_class()) {
        assert(self->class == slipway_class() &&
               "probe_link_target: the too-far slipway struct's targeted_by takes NULL or a slipway, nothing else "
               "-- stop, do not widen the member");
        ((slipway_too_far_t *)et)->targeted_by = entity_id_link(self);
        return;
    }
    assert(et == &g_targeting_track_target.header && "probe_link_target: the target is a planet, a slipway or the static throwaway struct, nothing else");
    assert(targeting_track_target_is_holder_class(self->class) &&
           "probe_link_target: the throwaway target's targeted_by takes NULL, a host whose drag is "
           "targeting_track_method or a wormhole, nothing else -- stop, do not widen the member");
    g_targeting_track_target.targeted_by = entity_id_link(self);
}

entity_t *targeting_target(const targeting_t *self) {
    switch (self->target.kind) {
    case TARGETING_TARGET_ENTITY:
        return entity_get(self->target.id);
    case TARGETING_TARGET_TRACK_THROWAWAY:
        return &g_targeting_track_target.header;
    case TARGETING_TARGET_SLIPWAY_TOO_FAR:
        return slipway_too_far();
    case TARGETING_TARGET_WORMHOLE_TOO_FAR:
        return &wormhole_too_far_target_wrapper()->header;
    case TARGETING_TARGET_NONE:
        break;
    }
    return NULL;
}

void targeting_set_target(targeting_t *self, entity_t *target) {
    targeting_target_t value = {TARGETING_TARGET_NONE, ENTITY_ID_NONE};

    if (target == &g_targeting_track_target.header) {
        value.kind = TARGETING_TARGET_TRACK_THROWAWAY;
    } else if (target == slipway_too_far()) {
        value.kind = TARGETING_TARGET_SLIPWAY_TOO_FAR;
    } else if (target == &wormhole_too_far_target_wrapper()->header) {
        value.kind = TARGETING_TARGET_WORMHOLE_TOO_FAR;
    } else if (target != NULL) {
        value.kind = TARGETING_TARGET_ENTITY;
        value.id = entity_id_link(target);
    }
    self->target = value;
}

targeting_t *targeting_of(entity_t *e) {
    if (e->class->lineage & LINEAGE(LINEAGE_PROBE)) {
        return &((probe_t *)e)->targeting;
    }
    if (planet_is_family(e)) {
        planet_t *planet = (planet_t *)e;
        return (planet->has & PLANET_HAS_TARGETING) ? &planet->targeting : NULL;
    }
    if (e->class == slipway_class()) {
        return &((slipway_t *)e)->targeting;
    }
    if (e->class == wormhole_class()) {
        return &((wormhole_t *)e)->targeting;
    }
    return NULL;
}

void targeting_track(entity_t *self, mouse_pos_t mouse) {
    targeting_t *targeting;
    vec_t mouse_world_vec;
    vec_t p_vec;
    p8_fixed_t distance;
    entity_t *candidate;
    entity_t *target;

    if (self == NULL) {
        return;
    }
    targeting = targeting_of(self);
    assert(targeting != NULL && "targeting_track: a class that carries no targeting is no receiver -- stop");

    mouse_world_vec = mouse_pos_world(mouse);
    p_vec = vec_div(mouse_world_vec, p8_fixed_from_int(128));
    assert(((self->class->lineage & LINEAGE(LINEAGE_PROBE)) || planet_is_family(self) || self->class == wormhole_class()) &&
           "targeting_track: a host neither a probe, of the planet family nor a wormhole, none of whose drag runs it -- stop");
    if (mouse_pos_has_world(mouse)) {
        entity_set_pos(self, mouse_world_vec);
    } else {
        entity_clear_pos(self);
    }
    entity_set_map_pos(self, p_vec);

    candidate = NULL;
    distance = entity_geom_nearest_except(p_vec, entities_by_tag(TAG_NODE), self, &candidate);

    target = NULL;

    if (distance < targeting->archetype->attach_distance && candidate != NULL) {
        target = candidate;
    } else {
        targeting_track_ctx_t ctx;
        entity_t *first;

        ctx.self_map_pos = p_vec;
        first = entity_list_find_first(entities_by_tag(TAG_SLIPWAY), targeting_track_slipway_predicate, &ctx);

        if (first != NULL) {
            target = first;
        } else {
            if (!probe_panel_box_contains(mouse_pos_screen(mouse))) {
                target = &g_targeting_track_target.header;
            } else {
                target = NULL;
            }
        }
    }

    assert((target == NULL ||
            probe_is_target_class(target->class)) &&
           "targeting_track: the target is NULL or what it aims at (a planet, a class with a settled state, a slipway, "
           "or the static throwaway struct), nothing else");
    targeting_set_target(targeting, target);

    if (targeting_target(targeting) != NULL) {
        probe_link_target(targeting_target(targeting), self);
    }
}

void targeting_track_method(entity_t *self, mouse_pos_t mouse) {
    targeting_track(self, mouse);
}

static const char *probe_target_status_text(const entity_t *et) {
    if (et->class == wormhole_too_far_target_class()) {
        return ((const wormhole_too_far_target_t *)et)->status_text;
    }
    if (et->class == slipway_too_far_class()) {
        return ((const slipway_too_far_t *)et)->status_text;
    }
    return NULL;
}

void targeting_tooltip(const p8_graphics_t *gfx, p8_vram_t *mem, entity_t *self) {
    if (self == NULL) {
        return;
    }
    assert(targeting_of(self) != NULL && "targeting_tooltip: a class that carries no targeting is no receiver -- stop");

    targeting_tooltip_show(gfx, mem, self->state, targeting_of(self));
}

void targeting_tooltip_show(const p8_graphics_t *gfx, p8_vram_t *mem, state_t state, const targeting_t *pb) {
    vec_t mp;
    const mouse_t *je;
    const char *display = "";

    if (state != ENTITY_STATE_PLACING) {
        return;
    }

    je = main_mouse_typed();
    mp = je != NULL ? entity_pos(&je->header) : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));

    if (targeting_target(pb) != NULL) {
        const char *qs = probe_target_status_text(targeting_target(pb));
        if (qs != NULL) {
            display = qs;
        } else {
            if (gfx != NULL && mem != NULL) {
                p8_spr_default(gfx, mem, 31, probe_to_int(p8_fixed_sub(mp.x, p8_fixed_from_int(3))),
                                   probe_to_int(p8_fixed_sub(mp.y, p8_fixed_from_int(8))));
            }
        }
    } else if (pb->has_cost) {
        display = p8_concat2(frame_zone(), p8_tostr_fixed(frame_zone(), pb->cost), "$");
    }

    if (gfx != NULL && mem != NULL) {
        p8_fixed_t a = P8_FIXED_LIT(0x00008000, 0.5);
        hud_print_outlined(gfx, mem, display, mp.x, p8_fixed_sub(mp.y, p8_fixed_from_int(8)), 8, a);
    }
}

void targeting_tooltip_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    targeting_tooltip(dispatch_context_gfx(), dispatch_context_mem(), self);
}

static void probe_host_raise_is_dead(entity_t *self) {
    assert(entity_has(self, ENTITY_HAS_IS_DEAD) && "targeting_confirm: a host that carries no is_dead, none of whose drop runs it -- stop");
    self->is_dead = 1;
}

static const targeting_t *targeting_charge(entity_t *self) {
    const targeting_t *targeting;
    int has_target;
    int spend_ok = 0;

    if (self == NULL) {
        return NULL;
    }
    targeting = targeting_of(self);
    assert(targeting != NULL && "targeting_confirm: a class that carries no targeting is no receiver -- stop");

    has_target = targeting_target(targeting) != NULL;

    if (!has_target) {
        spend_ok = economy_spend(main_economy_typed(), targeting->has_cost ? targeting->cost : p8_fixed_from_int(0));
    }

    assert((!has_target || !spend_ok) && "targeting_confirm: economy_spend must stay short-circuited when a target is set");

    if (!has_target && spend_ok) {
        state_t confirmed = targeting->archetype->confirmed_state;

        entity_set_state(self, confirmed != ENTITY_STATE_NONE ? confirmed : ENTITY_STATE_NORMAL);
        return targeting;
    }

    probe_host_raise_is_dead(self);
    return NULL;
}

int targeting_confirm(entity_t *self) {
    const targeting_t *targeting = targeting_charge(self);

    if (targeting == NULL) {
        return 0;
    }
    if (targeting->archetype->clock_skips_recompute) {
        economy_tick_clock(main_economy_typed(), targeting->archetype->clock_ticks);
    } else {
        economy_advance_clock(main_economy_typed(), targeting->archetype->clock_ticks);
    }
    return 1;
}

int targeting_confirm_without_clock(entity_t *self) {
    return targeting_charge(self) != NULL;
}

void targeting_confirm_method(entity_t *self, entity_t *dropped) {
    (void)dropped;
    (void)targeting_confirm(self);
}

particle_t *probe_spawn_particle(entity_t *self) {
    const emitter_t *emitter = NULL;
    p8_fixed_t angle_slices = 0, state_time = 0, base_radius = 0, speed_factor = 0, lifetime = 0;
    int pk = 0;
    p8_fixed_t a, rnd_angle_slices, rnd_half;
    vec_t p, v;
    particle_t *particle;

    if (self != NULL) {
        emitter = emitter_of(self);
        assert(emitter != NULL && "probe_spawn_particle: a class that carries no particle emitter spawns nothing -- stop");
        angle_slices = emitter->angle_slices;
        state_time = self->state_time;
        base_radius = emitter->base_radius;
        speed_factor = emitter->speed_factor;
        lifetime = emitter->archetype->lifetime;
        pk = emitter->archetype->quarter_turn != 0;
    }

    /* Two statements: C leaves argument order unspecified, and rnd(angle_slices) must come first. */
    rnd_angle_slices = visual_random_draw(angle_slices);
    rnd_half = visual_random_draw(P8_FIXED_LIT(0x00008000, 0.5));
    a = p8_fixed_add(
        p8_fixed_div(p8_fixed_add(p8_fixed_flr(rnd_angle_slices), rnd_half), angle_slices),
        p8_fixed_mul(state_time, P8_FIXED_LIT(0x00000041, 0.001)));

    p = vec_from_polar(p8_fixed_add(base_radius, visual_random_draw_one()), a);
    v = vec_mul(p, speed_factor);

    if (pk) {
        v = vec_perpendicular(v);
    }

    particle = particle_take();
    particle->pos = p;
    particle->velocity = v;
    particle->has_acceleration = 0;
    particle->acceleration = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    particle->life = lifetime;

    return particle;
}

void probe_release_method(entity_t *self) {
    const probe_t *g_ = (const probe_t *)self;

    emitter_release_particles(emitter_of(self));
    if (g_->grab_arena != NULL && self->state != ENTITY_STATE_SETTLED) {
        grab_arena_give_back(g_->grab_arena);
    }
}

static const entity_class_t probe_class_def = {
    .draw = emitter_update_method,
    .draw_overlay = targeting_tooltip_method,
    .drag = targeting_track_method,
    .drop = targeting_confirm_method,
    .spawn_particle = probe_spawn_particle,
    .release = probe_release_method,
    .tags = TAG(TAG_PROBE),
    .lineage = LINEAGE(LINEAGE_PROBE),
};

void probe_init_instance(probe_t *self, const entity_class_t *cls, const probe_archetype_t *archetype) {
    entity_init_instance(&self->header, cls, archetype->state, archetype->state_time);
    self->archetype = archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->header.has |= ENTITY_HAS_FOLD_ANCHOR;
    self->header.fold_anchor = self->archetype->fold_anchor;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->emitter.archetype = &self->archetype->emitter;
    self->emitter.has = 0;
    if (self->archetype->has_pos) {
        entity_set_pos(&self->header, self->archetype->pos);
    }
    self->targeting.archetype = &self->archetype->targeting;
    targeting_set_target(&self->targeting, NULL);
    self->header.is_dead = 0;
    self->grab_arena = NULL;
    self->targeting.cost = self->archetype->targeting.cost;
    self->targeting.has_cost = self->archetype->targeting.has_cost;
    self->emitter.speed_factor = self->archetype->emitter.speed_factor;
    self->emitter.angle_slices = self->archetype->emitter.angle_slices;
    self->emitter.trail_colors = self->archetype->emitter.trail_colors;
    self->emitter.matrix_xx = self->archetype->emitter.matrix_xx;
    self->emitter.matrix_xy = self->archetype->emitter.matrix_xy;
    self->emitter.matrix_yx = self->archetype->emitter.matrix_yx;
    self->emitter.matrix_yy = self->archetype->emitter.matrix_yy;
    self->emitter.base_radius = self->archetype->emitter.base_radius;
}

static probe_t *probe_create(const probe_keys_t *keys) {
    probe_t *self;

    assert(g_probe_up && "probe_init() must run first");
    grab_arena_open(ZONE_BYTES(sizeof *self));
    self = (probe_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);
    probe_init_instance(self, &probe_class_def, &probe_archetype);
    self->grab_arena = grab_arena_construction_zone();
    grab_arena_close();
    if (keys != NULL) {
        self->header.state_time = keys->state_time;
        if (keys->has_angle_slices) {
            self->emitter.angle_slices = keys->angle_slices;
        }
        if (keys->has_base_radius) {
            self->emitter.base_radius = keys->base_radius;
        }
        if (keys->has_speed_factor) {
            self->emitter.speed_factor = keys->speed_factor;
        }
    }
    emitter_init((entity_t *)self);
    entity_construct(&self->header);
    return self;
}

void probe_init(void) {
    probe_shutdown();

    g_probe_up = 1;

    g_main_economy_placeholder.research_points = 0;
    g_main_economy_placeholder.has_research_points = 0;
    g_main_economy_placeholder.credits = 0;
    g_main_economy_placeholder.has_credits = 0;
    probe_set_main_economy(&g_main_economy_placeholder.header);
}

void probe_shutdown(void) {
    g_probe_up = 0;
    probe_set_main_economy(NULL);
}

int probe_ready(void) {
    return g_probe_up;
}

const entity_class_t *probe_class(void) {
    return &probe_class_def;
}

probe_t *probe_new(const probe_keys_t *keys) {
    return probe_create(keys);
}

const entity_class_t *main_economy_placeholder_class(void) {
    return &main_economy_placeholder_class_def;
}

entity_t *main_economy(void) {
    switch (g_main_economy_state) {
    case MAIN_ECONOMY_PLACEHOLDER:
        return &g_main_economy_placeholder.header;
    case MAIN_ECONOMY_REAL:
        return entity_get(g_main_economy);
    default:
        return NULL;
    }
}

economy_t *main_economy_typed(void) {
    entity_t *e = g_main_economy_state == MAIN_ECONOMY_REAL ? entity_get(g_main_economy) : NULL;

    assert((e == NULL || e->class == economy_class()) && "main_economy_typed: only the placeholder or an economy is ever stored");
    return (economy_t *)e;
}

const main_economy_placeholder_t *main_economy_placeholder(void) {
    return g_main_economy_state == MAIN_ECONOMY_PLACEHOLDER ? &g_main_economy_placeholder : NULL;
}

void probe_set_main_economy(entity_t *instance) {
    if (instance == NULL) {
        g_main_economy_state = MAIN_ECONOMY_NONE;
        g_main_economy = ENTITY_ID_NONE;
    } else if (instance == &g_main_economy_placeholder.header) {
        g_main_economy_state = MAIN_ECONOMY_PLACEHOLDER;
        g_main_economy = ENTITY_ID_NONE;
    } else {
        g_main_economy_state = MAIN_ECONOMY_REAL;
        g_main_economy = entity_id_link(instance);
    }
}

const entity_class_t *targeting_track_target_class(void) {
    return &targeting_track_target_class_def;
}
