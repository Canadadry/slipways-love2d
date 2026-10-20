#include "game/menu.h"

#include <assert.h>
#include <string.h>

#include "game/main_economy.h"
#include "engine/entity_class.h"
#include "engine/entity_registry.h"
#include "game/economy.h"
#include "p8/fixed.h"
#include "game/game_data.h"
#include "game/layout.h"
#include "game/message_box.h"
#include "game/score_panel.h"
#include "game/score_panel_display.h"
#include "game/tech_gate.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "p8/str.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"

static int g_menu_up = 0;
static vec_t g_galaxy_menu_pos[GALAXY_MENU_COUNT];

static void galaxy_menu_button4_on_click_method(button_t *self);

typedef struct {
    entity_t header;
    uint16_t has;
    vec_t fixed_pos;
    vec_t row_step;
    p8_fixed_t back_color;
    int has_back_color;
} pause_menu_open_box_t;

#define PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS (1u << 0)
#define PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP (1u << 1)

static const entity_class_t pause_menu_open_box_class_def = {0};

static pause_menu_open_box_t g_pause_menu_open_box = {
    .header = {.class = &pause_menu_open_box_class_def},
};

static void tech_tree_screen_row_on_click_method(button_t *self) {
    if (self == NULL) {
        return;
    }
    if (self->technology != NULL) {
        economy_invent(main_economy_typed(), self->technology);
    }
}

static void tech_tree_screen_step(const technologies_entry_t *t, button_keys_t *b) {
    static const data_item_t pair_items[] = {{.number = 24}, {.number = 5}};
    static const data_row_t pair = {.count = 2, .items = pair_items};
    p8_fixed_t research_cost_result;
    char *research_cost_str;

    layout_row_keys_append(&b->layout_row_keys, layout_row_cell_fixed(p8_fixed_from_int(t->icon)));
    layout_row_keys_append(&b->layout_row_keys, layout_row_cell_string(t->name));
    b->layout_row_keys.has_color = 1;
    b->layout_row_keys.color = p8_fixed_from_int(6);

    research_cost_result = tech_gate_research_cost(t);
    research_cost_str = p8_tostr_fixed(frame_zone(), research_cost_result);

    layout_row_keys_append(&b->side_tooltip_row, layout_row_cell_row(&pair));
    layout_row_keys_append(&b->side_tooltip_row, layout_row_cell_string(research_cost_str));
    b->has_side_tooltip = 1;
    b->has_side_tooltip_row = 1;

    b->has_tooltip = 1;
    button_keys_set_tooltip_row(b, t->tooltip_rows);

    /* An unaffordable tech gets on_click present but NULL, not absent. */
    b->has_on_click = 1;
    if (tech_gate_affordable_flags(t)) {
        b->technology = t;
        b->on_click = tech_tree_screen_row_on_click_method;
    }
}

static const data_row_t *invented_lines(const pointer_list_t *list) {
    int n = list != NULL ? list->count : 0;
    size_t size = sizeof(data_row_t) + (size_t)n * sizeof(data_item_t);
    data_row_t *row = (data_row_t *)allocator_alloc(frame_zone(), size);
    data_item_t *items = (data_item_t *)(row + 1);
    int i;

    memset(row, 0, size);
    for (i = 0; i < n; i++) {
        items[i].text = list->items[i];
    }
    row->count = n;
    row->items = items;
    return row;
}

void menu_show_tech_tree_screen(void) {
    const economy_t *economy = main_economy_typed();
    p8_fixed_t current_era = p8_fixed_from_int(0);
    int invented_count = 0;
    p8_fixed_t next_threshold = p8_fixed_from_int(0);
    int tier_thresholds;
    available_technologies_kept_t available_technologies_list;
    button_keys_t *buttons;
    int row_count = 0;
    int index;
    score_panel_layout_block_t *layout_block;
    open_box_frame_t frame = {0};

    if (economy != NULL) {
        current_era = economy->era;
    }
    if (economy != NULL) {
        invented_count = economy->invented.count;
    }

    if (int_seq_at(tier_thresholds_table, TIER_THRESHOLDS_COUNT, p8_fixed_flr_int(current_era) - 1, &tier_thresholds)) {
        next_threshold = p8_fixed_from_int(tier_thresholds);
    }

    /* 1 lists every tech of the era, affordable or not; the galaxy row's refresh must pass 0. */
    available_technologies_list = tech_gate_available_technologies(1);
    buttons = (button_keys_t *)allocator_alloc(frame_zone(), ((size_t)available_technologies_list.count + 1) * sizeof *buttons);
    memset(buttons, 0, ((size_t)available_technologies_list.count + 1) * sizeof *buttons);
    for (index = 0; index < available_technologies_list.count; index++) {
        tech_tree_screen_step(available_technologies_list.items[index], &buttons[row_count++]);
    }

    if (invented_count > 0) {
        button_keys_t *extra = &buttons[row_count++];
        char *invented_text = p8_tostr_fixed(frame_zone(), p8_fixed_from_int(invented_count));
        char *label = p8_concat2(frame_zone(), invented_text, " invented");
        layout_row_keys_append(&extra->layout_row_keys, layout_row_cell_fixed(p8_fixed_from_int(30)));
        layout_row_keys_append(&extra->layout_row_keys, layout_row_cell_string(label));
        extra->layout_row_keys.has_color = 1;
        extra->layout_row_keys.color = p8_fixed_from_int(3);
        extra->has_tooltip = 1;
        button_keys_set_tooltip_row(extra, invented_lines(&economy->invented));
    }

    layout_block = score_panel_layout_block();
    if (layout_block != NULL) {
        p8_fixed_t tier_thresholds_era = p8_fixed_from_int(0);
        char *era_str;
        char *diff1_str;
        char *diff2_str;
        char *tmp1;
        char *tmp2;
        char *tmp3;
        char *ep;

        if (int_seq_at(tier_thresholds_table, TIER_THRESHOLDS_COUNT, p8_fixed_flr_int(current_era), &tier_thresholds)) {
            tier_thresholds_era = p8_fixed_from_int(tier_thresholds);
        }

        era_str = p8_tostr_fixed(frame_zone(), current_era);
        diff1_str = p8_tostr_fixed(frame_zone(), p8_fixed_sub(p8_fixed_from_int(invented_count), next_threshold));
        diff2_str = p8_tostr_fixed(frame_zone(), p8_fixed_sub(tier_thresholds_era, next_threshold));

        tmp1 = p8_concat2(frame_zone(), "level ", era_str);
        tmp2 = p8_concat2(frame_zone(), tmp1, " (");
        tmp3 = p8_concat2(frame_zone(), tmp2, diff1_str);
        tmp1 = p8_concat2(frame_zone(), tmp3, "/");
        tmp2 = p8_concat2(frame_zone(), tmp1, diff2_str);
        ep = p8_concat2(frame_zone(), tmp2, ")");

        open_box_frame_set_description(&frame, ep);
        if (layout_block->has & SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS) {
            frame.has_fixed_pos = 1;
            frame.fixed_pos = layout_block->fixed_pos;
        }
        frame.has_back_color = 1;
        frame.back_color = layout_block->back_color;
        if (layout_block->has & SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP) {
            frame.has_row_step = 1;
            frame.row_step = layout_block->row_step;
        }
        message_box_open_box(&frame, buttons, row_count);
    }
}

static void galaxy_menu_button1_on_click_method(button_t *self) {
    economy_t *fm = main_economy_typed();
    (void)self;
    if (fm != NULL) {
        fm->year_ticks = p8_fixed_from_int(1);
    }
    economy_new_year(fm);
}

static void galaxy_menu_button2_on_click_method(button_t *self) {
    (void)self;
    menu_show_tech_tree_screen();
}

static void galaxy_menu_button2_refresh_method(button_t *self) {
    available_technologies_kept_t available_technologies_list;
    int available_technologies_count;
    layout_row_keys_t keys = {0};
    const allocator_t *slot;

    if (self == NULL) {
        return;
    }

    /* Passing 1 here would light the icon whenever any tech of the era is left. */
    available_technologies_list = tech_gate_available_technologies(0);
    available_technologies_count = available_technologies_list.count;

    layout_row_keys_append(&keys, layout_row_cell_fixed(p8_fixed_from_int(available_technologies_count > 0 ? 24 : 37)));
    slot = ui_zone_slot(SLOT_KIND_ROW);
    self->layout_row = layout_row_new(slot, &keys);
    if (self->layout_row_slot != NULL) {
        arena_release(self->layout_row_slot);
    }
    self->layout_row_slot = slot;
}

static void galaxy_menu_button3_on_click_method(button_t *self) {
    score_panel_t *ju = main_score_panel();
    modal_backdrop_keys_t keys = {0};
    modal_backdrop_t *built;
    (void)self;
    if (ju != NULL) {
        ju->is_shown = 1;
        ju->has_is_shown = 1;
    }
    keys.on_click = score_panel_toggle_on_click();
    built = modal_backdrop_new(&keys);
    (void)built;
}

/* Must stay non-NULL: ui_button draws a button without on_click in color 5 instead of 12/13. */
static void pause_menu_row2_on_click_method(button_t *self) {
    (void)self;
}

static void pause_menu_row3_on_click_method(button_t *self) {
    (void)self;
}

static void pause_menu_row4_on_click_method(button_t *self) {
    (void)self;
}

static const button_fn_t galaxy_menu_on_click_table[GALAXY_MENU_COUNT] = {
    galaxy_menu_button1_on_click_method,
    galaxy_menu_button2_on_click_method,
    galaxy_menu_button3_on_click_method,
    galaxy_menu_button4_on_click_method,
};
static const button_fn_t galaxy_menu_refresh_table[GALAXY_MENU_COUNT] = {NULL, galaxy_menu_button2_refresh_method, NULL, NULL};

static void galaxy_menu_button4_on_click_method(button_t *self) {
    const button_fn_t my[PAUSE_MENU_COUNT] = {
        score_panel_init_field_value(),
        pause_menu_row2_on_click_method,
        pause_menu_row3_on_click_method,
        pause_menu_row4_on_click_method,
    };
    button_keys_t bs[PAUSE_MENU_COUNT];
    open_box_frame_t frame = {0};
    int i;
    (void)self;

    if (!g_menu_up) {
        return;
    }
    for (i = 0; i < PAUSE_MENU_COUNT; i++) {
        const pause_menu_entry_t *e = &pause_menu_table[i];

        memset(&bs[i], 0, sizeof bs[i]);
        bs[i].layout_row_keys = layout_row_keys_of_row(e->layout_row);
        bs[i].has_on_click = 1;
        bs[i].on_click = my[i];
    }
    frame.has_fixed_pos = (g_pause_menu_open_box.has & PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS) != 0;
    frame.fixed_pos = g_pause_menu_open_box.fixed_pos;
    frame.has_back_color = g_pause_menu_open_box.has_back_color;
    frame.back_color = g_pause_menu_open_box.back_color;
    frame.has_row_step = (g_pause_menu_open_box.has & PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP) != 0;
    frame.row_step = g_pause_menu_open_box.row_step;
    message_box_open_box(&frame, bs, PAUSE_MENU_COUNT);
}

button_t *galaxy_menu_button(int i) {
    const galaxy_menu_entry_t *e;
    button_keys_t keys = {0};

    assert(g_menu_up && i >= 1 && i <= GALAXY_MENU_COUNT && "galaxy_menu_button: the galaxy menu has GALAXY_MENU_COUNT rows, after menu_init()");
    e = &galaxy_menu_table[i - 1];
    keys.layout_row_keys = layout_row_keys_of_row(e->layout_row);
    keys.has_pos = 1;
    keys.pos = g_galaxy_menu_pos[i - 1];
    keys.has_on_click = 1;
    keys.on_click = galaxy_menu_on_click_table[i - 1];
    keys.refresh = galaxy_menu_refresh_table[i - 1];
    return button_new(&keys);
}

button_fn_t galaxy_menu_on_click(int i) {
    assert(i >= 1 && i <= GALAXY_MENU_COUNT);
    return galaxy_menu_on_click_table[i - 1];
}

button_t *galaxy_menu_find_button(int i) {
    button_fn_t on_click = galaxy_menu_on_click(i);
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    for (int k = 0; k < bucket->count; k++) {
        entity_t *e = bucket->items[k];
        if (!e->is_dead && e->class == button_class() && ((const button_t *)e)->on_click == on_click) {
            return (button_t *)e;
        }
    }
    return NULL;
}

const entity_t *pause_menu_open_box(void) {
    return g_menu_up ? &g_pause_menu_open_box.header : NULL;
}

const entity_class_t *pause_menu_open_box_class(void) {
    return &pause_menu_open_box_class_def;
}

int pause_menu_shown(void) {
    const ui_t *ui = main_ui();
    if (ui == NULL) {
        return 0;
    }
    entity_id_list_iter_t it = entity_id_list_all(&ui->ephemerals);
    entity_t *e;
    while (entity_id_list_iter_next(&it, &e)) {
        if (e->class == button_class() && ((const button_t *)e)->on_click == pause_menu_row2_on_click_method) {
            return 1;
        }
    }
    return 0;
}

void menu_init(void) {
    int i;

    menu_shutdown();

    for (i = 0; i < GALAXY_MENU_COUNT; i++) {
        g_galaxy_menu_pos[i] = vec_make(p8_fixed_from_int(galaxy_menu_table[i].pos_x),
                                 p8_fixed_from_int(galaxy_menu_table[i].pos_y));
    }
    g_pause_menu_open_box.fixed_pos = vec_make(p8_fixed_from_int(pause_menu_tail.fixed_pos_x), p8_fixed_from_int(pause_menu_tail.fixed_pos_y));
    g_pause_menu_open_box.row_step = vec_make(p8_fixed_from_int(pause_menu_tail.row_step_x), p8_fixed_from_int(pause_menu_tail.row_step_y));
    g_pause_menu_open_box.has |= PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS | PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP;
    g_pause_menu_open_box.back_color = p8_fixed_from_int(pause_menu_tail.back_color);
    g_pause_menu_open_box.has_back_color = 1;
    g_menu_up = 1;
}

void menu_shutdown(void) {
    g_menu_up = 0;
    g_pause_menu_open_box.has &= (uint16_t)~(PAUSE_MENU_OPEN_BOX_HAS_FIXED_POS | PAUSE_MENU_OPEN_BOX_HAS_ROW_STEP);
    g_pause_menu_open_box.back_color = 0;
    g_pause_menu_open_box.has_back_color = 0;
}
