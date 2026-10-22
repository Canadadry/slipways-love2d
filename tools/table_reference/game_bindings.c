#include "game/bindings.c"
#include "table_reference.h"

#define BINDINGS_CONSTANTS(X) \
    X(BINDINGS_MODE_TITLE) \
    X(BINDINGS_MODE_JUMP) \
    X(BINDINGS_MODE_AIM) \
    X(BINDINGS_MODE_SLIPWAY) \
    X(BINDINGS_MODE_MENU) \
    X(BINDINGS_MODE_PAUSE) \
    X(BINDINGS_MODE_BUILD_BAR) \
    X(BINDINGS_MODE_PLACEMENT) \
    X(BINDINGS_MODE_PANELS) \
    X(BINDINGS_MODE_COUNT) \
    X(BINDINGS_ACTION_MOVE_UP) \
    X(BINDINGS_ACTION_MOVE_DOWN) \
    X(BINDINGS_ACTION_MOVE_LEFT) \
    X(BINDINGS_ACTION_MOVE_RIGHT) \
    X(BINDINGS_ACTION_CHOOSE_ROW) \
    X(BINDINGS_ACTION_JUMP_UP) \
    X(BINDINGS_ACTION_JUMP_DOWN) \
    X(BINDINGS_ACTION_JUMP_LEFT) \
    X(BINDINGS_ACTION_JUMP_RIGHT) \
    X(BINDINGS_ACTION_ACT_ON_HOVERED) \
    X(BINDINGS_ACTION_BACK) \
    X(BINDINGS_ACTION_ENTER_AIM) \
    X(BINDINGS_ACTION_OPEN_MENU) \
    X(BINDINGS_ACTION_OPEN_BUILD_BAR) \
    X(BINDINGS_ACTION_OPEN_TECH_PANEL) \
    X(BINDINGS_ACTION_PAUSE) \
    X(BINDINGS_ACTION_SELECT_TAP_OR_YEAR) \
    X(BINDINGS_ACTION_PAN_UP) \
    X(BINDINGS_ACTION_PAN_DOWN) \
    X(BINDINGS_ACTION_PAN_LEFT) \
    X(BINDINGS_ACTION_PAN_RIGHT) \
    X(BINDINGS_ACTION_PROBE_OR_FOLD) \
    X(BINDINGS_ACTION_LEAVE_AIM) \
    X(BINDINGS_ACTION_BUILD_ON_TARGET) \
    X(BINDINGS_ACTION_CANCEL_SLIPWAY) \
    X(BINDINGS_ACTION_CLOSE_MENU) \
    X(BINDINGS_ACTION_CLOSE_PAUSE) \
    X(BINDINGS_ACTION_PICK_LEFT) \
    X(BINDINGS_ACTION_PICK_RIGHT) \
    X(BINDINGS_ACTION_TAKE_STRUCTURE) \
    X(BINDINGS_ACTION_CLOSE_BAR) \
    X(BINDINGS_ACTION_PLACE_STRUCTURE) \
    X(BINDINGS_ACTION_CANCEL_PLACEMENT) \
    X(BINDINGS_ACTION_CLOSE_PANEL) \
    X(BINDINGS_ACTION_NEXT_PANEL) \
    X(BINDINGS_ACTION_COUNT)

#define RENDER_CONSTANT(name) table_reference_int(module, name, #name);

void table_reference_game_bindings(void) {
    const char *module = "game.bindings";
    BINDINGS_CONSTANTS(RENDER_CONSTANT)
    table_reference_int(module, (long long)sizeof(((bindings_error_t *)0)->text), "SIZEOF[\"bindings_error_t.text\"]");
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        table_reference_string(module, mode_names[mode], "mode_names[%d]", mode);
    }
    for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
        table_reference_string(module, action_names[action], "action_names[%d]", action);
    }
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            table_reference_int(module, mode_actions[mode][action], "mode_actions[%d][%d]", mode, action);
        }
    }
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            table_reference_int("game.bindings_default", bindings_default.masks[mode][action],
                                "bindings_default.masks[%d][%d]", mode, action);
        }
    }
}
