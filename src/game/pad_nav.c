#include "game/pad_nav.h"

#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/frame_zone.h"
#include "engine/geom.h"
#include "game/background.h"
#include "game/cheat.h"
#include "game/main_economy.h"
#include "game/menu.h"
#include "game/message_box.h"
#include "game/pad_build.h"
#include "game/pad_cycle.h"
#include "game/pad_jump.h"
#include "game/pad_panel.h"
#include "game/pad_select.h"
#include "game/pad_target.h"
#include "game/planet.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/ui.h"
#include "p8/vram.h"

#include <assert.h>

int pad_nav_panel_open(void) {
    return pad_panel_open();
}

static int on_the_map(const pad_nav_t *nav) {
    assert(!(pad_build_is_open(&nav->build) && pad_cycle_panel(&nav->cycle) != PAD_CYCLE_CLOSED) &&
           "pad_nav: the build bar and a cycle panel are never open together");
    return !pad_build_is_open(&nav->build) && pad_cycle_panel(&nav->cycle) == PAD_CYCLE_CLOSED;
}

static int cycle_open(const pad_nav_t *nav) {
    return pad_cycle_panel(&nav->cycle) != PAD_CYCLE_CLOSED;
}

entity_t *pad_nav_last_acted(const pad_nav_t *nav) {
    return main_economy_typed() != NULL ? pad_back_target(&nav->back, NULL) : NULL;
}

int pad_nav_select_held(const pad_nav_t *nav) {
    return nav->select.held_frames;
}

int pad_nav_year_chain_open(const pad_nav_t *nav) {
    return nav->select.chain_frames > 0;
}

static int game_unseen(const pad_nav_t *nav) {
    const economy_t *economy = main_economy_typed();
    return economy != NULL && entity_get(nav->game) != &economy->header;
}

static pad_nav_map_mode_t map_mode_now(const pad_nav_t *nav) {
    if (!game_unseen(nav)) {
        return nav->map_mode;
    }
    return pad_jump_any_candidate(entities_by_tag(TAG_NODE)) ? PAD_NAV_MAP_JUMP : PAD_NAV_MAP_AIM;
}

int pad_nav_aim_mode(const pad_nav_t *nav) {
    return main_economy_typed() != NULL && on_the_map(nav) && !pad_nav_panel_open() &&
           map_mode_now(nav) == PAD_NAV_MAP_AIM;
}

static entity_t *live(entity_t *e) {
    return e != NULL && !e->is_dead ? e : NULL;
}

int pad_nav_jump_mode(const pad_nav_t *nav) {
    return main_economy_typed() != NULL && on_the_map(nav) && !pad_nav_panel_open() &&
           map_mode_now(nav) == PAD_NAV_MAP_JUMP;
}

entity_t *pad_nav_hovered(const pad_nav_t *nav) {
    return nav->map_mode == PAD_NAV_MAP_JUMP && !game_unseen(nav) ? pad_jump_hovered(&nav->jump) : NULL;
}

entity_t *pad_nav_slipway_start(const pad_nav_t *nav) {
    return map_mode_now(nav) == PAD_NAV_MAP_SLIPWAY ? live(entity_get(nav->slipway.start)) : NULL;
}

entity_t *pad_nav_slipway_target(const pad_nav_t *nav) {
    entity_t *target = map_mode_now(nav) == PAD_NAV_MAP_SLIPWAY ? entity_get(nav->slipway.target) : NULL;
    return pad_target_is_candidate(target) ? target : NULL;
}

int pad_nav_build_bar_open(const pad_nav_t *nav) {
    return main_economy_typed() != NULL && pad_build_is_open(&nav->build);
}

int pad_nav_tech_open(const pad_nav_t *nav) {
    return main_economy_typed() != NULL && pad_cycle_panel(&nav->cycle) == PAD_CYCLE_TECH;
}

int pad_nav_score_open(const pad_nav_t *nav) {
    return main_economy_typed() != NULL && pad_cycle_panel(&nav->cycle) == PAD_CYCLE_SCORE;
}

entity_t *pad_nav_build_icon(const pad_nav_t *nav) {
    return pad_nav_build_bar_open(nav) ? pad_build_icon(&nav->build) : NULL;
}

static entity_t *placing_structure(entity_t *e) {
    e = live(e);
    return e != NULL && planet_is_family(e) && e->state == ENTITY_STATE_PLACING ? e : NULL;
}

static entity_t *held_structure(void) {
    return main_ui() != NULL ? placing_structure(ui_left_held(main_ui())) : NULL;
}

entity_t *pad_nav_placing(const pad_nav_t *nav) {
    return nav->placement == PAD_NAV_PLACEMENT_CARRYING ? held_structure() : NULL;
}

int pad_nav_is_item(const entity_t *e) {
    return pad_panel_is_item(e);
}

entity_t *pad_nav_first_item(void) {
    return pad_panel_first_item();
}

entity_t *pad_nav_neighbor(const entity_t *item, pad_direction_t direction) {
    return pad_panel_neighbor(item, direction);
}

enum { HELD_FRAMES_CAP = 41 };

static void reset_aim_mode(pad_nav_t *nav) {
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        nav->held_frames[i] = 0;
    }
    nav->probe_or_fold_engaged = 0;
}

static int pan_speed(int held_frames) {
    if (held_frames == 0) {
        return 0;
    }
    if (held_frames <= 10) {
        return 1;
    }
    return held_frames <= 40 ? 2 : 4;
}

static void aim_pan(pad_nav_t *nav, bindings_mode_t mode, int pad_buttons) {
    static const bindings_action_t pan_action[PAD_JUMP_DIRECTIONS] = {
        BINDINGS_ACTION_PAN_UP, BINDINGS_ACTION_PAN_DOWN, BINDINGS_ACTION_PAN_LEFT, BINDINGS_ACTION_PAN_RIGHT
    };
    assert(bindings_mode_has_action(mode, BINDINGS_ACTION_PAN_UP) &&
           "aim_pan: a mode with pan_* actions (aim or placement)");
    int speed[PAD_JUMP_DIRECTIONS];
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        if (!(pad_buttons & bindings_mask(&nav->bindings, mode, pan_action[i]))) {
            nav->held_frames[i] = 0;
        } else if (nav->held_frames[i] < HELD_FRAMES_CAP) {
            nav->held_frames[i]++;
        }
        speed[i] = pan_speed(nav->held_frames[i]);
    }
    assert(pad_jump_direction(0) == PAD_DIRECTION_UP && pad_jump_direction(1) == PAD_DIRECTION_DOWN &&
           pad_jump_direction(2) == PAD_DIRECTION_LEFT && pad_jump_direction(3) == PAD_DIRECTION_RIGHT &&
           "aim_pan: speed[] and pan_action[] are indexed Up, Down, Left, Right");
    camera_t *camera = main_camera_typed();
    if (camera != NULL) {
        camera_pan(camera, speed[3] - speed[2], speed[1] - speed[0]);
    }
}

static void reset_map_mode(pad_nav_t *nav) {
    nav->map_mode = PAD_NAV_MAP_JUMP;
    pad_jump_hover(&nav->jump, NULL);
    pad_jump_repeat_clear(&nav->jump);
    nav->marker_frames = 0;
    nav->placement = PAD_NAV_PLACEMENT_NONE;
    nav->suspended_carry = ENTITY_ID_NONE;
    pad_slipway_init(&nav->slipway);
    pad_select_init(&nav->select);
}

static void close_panels(pad_nav_t *nav) {
    pad_build_close(&nav->build);
    pad_cycle_set(&nav->cycle, PAD_CYCLE_CLOSED);
}

void pad_nav_init(pad_nav_t *nav, const bindings_t *bindings) {
    assert(bindings != NULL && "pad_nav_init: the caller hands its compiled table, never NULL");
    nav->bindings = *bindings;
    nav->focus = ENTITY_ID_NONE;
    pad_build_init(&nav->build);
    pad_cycle_init(&nav->cycle);
    nav->game = ENTITY_ID_NONE;
    pad_jump_init(&nav->jump);
    pad_back_init(&nav->back);
    reset_map_mode(nav);
    nav->release_pending = 0;
    nav->pause_due = 0;
    nav->pause_choose_row_armed = 0;
    nav->menu_choose_row_armed = 0;
    nav->previous_pad = 0;
    nav->chord_held = 0;
    nav->left_was_down = 0;
    nav->cursor_x = 0;
    nav->cursor_y = 0;
    reset_aim_mode(nav);
}

const bindings_t *pad_nav_bindings(const pad_nav_t *nav) {
    return &nav->bindings;
}

static int action_pressed(const pad_nav_t *nav, int pressed, bindings_mode_t mode, bindings_action_t action) {
    return (pressed & bindings_mask(&nav->bindings, mode, action)) != 0;
}

static int action_held(const pad_nav_t *nav, int pad_buttons, bindings_mode_t mode, bindings_action_t action) {
    return (pad_buttons & bindings_mask(&nav->bindings, mode, action)) != 0;
}

entity_t *pad_nav_focus(const pad_nav_t *nav) {
    entity_t *focus = entity_get(nav->focus);
    return pad_nav_is_item(focus) ? focus : NULL;
}

static int planet_menu_open(const pad_nav_t *nav) {
    return on_the_map(nav) && pad_panel_box_open() && !pause_menu_shown();
}

static int armed_panel_open(const pad_nav_t *nav) {
    return (on_the_map(nav) || cycle_open(nav)) && pad_nav_panel_open() && !pause_menu_shown();
}

static bindings_mode_t panel_mode(const pad_nav_t *nav) {
    if (pause_menu_shown()) {
        return BINDINGS_MODE_PAUSE;
    }
    if (main_economy_typed() == NULL) {
        return BINDINGS_MODE_TITLE;
    }
    return cycle_open(nav) ? BINDINGS_MODE_PANELS : BINDINGS_MODE_MENU;
}

static pad_direction_t first_pressed_move(const pad_nav_t *nav, int pressed, bindings_mode_t mode) {
    static const bindings_action_t move_action[PAD_JUMP_DIRECTIONS] = {
        BINDINGS_ACTION_MOVE_UP, BINDINGS_ACTION_MOVE_DOWN, BINDINGS_ACTION_MOVE_LEFT, BINDINGS_ACTION_MOVE_RIGHT
    };
    assert(bindings_mode_has_action(mode, BINDINGS_ACTION_MOVE_UP) &&
           "first_pressed_move: a mode with move_* actions (title, menu, pause or panels)");
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        if (action_pressed(nav, pressed, mode, move_action[i])) {
            return pad_jump_direction(i);
        }
    }
    return PAD_DIRECTION_NONE;
}

static void panel_step(pad_nav_t *nav, int pad_buttons, int pressed, p8_input_frame_t *played) {
    bindings_mode_t mode = panel_mode(nav);
    pad_direction_t direction = first_pressed_move(nav, pressed, mode);
    int choose_held = action_held(nav, pad_buttons, mode, BINDINGS_ACTION_CHOOSE_ROW);
    if (pause_menu_shown()) {
        nav->pause_choose_row_armed |= action_pressed(nav, pressed, BINDINGS_MODE_PAUSE, BINDINGS_ACTION_CHOOSE_ROW);
        choose_held &= nav->pause_choose_row_armed;
    } else if (armed_panel_open(nav)) {
        nav->menu_choose_row_armed |= action_pressed(nav, pressed, mode, BINDINGS_ACTION_CHOOSE_ROW);
        choose_held &= nav->menu_choose_row_armed;
    }
    nav->focus = entity_id_link(pad_panel_step(entity_get(nav->focus), choose_held, direction, &nav->cursor_x,
                                               &nav->cursor_y, &played->mouse_buttons));
}

/* Keep in step with pad_panel.c's box_at for layers under 10: world point minus the camera, floored. */
static void screen_point(const entity_t *e, int *x, int *y) {
    vec_t at = entity_pos(e);
    const camera_t *camera = main_camera_typed();
    if (camera != NULL) {
        at = vec_sub(at, entity_pos(&camera->header));
    }
    *x = p8_fixed_flr_int(at.x);
    *y = p8_fixed_flr_int(at.y);
}

static entity_t *entity_under(int x, int y, int (*accept)(const entity_t *e)) {
    return pad_panel_entity_under(x, y, PAD_PANEL_ANY_LAYER, accept);
}

static int presses_off_background(const entity_t *e) {
    return e->class->left_press != NULL && e->class != background_class();
}

static void click(pad_nav_t *nav, p8_input_frame_t *played) {
    played->mouse_buttons = P8_MOUSE_LEFT;
    nav->release_pending = 1;
}

static void enter_aim_mode(pad_nav_t *nav) {
    nav->map_mode = PAD_NAV_MAP_AIM;
    nav->cursor_x = P8_SCREEN_WIDTH / 2;
    nav->cursor_y = P8_SCREEN_HEIGHT / 2;
    pad_jump_repeat_clear(&nav->jump);
}

static int pressed_before(const entity_t *planet, int x, int y) {
    return pad_panel_entity_under(x, y, p8_fixed_flr_int(planet->layer), pad_panel_is_candidate) != NULL;
}

static entity_t *held_slipway(void) {
    entity_t *held = main_ui() != NULL ? live(ui_left_held(main_ui())) : NULL;
    return held != NULL && held->class == slipway_class() ? held : NULL;
}

static void drop_held_slipway(void) {
    entity_t *held = held_slipway();
    if (held != NULL) {
        held->class->drop(held, NULL);
        ui_set_left_held(main_ui(), NULL);
    }
}

static void centre_world_point(int *x, int *y) {
    const camera_t *camera = main_camera_typed();
    vec_t at = camera != NULL ? camera_pos_before_shake(camera) : vec_make(0, 0);
    *x = p8_fixed_flr_int(at.x) + P8_SCREEN_WIDTH / 2;
    *y = p8_fixed_flr_int(at.y) + P8_SCREEN_HEIGHT / 2;
}

static void edge_scroll(const entity_t *hovered);

static entity_list_t live_candidates(void) {
    return pad_jump_candidates(frame_zone(), entities_by_tag(TAG_NODE));
}

static void back_to_jump_mode(pad_nav_t *nav, const entity_t *e) {
    nav->map_mode = PAD_NAV_MAP_JUMP;
    pad_jump_hover(&nav->jump, pad_target_is_candidate(e) ? e : NULL);
    pad_slipway_init(&nav->slipway);
}

/* Seeded, not pad_jump_hover: jump mode must never reseed on the first colony afterwards. */
static void leave_aim_mode(pad_nav_t *nav) {
    if (!pad_jump_any_candidate(entities_by_tag(TAG_NODE))) {
        return;
    }
    entity_list_t candidates = live_candidates();
    int x, y;
    centre_world_point(&x, &y);
    entity_t *nearest = pad_jump_nearest(x, y, &candidates);
    nav->map_mode = PAD_NAV_MAP_JUMP;
    pad_jump_hover_seeded(&nav->jump, nearest);
    pad_jump_repeat_clear(&nav->jump);
}

static int cancel_slipway(pad_nav_t *nav) {
    if (nav->map_mode != PAD_NAV_MAP_SLIPWAY) {
        return 0;
    }
    int held = held_slipway() != NULL;
    drop_held_slipway();
    back_to_jump_mode(nav, pad_nav_slipway_start(nav));
    return held;
}

static int select_tap(pad_nav_t *nav, const entity_t *current, const entity_t **shown) {
    entity_list_t candidates = live_candidates();
    entity_t *next = pad_select_next(&candidates, current);
    int jumps = next != NULL && next != current;
    if (jumps) {
        back_to_jump_mode(nav, next);
        pad_jump_hover_seeded(&nav->jump, next);
        pad_jump_repeat_clear(&nav->jump);
        edge_scroll(next);
        *shown = next;
        screen_point(next, &nav->cursor_x, &nav->cursor_y);
    }
    return jumps;
}

/* On a click's release frame probe_or_fold starts nothing: finish_step forces its press up. */
static void aim_mode_step(pad_nav_t *nav, int pad_buttons, int pressed, int back_returns, int release, int still,
                          int select_tapped, p8_input_frame_t *played, const entity_t **shown) {
    nav->focus = ENTITY_ID_NONE;
    nav->cursor_x = P8_SCREEN_WIDTH / 2;
    nav->cursor_y = P8_SCREEN_HEIGHT / 2;
    if (!still) {
        aim_pan(nav, BINDINGS_MODE_AIM, pad_buttons);
    }
    if (select_tapped && !nav->probe_or_fold_engaged && select_tap(nav, NULL, shown)) {
        return;
    }
    if (action_pressed(nav, pressed, BINDINGS_MODE_AIM, BINDINGS_ACTION_LEAVE_AIM) && !nav->probe_or_fold_engaged) {
        leave_aim_mode(nav);
        return;
    }
    if (!action_held(nav, pad_buttons, BINDINGS_MODE_AIM, BINDINGS_ACTION_PROBE_OR_FOLD)) {
        nav->probe_or_fold_engaged = 0;
    } else if (action_pressed(nav, pressed, BINDINGS_MODE_AIM, BINDINGS_ACTION_PROBE_OR_FOLD) && !release) {
        nav->probe_or_fold_engaged = entity_under(nav->cursor_x, nav->cursor_y, presses_off_background) == NULL;
    }
    if (nav->probe_or_fold_engaged) {
        played->mouse_buttons = P8_MOUSE_LEFT;
    } else if (back_returns) {
        leave_aim_mode(nav);
    }
}

static void reset_on_new_game(pad_nav_t *nav) {
    if (!game_unseen(nav)) {
        return;
    }
    pad_nav_map_mode_t start = map_mode_now(nav);
    pad_jump_init(&nav->jump);
    pad_back_init(&nav->back);
    nav->game = entity_id_link(&main_economy_typed()->header);
    reset_map_mode(nav);
    if (start == PAD_NAV_MAP_AIM) {
        enter_aim_mode(nav);
    }
}

/* Never pause, which only pause_step closes: cleared on a release frame, the resumed carry would press the map. */
static void back_closes_the_box(void) {
    if (pad_panel_box_open() && !pause_menu_shown()) {
        ui_clear_ephemerals(main_ui());
    }
}

static void show_marker(pad_nav_t *nav, const entity_t *shown) {
    ui_t *ui = main_ui();
    if (shown == NULL) {
        nav->marker_frames = 0;
    }
    if (ui != NULL) {
        ui_set_pad_selection(ui, shown, nav->marker_frames);
    }
    if (shown != NULL) {
        nav->marker_frames = (nav->marker_frames + 1) % UI_PAD_BLINK_PERIOD;
    }
}

static int own_panel_up(const pad_nav_t *nav) {
    if (pad_cycle_panel(&nav->cycle) == PAD_CYCLE_TECH) {
        return pad_panel_box_open() && !pause_menu_shown();
    }
    if (pad_cycle_panel(&nav->cycle) == PAD_CYCLE_SCORE) {
        return pad_nav_panel_open() && !pad_panel_box_open() && pad_panel_first_item() == NULL;
    }
    return 0;
}

static int foreign_panel_open(const pad_nav_t *nav) {
    return pad_nav_panel_open() && !own_panel_up(nav);
}

/* A slipway released on this frame is judged on the next: the shoulders and pause wait for it. */
static int map_action_due(const pad_nav_t *nav) {
    return nav->map_mode == PAD_NAV_MAP_SLIPWAY && nav->slipway.phase == PAD_SLIPWAY_RELEASED;
}

static int shoulders_can_switch(const pad_nav_t *nav, int release) {
    if ((foreign_panel_open(nav) && !planet_menu_open(nav)) || release || pad_cycle_landing_due(&nav->cycle)) {
        return 0;
    }
    return !on_the_map(nav) || !map_action_due(nav);
}

typedef enum { CANCELLED_NOTHING, CANCELLED_STRUCTURE, CANCELLED_SLIPWAY } cancelled_t;

/* The caller owes a cancelled structure cancel_point, button up; a cancelled slipway is already dropped. */
static cancelled_t unwind_sub_modes(pad_nav_t *nav) {
    if (!on_the_map(nav)) {
        return CANCELLED_NOTHING;
    }
    if (planet_menu_open(nav)) {
        ui_clear_ephemerals(main_ui());
    }
    if (pad_nav_placing(nav) != NULL) {
        return CANCELLED_STRUCTURE;
    }
    return cancel_slipway(nav) ? CANCELLED_SLIPWAY : CANCELLED_NOTHING;
}

static void leave_panel(pad_nav_t *nav, p8_input_frame_t *played) {
    if (!own_panel_up(nav)) {
        return;
    }
    if (pad_cycle_panel(&nav->cycle) == PAD_CYCLE_TECH) {
        ui_clear_ephemerals(main_ui());
    } else {
        click(nav, played);
    }
}

static bindings_mode_t map_binding_mode(const pad_nav_t *nav) {
    if (pad_nav_placing(nav) != NULL) {
        return BINDINGS_MODE_PLACEMENT;
    }
    switch (map_mode_now(nav)) {
    case PAD_NAV_MAP_AIM:
        return BINDINGS_MODE_AIM;
    case PAD_NAV_MAP_SLIPWAY:
        return BINDINGS_MODE_SLIPWAY;
    default:
        return BINDINGS_MODE_JUMP;
    }
}

/* The mode the dispatch would run from the nav state at the call: decide it before anything is unwound. */
static bindings_mode_t nav_mode(const pad_nav_t *nav) {
    if (on_the_map(nav) && pad_nav_placing(nav) != NULL) {
        return BINDINGS_MODE_PLACEMENT;
    }
    if (pad_nav_panel_open()) {
        return panel_mode(nav);
    }
    if (cycle_open(nav)) {
        return BINDINGS_MODE_PANELS;
    }
    if (pad_build_is_open(&nav->build)) {
        return BINDINGS_MODE_BUILD_BAR;
    }
    return map_binding_mode(nav);
}

static bindings_mode_t back_mode(const pad_nav_t *nav) {
    if (pad_panel_box_open() && !pause_menu_shown()) {
        return cycle_open(nav) ? BINDINGS_MODE_PANELS : BINDINGS_MODE_MENU;
    }
    return map_binding_mode(nav);
}

static bindings_action_t back_action(bindings_mode_t mode) {
    switch (mode) {
    case BINDINGS_MODE_JUMP:
    case BINDINGS_MODE_AIM:
        return BINDINGS_ACTION_BACK;
    case BINDINGS_MODE_SLIPWAY:
        return BINDINGS_ACTION_CANCEL_SLIPWAY;
    case BINDINGS_MODE_PLACEMENT:
        return BINDINGS_ACTION_CANCEL_PLACEMENT;
    case BINDINGS_MODE_MENU:
        return BINDINGS_ACTION_CLOSE_MENU;
    case BINDINGS_MODE_PANELS:
        return BINDINGS_ACTION_CLOSE_PANEL;
    default:
        assert(!"back_action: back_mode returns only a map, menu or panels mode");
        return BINDINGS_ACTION_BACK;
    }
}

static bindings_action_t tech_shoulder_action(bindings_mode_t mode) {
    return mode == BINDINGS_MODE_PANELS ? BINDINGS_ACTION_NEXT_PANEL : BINDINGS_ACTION_OPEN_TECH_PANEL;
}

static int one_shoulder(const pad_nav_t *nav, int pressed, bindings_mode_t mode) {
    return action_pressed(nav, pressed, mode, tech_shoulder_action(mode)) !=
           action_pressed(nav, pressed, mode, BINDINGS_ACTION_OPEN_BUILD_BAR);
}

static void shoulder_switch(pad_nav_t *nav, int pressed, bindings_mode_t mode, p8_input_frame_t *played) {
    leave_panel(nav, played);
    if (action_pressed(nav, pressed, mode, tech_shoulder_action(mode))) {
        pad_build_close(&nav->build);
        pad_cycle_set(&nav->cycle, pad_cycle_next(pad_cycle_panel(&nav->cycle)));
    } else if (pad_build_is_open(&nav->build)) {
        pad_build_close(&nav->build);
    } else {
        pad_cycle_set(&nav->cycle, PAD_CYCLE_CLOSED);
        pad_build_open(&nav->build);
    }
}

/* Off screen, so a press finds no left_press under the cursor. */
enum { PARKED_X = -16, PARKED_Y = -16 };

enum { YEAR_SKIP_BUTTON = 1, PAUSE_BUTTON = 4 };

static entity_t *cycle_button(const pad_nav_t *nav) {
    int index = pad_cycle_hud_button(pad_cycle_panel(&nav->cycle));
    return index != 0 ? (entity_t *)galaxy_menu_find_button(index) : NULL;
}

static void build_step(pad_nav_t *nav, int pressed, int release, p8_input_frame_t *played) {
    entity_t *icon = pad_build_focus(&nav->build);
    if (icon != NULL && action_pressed(nav, pressed, BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_TAKE_STRUCTURE) && !release) {
        played->mouse_buttons = P8_MOUSE_LEFT;
        nav->placement = PAD_NAV_PLACEMENT_PRESSED;
    }
    /* The bar has no up or down: it borrows the panels' pair, which ranks before left and right. */
    if (action_pressed(nav, pressed, BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_UP) ||
        action_pressed(nav, pressed, BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_DOWN)) {
        return;
    }
    if (action_pressed(nav, pressed, BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_PICK_LEFT)) {
        pad_build_pick(&nav->build, -1);
    } else if (action_pressed(nav, pressed, BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_PICK_RIGHT)) {
        pad_build_pick(&nav->build, 1);
    }
}

static void place_cursor(pad_nav_t *nav) {
    entity_t *button = cycle_button(nav);
    entity_t *icon = pad_build_is_open(&nav->build) ? pad_build_focus(&nav->build) : NULL;
    if (on_the_map(nav)) {
        entity_t *hovered = pad_nav_hovered(nav);
        if (hovered != NULL) {
            screen_point(hovered, &nav->cursor_x, &nav->cursor_y);
        } else {
            nav->cursor_x = P8_SCREEN_WIDTH / 2;
            nav->cursor_y = P8_SCREEN_HEIGHT / 2;
        }
    } else if (button != NULL) {
        pad_panel_focus_point(button, &nav->cursor_x, &nav->cursor_y);
    } else if (icon != NULL) {
        pad_panel_focus_point(icon, &nav->cursor_x, &nav->cursor_y);
    } else {
        nav->cursor_x = PARKED_X;
        nav->cursor_y = PARKED_Y;
    }
}

static void bar_or_cycle_step(pad_nav_t *nav, int pad_buttons, int pressed, int release, p8_input_frame_t *played) {
    if (pad_nav_panel_open()) {
        panel_step(nav, pad_buttons, pressed, played);
        return;
    }
    nav->focus = ENTITY_ID_NONE;
    if (pad_build_is_open(&nav->build)) {
        build_step(nav, pressed, release, played);
    }
    place_cursor(nav);
}

static int begin_placement(pad_nav_t *nav) {
    if (nav->placement == PAD_NAV_PLACEMENT_CARRYING && pad_nav_placing(nav) == NULL) {
        nav->placement = PAD_NAV_PLACEMENT_NONE;
    }
    if (nav->placement != PAD_NAV_PLACEMENT_PRESSED) {
        return 0;
    }
    if (held_structure() == NULL) {
        nav->placement = PAD_NAV_PLACEMENT_NONE;
        return 0;
    }
    close_panels(nav);
    nav->placement = PAD_NAV_PLACEMENT_CARRYING;
    if (nav->map_mode == PAD_NAV_MAP_JUMP) {
        enter_aim_mode(nav);
    }
    return 1;
}

static void cancel_point(pad_nav_t *nav) {
    entity_t *icon = pad_build_icon(&nav->build);
    if (icon != NULL) {
        pad_panel_focus_point(icon, &nav->cursor_x, &nav->cursor_y);
    } else {
        nav->cursor_x = PARKED_X;
        nav->cursor_y = PARKED_Y;
    }
    vec_t at = { p8_fixed_from_int(nav->cursor_x), p8_fixed_from_int(nav->cursor_y) };
    assert((ui_panel_box() == NULL || !box_contains(*ui_panel_box(), at)) &&
           "cancel_point: inside the ui's panel box the drop would be judged on the map, not cancelled");
}

static void placement_step(pad_nav_t *nav, int pad_buttons, int pressed, p8_input_frame_t *played) {
    nav->focus = ENTITY_ID_NONE;
    nav->probe_or_fold_engaged = 0;
    if (action_pressed(nav, pressed, BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_CANCEL_PLACEMENT) || pad_nav_panel_open()) {
        cancel_point(nav);
        return;
    }
    nav->cursor_x = P8_SCREEN_WIDTH / 2;
    nav->cursor_y = P8_SCREEN_HEIGHT / 2;
    aim_pan(nav, BINDINGS_MODE_PLACEMENT, pad_buttons);
    /* place_structure drops by letting the held left button go for one frame: the ui's drop runs the cart's rules. */
    if (action_pressed(nav, pressed, BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PLACE_STRUCTURE)) {
        return;
    }
    played->mouse_buttons = P8_MOUSE_LEFT;
}

static void seed_hovered(pad_nav_t *nav) {
    if (pad_jump_hovered(&nav->jump) != NULL) {
        return;
    }
    entity_list_t candidates = live_candidates();
    int x, y;
    centre_world_point(&x, &y);
    (void)pad_jump_reseed(&nav->jump, &candidates, x, y);
}

static void rest_screen_point(const camera_t *camera, const entity_t *hovered, int *x, int *y) {
    vec_t from = camera_pos_before_shake(camera);
    pad_jump_world_point(hovered, x, y);
    *x -= p8_fixed_flr_int(from.x);
    *y -= p8_fixed_flr_int(from.y);
}

static void edge_scroll(const entity_t *hovered) {
    camera_t *camera = main_camera_typed();
    if (camera == NULL) {
        return;
    }
    int x, y, dx, dy;
    rest_screen_point(camera, hovered, &x, &y);
    pad_jump_scroll(x, y, &dx, &dy);
    if (dx != 0 || dy != 0) {
        camera_pan(camera, dx, dy);
    }
}

static int pressable(const entity_t *e) {
    const camera_t *camera = main_camera_typed();
    int x, y;
    if (camera == NULL) {
        return 0;
    }
    rest_screen_point(camera, e, &x, &y);
    if (!pad_jump_inside(x, y)) {
        return 0;
    }
    screen_point(e, &x, &y);
    return !pressed_before(e, x, y);
}

static int click_hovered(pad_nav_t *nav, const entity_t *hovered, p8_input_frame_t *played) {
    if (!pressable(hovered)) {
        return 0;
    }
    click(nav, played);
    nav->menu_choose_row_armed = 0;
    return 1;
}

static void press_slipway_start(pad_nav_t *nav, const entity_t *start, p8_input_frame_t *played) {
    screen_point(start, &nav->cursor_x, &nav->cursor_y);
    played->mouse_buttons = P8_MOUSE_LEFT;
}

static int act_on_hovered_starts_slipway(pad_nav_t *nav, const entity_t *hovered, p8_input_frame_t *played) {
    if (!pad_slipway_can_start(hovered) || !pressable(hovered) || nav->left_was_down) {
        return 0;
    }
    press_slipway_start(nav, hovered, played);
    nav->map_mode = PAD_NAV_MAP_SLIPWAY;
    pad_slipway_init(&nav->slipway);
    nav->slipway.start = entity_id_link(hovered);
    nav->slipway.phase = PAD_SLIPWAY_PRESSED;
    return 1;
}

static void jump_from(pad_nav_t *nav, const entity_t *hovered, pad_direction_t direction) {
    entity_list_t candidates = live_candidates();
    entity_t *target = pad_target_pick(hovered, direction, &candidates);
    if (target != NULL) {
        pad_jump_hover(&nav->jump, target);
    }
}

static void jump_step(pad_nav_t *nav, int pad_buttons, int pressed, int release, int back_returns,
                      int select_tapped, p8_input_frame_t *played, const entity_t **shown) {
    reset_aim_mode(nav);
    nav->focus = ENTITY_ID_NONE;
    seed_hovered(nav);
    if (action_pressed(nav, pressed, BINDINGS_MODE_JUMP, BINDINGS_ACTION_ENTER_AIM)) {
        enter_aim_mode(nav);
        return;
    }
    /* Before the click check: the hold-repeat count keeps running through a click's frames. */
    pad_direction_t direction =
        pad_jump_repeat_step(&nav->jump, &nav->bindings, BINDINGS_MODE_JUMP, pad_buttons, pressed);
    entity_t *hovered = pad_jump_hovered(&nav->jump);
    int clicking = release || nav->release_pending;
    entity_t *back = back_returns && !clicking ? pad_back_target(&nav->back, hovered) : NULL;
    if (back != NULL) {
        pad_jump_hover(&nav->jump, back);
        edge_scroll(back);
        *shown = back;
        screen_point(back, &nav->cursor_x, &nav->cursor_y);
        return;
    }
    if (hovered == NULL) {
        nav->cursor_x = P8_SCREEN_WIDTH / 2;
        nav->cursor_y = P8_SCREEN_HEIGHT / 2;
        return;
    }
    if (!clicking && action_pressed(nav, pressed, BINDINGS_MODE_JUMP, BINDINGS_ACTION_OPEN_MENU)) {
        clicking = click_hovered(nav, hovered, played);
    }
    if (!clicking && action_pressed(nav, pressed, BINDINGS_MODE_JUMP, BINDINGS_ACTION_ACT_ON_HOVERED)
        && hovered->state == ENTITY_STATE_KNOWN) {
        clicking = click_hovered(nav, hovered, played);
        if (clicking) {
            pad_back_record(&nav->back, hovered);
        }
    }
    if (!clicking && action_pressed(nav, pressed, BINDINGS_MODE_JUMP, BINDINGS_ACTION_ACT_ON_HOVERED)
        && act_on_hovered_starts_slipway(nav, hovered, played)) {
        pad_back_record(&nav->back, hovered);
        return;
    }
    if (!clicking && select_tapped && select_tap(nav, hovered, shown)) {
        return;
    }
    if (direction != PAD_DIRECTION_NONE && !clicking) {
        jump_from(nav, hovered, direction);
        hovered = pad_jump_hovered(&nav->jump);
    }
    if (!clicking) {
        edge_scroll(hovered);
        *shown = hovered;
    }
    screen_point(hovered, &nav->cursor_x, &nav->cursor_y);
}

/* Not pressable: after the outcome the start's screen point may lie off screen. */
static int press_start_again(pad_nav_t *nav, const entity_t *start, p8_input_frame_t *played) {
    if (!pad_slipway_can_start(start)) {
        return 0;
    }
    int x, y;
    screen_point(start, &x, &y);
    if (pressed_before(start, x, y)) {
        return 0;
    }
    press_slipway_start(nav, start, played);
    return 1;
}

static void slipway_step(pad_nav_t *nav, int pad_buttons, int pressed, int back_returns, int release,
                         p8_input_frame_t *played, const entity_t **shown) {
    reset_aim_mode(nav);
    nav->focus = ENTITY_ID_NONE;
    entity_t *start = pad_nav_slipway_start(nav);
    if (start == NULL || !planet_is_family(start)) {
        drop_held_slipway();
        back_to_jump_mode(nav, pad_nav_slipway_target(nav));
        return;
    }
    if (nav->slipway.phase == PAD_SLIPWAY_RELEASED) {
        pad_slipway_outcome_t outcome =
            pad_slipway_outcome(start, entity_get(nav->slipway.target), nav->slipway.linked_before);
        if (outcome == PAD_SLIPWAY_BUILT) {
            pad_back_record(&nav->back, entity_get(nav->slipway.target));
        }
        pad_slipway_rebase(&nav->slipway, outcome);
        start = live(entity_get(nav->slipway.start));
        if (!press_start_again(nav, start, played)) {
            back_to_jump_mode(nav, start);
        }
        return;
    }
    if (nav->slipway.phase == PAD_SLIPWAY_PRESSED) {
        if (held_slipway() == NULL) {
            back_to_jump_mode(nav, start);
            return;
        }
        nav->slipway.phase = PAD_SLIPWAY_HELD;
    }
    if (back_returns) {
        drop_held_slipway();
        back_to_jump_mode(nav, start);
        screen_point(start, &nav->cursor_x, &nav->cursor_y);
        return;
    }
    pad_direction_t direction =
        pad_jump_repeat_step(&nav->jump, &nav->bindings, BINDINGS_MODE_SLIPWAY, pad_buttons, pressed);
    entity_t *target = pad_nav_slipway_target(nav);
    nav->slipway.target = entity_id_link(target);
    int build_on_target = action_pressed(nav, pressed, BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_BUILD_ON_TARGET);
    if (build_on_target && !release && target != NULL && pressable(target)) {
        pad_slipway_release(&nav->slipway, pad_slipway_between(start, target) != NULL);
        screen_point(target, &nav->cursor_x, &nav->cursor_y);
        return;
    }
    if (direction != PAD_DIRECTION_NONE && !build_on_target) {
        entity_list_t candidates = live_candidates();
        entity_list_t targets = pad_slipway_candidates(frame_zone(), &candidates, start);
        entity_t *picked = pad_target_pick(target != NULL ? target : start, direction, &targets);
        if (picked != NULL) {
            target = picked;
            nav->slipway.target = entity_id_link(target);
        }
    }
    const entity_t *aimed = target != NULL ? target : start;
    edge_scroll(aimed);
    screen_point(aimed, &nav->cursor_x, &nav->cursor_y);
    played->mouse_buttons = P8_MOUSE_LEFT;
    *shown = aimed;
}

static void map_step(pad_nav_t *nav, int pad_buttons, int pressed, int release, int back_returns,
                     pad_select_event_t select, int year_release, p8_input_frame_t *played, const entity_t **shown,
                     int *repeat_kept) {
    if (pad_nav_placing(nav) != NULL) {
        placement_step(nav, pad_buttons, pressed, played);
    } else if (pad_nav_panel_open()) {
        reset_aim_mode(nav);
        if (action_pressed(nav, pressed, BINDINGS_MODE_MENU, BINDINGS_ACTION_OPEN_MENU) && !release &&
            !nav->release_pending && planet_menu_open(nav)) {
            ui_clear_ephemerals(main_ui());
        } else {
            panel_step(nav, pad_buttons, pressed, played);
        }
    } else if (pad_nav_aim_mode(nav)) {
        aim_mode_step(nav, pad_buttons, pressed, back_returns, release, year_release, select == PAD_SELECT_TAP, played,
                      shown);
    } else if (nav->map_mode == PAD_NAV_MAP_SLIPWAY) {
        slipway_step(nav, pad_buttons, pressed, back_returns, release, played, shown);
        *repeat_kept = 1;
    } else {
        assert(pad_nav_jump_mode(nav) &&
               "map_step: after placement, panels, aim and slipway mode, only jump mode is left");
        jump_step(nav, pad_buttons, pressed, release, back_returns, select == PAD_SELECT_TAP, played, shown);
        *repeat_kept = 1;
    }
}

static int switch_panels(pad_nav_t *nav, int pressed, int release, int foreign_was_open, cancelled_t *cancelled,
                         p8_input_frame_t *played) {
    *cancelled = CANCELLED_NOTHING;
    if (main_economy_typed() == NULL) {
        close_panels(nav);
        return 0;
    }
    bindings_mode_t mode = nav_mode(nav);
    if (one_shoulder(nav, pressed, mode) && shoulders_can_switch(nav, release)) {
        *cancelled = unwind_sub_modes(nav);
        shoulder_switch(nav, pressed, mode, played);
        return 1;
    }
    /* The bar's state, not nav_mode: a box shown over the bar would read menu's buttons. */
    int bar = pad_build_is_open(&nav->build);
    int leaving = action_pressed(nav, pressed, bar ? BINDINGS_MODE_BUILD_BAR : BINDINGS_MODE_PANELS,
                                 bar ? BINDINGS_ACTION_CLOSE_BAR : BINDINGS_ACTION_CLOSE_PANEL);
    if (!on_the_map(nav) && leaving && !foreign_was_open && !release) {
        leave_panel(nav, played);
        close_panels(nav);
        return 1;
    }
    return 0;
}

static entity_t *pause_button_free(const pad_nav_t *nav, int release) {
    if (main_economy_typed() == NULL || pad_nav_panel_open() || release || map_action_due(nav) ||
        nav->probe_or_fold_engaged || cycle_open(nav)) {
        return NULL;
    }
    return (entity_t *)galaxy_menu_find_button(PAUSE_BUTTON);
}

/* Held by the ui, the structure would be dragged onto a pressed row and dropped there. */
static void suspend_placement(pad_nav_t *nav, const entity_t *carried) {
    nav->suspended_carry = entity_id_link(carried);
    nav->placement = PAD_NAV_PLACEMENT_SUSPENDED;
    ui_set_left_held(main_ui(), NULL);
}

/* Every close of pause leaves the left button down on its frame, so the resumed carry makes no press edge. */
static int resume_placement(pad_nav_t *nav) {
    if (nav->placement != PAD_NAV_PLACEMENT_SUSPENDED) {
        return 0;
    }
    entity_t *suspended = placing_structure(entity_get(nav->suspended_carry));
    if (suspended == NULL || main_economy_typed() == NULL) {
        nav->placement = PAD_NAV_PLACEMENT_NONE;
        nav->suspended_carry = ENTITY_ID_NONE;
        return 0;
    }
    if (pause_menu_shown()) {
        return 0;
    }
    ui_set_left_held(main_ui(), suspended);
    nav->placement = PAD_NAV_PLACEMENT_CARRYING;
    nav->suspended_carry = ENTITY_ID_NONE;
    return 1;
}

static int pause_step(pad_nav_t *nav, int pressed, int release, p8_input_frame_t *played) {
    if (pause_menu_shown()) {
        if (!action_pressed(nav, pressed, BINDINGS_MODE_PAUSE, BINDINGS_ACTION_CLOSE_PAUSE) || release) {
            return 0;
        }
        nav->cursor_x = P8_SCREEN_WIDTH / 2;
        nav->cursor_y = P8_SCREEN_HEIGHT / 2;
        click(nav, played);
        return 1;
    }
    int due = nav->pause_due;
    nav->pause_due = 0;
    int opening = due || action_pressed(nav, pressed, nav_mode(nav), BINDINGS_ACTION_PAUSE);
    entity_t *button = opening ? pause_button_free(nav, release) : NULL;
    if (button == NULL) {
        return 0;
    }
    pad_panel_focus_point(button, &nav->cursor_x, &nav->cursor_y);
    entity_t *carried = pad_nav_placing(nav);
    if (carried != NULL) {
        suspend_placement(nav, carried);
    } else {
        (void)cancel_slipway(nav);
    }
    /* The opening click needs the left button up on the frame before. */
    if (nav->left_was_down && !due) {
        nav->pause_due = 1;
    } else {
        click(nav, played);
    }
    return 1;
}

static entity_t *lit_button(const pad_nav_t *nav) {
    return main_economy_typed() != NULL ? cycle_button(nav) : NULL;
}

static void finish_step(pad_nav_t *nav, int release, const entity_t *shown, p8_input_frame_t *played) {
    if (release) {
        played->mouse_buttons = 0;
    }
    nav->left_was_down = (played->mouse_buttons & P8_MOUSE_LEFT) != 0;
    played->mouse_x = nav->cursor_x;
    played->mouse_y = nav->cursor_y;
    show_marker(nav, shown);
    if (main_ui() != NULL) {
        ui_set_pad_tab(main_ui(), lit_button(nav));
        ui_set_pad_crosshair(main_ui(), pad_nav_aim_mode(nav) && pad_nav_placing(nav) == NULL);
    }
}

static int select_allowed(const pad_nav_t *nav, int panels_changing, int release) {
    return main_economy_typed() != NULL && on_the_map(nav) && !panels_changing && !pause_menu_shown() &&
           !pad_nav_panel_open() && (nav->map_mode == PAD_NAV_MAP_JUMP || nav->map_mode == PAD_NAV_MAP_AIM) &&
           nav->placement == PAD_NAV_PLACEMENT_NONE && !nav->probe_or_fold_engaged && !release && !nav->release_pending;
}

/* Read in every mode, so the latch sees a Select held from elsewhere. Jump's and aim's buttons both count in either. */
static int select_held(const pad_nav_t *nav, int pad_buttons) {
    return action_held(nav, pad_buttons, BINDINGS_MODE_JUMP, BINDINGS_ACTION_SELECT_TAP_OR_YEAR) ||
           action_held(nav, pad_buttons, BINDINGS_MODE_AIM, BINDINGS_ACTION_SELECT_TAP_OR_YEAR);
}

static int select_ends_year(pad_nav_t *nav, p8_input_frame_t *played) {
    entity_t *button = (entity_t *)galaxy_menu_find_button(YEAR_SKIP_BUTTON);
    if (button == NULL || nav->left_was_down) {
        return 0;
    }
    pad_panel_focus_point(button, &nav->cursor_x, &nav->cursor_y);
    click(nav, played);
    pad_select_year_ended(&nav->select);
    return 1;
}

static int without_chord(pad_nav_t *nav, int raw_buttons) {
    const int face = cheat_chord_buttons();
    if ((raw_buttons & ~nav->previous_pad & face) == face) {
        nav->chord_held = 1;
    } else if (!(raw_buttons & face)) {
        nav->chord_held = 0;
    }
    return nav->chord_held ? raw_buttons & ~face : raw_buttons;
}

static void run_step(pad_nav_t *nav, int raw_buttons, p8_input_frame_t *played) {
    int pad_buttons = without_chord(nav, raw_buttons);
    int pressed = pad_buttons & ~nav->previous_pad;
    int release = nav->release_pending;
    nav->previous_pad = raw_buttons;
    nav->release_pending = 0;
    played->mouse_buttons = 0;
    const entity_t *shown = NULL;
    if (!pause_menu_shown()) {
        nav->pause_choose_row_armed = 0;
    }
    if (!armed_panel_open(nav)) {
        nav->menu_choose_row_armed = 0;
    }
    reset_on_new_game(nav);
    int panels_changed = begin_placement(nav);
    if (resume_placement(nav)) {
        release = 0;
    }
    int select_down = select_held(nav, pad_buttons);
    if (!panels_changed && pause_step(nav, pressed, release, played)) {
        (void)pad_select_step(&nav->select, select_down, 0);
        pad_jump_repeat_clear(&nav->jump);
        reset_aim_mode(nav);
        nav->focus = ENTITY_ID_NONE;
        finish_step(nav, release, NULL, played);
        return;
    }
    int foreign_was_open = foreign_panel_open(nav);
    bindings_mode_t back_in = back_mode(nav);
    int back = bindings_mask(&nav->bindings, back_in, back_action(back_in));
    int back_returns = (pressed & back) && !pad_panel_box_open();
    if (pressed & back) {
        back_closes_the_box();
        if (back_returns && on_the_map(nav) && shown_message_box() != NULL) {
            message_box_close_shown();
            back_returns = 0;
            pressed &= ~back;
        }
    }
    if (pad_nav_panel_open()) {
        (void)cancel_slipway(nav);
    }
    cancelled_t cancelled = CANCELLED_NOTHING;
    int repeat_kept = 0;
    if (!panels_changed) {
        panels_changed = switch_panels(nav, pressed, release, foreign_was_open, &cancelled, played);
    }
    /* A leaving click and a landing click never share a frame: the landing waits for the release. */
    int clicks_due = release || nav->release_pending || cancelled;
    int landed = pad_cycle_landing_due(&nav->cycle) && !clicks_due;
    if (landed) {
        pad_cycle_landed(&nav->cycle);
        if (cycle_button(nav) != NULL) {
            click(nav, played);
        }
    } else if (!panels_changed && cycle_open(nav) && !pad_cycle_landing_due(&nav->cycle) && !clicks_due &&
               !own_panel_up(nav)) {
        close_panels(nav);
        panels_changed = 1;
    }
    int year_release = pad_select_year_just_ended(&nav->select);
    pad_select_event_t select =
        pad_select_step(&nav->select, select_down, select_allowed(nav, panels_changed || landed, release));
    /* The frame that opens or closes the bar or a cycle panel runs no other step, so one press never acts twice. */
    if (panels_changed || landed) {
        reset_aim_mode(nav);
        nav->focus = ENTITY_ID_NONE;
        place_cursor(nav);
        if (cancelled == CANCELLED_STRUCTURE) {
            cancel_point(nav);
        } else if (pad_nav_placing(nav) != NULL) {
            played->mouse_buttons = P8_MOUSE_LEFT;
        }
    } else if (select == PAD_SELECT_YEAR && select_ends_year(nav, played)) {
        reset_aim_mode(nav);
        nav->focus = ENTITY_ID_NONE;
    } else if (on_the_map(nav)) {
        map_step(nav, pad_buttons, pressed, release, back_returns, select, year_release, played, &shown, &repeat_kept);
    } else {
        reset_aim_mode(nav);
        bar_or_cycle_step(nav, pad_buttons, pressed, release, played);
    }
    if (!repeat_kept) {
        pad_jump_repeat_clear(&nav->jump);
    }
    finish_step(nav, release, shown, played);
}

void pad_nav_step(pad_nav_t *nav, int raw_buttons, p8_input_frame_t *played) {
    pad_panel_step_begin();
    run_step(nav, raw_buttons, played);
    pad_panel_step_end();
}
