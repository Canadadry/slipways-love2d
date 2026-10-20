#include "game/structure_icon.h"
#include "engine/entity_id.h"

#include <assert.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "engine/entity_geom.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "engine/entity.h"
#include "game/layout.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/probe.h"
#include "p8/str.h"
#include "game/tech_gate.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

static int g_structure_icon_up = 0;

static vec_t g_cursor;
static int g_has_cursor = 0;

static void structure_icon_clear_cursor(void) {
    g_cursor = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    g_has_cursor = 0;
}
static p8_fixed_t g_id_counter = 0;
static int g_has_id_counter = 0;

static int structure_icon_to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

static const entity_kind_t *structure_icon_own_kind(const structure_icon_t *self) {
    return self != NULL ? self->kind : NULL;
}

static const entity_class_t structure_icon_kind_view_class_def = {0};

structure_icon_kind_t structure_icon_kind_view(const structure_icon_t *self) {
    const entity_kind_t *kind = structure_icon_own_kind(self);
    structure_icon_kind_t view;

    entity_init_header(&view.header, &structure_icon_kind_view_class_def);
    view.required_tech = NULL;
    view.sprite = 0;
    view.has_sprite = 0;
    view.state = ENTITY_STATE_NONE;
    view.cost = 0;
    view.has_cost = 0;
    if (kind != NULL) {
        view.required_tech = kind->record->required_tech;
        view.sprite = kind->record->sprite;
        view.has_sprite = kind->record->has_sprite;
        view.state = kind->record->state;
        view.cost = kind->record->cost;
        view.has_cost = kind->record->has_cost;
    }
    return view;
}

const entity_class_t *structure_icon_kind_view_class(void) {
    return &structure_icon_kind_view_class_def;
}

void structure_icon_refresh(structure_icon_t *self) {
    const char *rr;

    if (self == NULL) {
        return;
    }

    if (self->has_is_revealed && self->is_revealed) {
        return;
    }

    rr = structure_icon_kind_view(self).required_tech;

    if (!tech_gate_is_unlocked(rr)) {
        return;
    }

    assert(g_structure_icon_up && "structure_icon_init() must run before structure_icon_refresh moves the shared cursor");

    if (g_has_cursor) {
        entity_set_pos(&self->header, g_cursor);
    } else {
        entity_clear_pos(&self->header);
    }
    self->is_revealed = 1;
    self->has_is_revealed = 1;

    structure_icon_set_cursor(vec_sub(structure_icon_cursor(), vec_make(p8_fixed_from_int(10), p8_fixed_from_int(0))));
}

int structure_icon_is_revealed(const entity_t *e) {
    if (e == NULL || e->is_dead || e->class != structure_icon_class()) {
        return 0;
    }
    const structure_icon_t *icon = (const structure_icon_t *)e;
    return icon->has_is_revealed && icon->is_revealed;
}

static void structure_icon_refresh_method(entity_t *self) {
    structure_icon_refresh((structure_icon_t *)self);
}

down_t structure_icon_left_press(structure_icon_t *self) {
    p8_fixed_t new_id;
    const entity_kind_t *entity_kind;
    char *id_num_str;
    planet_keys_t keys = {0};
    entity_t *held;
    down_t out = {0, ENTITY_ID_NONE};

    assert(g_structure_icon_up && "structure_icon_init() must run before structure_icon_left_press bumps the shared identifier counter");
    new_id = p8_fixed_add(g_has_id_counter ? g_id_counter : p8_fixed_from_int(0),
                             p8_fixed_from_int(1));
    g_id_counter = new_id;
    g_has_id_counter = 1;

    entity_kind = structure_icon_own_kind(self);
    if (entity_kind == NULL) {
        return out;
    }

    id_num_str = p8_tostr_fixed(frame_zone(), new_id);
    planet_keys_set_id(&keys, p8_concat2(frame_zone(), "s", id_num_str));
    grab_arena_open(planet_kind_construction_bytes(entity_kind));
    held = entity_kind->new_entity(&keys);
    grab_arena_close();
    out.held = entity_id_link(held);
    out.stop = held != NULL;
    return out;
}

static down_t structure_icon_left_press_method(entity_t *self, mouse_pos_t mouse) {
    (void)mouse;
    return structure_icon_left_press((structure_icon_t *)self);
}

void structure_icon_draw(p8_graphics_t *gfx, p8_vram_t *mem, structure_icon_t *self, vec_t p) {
    structure_icon_kind_t view;

    if (self == NULL || !(self->has_is_revealed && self->is_revealed)) {
        return;
    }
    if (gfx == NULL || mem == NULL) {
        return;
    }

    p8_palt(mem, 3, 0);

    view = structure_icon_kind_view(self);
    p8_spr_default(gfx, mem, view.has_sprite ? structure_icon_to_int(view.sprite) : 0, structure_icon_to_int(p.x),
                      structure_icon_to_int(p.y));

    if (self->header.is_hovered) {
        if (structure_icon_own_kind(self) != NULL) {
            targeting_t pb = {.archetype = &targeting_archetype_none};

            pb.has_cost = view.has_cost;
            pb.cost = view.cost;
            targeting_tooltip_show(gfx, mem, view.state, &pb);
        }
        if (self->tooltip != NULL) {
            tooltip_draw_at(gfx, mem, self->tooltip, p8_fixed_from_int(0), p8_fixed_from_int(0));
        }
    }

    p8_palt(mem, 3, 1);
}

static void structure_icon_draw_method(entity_t *self, vec_t screen_pos) {
    structure_icon_draw(dispatch_context_gfx(), dispatch_context_mem(), (structure_icon_t *)self, screen_pos);
}

static int structure_icon_hit_test_method(entity_t *self, mouse_pos_t mouse) {
    return entity_geom_contains_cursor(self, mouse);
}

static const entity_class_t structure_icon_class_def = {
    .draw = structure_icon_draw_method,
    .hit_test = structure_icon_hit_test_method,
    .refresh = structure_icon_refresh_method,
    .left_press = structure_icon_left_press_method,
};

static structure_icon_t *structure_icon_create(const structure_icon_keys_t *keys) {
    structure_icon_t *self = (structure_icon_t *)allocator_alloc(game_zone(), sizeof *self);

    assert(g_structure_icon_up && "structure_icon_init() must run before a structure icon is created");
    entity_init_instance(&self->header, &structure_icon_class_def, structure_icon_archetype.state, structure_icon_archetype.state_time);
    self->archetype = &structure_icon_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->header.has |= ENTITY_HAS_HIT_BOX;
    self->header.hit_box = self->archetype->hit_box;
    entity_set_pos(&self->header, self->archetype->pos);
    self->kind = NULL;
    self->tooltip = NULL;
    self->is_revealed = 0;
    self->has_is_revealed = 0;
    self->header.has |= ENTITY_HAS_IS_HOVERED;
    self->header.is_hovered = 0;
    if (keys != NULL) {
        self->kind = keys->kind;
        self->tooltip = keys->tooltip;
    }
    entity_construct(&self->header);
    return self;
}

void structure_icon_init(void) {
    structure_icon_shutdown();

    g_structure_icon_up = 1;
    structure_icon_clear_cursor();
    g_id_counter = p8_fixed_from_int(0);
    g_has_id_counter = 1;
}

void structure_icon_shutdown(void) {
    g_structure_icon_up = 0;
    structure_icon_clear_cursor();
}

int structure_icon_ready(void) {
    return g_structure_icon_up;
}

const entity_class_t *structure_icon_class(void) {
    return &structure_icon_class_def;
}

structure_icon_t *structure_icon_new(const structure_icon_keys_t *keys) {
    return structure_icon_create(keys);
}

const vec_t *structure_icon_rest_cursor(void) {
    return g_structure_icon_up ? &structure_icon_archetype.rest_cursor : NULL;
}

int structure_icon_has_cursor(void) {
    return g_has_cursor;
}

vec_t structure_icon_cursor(void) {
    return g_has_cursor ? g_cursor : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

void structure_icon_set_id_counter(p8_fixed_t id) {
    assert(g_structure_icon_up && "structure_icon_init() must run before structure_icon_set_id_counter, or the next init resets the counter");
    g_id_counter = id;
    g_has_id_counter = 1;
}

int structure_icon_id_counter(p8_fixed_t *out) {
    if (!g_has_id_counter) {
        return 0;
    }
    *out = g_id_counter;
    return 1;
}

void structure_icon_set_cursor(vec_t cursor) {
    g_cursor = cursor;
    g_has_cursor = 1;
}
