#include "game/planet.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/collect.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "game/gen.h"
#include "engine/entity.h"
#include "game/message_box.h"
#include "game/research.h"
#include "engine/frame_zone.h"
#include "game/grab_arena.h"

static int g_planet_up = 0;

static weighted_picker_t *g_type_picker = NULL;
static random_walk_t *g_planet_vanish_roll = NULL;

/* Shared by every bare planet, written in place, until planet_install_project gives it its own. */
static production_t g_planet_production_shared;

int planet_hit_test_method(entity_t *self, mouse_pos_t mouse) {
    return entity_geom_contains_cursor(self, mouse);
}

void planet_new_year_method(entity_t *self, p8_fixed_t year) {
    (void)year;
    planet_new_year((const planet_t *)self);
}

int planet_is_family(const entity_t *e) {
    return (e->class->lineage & LINEAGE(LINEAGE_PLANET)) != 0;
}

void planet_update_vanishing_method(entity_t *self, p8_fixed_t t) {
    planet_update_vanishing((planet_t *)self, t);
}

static const entity_class_t planet_class_def = {
    .draw = planet_draw_method,
    .skip_draw_off_screen = 1,
    .hit_test = planet_hit_test_method,
    .new_year = planet_new_year_method,
    .refresh = planet_refresh_method,
    .release = planet_release_method,
    .left_press = planet_left_press_method,
    .accept_drop = planet_accept_drop_method,
    .attach_slipway = planet_attach_slipway_method,
    .offer_trade = planet_offer_trade_method,
    .states = {[ENTITY_STATE_VANISHING] = planet_update_vanishing_method},
    .tags = TAG(TAG_PLANET) | TAG(TAG_NODE),
    .lineage = LINEAGE(LINEAGE_PLANET),
};

void planet_init_instance(planet_t *self, const entity_class_t *instance_class, const planet_archetype_t *archetype) {
    assert(g_planet_up && "planet_init() must run first");
    entity_init_instance(&self->header, instance_class, archetype->state, archetype->state_time);
    self->archetype = archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->header.has |= ENTITY_HAS_HIT_BOX;
    self->header.hit_box = self->archetype->hit_box;
    self->header.has |= ENTITY_HAS_SHAKE;
    self->header.has |= ENTITY_HAS_FOLD_ANCHOR;
    self->header.fold_anchor = 0;
    self->header.has |= ENTITY_HAS_IS_DEAD;
    self->header.shake = self->archetype->shake;
    memset(self->planet_id, 0, sizeof self->planet_id);
    self->slipways = (entity_id_list_t){0};
    self->project = RESEARCH_PROJECT_ID_NONE;
    self->description = NULL;
    self->needs = (pointer_list_t){0};
    self->planet_type = NULL;
    self->layout_row = NULL;
    self->layout_row_slot = NULL;
    self->layout_row_signature = (planet_row_signature_t){0};
    self->targeted_by = ENTITY_ID_NONE;
    self->production_bonus = 0;
    self->has_production_bonus = 0;
    self->header.is_dead = 0;
    self->net_income = 0;
    self->has_net_income = 0;
    self->incoming_routes = (trade_route_list_t){0};
    self->outgoing_routes = (trade_route_list_t){0};
    self->development_tier = self->archetype->development_tier;
    self->has_development_tier = self->archetype->has_development_tier;
    self->header.has |= ENTITY_HAS_IS_HOVERED;
    self->header.is_hovered = 0;
    self->production = (production_t){0};
    self->synthesizable_resources = self->archetype->synthesizable_resources;
    self->has = 0;
}

static planet_t *planet_create(const planet_keys_t *keys) {
    planet_t *self = (planet_t *)allocator_alloc(grab_arena_construction_zone(), sizeof *self);

    planet_init_instance(self, &planet_class_def, &planet_archetype);
    planet_copy_keys(self, keys);
    planet_construct(self);
    entity_construct(&self->header);
    return self;
}

void planet_keys_set_id(planet_keys_t *keys, const char *id) {
    assert(strlen(id) < PLANET_ID_SIZE && "planet_keys_set_id: a planet id is \"p\"/\"s\" plus a counter -- stop, measure before widening");
    strcpy(keys->planet_id, id);
    keys->has_planet_id = 1;
}

const char *planet_id_or_null(const planet_t *self) {
    return (self->has & PLANET_HAS_ID) ? self->planet_id : NULL;
}

void planet_copy_keys(planet_t *self, const planet_keys_t *keys) {
    if (keys == NULL) {
        return;
    }
    if (keys->has_map_pos) {
        entity_set_map_pos(&self->header, keys->map_pos);
    }
    if (keys->has_pos) {
        entity_set_pos(&self->header, keys->pos);
    }
    if (keys->has_planet_id) {
        assert(memchr(keys->planet_id, '\0', PLANET_ID_SIZE) != NULL && "planet_copy_keys: set the id with planet_keys_set_id");
        memcpy(self->planet_id, keys->planet_id, PLANET_ID_SIZE);
        self->has |= PLANET_HAS_ID;
    }
}

void planet_init(void) {
    planet_shutdown();
    g_planet_up = 1;
    g_planet_production_shared = (production_t){0};
}

void planet_shutdown(void) {
    g_planet_up = 0;
    g_type_picker = NULL;
    g_planet_vanish_roll = NULL;
}

void planet_set_type_picker(weighted_picker_t *picker) {
    g_type_picker = picker;
}

void planet_set_vanish_roll(random_walk_t *ir) {
    g_planet_vanish_roll = ir;
}

weighted_picker_t *planet_type_picker(void) {
    return g_type_picker;
}

random_walk_t *planet_vanish_roll(void) {
    return g_planet_vanish_roll;
}

int planet_ready(void) {
    return g_planet_up;
}

const data_row_t *planet_mood_rows(void) {
    return planet_mood_rows_rows;
}

const int_seq_t *planet_made_resources(void) {
    return &planet_made_resources_seq;
}

const int_seq_t *planet_ring_colors(void) {
    return &planet_ring_colors_seq;
}

const data_row_t *planet_spacer(void) {
    return &planet_spacer_row;
}

production_t *planet_shared_production(void) {
    return &g_planet_production_shared;
}

production_t *planet_production(planet_t *self) {
    return (self->has & PLANET_HAS_PRODUCTION) ? &self->production : &g_planet_production_shared;
}

const production_t *planet_production_read(const planet_t *self) {
    return (self->has & PLANET_HAS_PRODUCTION) ? &self->production : &g_planet_production_shared;
}

const entity_class_t *planet_class(void) {
    return &planet_class_def;
}

planet_t *planet_new(const planet_keys_t *keys) {
    return planet_create(keys);
}

void planet_grab_arena_released(const planet_t *self) {
    grab_arena_give_back_if_placing(self->slipways.zone, self->header.state);
}

void planet_construct(planet_t *self) {
    planet_construct_in(self, grab_arena_construction_zone());
}

void planet_construct_in(planet_t *self, const allocator_t *zone) {
    if (self == NULL) {
        return;
    }

    entity_id_list_init(&self->slipways, zone);

    if (self->header.class->lineage & LINEAGE(LINEAGE_STRUCTURE)) {
        research_project_t *target = research_projects_at(p8_fixed_flr_int(self->archetype->project_index));

        if (target != NULL) {
            planet_install_project(self, target);
        }
    }
}

static int planet_project_need_count(const research_project_t *project) {
    int n = 0;

    for (int i = 0; project != NULL && i < project->inputs.count; i++) {
        n += project->inputs.items[i] != NULL;
    }
    return n;
}

static size_t planet_project_bytes(const research_project_t *project) {
    size_t n = (size_t)planet_project_need_count(project);

    return ZONE_BYTES(n * sizeof(void *)) + n * ZONE_BYTES(sizeof(need_t));
}

size_t planet_kind_construction_bytes(const entity_kind_t *kind) {
    const planet_archetype_t *archetype = kind->record->archetype;
    const research_project_t *target = research_projects_at(p8_fixed_flr_int(archetype->project_index));

    return ZONE_BYTES(sizeof(planet_t)) + (target != NULL ? planet_project_bytes(target) : 0);
}

void planet_install_project(planet_t *self, research_project_t *research_project_instance) {
    const allocator_t *zone = grab_arena_construction_zone();
    pointer_list_t jy;
    int n;

    research_project_assert(research_project_instance);
    if (self == NULL) {
        return;
    }
    n = planet_project_need_count(research_project_instance);

    self->project = research_project_id_of(research_project_instance);
    self->description = NULL;

    pointer_list_init(&jy, zone);
    if (n > 0) {
        pointer_list_init_reserve(&jy, zone, n);
    }
    for (int i = 0; n > 0 && i < research_project_instance->inputs.count; i++) {
        const need_t *input = research_project_instance->inputs.items[i];
        need_t *copy;

        if (input == NULL) {
            continue;
        }
        copy = (need_t *)allocator_alloc(zone, sizeof *copy);
        *copy = *input;
        pointer_list_add(&jy, copy);
    }
    self->needs = jy;
    self->has |= PLANET_HAS_NEEDS;

    self->production.has_resource = research_project_instance != NULL && research_project_instance->has_output;
    self->production.resource = self->production.has_resource ? research_project_instance->output : 0;
    self->production.amount = p8_fixed_from_int(0);
    self->has |= PLANET_HAS_PRODUCTION;
}

int planet_is_colonized(const planet_t *self) {
    if (self == NULL) {
        return 0;
    }
    if (!self->archetype->is_planet) {
        return 0;
    }
    return self->header.state == ENTITY_STATE_SETTLED;
}

void planet_restart_shake(planet_t *self) {
    if (self == NULL) {
        return;
    }
    self->header.shake = P8_FIXED_LIT(0x00014000, 1.25);
}

void planet_update_vanishing(planet_t *self, p8_fixed_t t) {
    if (self == NULL) {
        return;
    }
    self->header.is_dead = t > p8_fixed_from_int(60);
    assert((!self->header.is_dead || self->slipways.count == 0) &&
           "a linked planet never dies: the routes kept in outgoing_routes would name it, and the cart never drops them");
}

void planet_roll_type(planet_t *self) {
    const planet_types_entry_t *nk = NULL;
    if (self == NULL) {
        return;
    }

    if (g_planet_vanish_roll != NULL) {
        p8_fixed_t roll = random_walk_next(g_planet_vanish_roll);
        int use_vanishing = 0;

        if (roll < P8_FIXED_LIT(0x00007333, 0.45)) {
            use_vanishing = planet_types_lookup("vanishing") != NULL;
        }
        if (use_vanishing) {
            nk = planet_types_lookup("vanishing");
        } else if (g_type_picker != NULL) {
            nk = weighted_picker_next(g_type_picker);
        }
    }
    self->planet_type = nk;
    self->description = nk != NULL ? nk->description : NULL;

    entity_set_state((entity_t *)self, nk != NULL && nk->state != ENTITY_STATE_NONE ? nk->state : ENTITY_STATE_KNOWN);
    planet_restart_shake(self);
}

void planet_new_year(const planet_t *self) {
    if (self == NULL) {
        return;
    }
    if (!self->has_net_income || self->net_income == p8_fixed_from_int(0)) {
        return;
    }
    if (!entity_geom_is_on_screen((const entity_t *)self)) {
        return;
    }
    {
        toast_keys_t keys = {0};
        int color = 0;
        char *text;

        keys.source = entity_id_link(&self->header);
        text = collect_format_signed(frame_zone(), self->net_income, 10, "$", &color);
        toast_keys_set_text(&keys, text);
        keys.color = p8_fixed_from_int(color);
        toast_new(&keys);
    }
}

static p8_fixed_t bob_delta(p8_fixed_t t) {
    return p8_fixed_mul(p8_fixed_sin(p8_fixed_mul(t, P8_FIXED_LIT(0x00000147, 0.005))), P8_FIXED_LIT(0x00000f5c, 0.06));
}

void planet_bob(entity_t *self, p8_fixed_t t) {
    assert((self == NULL || planet_is_family(self)) && "planet_bob: self is a planet (structure and trading hub settled updates) -- stop");
    if (self != NULL && entity_has_pos(self)) {
        self->pos.y = p8_fixed_add(self->pos.y, bob_delta(t));
    }
}

p8_fixed_t planet_bob_total(p8_fixed_t state_time) {
    uint32_t frames = (uint32_t)state_time >> 16;
    p8_fixed_t total = 0;
    p8_fixed_t t = 0;
    for (uint32_t i = 0; i < frames; i++) {
        total = p8_fixed_add(total, bob_delta(t));
        t = p8_fixed_add(t, p8_fixed_from_int(1));
    }
    return total;
}
