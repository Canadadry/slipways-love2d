#include "game/ui.h"

#include <assert.h>

#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/exit_queue.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "engine/entity_geom.h"
#include "game/mouse_button.h"
#include "game/planet_family.h"
#include "game/scroll.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"
#include "engine/game_zone.h"

static int g_ui_up = 0;
typedef enum { MAIN_MOUSE_NONE, MAIN_MOUSE_PLACEHOLDER, MAIN_MOUSE_REAL } main_mouse_state_t;

static main_mouse_state_t g_main_mouse_state = MAIN_MOUSE_NONE;
static entity_id_t g_main_mouse;
static entity_id_t g_main_ui;

const box_t *ui_panel_box(void) {
    return g_ui_up ? &ui_archetype.panel_box : NULL;
}

entity_id_list_t ui_ephemerals_new(void) {
    entity_id_list_t list;

    entity_id_list_init_reserve(&list, ui_zone_slot(SLOT_KIND_EPHEMERALS), UI_EPHEMERALS_RESERVE);
    return list;
}

void ui_set_ephemerals(ui_t *self, entity_id_list_t list) {
    const allocator_t *old_zone = self->ephemerals.zone;
    int owned_before = (self->has & UI_HAS_OWN_EPHEMERALS) != 0;

    ui_zone_record_ephemerals(list.count);
    self->ephemerals = list;
    self->has |= UI_HAS_OWN_EPHEMERALS;
    self->ephemerals_serial++;
    if (owned_before) {
        exit_queue_put_arena(old_zone);
    }
}

/* The screen point is the mouse's raw header pos, as the old &mouse->header.pos: entity_pos() would zero it when the position bit is absent. */
static mouse_pos_t ui_mouse_pos(const ui_t *self, const mouse_t *mouse) {
    mouse_pos_t out = {0};

    if (self->has & UI_HAS_MOUSE_WORLD) {
        out.world = self->mouse_world;
        out.has |= MOUSE_POS_HAS_WORLD;
    }
    if (mouse != NULL) {
        out.screen = mouse->header.pos;
        out.has |= MOUSE_POS_HAS_SCREEN;
    }
    return out;
}

static const entity_class_t main_mouse_placeholder_class_def = {0};

typedef struct {
    entity_t header;
} main_mouse_placeholder_t;

static main_mouse_placeholder_t g_main_mouse_placeholder = {{.class = &main_mouse_placeholder_class_def}};

static int ui_to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

static vec_t ui_pos_or_zero(const entity_t *e) {
    return e != NULL ? entity_pos(e) : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

static const mouse_button_t *ui_main_mouse_button(void) {
    const mouse_button_t *b;

    assert(main_mouse() != NULL && "ui_main_mouse_button: reading a button of an absent main mouse stops the run");
    if (main_mouse_typed() == NULL) {
        return NULL;
    }
    b = mouse_left_button();
    return b != NULL && b->has_held ? b : NULL;
}

static void ui_draw_overlay_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    ui_draw_overlay(dispatch_context_gfx(), dispatch_context_mem(), (const ui_t *)self);
}

static void ui_ephemerals_release(ui_t *self) {
    if (self->has & UI_HAS_OWN_EPHEMERALS) {
        arena_release(self->ephemerals.zone);
    }
    self->has &= (uint16_t)~UI_HAS_OWN_EPHEMERALS;
    self->ephemerals = (entity_id_list_t){0};
}

static void ui_release_method(entity_t *self) {
    ui_ephemerals_release((ui_t *)self);
}

static const entity_class_t ui_class_def = {
    .draw_overlay = ui_draw_overlay_method,
    .release = ui_release_method,
};

static ui_t *ui_create(void) {
    ui_t *self;

    assert(g_ui_up && "ui_init() must run first");
    self = (ui_t *)allocator_alloc(game_zone(), sizeof *self);
    entity_init_instance(&self->header, &ui_class_def, ui_archetype.state, ui_archetype.state_time);
    self->archetype = &ui_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->has = 0;
    self->ephemerals_serial = 0;
    self->ephemerals = (entity_id_list_t){0};
    self->mouse_world = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    self->hovered_by_layer.layer_count = UI_HOVER_LAYERS;
    self->hovered_by_layer.written = 0;
    self->hovered = ENTITY_ID_NONE;
    self->previous_hovered = ENTITY_ID_NONE;
    self->left_held = ENTITY_ID_NONE;
    self->pad_selection = ENTITY_ID_NONE;
    self->pad_selection_frames = 0;
    self->pad_tab = ENTITY_ID_NONE;
    self->pad_crosshair = 0;
    entity_construct(&self->header);
    return self;
}

void ui_init(void) {
    ui_shutdown();
    g_ui_up = 1;

    ui_set_main_mouse(&g_main_mouse_placeholder.header);
}

void ui_shutdown(void) {
    g_ui_up = 0;
    ui_set_main_mouse(NULL);
    g_main_ui = ENTITY_ID_NONE;
}

const entity_class_t *main_mouse_placeholder_class(void) {
    return &main_mouse_placeholder_class_def;
}

int ui_ready(void) {
    return g_ui_up;
}

const entity_class_t *ui_class(void) {
    return &ui_class_def;
}

ui_t *ui_new(void) {
    return ui_create();
}

entity_t *ui_hovered(const ui_t *self) {
    return entity_get(self->hovered);
}

entity_t *ui_previous_hovered(const ui_t *self) {
    return entity_get(self->previous_hovered);
}

entity_t *ui_left_held(const ui_t *self) {
    return entity_get(self->left_held);
}

entity_t *ui_pad_selection(const ui_t *self) {
    return entity_get(self->pad_selection);
}

void ui_set_pad_selection(ui_t *self, const entity_t *selection, int frames_shown) {
    assert(frames_shown >= 0 && "ui_set_pad_selection: frames_shown counts up from 0");
    self->pad_selection = entity_id_link(selection);
    self->pad_selection_frames = frames_shown;
}

entity_t *ui_pad_tab(const ui_t *self) {
    return entity_get(self->pad_tab);
}

void ui_set_pad_tab(ui_t *self, const entity_t *button) {
    self->pad_tab = entity_id_link(button);
}

void ui_set_pad_crosshair(ui_t *self, int shown) {
    self->pad_crosshair = shown;
}

entity_t *ui_pad_circle(const ui_t *self) {
    return self->pad_selection_frames % UI_PAD_BLINK_PERIOD < UI_PAD_BLINK_PERIOD / 2 ? ui_pad_selection(self) : NULL;
}

void ui_forget_world(ui_t *self) {
    if (self->has & UI_HAS_OWN_EPHEMERALS) {
        entity_id_list_clear(&self->ephemerals);
    }
    self->hovered_by_layer.written = 0;
}

const entity_id_list_t *ui_hovered_layer(const ui_t *self, int layer) {
    static const entity_id_list_t empty = {0};

    assert(layer >= 0 && layer < UI_HOVER_LAYERS);
    return (self->hovered_by_layer.written & (1u << layer)) ? &self->hovered_by_layer.layers[layer] : &empty;
}

entity_t *main_mouse(void) {
    switch (g_main_mouse_state) {
    case MAIN_MOUSE_PLACEHOLDER:
        return &g_main_mouse_placeholder.header;
    case MAIN_MOUSE_REAL:
        return entity_get(g_main_mouse);
    default:
        return NULL;
    }
}

mouse_t *main_mouse_typed(void) {
    entity_t *e = g_main_mouse_state == MAIN_MOUSE_REAL ? entity_get(g_main_mouse) : NULL;

    assert((e == NULL || e->class == mouse_class()) && "main_mouse_typed: only the placeholder or a mouse is ever stored");
    return (mouse_t *)e;
}

/* Never frees the outgoing instance: a click inside ui_update replaces it while `self` is still the old one. */
void ui_set_main_mouse(entity_t *instance) {
    if (instance == NULL) {
        g_main_mouse_state = MAIN_MOUSE_NONE;
        g_main_mouse = ENTITY_ID_NONE;
    } else if (instance == &g_main_mouse_placeholder.header) {
        g_main_mouse_state = MAIN_MOUSE_PLACEHOLDER;
        g_main_mouse = ENTITY_ID_NONE;
    } else {
        g_main_mouse_state = MAIN_MOUSE_REAL;
        g_main_mouse = entity_id_link(instance);
    }
}

ui_t *main_ui(void) {
    entity_t *e = entity_get(g_main_ui);

    assert((e == NULL || e->class == &ui_class_def) && "main_ui: only a ui is ever stored");
    return (ui_t *)e;
}

/* Never frees the outgoing ui, for the same reason as ui_set_main_mouse(). */
void ui_set_main_ui(ui_t *ui) {
    g_main_ui = entity_id_link(ui != NULL ? &ui->header : NULL);
}

void ui_update(p8_vram_t *mem, ui_t *self) {
    entity_id_list_t ephemerals_before;
    uint16_t serial_before;
    vec_t qa;
    int hit;
    const mouse_button_t *left;
    const mouse_t *je;
    const camera_t *mo;

    (void)mem;
    if (self == NULL) {
        return;
    }

    ephemerals_before = self->ephemerals;
    serial_before = self->ephemerals_serial;

    je = main_mouse_typed();
    mo = main_camera_typed();
    qa = vec_add(ui_pos_or_zero(je != NULL ? &je->header : NULL),
                       ui_pos_or_zero(mo != NULL ? &mo->header : NULL));
    self->mouse_world = qa;
    self->has |= UI_HAS_MOUSE_WORLD;

    ui_update_hover(self);

    hit = (ephemerals_before.count == 0) || entity_id_list_has(&ephemerals_before, self->hovered);
    if (hit) {
        ui_handle_button(self, MOUSE_SIDE_LEFT);
    }

    left = ui_main_mouse_button();
    if (left != NULL && left->pressed && self->ephemerals_serial == serial_before) {
        ui_clear_ephemerals(self);
    }

    self->previous_hovered = self->hovered;
}

static void ui_draw_pad_crosshair(const p8_graphics_t *gfx, p8_vram_t *mem) {
    int x = P8_SCREEN_WIDTH / 2;
    int y = P8_SCREEN_HEIGHT / 2;
    for (int d = 1; d <= UI_PAD_CROSSHAIR_ARM; d++) {
        p8_pset(gfx, mem, x - d, y, UI_PAD_CROSSHAIR_COLOUR);
        p8_pset(gfx, mem, x + d, y, UI_PAD_CROSSHAIR_COLOUR);
        p8_pset(gfx, mem, x, y - d, UI_PAD_CROSSHAIR_COLOUR);
        p8_pset(gfx, mem, x, y + d, UI_PAD_CROSSHAIR_COLOUR);
    }
}

void ui_draw_overlay(const p8_graphics_t *gfx, p8_vram_t *mem, const ui_t *self) {
    if (self == NULL || gfx == NULL || mem == NULL) {
        return;
    }
    if (self->pad_crosshair) {
        ui_draw_pad_crosshair(gfx, mem);
    }
}

static inline int ui_hit_test(entity_t *e, mouse_pos_t mouse) {
    int (*hit_test)(entity_t *, mouse_pos_t) = e->class->hit_test;

    if (hit_test == planet_hit_test_method) {
        return entity_geom_contains_cursor_inline(e, mouse);
    }
    return hit_test != NULL && hit_test(e, mouse);
}

void ui_update_hover(ui_t *self) {
    mouse_pos_t mouse;
    const entity_list_t *hit_test_bucket;
    entity_t *previous;
    entity_t *hovered = NULL;
    const allocator_t *zone = NULL;
    int index;

    if (self == NULL) {
        return;
    }

    self->hovered_by_layer.written = 0;
    self->has |= UI_HAS_HOVERED_BY_LAYER;
    self->hovered = ENTITY_ID_NONE;

    mouse = ui_mouse_pos(self, main_mouse_typed());

    hit_test_bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (index = 0; index < hit_test_bucket->count; index++) {
        entity_t *e = hit_test_bucket->items[index];

        if (ui_hit_test(e, mouse)) {
            if (entity_has(e, ENTITY_HAS_LAYER)) {
                int fa = ui_to_int(e->layer);

                assert(fa >= 0 && fa < UI_HOVER_LAYERS &&
                       "ui_update_hover: a hovered entity's layer outside 0..15, the layers ui_send_to_hovered walks -- stop, do not widen");
                if (!(self->hovered_by_layer.written & (1u << fa))) {
                    if (zone == NULL) {
                        zone = frame_zone();
                    }
                    entity_id_list_init(&self->hovered_by_layer.layers[fa], zone);
                    self->hovered_by_layer.written |= (uint16_t)(1u << fa);
                }
                entity_id_list_add(&self->hovered_by_layer.layers[fa], entity_id_link(e));
            }
            hovered = e;
            self->hovered = entity_id_link(e);
        }
    }

    /* Clear before set, so an entity hovered two frames in a row stays hovered. */
    previous = ui_previous_hovered(self);
    if (previous != NULL) {
        assert(entity_has(previous, ENTITY_HAS_IS_HOVERED) &&
               "ui_update_hover: the previously hovered entity has no is_hovered member -- stop, do not add one");
        previous->is_hovered = 0;
    }

    if (hovered != NULL) {
        assert(entity_has(hovered, ENTITY_HAS_IS_HOVERED) &&
               "ui_update_hover: the hovered entity has no is_hovered member -- stop, do not add one");
        hovered->is_hovered = 1;
    }
}

static int ui_is_held_class(const entity_class_t *cls) {
    return cls == slipway_class() || cls == probe_class() || cls == wormhole_class() ||
           (cls->states[ENTITY_STATE_SETTLED] != NULL && cls != protostar_class());
}

void ui_set_left_held(ui_t *self, const entity_t *held) {
    assert((held == NULL || ui_is_held_class(held->class)) &&
           "ui_set_left_held: only a class a left press hands over can be held on the left");
    self->left_held = entity_id_link(held);
}

void ui_handle_button(ui_t *self, mouse_side_t side) {
    const mouse_button_t *button;
    entity_t *held_inst;
    const mouse_t *mouse;

    if (self == NULL) {
        return;
    }

    (void)side;
    button = ui_main_mouse_button();
    mouse = main_mouse_typed();

    if (button != NULL && button->pressed) {
        entity_t *kv = NULL;
        down_t kn;
        const entity_t *given;

        if (ui_send_to_hovered(self, SEND_TO_HOVERED_LEFT_PRESS, ui_mouse_pos(self, mouse), NULL, &kv, &kn) &&
            (given = entity_get(kn.held)) != NULL) {
            assert(ui_is_held_class(given->class) &&
                   "ui_handle_button: a left press hands over a slipway; a probe or a wormhole from the background; a "
                   "structure, processor, slipgate, infraspace_gate, trading_hub, synthesizer or ascension from an icon");
            self->left_held = kn.held;
        }
    }

    held_inst = ui_left_held(self);

    if (held_inst != NULL && held_inst->class->drag != NULL) {
        held_inst->class->drag(held_inst, ui_mouse_pos(self, mouse));
    }

    if (button != NULL && button->released && held_inst != NULL) {
        entity_t *target = NULL;
        down_t down;

        (void)ui_send_to_hovered(self, SEND_TO_HOVERED_ACCEPT_DROP, (mouse_pos_t){0}, held_inst, &target, &down);

        if (held_inst->class->drop != NULL) {
            held_inst->class->drop(held_inst, target);
        }
        self->left_held = ENTITY_ID_NONE;
    }
}

/* Layers descend from 15 to 0 and the first `stop` wins: that order is the dispatch priority. */
int ui_send_to_hovered(ui_t *self, send_to_hovered_slot_t message, mouse_pos_t mouse, const entity_t *held,
                entity_t **out_target, down_t *out_down) {
    int layer;

    if (self == NULL) {
        return 0;
    }

    if (!(self->has & UI_HAS_HOVERED_BY_LAYER)) {
        return 0;
    }

    for (layer = UI_HOVER_LAYERS - 1; layer >= 0; layer--) {
        entity_id_list_iter_t it = entity_id_list_all(ui_hovered_layer(self, layer));
        entity_t *h;

        while (entity_id_list_iter_next(&it, &h)) {
            down_t kn = {0, ENTITY_ID_NONE};

            switch (message) {
            case SEND_TO_HOVERED_LEFT_PRESS:
                if (h->class->left_press != NULL) {
                    kn = h->class->left_press(h, mouse);
                }
                break;
            case SEND_TO_HOVERED_ACCEPT_DROP:
                if (h->class->accept_drop != NULL) {
                    kn = h->class->accept_drop(h, held);
                }
                break;
            }
            if (kn.stop) {
                *out_target = h;
                *out_down = kn;
                return 1;
            }
        }
    }

    return 0;
}

void ui_clear_ephemerals(ui_t *self) {
    if (self == NULL) {
        return;
    }

    {
        entity_id_list_iter_t it = entity_id_list_all(&self->ephemerals);
        entity_t *e;
        while (entity_id_list_iter_next(&it, &e)) {
            entity_unregister(e);
        }
    }

    ui_set_ephemerals(self, ui_ephemerals_new());
}
