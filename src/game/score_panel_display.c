#include "game/score_panel_display.h"

#include <assert.h>
#include <string.h>

#include "game/economy_score.h"
#include "game/main_economy.h"
#include "game/layout.h"
#include "game/message_box.h"
#include "game/score_panel.h"
#include "p8/str.h"
#include "p8/vram.h"
#include "game/ui_button.h"
#include "engine/entity_registry.h"
#include "engine/frame_zone.h"


static int g_score_panel_new_year_keep_open = 0;

static int score_panel_new_year_keep_open_method(message_box_t *self) {
    (void)self;
    g_score_panel_new_year_keep_open -= 1;
    return g_score_panel_new_year_keep_open > 0;
}

static const data_row_t *score_panel_year_banner_label_row(void) {
    const score_panel_year_banner_t *year_banner = score_panel_year_banner();

    return year_banner != NULL ? &year_banner->second : NULL;
}

void score_panel_new_year(p8_fixed_t year) {
    score_panel_year_banner_t *year_banner = score_panel_year_banner();
    const data_row_t *countdown_rows = score_panel_countdown_rows();
    const data_row_t *message = NULL;
    char *year_str;
    int countdown_index;

    if (year_banner == NULL) {
        return;
    }

    countdown_index = 3426 - p8_fixed_flr_int(year);
    if (countdown_index >= 1 && countdown_index <= countdown_rows->count) {
        const data_item_t *item = &countdown_rows->items[countdown_index - 1];

        assert((item->row != NULL || item->is_false) && "score_panel_new_year: the countdown rows hold rows and false only");
        message = item->row;
    }

    year_str = p8_tostr_fixed(frame_zone(), year);
    assert(strlen("year ") + strlen(year_str) <= SCORE_PANEL_TEXT_CAP && "score_panel_new_year: a year is at most a 5-digit integer");
    strcpy(year_banner->year, "year ");
    strcat(year_banner->year, year_str);

    year_banner->second_items[0] = (data_item_t){.text = year_banner->year};

    if (message != NULL) {
        year_banner->items[2] = (data_item_t){.row = message};
        year_banner->row.count = 3;
        g_score_panel_new_year_keep_open = 90;
        year_banner->keep_open = score_panel_new_year_keep_open_method;
    } else {
        year_banner->row.count = 2;
    }

    {
        message_box_keys_t keys = {0};
        message_box_t *made;
        int i;

        keys.keep_open = year_banner->keep_open;
        made = message_box_new(&keys);
        for (i = 0; i < year_banner->row.count; i++) {
            if (year_banner->row.items[i].row == &year_banner->second) {
                /* The cart's year boxes share the banner: a box still drawn when the next year opens shows the new year. */
                message_box_add_shared_row(made, score_panel_year_banner_label_row);
            } else {
                message_box_add_row(made, year_banner->row.items[i].row);
            }
        }
    }
}

static void score_panel_toggle_visibility(void) {
    score_panel_t *ju = main_score_panel();
    if (ju != NULL) {
        ju->is_shown = !(ju->has_is_shown && ju->is_shown);
        ju->has_is_shown = 1;
    }
}

static int score_panel_toggle_on_click_method(modal_backdrop_t *self) {
    (void)self;
    score_panel_toggle_visibility();
    return 1;
}

static void score_panel_toggle_button_on_click_method(button_t *self) {
    (void)self;
    score_panel_toggle_visibility();
}

static int score_panel_update_keep_open_method(message_box_t *self) {
    const score_panel_t *ju = main_score_panel();
    (void)self;
    return ju != NULL && ju->has_is_shown && ju->is_shown;
}

typedef struct {
    final_score_t final_score;
} score_panel_update_ctx_t;

static char *score_panel_final_score_text(const final_score_t *s, int i) {
    switch (i) {
    case 1:
        return p8_tostr_fixed(frame_zone(), s->development);
    case 2:
        return p8_tostr_fixed(frame_zone(), s->population);
    case 3:
        return p8_tostr_fixed(frame_zone(), s->technology);
    case 4:
        return economy_score_percent_text(frame_zone(), s->happiness);
    case 5:
        return p8_tostr_fixed(frame_zone(), s->total);
    case 6:
        return p8_tostr_fixed(frame_zone(), s->tier);
    default:
        return p8_concat2(frame_zone(), "", "");
    }
}

static void score_panel_update_add_row(message_box_t *box, const score_rows_entry_t *entry, const score_panel_update_ctx_t *context) {
    int score_i;
    data_item_t values[5] = {{0}};
    int has_value[5] = {0};
    layout_row_keys_t keys = {0};
    int index;

    if (!entry->has_score) {
        message_box_add_row(box, &entry->row);
        return;
    }

    assert(entry->row.count <= 5 && "score_panel_update_add_row: a scored row holds at most the five items its line lays out");
    for (index = 0; index < entry->row.count; index++) {
        values[index] = entry->row.items[index];
        has_value[index] = 1;
    }

    score_i = entry->score;

    values[2] = (data_item_t){.text = score_panel_final_score_text(&context->final_score, score_i)};
    has_value[2] = 1;

    if (score_i == 0) {
        const economy_t *fm = main_economy_typed();
        const char *end_status_str = fm != NULL && fm->has_end_status && fm->end_status != NULL ? fm->end_status : "current score";
        const button_t *dk = economy_score_difficulty();
        const char *px = "";
        char *piece1;
        char *piece2;

        if (dk != NULL && dk->name != NULL) {
            px = dk->name;
        }
        piece1 = p8_concat2(frame_zone(), end_status_str, " (");
        piece2 = p8_concat2(frame_zone(), piece1, px);
        values[0] = (data_item_t){.text = p8_concat2(frame_zone(), piece2, ")")};
        has_value[0] = 1;
    }

    if (score_i == 6) {
        for (index = 0; index < 5; index++) {
            int icon = (index + 1 <= p8_fixed_flr_int(context->final_score.tier)) ? 56 : 57;
            values[index] = (data_item_t){.number = icon};
            has_value[index] = 1;
        }
    }

    for (index = 0; index < 5; index++) {
        if (has_value[index]) {
            layout_row_keys_append(&keys, layout_row_cell_item(&values[index]));
        }
    }
    keys.has_color = entry->row.has_color;
    keys.color = p8_fixed_from_int(entry->row.color);
    keys.has_score = 1;
    keys.score = p8_fixed_from_int(score_i);
    message_box_add_layout_row(box, &keys, vec_make(p8_fixed_from_int(entry->pos_x), p8_fixed_from_int(entry->pos_y)), entry->has_pos,
                               P8_FIXED_LIT(0x00008000, 0.5));
}

void score_panel_update(score_panel_t *self) {
    const economy_t *economy = main_economy_typed();
    int has_end_status = economy != NULL && economy->has_end_status && economy->end_status != NULL;
    int has_curtain = self != NULL && self->has_curtain;

    if (has_end_status && !has_curtain) {
        static const data_item_t back_items[] = {{.number = 10}, {.number = 7}};
        static const data_row_t back = {.count = 2, .items = back_items};
        button_keys_t restart_keys = {0};
        button_keys_t toggle_keys = {0};
        button_t *made1;
        button_t *made2;
        const vec_t first_line_pos =
            vec_make(p8_fixed_from_int(P8_SCREEN_WIDTH / 2 - 13), p8_fixed_from_int(P8_SCREEN_HEIGHT / 2 + 36));
        const vec_t second_line_pos =
            vec_make(p8_fixed_from_int(P8_SCREEN_WIDTH / 2 - 25), p8_fixed_from_int(P8_SCREEN_HEIGHT / 2 + 36));

        (void)modal_backdrop_new(NULL);

        layout_row_keys_append(&restart_keys.layout_row_keys, layout_row_cell_string(" again? "));
        restart_keys.has_layer = 1;
        restart_keys.layer = p8_fixed_from_int(15);
        restart_keys.has_pos = 1;
        restart_keys.pos = first_line_pos;
        restart_keys.has_on_click = 1;
        restart_keys.on_click = score_panel_init_field_value();
        made1 = button_new(&restart_keys);
        (void)made1;

        layout_row_keys_append(&toggle_keys.layout_row_keys, layout_row_cell_row(&back));
        toggle_keys.has_layer = 1;
        toggle_keys.layer = p8_fixed_from_int(15);
        toggle_keys.has_pos = 1;
        toggle_keys.pos = second_line_pos;
        toggle_keys.has_on_click = 1;
        toggle_keys.on_click = score_panel_toggle_button_on_click_method;
        made2 = button_new(&toggle_keys);
        (void)made2;

        if (self != NULL) {
            self->is_shown = 1;
            self->has_is_shown = 1;
            self->curtain = 0;
            self->has_curtain = 1;
            entity_registry_set_layer(&self->header, p8_fixed_from_int(13));
        }
    }

    {
        int is_shown = self != NULL && self->has_is_shown && self->is_shown;
        int last_box_equals_shown_message_box = 0;

        /* Without the NULL test, NULL == NULL would fire on the first eligible tick. */
        if (self != NULL) {
            message_box_t *last_box = score_panel_last_box(self);
            last_box_equals_shown_message_box = last_box != NULL && shown_message_box() == last_box;
        }

        if (is_shown && !last_box_equals_shown_message_box) {
            score_panel_update_ctx_t ctx = {economy_score_final()};
            int is_ready = score_panel_ready();
            message_box_keys_t keys = {0};
            message_box_t *made;
            int i;

            keys.has_duration_scale = 1;
            keys.duration_scale = p8_fixed_from_int(3);
            keys.keep_open = score_panel_update_keep_open_method;
            made = message_box_new(&keys);
            for (i = 0; is_ready && i < SCORE_ROWS_COUNT; i++) {
                score_panel_update_add_row(made, &score_rows_table[i], &ctx);
            }

            if (self != NULL) {
                score_panel_set_last_box(self, made);
            }
        }
    }
}

void score_panel_display_init(void) {
    score_panel_display_shutdown();
    g_score_panel_new_year_keep_open = 0;
}

void score_panel_display_shutdown(void) {
}

modal_backdrop_on_click_fn_t score_panel_toggle_on_click(void) {
    return score_panel_toggle_on_click_method;
}

void score_panel_new_year_method(entity_t *self, p8_fixed_t year) {
    (void)self;
    score_panel_new_year(year);
}

void score_panel_update_method(entity_t *self, p8_fixed_t t) {
    (void)t;
    score_panel_update((score_panel_t *)self);
}
