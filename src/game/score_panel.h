#ifndef GAME_SCORE_PANEL_H
#define GAME_SCORE_PANEL_H

#include "p8/fixed.h"
#include "game/game_data.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/map_tables.h"
#include "game/message_box.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

typedef struct {
    entity_t header;
    const score_panel_archetype_t *archetype;
    entity_id_t last_box;
    int is_shown;
    int has_is_shown;
    p8_fixed_t curtain;
    int has_curtain;
} score_panel_t;

typedef struct {
    entity_t header;
    uint16_t has;
    vec_t fixed_pos;
    vec_t row_step;
    p8_fixed_t back_color;
} score_panel_layout_block_t;

enum {
    SCORE_PANEL_LAYOUT_BLOCK_HAS_FIXED_POS = 1u << 0,
    SCORE_PANEL_LAYOUT_BLOCK_HAS_ROW_STEP = 1u << 1,
};

enum { SCORE_PANEL_TEXT_CAP = 16 };

typedef struct {
    data_row_t row;
    data_item_t items[7];
    data_row_t subs[7];
    data_item_t sub_items[7][4];
    char texts[7][SCORE_PANEL_TEXT_CAP + 1];
} score_panel_readouts_t;

typedef struct {
    data_row_t row;
    data_item_t items[3];
    data_row_t second;
    data_item_t second_items[5];
    char year[SCORE_PANEL_TEXT_CAP + 1];
    message_box_keep_open_fn_t keep_open;
} score_panel_year_banner_t;

void score_panel_init(void);

void score_panel_shutdown(void);

int score_panel_ready(void);
const entity_class_t *score_panel_class(void);

score_panel_readouts_t *score_panel_readouts(void);
score_panel_year_banner_t *score_panel_year_banner(void);
score_panel_layout_block_t *score_panel_layout_block(void);
const entity_class_t *score_panel_layout_block_class(void);
const data_row_t *score_panel_frame_rows(void);
const data_row_t *score_panel_countdown_rows(void);

score_panel_t *score_panel_new(void);

message_box_t *score_panel_last_box(const score_panel_t *self);
void score_panel_set_last_box(score_panel_t *self, message_box_t *box);

button_fn_t score_panel_init_field_value(void);
void score_panel_set_init(button_fn_t fn);

score_panel_t *main_score_panel(void);
void score_panel_set_main(score_panel_t *instance);

void score_panel_refresh(void);

void score_panel_draw(p8_graphics_t *gfx, p8_vram_t *mem, score_panel_t *self);

void score_panel_new_year_method(entity_t *self, p8_fixed_t year);
void score_panel_update_method(entity_t *self, p8_fixed_t t);

void score_panel_display_init(void);
void score_panel_display_shutdown(void);

#endif
