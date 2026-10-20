#include "game/score_panel.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/collect.h"
#include "engine/dispatch_context.h"
#include "game/economy_formulas.h"
#include "engine/entity_registry.h"
#include "game/main_economy.h"
#include "game/game_data.h"
#include "game/hud.h"
#include "engine/entity.h"
#include "engine/entity_id.h"
#include "game/map_tables.h"
#include "game/draw_functions.h"
#include "p8/str.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

static int g_score_panel_up = 0;
static entity_id_t g_main_score_panel;

static p8_fixed_t fixed_min(p8_fixed_t a, p8_fixed_t b) {
    return (a < b) ? a : b;
}

static score_panel_readouts_t g_score_panel_readouts;
static score_panel_year_banner_t g_score_panel_year_banner;

static void score_panel_row_copy(data_row_t *dst, data_item_t *items, int cap, const data_row_t *src) {
    int i;

    assert(src->count <= cap && "score_panel_row_copy: a copied row of the score panel holds no more items than its generated row");
    for (i = 0; i < src->count; i++) {
        items[i] = src->items[i];
    }
    *dst = *src;
    dst->items = items;
}

static const entity_class_t score_panel_layout_block_class_def = {0};

static score_panel_layout_block_t g_score_panel_layout_block = {
    {.class = &score_panel_layout_block_class_def},
    SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS | SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP,
    {0x0, 0xb0000},
    {0x0, 0xb0000},
    0xa0000,
};

score_panel_readouts_t *score_panel_readouts(void) {
    return g_score_panel_up ? &g_score_panel_readouts : NULL;
}

score_panel_year_banner_t *score_panel_year_banner(void) {
    return g_score_panel_up ? &g_score_panel_year_banner : NULL;
}

score_panel_layout_block_t *score_panel_layout_block(void) {
    return g_score_panel_up ? &g_score_panel_layout_block : NULL;
}

const entity_class_t *score_panel_layout_block_class(void) {
    return &score_panel_layout_block_class_def;
}

const data_row_t *score_panel_frame_rows(void) {
    return score_panel_archetype.frame_rows;
}

const data_row_t *score_panel_countdown_rows(void) {
    return score_panel_archetype.countdown_rows;
}

static void score_panel_init_placeholder(button_t *self) {
    (void)self;
}

static button_fn_t g_init_method = score_panel_init_placeholder;

button_fn_t score_panel_init_field_value(void) {
    return g_init_method;
}

void score_panel_set_init(button_fn_t fn) {
    g_init_method = (fn != NULL) ? fn : score_panel_init_placeholder;
}

void score_panel_refresh(void) {
    const economy_t *economy = main_economy_typed();
    const main_economy_placeholder_t *economy_placeholder = main_economy_placeholder();
    p8_fixed_t main_economy_income = economy != NULL ? economy->income : p8_fixed_from_int(0);
    p8_fixed_t main_economy_credits = p8_fixed_from_int(0);
    data_item_t main_economy_research_points = {.is_nil = 1};
    p8_fixed_t main_economy_happiness = economy != NULL && economy->has_happiness ? economy->happiness : p8_fixed_from_int(0);
    p8_fixed_t main_economy_year_ticks = economy != NULL ? economy->year_ticks : p8_fixed_from_int(0);
    data_item_t main_economy_year = economy != NULL ? (data_item_t){.is_fixed = 1, .fixed = economy->year}
                                       : (data_item_t){.is_nil = 1};
    const resource_map_t *main_economy_production = economy != NULL && (economy->has & ECONOMY_HAS_PRODUCTION) ? &economy->production : NULL;
    score_panel_readouts_t *readouts_state;
    data_item_t readouts[7];
    char *text;
    char *credits_str;
    char *happiness_str;
    char *production_text;
    int income_color = 0;
    int index;

    if (economy != NULL) {
        main_economy_credits = economy->credits;
        main_economy_research_points = (data_item_t){.is_fixed = 1, .fixed = economy->research_points};
    } else if (economy_placeholder != NULL) {
        if (economy_placeholder->has_credits) {
            main_economy_credits = economy_placeholder->credits;
        }
        if (economy_placeholder->has_research_points) {
            main_economy_research_points = (data_item_t){.is_fixed = 1, .fixed = economy_placeholder->research_points};
        }
    }

    text = collect_format_signed(frame_zone(), main_economy_income, 0, NULL, &income_color);

    credits_str = p8_tostr_fixed(frame_zone(), main_economy_credits);
    readouts[0] = (data_item_t){.text = p8_concat2(frame_zone(), credits_str, "$")};

    readouts[1] = (data_item_t){.text = text};

    readouts[2] = main_economy_research_points;

    {
        int resource_index;
        p8_fixed_t production_val = p8_fixed_from_int(0);
        if (resource_code_lookup("k", &resource_index)) {
            resource_map_get(main_economy_production, p8_fixed_from_int(resource_index), &production_val);
        }
        production_text =
            p8_tostr_fixed(frame_zone(), p8_fixed_add(production_val, p8_fixed_from_int(1)));
        readouts[3] = (data_item_t){.text = p8_concat2(frame_zone(), "+", production_text)};
    }

    happiness_str = p8_tostr_fixed(frame_zone(), main_economy_happiness);
    readouts[4] = (data_item_t){.text = p8_concat2(frame_zone(), happiness_str, "%")};

    {
        int lo = p8_fixed_flr_int(main_economy_year_ticks);
        readouts[5] = (lo >= 1 && lo <= PERIOD_NAMES_COUNT) ? (data_item_t){.text = period_names_table[lo - 1]}
                                                  : (data_item_t){.is_nil = 1};
    }

    readouts[6] = main_economy_year;

    readouts_state = score_panel_readouts();
    if (readouts_state != NULL) {
        for (index = 0; index < 7; index++) {
            if (readouts[index].text != NULL) {
                assert(strlen(readouts[index].text) <= SCORE_PANEL_TEXT_CAP && "score_panel_refresh: a readout text is a number with its suffix, or a month");
                strcpy(readouts_state->texts[index], readouts[index].text);
                readouts[index].text = readouts_state->texts[index];
            }
            readouts_state->sub_items[index][0] = readouts[index];
        }
    }
}

static void score_panel_refresh_method(entity_t *self) {
    (void)self;
    score_panel_refresh();
}

enum { SCORE_PANEL_DRAW_ARTICLES_CAP = 5 + 7 };

void score_panel_draw(p8_graphics_t *gfx, p8_vram_t *mem, score_panel_t *self) {
    const data_row_t *i_ = score_panel_frame_rows();
    score_panel_readouts_t *readouts_state = score_panel_readouts();
    const data_row_t *combined[SCORE_PANEL_DRAW_ARTICLES_CAP];
    int n = 0;
    int i;

    assert(i_->count + (readouts_state != NULL ? readouts_state->row.count : 0) <= SCORE_PANEL_DRAW_ARTICLES_CAP &&
           "score_panel_draw: the frame rows and the readouts are 5 + 7 articles");
    for (i = 0; i < i_->count; i++) {
        combined[n++] = i_->items[i].row;
    }
    for (i = 0; readouts_state != NULL && i < readouts_state->row.count; i++) {
        combined[n++] = readouts_state->row.items[i].row;
    }

    draw_functions_draw_rows_default(gfx, mem, combined, n);

    if (self != NULL && self->has_curtain) {
        p8_fixed_t gz = p8_fixed_add(self->curtain, P8_FIXED_LIT(0x00003333, 0.2));
        p8_fixed_t capped = fixed_min(gz, P8_FIXED_LIT(0x00033333, 3.2));
        p8_fixed_t h = p8_fixed_mul(capped, capped);

        self->curtain = gz;
        hud_draw_bar(gfx, mem, p8_fixed_from_int(0), h, 0);
        /* No colour on purpose: the bottom bar inherits the pen colour the line above leaves. */
        hud_draw_bar_default(gfx, mem, p8_fixed_sub(p8_fixed_from_int(P8_SCREEN_HEIGHT - 1), h),
                             p8_fixed_from_int(P8_SCREEN_HEIGHT - 1));
    }
}

static void score_panel_draw_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    score_panel_draw(dispatch_context_gfx(), dispatch_context_mem(), (score_panel_t *)self);
}

static const entity_class_t score_panel_class_def = {
    .draw = score_panel_draw_method,
    .new_year = score_panel_new_year_method,
    .refresh = score_panel_refresh_method,
    .states = {[ENTITY_STATE_NORMAL] = score_panel_update_method},
};

static score_panel_t *score_panel_create(void) {
    score_panel_t *self;

    assert(g_score_panel_up && "score_panel_init() must run first");
    self = (score_panel_t *)allocator_alloc(game_zone(), sizeof *self);
    entity_init_instance(&self->header, &score_panel_class_def, score_panel_archetype.state, score_panel_archetype.state_time);
    self->archetype = &score_panel_archetype;
    entity_registry_set_layer(&self->header, self->archetype->layer);
    self->last_box = ENTITY_ID_NONE;
    self->is_shown = 0;
    self->has_is_shown = 0;
    self->curtain = 0;
    self->has_curtain = 0;
    entity_construct(&self->header);
    return self;
}

void score_panel_init(void) {
    int i;

    score_panel_shutdown();

    g_score_panel_up = 1;
    score_panel_row_copy(&g_score_panel_readouts.row, g_score_panel_readouts.items, 7, &score_panel_readouts_row);
    for (i = 0; i < g_score_panel_readouts.row.count; i++) {
        score_panel_row_copy(&g_score_panel_readouts.subs[i], g_score_panel_readouts.sub_items[i], 4, score_panel_readouts_row.items[i].row);
        g_score_panel_readouts.items[i].row = &g_score_panel_readouts.subs[i];
    }
    score_panel_row_copy(&g_score_panel_year_banner.row, g_score_panel_year_banner.items, 3, &score_panel_year_banner_row);
    score_panel_row_copy(&g_score_panel_year_banner.second, g_score_panel_year_banner.second_items, 5, score_panel_year_banner_row.items[1].row);
    g_score_panel_year_banner.items[1].row = &g_score_panel_year_banner.second;
    g_score_panel_year_banner.keep_open = NULL;

    score_panel_set_init(NULL);
    g_main_score_panel = ENTITY_ID_NONE;

    score_panel_display_init();
}

void score_panel_shutdown(void) {
    score_panel_display_shutdown();
    g_score_panel_up = 0;
    score_panel_set_init(NULL);
    g_main_score_panel = ENTITY_ID_NONE;
}

int score_panel_ready(void) {
    return g_score_panel_up;
}

const entity_class_t *score_panel_class(void) {
    return &score_panel_class_def;
}

score_panel_t *score_panel_new(void) {
    return score_panel_create();
}

message_box_t *score_panel_last_box(const score_panel_t *self) {
    return (message_box_t *)entity_get(self->last_box);
}

void score_panel_set_last_box(score_panel_t *self, message_box_t *box) {
    self->last_box = entity_id_link(box != NULL ? &box->header : NULL);
}

score_panel_t *main_score_panel(void) {
    entity_t *e = entity_get(g_main_score_panel);

    assert((e == NULL || e->class == &score_panel_class_def) && "main_score_panel: only a score panel is ever stored");
    return (score_panel_t *)e;
}

void score_panel_set_main(score_panel_t *instance) {
    g_main_score_panel = entity_id_link(instance != NULL ? &instance->header : NULL);
}
