#include "game/planet_display.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "engine/collect.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "engine/entity_geom.h"
#include "game/main_economy.h"
#include "game/game_data.h"
#include "game/hud.h"
#include "game/image_slot.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "game/map_tables.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/row_image.h"
#include "game/slipway.h"
#include "p8/str.h"
#include "game/tech_gate.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"
#include "game/visual_random.h"

static layout_row_cell_t planet_production_cell(const production_t *kw) {
    layout_row_tuple_t tuple = {0};

    if (kw->has_resource) {
        tuple.count = 2;
        tuple.scalars[0] = layout_row_scalar_fixed(kw->resource);
        tuple.scalars[1] = layout_row_scalar_fixed(p8_fixed_from_int(5));
    }
    return layout_row_cell_tuple(tuple);
}

enum { ROW_SIGNATURE_VALID = 1, ROW_SIGNATURE_HAS_OUTPUT = 2 };
enum { ROW_NEED_HAS_RESOURCE = 1, ROW_NEED_SATISFIED_ZERO = 2 };

static planet_row_signature_t planet_row_signature(const pointer_list_t *needs_list, int has_output, p8_fixed_t output,
                                                   p8_fixed_t amount, int route_count) {
    planet_row_signature_t signature;
    pointer_list_iter_t it;
    void *entry;

    assert(route_count >= 0 && route_count <= UINT16_MAX && "planet_row_signature: a wider route count would alias in the signature");
    memset(&signature, 0, sizeof signature);
    signature.flags = has_output ? ROW_SIGNATURE_HAS_OUTPUT : 0;
    signature.output = output;
    signature.amount = amount;
    signature.route_count = (uint16_t)route_count;
    if (needs_list != NULL && needs_list->count > PLANET_ROW_SIGNATURE_NEEDS) {
        return signature;
    }
    if (needs_list != NULL) {
        it = pointer_list_all(needs_list);
        while (pointer_list_iter_next(&it, &entry)) {
            if (entry != NULL) {
                const need_t *need = entry;
                resource_t resource = need_resource(need);

                if (resource.kind == RESOURCE_FIXED) {
                    signature.need_flags[signature.needs_count] |= ROW_NEED_HAS_RESOURCE;
                    signature.need_resources[signature.needs_count] = resource.number;
                }
                if (need->has_satisfied && need->satisfied == 0) {
                    signature.need_flags[signature.needs_count] |= ROW_NEED_SATISFIED_ZERO;
                }
            }
            signature.needs_count++;
        }
    }
    signature.flags |= ROW_SIGNATURE_VALID;
    return signature;
}

static int planet_row_signature_equal(const planet_row_signature_t *left, const planet_row_signature_t *right) {
    int i;

    if (left->output != right->output || left->amount != right->amount || left->route_count != right->route_count ||
        left->needs_count != right->needs_count || left->flags != right->flags) {
        return 0;
    }
    for (i = 0; i < left->needs_count; i++) {
        if (left->need_flags[i] != right->need_flags[i] || left->need_resources[i] != right->need_resources[i]) {
            return 0;
        }
    }
    return 1;
}

void planet_refresh(planet_t *self) {
    const pointer_list_t *needs_list;
    const trade_route_list_t *by_list;
    int has_output;
    p8_fixed_t output;
    p8_fixed_t mu;
    layout_row_keys_t keys;
    layout_row_t *layout_row_result = NULL;
    const allocator_t *slot;
    frame_zone_mark_t scratch;
    int route_count;
    planet_row_signature_t signature;

    if (self == NULL) {
        return;
    }

    needs_list = (self->has & PLANET_HAS_NEEDS) ? &self->needs : NULL;
    {
        const production_t *kw = planet_production_read(self);
        has_output = kw->has_resource;
        output = kw->resource;
        mu = kw->amount;
    }
    by_list = (self->has & PLANET_HAS_OUTGOING_ROUTES) ? &self->outgoing_routes : NULL;
    route_count = by_list != NULL ? trade_route_list_count(by_list) : 0;
    signature = planet_row_signature(needs_list, has_output, output, mu, route_count);
    if ((signature.flags & ROW_SIGNATURE_VALID) && planet_row_signature_equal(&signature, &self->layout_row_signature)) {
        return;
    }

    scratch = frame_zone_mark();
    slot = ui_zone_slot(SLOT_KIND_ROW);
    if (research_display_planet_resource_row_keys(needs_list, has_output, output, mu, p8_fixed_from_int(route_count),
                                                  p8_fixed_from_int(0), &keys)) {
        layout_row_result = layout_row_new(slot, &keys);
    } else {
        arena_release(slot);
        slot = NULL;
    }
    if (self->layout_row_slot != NULL) {
        arena_release(self->layout_row_slot);
    }
    self->layout_row = layout_row_result;
    self->layout_row_slot = slot;
    self->layout_row_signature = signature;
    frame_zone_rewind(scratch);
}

void planet_release_row(entity_t *self) {
    planet_t *jo = (planet_t *)self;

    if (jo->layout_row_slot != NULL) {
        arena_release(jo->layout_row_slot);
    }
    jo->layout_row = NULL;
    jo->layout_row_slot = NULL;
    jo->layout_row_signature.flags = 0;
}

void planet_release_method(entity_t *self) {
    planet_release_row(self);
    planet_grab_arena_released((const planet_t *)self);
}

typedef struct {
    pointer_list_t received_needs;
} planet_inspection_panel_ctx_t;

static int planet_inspection_panel_filter_step(void *e, void *ctx_ptr) {
    planet_inspection_panel_ctx_t *ctx = (planet_inspection_panel_ctx_t *)ctx_ptr;
    p8_fixed_t gc = p8_fixed_from_int(0);
    int count;
    int j;

    if (e != NULL) {
        const need_t *i = e;
        if (i->has_satisfied) {
            gc = i->satisfied;
        }
    }
    count = p8_fixed_flr_int(gc);
    for (j = 0; j < count; j++) {
        pointer_list_add(&ctx->received_needs, e);
    }
    return gc == p8_fixed_from_int(0);
}

#define PLANET_INSPECTION_PANEL_ROWS 4

static char *planet_dollar(const char *s) {
    return p8_concat2(frame_zone(), s, "$");
}

static open_box_frame_t planet_open_box_frame(const planet_t *self) {
    open_box_frame_t frame = {0};

    if (self->description != NULL) {
        open_box_frame_set_description(&frame, self->description);
    }
    frame.has_back_color = 1;
    frame.back_color = self->archetype->back_color;
    frame.has_row_step = 1;
    frame.row_step = self->archetype->row_step;
    frame.has_pos = entity_has_pos(&self->header);
    frame.pos = self->header.pos;
    return frame;
}

void planet_inspection_panel(const planet_t *self) {
    button_keys_t buttons[PLANET_INSPECTION_PANEL_ROWS];
    int row_count = 0;
    const data_row_t *icon_row = NULL;
    planet_inspection_panel_ctx_t filter_context;
    const pointer_list_t *needs;
    pointer_list_t unmet_needs;
    open_box_frame_t frame;
    int index;

    if (self == NULL) {
        return;
    }

    memset(buttons, 0, sizeof buttons);
    if ((self->header.class->lineage & LINEAGE(LINEAGE_STRUCTURE)) && self->archetype->name != NULL) {
        icon_row = self->archetype->name;
    } else if (self->has_development_tier) {
        int mi = p8_fixed_flr_int(self->development_tier);

        if (mi >= 1 && mi <= PLANET_MOOD_ROWS_COUNT) {
            icon_row = &planet_mood_rows()[mi - 1];
        }
    }
    if (icon_row != NULL) {
        buttons[0].layout_row_keys = layout_row_keys_of_row(icon_row);
    }
    buttons[0].has_side_tooltip = 1;
    if (self->has_net_income) {
        int discard_color = 0;
        layout_row_keys_t *h_keys = &buttons[0].side_tooltip_row;

        layout_row_keys_append(h_keys, layout_row_cell_string(planet_dollar(collect_format_signed(frame_zone(), self->net_income, 0, NULL, &discard_color))));
        h_keys->has_background = 1;
        h_keys->background_is_bool = self->net_income < p8_fixed_from_int(0);
        h_keys->background = self->net_income >= p8_fixed_from_int(0) ? p8_fixed_from_int(1) : 0;
        buttons[0].has_side_tooltip_row = 1;
    }
    row_count = 1;

    pointer_list_init(&filter_context.received_needs, frame_zone());

    needs = (self->has & PLANET_HAS_NEEDS) ? &self->needs : NULL;
    unmet_needs = collect_filter_pointer_list(frame_zone(), needs, planet_inspection_panel_filter_step, &filter_context);

    if (unmet_needs.count > 0) {
        layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string("needs:"));
        for (index = 0; index < unmet_needs.count; index++) {
            layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_row(unmet_needs.items[index]));
        }
        row_count++;
    } else {
        p8_fixed_t development_tier_fixed = self->has_development_tier ? self->development_tier : p8_fixed_from_int(0);
        const production_t *kw = planet_production_read(self);
        int r_w;
        int resource_is_r_w;

        resource_is_r_w = kw->has_resource && resource_code_lookup("w", &r_w) && kw->resource == p8_fixed_from_int(r_w);

        if (development_tier_fixed < p8_fixed_from_int(4) && needs != NULL && needs->count > 0 && !resource_is_r_w) {
            int last_only = (self->header.class->lineage & LINEAGE(LINEAGE_STRUCTURE)) && self->archetype->is_structure;

            layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string("wants more:"));
            if (last_only) {
                layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_row(needs->items[needs->count - 1]));
            } else {
                for (index = 0; index < needs->count; index++) {
                    layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_row(needs->items[index]));
                }
            }
            row_count++;
        }
    }

    if (filter_context.received_needs.count >= 1) {
        layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string("receives:"));
        for (index = 0; index < filter_context.received_needs.count; index++) {
            layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_row(filter_context.received_needs.items[index]));
        }
        row_count++;
    }

    {
        const production_t *kw = planet_production_read(self);
        p8_fixed_t mu = kw->amount;

        if (mu > p8_fixed_from_int(0)) {
            int makes = kw->has_resource && int_seq_has_fixed(planet_made_resources(), kw->resource);

            if (makes) {
                char *amount_str = p8_tostr_fixed(frame_zone(), mu);
                layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string("makes:"));
                layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string(amount_str));
            } else {
                const trade_route_list_t *by = (self->has & PLANET_HAS_OUTGOING_ROUTES) ? &self->outgoing_routes : NULL;
                int by_count = (by != NULL) ? trade_route_list_count(by) : 0;
                char *count_str = p8_tostr_fixed(frame_zone(), p8_fixed_from_int(by_count));
                char *amount_str = p8_tostr_fixed(frame_zone(), mu);
                char *combined = p8_concat2(frame_zone(), p8_concat2(frame_zone(), count_str, "/"), amount_str);
                layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string("exports:"));
                layout_row_keys_append(&buttons[row_count].layout_row_keys, layout_row_cell_string(combined));
            }
            layout_row_keys_append(&buttons[row_count].layout_row_keys, planet_production_cell(kw));
            row_count++;
        }
    }

    /* Starts at 1: row 0's layout row is a single icon, not a list. */
    for (index = 1; index < row_count; index++) {
        static const data_item_t gap_items[] = {{.is_false = 1}, {.number = 1}};
        static const data_row_t gap = {.count = 2, .items = gap_items};

        layout_row_keys_append(&buttons[index].layout_row_keys, layout_row_cell_row(&gap));
    }

    frame = planet_open_box_frame(self);
    message_box_open_box(&frame, buttons, row_count);
}

down_t planet_accept_drop(const planet_t *self, const entity_t *s) {
    down_t out = {0, ENTITY_ID_NONE};

    if (self == NULL) {
        return out;
    }
    if (self->header.state != ENTITY_STATE_SETTLED) {
        return out;
    }
    if (s != NULL && s->class == slipway_class() && slipway_start((const slipway_t *)s) == &self->header) {
        planet_inspection_panel(self);
        return out;
    }
    out.stop = 1;
    return out;
}

static void planet_projects_panel_on_click_method(button_t *self) {
    planet_t *pt;
    research_project_t *b;
    p8_fixed_t od = p8_fixed_from_int(0);

    if (self == NULL) {
        return;
    }
    pt = (planet_t *)entity_get(self->project_planet);
    b = research_project_get(self->project);
    if (self->has_project_cost) {
        od = self->project_cost;
    }

    assert(pt != NULL && planet_is_family(&pt->header) && "the projects panel button lost its captured planet");
    if (economy_spend(main_economy_typed(), od)) {
        planet_apply_project(pt, b);
    }
}

static void planet_projects_panel_keys(planet_t *self, research_project_t *b, button_keys_t *keys) {
    p8_fixed_t od = research_project_cost(b);

    memset(keys, 0, sizeof *keys);
    if (b->has_icon_sprite) {
        keys->layout_row_keys = research_project_icon_row_keys(b);
    } else {
        (void)research_project_layout_row_keys(b, &keys->layout_row_keys);
    }

    keys->has_side_tooltip = 1;
    keys->side_tooltip_is_false = od <= p8_fixed_from_int(0);
    if (od > p8_fixed_from_int(0)) {
        layout_row_keys_append(&keys->side_tooltip_row, layout_row_cell_string(planet_dollar(p8_tostr_fixed(frame_zone(), od))));
        keys->side_tooltip_row.has_color = 1;
        keys->side_tooltip_row.color = p8_fixed_from_int(13);
        keys->has_side_tooltip_row = 1;
    }

    keys->project_planet = entity_id_link(&self->header);
    keys->project = research_project_id_of(b);
    keys->has_project_cost = 1;
    keys->project_cost = od;
    keys->has_on_click = 1;
    keys->on_click = planet_projects_panel_on_click_method;
    keys->has_tooltip = 1;
    button_keys_set_tooltip_row(keys, research_project_tooltip(b));
}

void planet_projects_panel(planet_t *self) {
    available_projects_kept_t available_projects_list;
    button_keys_t *bs;
    open_box_frame_t frame;
    size_t cap;
    int n = 0;
    int i;

    if (self == NULL) {
        return;
    }

    available_projects_list = research_display_available_projects((const entity_t *)self);
    cap = available_projects_list.count > 0 ? (size_t)available_projects_list.count : 1;
    bs = (button_keys_t *)allocator_alloc(frame_zone(), cap * sizeof *bs);
    memset(bs, 0, cap * sizeof *bs);
    for (i = 0; i < available_projects_list.count; i++) {
        planet_projects_panel_keys(self, research_project_get(available_projects_list.items[i]), &bs[n]);
        n++;
    }

    frame = planet_open_box_frame(self);
    message_box_open_box(&frame, bs, n);
}

static void planet_draw_on_screen(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos);

#define PLANET_DISC_RADIUS 7
#define PLANET_RING_RADIUS 6

static int planet_images_built;

static void planet_draw_disc_image(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_circfill(gfx, mem, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, PLANET_DISC_RADIUS, 0);
}

static void planet_draw_ring_image(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    p8_circ(gfx, mem, PLANET_RING_RADIUS, PLANET_RING_RADIUS, PLANET_RING_RADIUS, *(const int *)context);
}

/* Draws no visual random number and reads no game state: the image cache's rule. */
static void planet_build_images(p8_graphics_t *gfx, p8_vram_t *mem) {
    const int_seq_t *ring_colors = planet_ring_colors();
    int index;

    assert(gfx->fillp_pattern == 0 && "planet images are built with no fill pattern");
    if (planet_images_built) {
        return;
    }
    p8_image_build(gfx, mem, IMAGE_SLOT_PLANET_DISC, 2 * PLANET_DISC_RADIUS + 1, 2 * PLANET_DISC_RADIUS + 1,
                   planet_draw_disc_image, NULL);
    assert(ring_colors->count == PLANET_RING_COLORS_COUNT);
    for (index = 0; index < PLANET_RING_COLORS_COUNT; index++) {
        p8_image_build(gfx, mem, IMAGE_SLOT_PLANET_RING_FIRST + index, 2 * PLANET_RING_RADIUS + 1,
                       2 * PLANET_RING_RADIUS + 1, planet_draw_ring_image, (void *)&ring_colors->items[index]);
    }
    planet_images_built = 1;
}

void planet_draw(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos) {
    if (self == NULL || !entity_geom_is_on_screen((const entity_t *)self)) {
        return;
    }
    planet_draw_on_screen(graphics, memory, self, screen_pos);
}

static void planet_draw_on_screen(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos) {
    vec_t shaken_pos;
    int screen_x;
    int screen_y;
    int spr_index = 43;

    /* Everything below draws at shaken_pos, never at screen_pos. */
    shaken_pos = entity_geom_shake(screen_pos, (entity_t *)self);

    if (self->header.state == ENTITY_STATE_VANISHING) {
        p8_fixed_t t = self->header.state_time;
        if (visual_random_draw(p8_fixed_from_int(30)) < p8_fixed_sub(t, p8_fixed_from_int(30))) {
            return;
        }
    }

    screen_x = p8_fixed_flr_int(shaken_pos.x);
    screen_y = p8_fixed_flr_int(shaken_pos.y);

    if (graphics->fillp_pattern == 0) {
        planet_build_images(graphics, memory);
        p8_image_draw(graphics, memory, IMAGE_SLOT_PLANET_DISC, screen_x - PLANET_DISC_RADIUS, screen_y - PLANET_DISC_RADIUS);
    } else {
        p8_circfill(graphics, memory, screen_x, screen_y, PLANET_DISC_RADIUS, 0);
    }

    hud_select_palette_block(memory, (self->header.class->lineage & LINEAGE(LINEAGE_TRADING_HUB)) ? self->palette_block : p8_fixed_from_int(0));
    p8_palt(memory, 3, 0);
    p8_palt(memory, 0, 1);

    if (self->planet_type != NULL) {
        spr_index = self->planet_type->sprite;
    } else if (self->header.class->lineage & LINEAGE(LINEAGE_STRUCTURE)) {
        spr_index = p8_fixed_flr_int(self->sprite);
    }
    p8_spr(graphics, memory, spr_index, screen_x - 4, screen_y - 4, 2, 2);
    hud_select_palette_block_default(memory);

    if (planet_is_colonized(self)) {
        int mi = p8_fixed_flr_int(self->has_development_tier ? self->development_tier : p8_fixed_from_int(0));
        int color = 0;
        if (int_seq_at(planet_ring_colors()->items, planet_ring_colors()->count, mi, &color) && graphics->fillp_pattern == 0) {
            planet_build_images(graphics, memory);
            p8_image_draw(graphics, memory, IMAGE_SLOT_PLANET_RING_FIRST + mi - 1, screen_x - PLANET_RING_RADIUS,
                          screen_y - PLANET_RING_RADIUS);
        } else {
            p8_circ(graphics, memory, screen_x, screen_y, PLANET_RING_RADIUS, color);
        }
        p8_spr_default(graphics, memory, mi + 1, screen_x - 3, screen_y - 6);
    }

    if (main_ui() != NULL && ui_pad_circle(main_ui()) == &self->header) {
        p8_circ(graphics, memory, screen_x, screen_y, 9, 7);
    }

    {
        if (self->layout_row != NULL) {
            vec_t offset =
                vec_add(shaken_pos, vec_make(p8_fixed_from_int(0), p8_fixed_from_int(7)));
            row_image_draw_at(graphics, memory, self->layout_row, offset, P8_FIXED_LIT(0x00008000, 0.5));
        }
    }

    {
        entity_t *targeted_by = entity_get(self->targeted_by);

        if (targeted_by != NULL) {
            p8_fixed_t dw = p8_fixed_from_int(0);
            p8_fixed_t radius_f;
            int radius;

            if (targeting_of(targeted_by) != NULL) {
                dw = targeting_of(targeted_by)->archetype->attach_distance;
            }
            radius_f = p8_fixed_mul(p8_fixed_sqrt(dw), p8_fixed_from_int(128));
            radius = p8_fixed_flr_int(radius_f);

            /* The cart's fractional .1 pattern is transparent: p8_fillp_transparent, not p8_fillp. */
            p8_fillp_transparent(graphics, 0xAAAAu);
            p8_circfill(graphics, memory, screen_x, screen_y, radius, 2);
            p8_fillp_reset(graphics);
            self->targeted_by = ENTITY_ID_NONE;
        }
    }
}

void planet_refresh_method(entity_t *self) {
    planet_refresh((planet_t *)self);
}

down_t planet_accept_drop_method(entity_t *self, const entity_t *s) {
    return planet_accept_drop((const planet_t *)self, s);
}

void planet_draw_method(entity_t *self, vec_t screen_pos) {
    assert(self->class->skip_draw_off_screen && entity_geom_is_on_screen(self) &&
           "planet_draw_method trusts draw_layers' cull: only a class with skip_draw_off_screen may use it");
    planet_draw_on_screen(dispatch_context_gfx(), dispatch_context_mem(), (planet_t *)self, screen_pos);
}

down_t planet_left_press_method(entity_t *self, mouse_pos_t mouse) {
    (void)mouse;
    return planet_left_press((planet_t *)self);
}

down_t planet_left_press(planet_t *self) {
    down_t out = {0, ENTITY_ID_NONE};

    if (self == NULL) {
        return out;
    }
    if (self->header.state == ENTITY_STATE_KNOWN) {
        planet_projects_panel(self);
        out.stop = 1;
        return out;
    }
    if (self->header.state == ENTITY_STATE_SETTLED) {
        slipway_keys_t keys = {0};
        slipway_t *made;

        assert(slipway_ready() && "slipway_init() must run before planet_left_press builds a slipway");
        keys.start = entity_id_link(&self->header);
        made = slipway_new(&keys);
        out.held = entity_id_link(made != NULL ? &made->header : NULL);
        out.stop = made != NULL;
    }
    return out;
}

void planet_apply_project(planet_t *self, research_project_t *research_project_instance) {
    planet_t *result_planet;
    p8_fixed_t fp = p8_fixed_from_int(2);

    research_project_assert(research_project_instance);
    if (self == NULL) {
        return;
    }
    result_planet = self;

    if (research_project_instance != NULL && research_project_instance->class_name != NULL) {
        const entity_kind_t *target_kind = NULL;
        planet_keys_t keys = {0};

        assert(self->slipways.count == 0 &&
               "a linked planet never dies: the routes kept in outgoing_routes would name it, and the cart never drops them");
        self->header.is_dead = 1;

        target_kind = planet_kind_named(research_project_instance->class_name);

        keys.map_pos = self->header.map_pos;
        keys.has_map_pos = entity_has_map_pos(&self->header);
        keys.pos = self->header.pos;
        keys.has_pos = entity_has_pos(&self->header);
        if (self->has & PLANET_HAS_ID) {
            planet_keys_set_id(&keys, self->planet_id);
        }

        result_planet = (target_kind != NULL) ? (planet_t *)target_kind->new_entity(&keys) : NULL;
    } else if (research_project_instance != NULL && research_project_instance->next_type != NULL) {
        self->planet_type = planet_types_lookup(research_project_instance->next_type);
    } else {
        entity_set_state((entity_t *)self, ENTITY_STATE_SETTLED);
        planet_install_project(self, research_project_instance);
        self->header.fold_anchor = p8_fixed_from_int(1);
        fp = tech_gate_if_unlocked("autoassemblers", p8_fixed_from_int(3), p8_fixed_from_int(2));
    }

    economy_advance_clock(main_economy_typed(), fp);

    planet_restart_shake(result_planet);
}
