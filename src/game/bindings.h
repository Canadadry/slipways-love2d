#ifndef GAME_BINDINGS_H
#define GAME_BINDINGS_H

typedef enum {
    BINDINGS_MODE_TITLE,
    BINDINGS_MODE_JUMP,
    BINDINGS_MODE_AIM,
    BINDINGS_MODE_SLIPWAY,
    BINDINGS_MODE_MENU,
    BINDINGS_MODE_PAUSE,
    BINDINGS_MODE_BUILD_BAR,
    BINDINGS_MODE_PLACEMENT,
    BINDINGS_MODE_PANELS,
    BINDINGS_MODE_COUNT
} bindings_mode_t;

typedef enum {
    BINDINGS_ACTION_MOVE_UP,
    BINDINGS_ACTION_MOVE_DOWN,
    BINDINGS_ACTION_MOVE_LEFT,
    BINDINGS_ACTION_MOVE_RIGHT,
    BINDINGS_ACTION_CHOOSE_ROW,
    BINDINGS_ACTION_JUMP_UP,
    BINDINGS_ACTION_JUMP_DOWN,
    BINDINGS_ACTION_JUMP_LEFT,
    BINDINGS_ACTION_JUMP_RIGHT,
    BINDINGS_ACTION_ACT_ON_HOVERED,
    BINDINGS_ACTION_BACK,
    BINDINGS_ACTION_ENTER_AIM,
    BINDINGS_ACTION_OPEN_MENU,
    BINDINGS_ACTION_OPEN_BUILD_BAR,
    BINDINGS_ACTION_OPEN_TECH_PANEL,
    BINDINGS_ACTION_PAUSE,
    BINDINGS_ACTION_SELECT_TAP_OR_YEAR,
    BINDINGS_ACTION_PAN_UP,
    BINDINGS_ACTION_PAN_DOWN,
    BINDINGS_ACTION_PAN_LEFT,
    BINDINGS_ACTION_PAN_RIGHT,
    BINDINGS_ACTION_PROBE_OR_FOLD,
    BINDINGS_ACTION_LEAVE_AIM,
    BINDINGS_ACTION_BUILD_ON_TARGET,
    BINDINGS_ACTION_CANCEL_SLIPWAY,
    BINDINGS_ACTION_CLOSE_MENU,
    BINDINGS_ACTION_CLOSE_PAUSE,
    BINDINGS_ACTION_PICK_LEFT,
    BINDINGS_ACTION_PICK_RIGHT,
    BINDINGS_ACTION_TAKE_STRUCTURE,
    BINDINGS_ACTION_CLOSE_BAR,
    BINDINGS_ACTION_PLACE_STRUCTURE,
    BINDINGS_ACTION_CANCEL_PLACEMENT,
    BINDINGS_ACTION_CLOSE_PANEL,
    BINDINGS_ACTION_NEXT_PANEL,
    BINDINGS_ACTION_COUNT
} bindings_action_t;

typedef struct {
    int masks[BINDINGS_MODE_COUNT][BINDINGS_ACTION_COUNT];
} bindings_t;

extern const bindings_t bindings_compiled;

extern const int bindings_compiled_personal;

extern const bindings_t bindings_default;

typedef struct {
    int line;
    char text[256];
} bindings_error_t;

int bindings_read_button_number(const char *text, const char **end);

int bindings_mask(const bindings_t *bindings, bindings_mode_t mode, bindings_action_t action);

int bindings_mode_has_action(bindings_mode_t mode, bindings_action_t action);

const char *bindings_mode_name(bindings_mode_t mode);
const char *bindings_action_name(bindings_action_t action);

int bindings_parse(const char *file_name, const char *text, bindings_t *out, bindings_error_t *error);

#endif
