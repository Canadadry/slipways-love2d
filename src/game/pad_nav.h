#ifndef GAME_PAD_NAV_H
#define GAME_PAD_NAV_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"
#include "game/bindings.h"
#include "game/pad_back.h"
#include "game/pad_build.h"
#include "game/pad_cycle.h"
#include "game/pad_jump.h"
#include "game/pad_select.h"
#include "game/pad_slipway.h"
#include "p8/input.h"

typedef enum { PAD_NAV_MAP_JUMP, PAD_NAV_MAP_AIM, PAD_NAV_MAP_SLIPWAY } pad_nav_map_mode_t;

typedef enum {
    PAD_NAV_PLACEMENT_NONE,
    PAD_NAV_PLACEMENT_PRESSED,
    PAD_NAV_PLACEMENT_CARRYING,
    PAD_NAV_PLACEMENT_SUSPENDED
} pad_nav_placement_t;

typedef struct {
    entity_id_t focus;
    pad_build_t build;
    pad_cycle_t cycle;
    pad_nav_placement_t placement;
    entity_id_t suspended_carry;
    int pause_due;
    int pause_choose_row_armed;
    int menu_choose_row_armed;
    pad_nav_map_mode_t map_mode;
    int marker_frames;
    int release_pending;
    int previous_pad;
    int chord_held;
    int left_was_down;
    int cursor_x;
    int cursor_y;
    int held_frames[PAD_JUMP_DIRECTIONS];
    int probe_or_fold_engaged;
    entity_id_t game;
    pad_jump_t jump;
    pad_slipway_t slipway;
    pad_back_t back;
    pad_select_t select;
    bindings_t bindings;
} pad_nav_t;

void pad_nav_init(pad_nav_t *nav, const bindings_t *bindings);

const bindings_t *pad_nav_bindings(const pad_nav_t *nav);

void pad_nav_step(pad_nav_t *nav, int raw_buttons, p8_input_frame_t *played);

entity_t *pad_nav_focus(const pad_nav_t *nav);

int pad_nav_panel_open(void);

int pad_nav_aim_mode(const pad_nav_t *nav);

int pad_nav_jump_mode(const pad_nav_t *nav);

entity_t *pad_nav_hovered(const pad_nav_t *nav);

entity_t *pad_nav_slipway_start(const pad_nav_t *nav);

entity_t *pad_nav_last_acted(const pad_nav_t *nav);

int pad_nav_select_held(const pad_nav_t *nav);

int pad_nav_year_chain_open(const pad_nav_t *nav);

entity_t *pad_nav_slipway_target(const pad_nav_t *nav);

int pad_nav_build_bar_open(const pad_nav_t *nav);

int pad_nav_tech_open(const pad_nav_t *nav);

int pad_nav_score_open(const pad_nav_t *nav);

entity_t *pad_nav_build_icon(const pad_nav_t *nav);

entity_t *pad_nav_placing(const pad_nav_t *nav);

entity_t *pad_nav_first_item(void);

entity_t *pad_nav_neighbor(const entity_t *item, pad_direction_t direction);

int pad_nav_is_item(const entity_t *e);

#endif
