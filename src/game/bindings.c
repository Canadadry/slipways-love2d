#include "game/bindings.h"

#include "p8/input.h"

#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define BINDINGS_MAX_LINE_LENGTH 127

static const char mode_names[BINDINGS_MODE_COUNT][12] = {
    [BINDINGS_MODE_TITLE] = "title",
    [BINDINGS_MODE_JUMP] = "jump",
    [BINDINGS_MODE_AIM] = "aim",
    [BINDINGS_MODE_SLIPWAY] = "slipway",
    [BINDINGS_MODE_MENU] = "menu",
    [BINDINGS_MODE_PAUSE] = "pause",
    [BINDINGS_MODE_BUILD_BAR] = "build_bar",
    [BINDINGS_MODE_PLACEMENT] = "placement",
    [BINDINGS_MODE_PANELS] = "panels",
};

static const char action_names[BINDINGS_ACTION_COUNT][20] = {
    [BINDINGS_ACTION_MOVE_UP] = "move_up",
    [BINDINGS_ACTION_MOVE_DOWN] = "move_down",
    [BINDINGS_ACTION_MOVE_LEFT] = "move_left",
    [BINDINGS_ACTION_MOVE_RIGHT] = "move_right",
    [BINDINGS_ACTION_CHOOSE_ROW] = "choose_row",
    [BINDINGS_ACTION_JUMP_UP] = "jump_up",
    [BINDINGS_ACTION_JUMP_DOWN] = "jump_down",
    [BINDINGS_ACTION_JUMP_LEFT] = "jump_left",
    [BINDINGS_ACTION_JUMP_RIGHT] = "jump_right",
    [BINDINGS_ACTION_ACT_ON_HOVERED] = "act_on_hovered",
    [BINDINGS_ACTION_BACK] = "back",
    [BINDINGS_ACTION_ENTER_AIM] = "enter_aim",
    [BINDINGS_ACTION_OPEN_MENU] = "open_menu",
    [BINDINGS_ACTION_OPEN_BUILD_BAR] = "open_build_bar",
    [BINDINGS_ACTION_OPEN_TECH_PANEL] = "open_tech_panel",
    [BINDINGS_ACTION_PAUSE] = "pause",
    [BINDINGS_ACTION_SELECT_TAP_OR_YEAR] = "select_tap_or_year",
    [BINDINGS_ACTION_PAN_UP] = "pan_up",
    [BINDINGS_ACTION_PAN_DOWN] = "pan_down",
    [BINDINGS_ACTION_PAN_LEFT] = "pan_left",
    [BINDINGS_ACTION_PAN_RIGHT] = "pan_right",
    [BINDINGS_ACTION_PROBE_OR_FOLD] = "probe_or_fold",
    [BINDINGS_ACTION_LEAVE_AIM] = "leave_aim",
    [BINDINGS_ACTION_BUILD_ON_TARGET] = "build_on_target",
    [BINDINGS_ACTION_CANCEL_SLIPWAY] = "cancel_slipway",
    [BINDINGS_ACTION_CLOSE_MENU] = "close_menu",
    [BINDINGS_ACTION_CLOSE_PAUSE] = "close_pause",
    [BINDINGS_ACTION_PICK_LEFT] = "pick_left",
    [BINDINGS_ACTION_PICK_RIGHT] = "pick_right",
    [BINDINGS_ACTION_TAKE_STRUCTURE] = "take_structure",
    [BINDINGS_ACTION_CLOSE_BAR] = "close_bar",
    [BINDINGS_ACTION_PLACE_STRUCTURE] = "place_structure",
    [BINDINGS_ACTION_CANCEL_PLACEMENT] = "cancel_placement",
    [BINDINGS_ACTION_CLOSE_PANEL] = "close_panel",
    [BINDINGS_ACTION_NEXT_PANEL] = "next_panel",
};

#define MENU_ROWS                                                                                                   \
    [BINDINGS_ACTION_MOVE_UP] = 1, [BINDINGS_ACTION_MOVE_DOWN] = 1, [BINDINGS_ACTION_MOVE_LEFT] = 1,              \
    [BINDINGS_ACTION_MOVE_RIGHT] = 1, [BINDINGS_ACTION_CHOOSE_ROW] = 1
#define JUMPS                                                                                                       \
    [BINDINGS_ACTION_JUMP_UP] = 1, [BINDINGS_ACTION_JUMP_DOWN] = 1, [BINDINGS_ACTION_JUMP_LEFT] = 1,              \
    [BINDINGS_ACTION_JUMP_RIGHT] = 1
#define PANS                                                                                                        \
    [BINDINGS_ACTION_PAN_UP] = 1, [BINDINGS_ACTION_PAN_DOWN] = 1, [BINDINGS_ACTION_PAN_LEFT] = 1,                 \
    [BINDINGS_ACTION_PAN_RIGHT] = 1
#define MAP_SHORTCUTS                                                                                               \
    [BINDINGS_ACTION_OPEN_BUILD_BAR] = 1, [BINDINGS_ACTION_OPEN_TECH_PANEL] = 1, [BINDINGS_ACTION_PAUSE] = 1

static const unsigned char mode_actions[BINDINGS_MODE_COUNT][BINDINGS_ACTION_COUNT] = {
    [BINDINGS_MODE_TITLE] = { MENU_ROWS },
    [BINDINGS_MODE_JUMP] = { JUMPS, MAP_SHORTCUTS, [BINDINGS_ACTION_ACT_ON_HOVERED] = 1, [BINDINGS_ACTION_BACK] = 1,
                             [BINDINGS_ACTION_ENTER_AIM] = 1, [BINDINGS_ACTION_OPEN_MENU] = 1,
                             [BINDINGS_ACTION_SELECT_TAP_OR_YEAR] = 1 },
    [BINDINGS_MODE_AIM] = { PANS, MAP_SHORTCUTS, [BINDINGS_ACTION_PROBE_OR_FOLD] = 1, [BINDINGS_ACTION_BACK] = 1,
                            [BINDINGS_ACTION_LEAVE_AIM] = 1, [BINDINGS_ACTION_SELECT_TAP_OR_YEAR] = 1 },
    [BINDINGS_MODE_SLIPWAY] = { JUMPS, MAP_SHORTCUTS, [BINDINGS_ACTION_BUILD_ON_TARGET] = 1,
                                [BINDINGS_ACTION_CANCEL_SLIPWAY] = 1 },
    [BINDINGS_MODE_MENU] = { MENU_ROWS, [BINDINGS_ACTION_CLOSE_MENU] = 1, [BINDINGS_ACTION_OPEN_MENU] = 1,
                             [BINDINGS_ACTION_OPEN_BUILD_BAR] = 1, [BINDINGS_ACTION_OPEN_TECH_PANEL] = 1 },
    [BINDINGS_MODE_PAUSE] = { MENU_ROWS, [BINDINGS_ACTION_CLOSE_PAUSE] = 1 },
    [BINDINGS_MODE_BUILD_BAR] = { MAP_SHORTCUTS, [BINDINGS_ACTION_PICK_LEFT] = 1, [BINDINGS_ACTION_PICK_RIGHT] = 1,
                                  [BINDINGS_ACTION_TAKE_STRUCTURE] = 1, [BINDINGS_ACTION_CLOSE_BAR] = 1 },
    [BINDINGS_MODE_PLACEMENT] = { PANS, MAP_SHORTCUTS, [BINDINGS_ACTION_PLACE_STRUCTURE] = 1,
                                  [BINDINGS_ACTION_CANCEL_PLACEMENT] = 1 },
    [BINDINGS_MODE_PANELS] = { MENU_ROWS, [BINDINGS_ACTION_CLOSE_PANEL] = 1, [BINDINGS_ACTION_OPEN_BUILD_BAR] = 1,
                               [BINDINGS_ACTION_NEXT_PANEL] = 1 },
};

int bindings_read_button_number(const char *text, const char **end) {
    assert(text != NULL && "bindings_read_button_number needs a text");
    assert(end != NULL && "bindings_read_button_number needs somewhere to say where the digits stop");
    *end = text;
    if (*text < '1' || *text > '9') {
        return 0;
    }
    const char *cursor = text;
    int number = 0;
    while (*cursor >= '0' && *cursor <= '9') {
        if (number <= P8_PAD_BUTTON_COUNT) {
            number = number * 10 + (*cursor - '0');
        }
        cursor++;
    }
    if (number > P8_PAD_BUTTON_COUNT) {
        return 0;
    }
    *end = cursor;
    return number;
}

int bindings_mode_has_action(bindings_mode_t mode, bindings_action_t action) {
    assert(mode >= 0 && mode < BINDINGS_MODE_COUNT && "bindings_mode_has_action: mode out of range");
    assert(action >= 0 && action < BINDINGS_ACTION_COUNT && "bindings_mode_has_action: action out of range");
    return mode_actions[mode][action];
}

int bindings_mask(const bindings_t *bindings, bindings_mode_t mode, bindings_action_t action) {
    assert(bindings != NULL && "bindings_mask needs a table");
    if (!bindings_mode_has_action(mode, action)) {
        return 0;
    }
    return bindings->masks[mode][action];
}

const char *bindings_mode_name(bindings_mode_t mode) {
    assert(mode >= 0 && mode < BINDINGS_MODE_COUNT && "bindings_mode_name: mode out of range");
    return mode_names[mode];
}

const char *bindings_action_name(bindings_action_t action) {
    assert(action >= 0 && action < BINDINGS_ACTION_COUNT && "bindings_action_name: action out of range");
    return action_names[action];
}

static int fail(bindings_error_t *error, const char *file_name, int line_number, const char *format, ...) {
    error->line = line_number;
    int prefix = snprintf(error->text, sizeof error->text, "%s:%d: ", file_name, line_number);
    if (prefix < 0 || (size_t)prefix >= sizeof error->text) {
        return 0;
    }
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(error->text + prefix, sizeof error->text - (size_t)prefix, format, arguments);
    va_end(arguments);
    return 0;
}

static const char *skip_blanks(const char *start, const char *end) {
    while (start < end && (*start == ' ' || *start == '\t')) {
        start++;
    }
    return start;
}

static const char *trim_end(const char *start, const char *end) {
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) {
        end--;
    }
    return end;
}

static void copy_token(char *destination, const char *start, const char *end) {
    size_t length = (size_t)(end - start);
    memcpy(destination, start, length);
    destination[length] = '\0';
}

static int find_mode(const char *name) {
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        if (strcmp(mode_names[mode], name) == 0) {
            return mode;
        }
    }
    return -1;
}

static int find_action(const char *name) {
    for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
        if (strcmp(action_names[action], name) == 0) {
            return action;
        }
    }
    return -1;
}

static int mode_mask(const bindings_t *bindings, int mode) {
    int mask = 0;
    for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
        mask |= bindings->masks[mode][action];
    }
    return mask;
}

static int parse_line(const char *file_name, int line_number, const char *start, const char *end, bindings_t *out,
                      int given_on_line[BINDINGS_MODE_COUNT][BINDINGS_ACTION_COUNT], bindings_error_t *error) {
    char token[BINDINGS_MAX_LINE_LENGTH + 1];
    const char *equal = memchr(start, '=', (size_t)(end - start));
    if (equal == NULL) {
        copy_token(token, start, end);
        return fail(error, file_name, line_number, "no '=' in \"%s\"", token);
    }
    const char *left_end = trim_end(start, equal);
    const char *dot = memchr(start, '.', (size_t)(left_end - start));
    if (dot == NULL) {
        copy_token(token, start, left_end);
        return fail(error, file_name, line_number, "expected mode.action, got \"%s\"", token);
    }
    copy_token(token, start, trim_end(start, dot));
    int mode = find_mode(token);
    if (mode < 0) {
        return fail(error, file_name, line_number, "unknown mode \"%s\"", token);
    }
    const char *action_start = skip_blanks(dot + 1, left_end);
    copy_token(token, action_start, left_end);
    int action = find_action(token);
    if (action < 0 || !mode_actions[mode][action]) {
        return fail(error, file_name, line_number, "unknown action \"%s\" in mode %s", token, mode_names[mode]);
    }
    int first_line = given_on_line[mode][action];
    if (first_line != 0) {
        return fail(error, file_name, line_number, "%s.%s already given on line %d", mode_names[mode],
                    action_names[action], first_line);
    }
    given_on_line[mode][action] = line_number;
    int bound_in_mode = mode_mask(out, mode);
    int line_mask = 0;
    const char *item = equal + 1;
    for (;;) {
        const char *comma = memchr(item, ',', (size_t)(end - item));
        const char *item_end = comma != NULL ? comma : end;
        const char *item_start = skip_blanks(item, item_end);
        copy_token(token, item_start, trim_end(item_start, item_end));
        if (token[0] == '\0') {
            return fail(error, file_name, line_number, "empty button in the line of %s.%s", mode_names[mode],
                        action_names[action]);
        }
        const char *digits_end = NULL;
        int number = bindings_read_button_number(token, &digits_end);
        if (number == 0 || *digits_end != '\0') {
            return fail(error, file_name, line_number, "bad button \"%s\": expected a number from 1 to %d", token,
                        P8_PAD_BUTTON_COUNT);
        }
        int bit = 1 << (number - 1);
        if (line_mask & bit) {
            return fail(error, file_name, line_number, "button %d listed twice for %s.%s", number, mode_names[mode],
                        action_names[action]);
        }
        if (bound_in_mode & bit) {
            return fail(error, file_name, line_number, "button %d already bound in mode %s", number, mode_names[mode]);
        }
        line_mask |= bit;
        if (comma == NULL) {
            break;
        }
        item = comma + 1;
    }
    out->masks[mode][action] = line_mask;
    return 1;
}

int bindings_parse(const char *file_name, const char *text, bindings_t *out, bindings_error_t *error) {
    assert(file_name != NULL && text != NULL && out != NULL && error != NULL && "bindings_parse: NULL argument");
    memset(out, 0, sizeof *out);
    memset(error, 0, sizeof *error);
    int given_on_line[BINDINGS_MODE_COUNT][BINDINGS_ACTION_COUNT] = { { 0 } };
    int line_number = 0;
    const char *line = text;
    while (*line != '\0') {
        line_number++;
        const char *newline = strchr(line, '\n');
        const char *end = newline != NULL ? newline : line + strlen(line);
        const char *next = newline != NULL ? newline + 1 : end;
        if (end > line && end[-1] == '\r') {
            end--;
        }
        /* Not (size_t)(end - line) inline: GCC -O2 with the sanitizers then sees a negative bound and fails
         * the sanitize-net build with -Wstringop-overread. */
        size_t length = end > line ? (size_t)(end - line) : 0;
        if (length > BINDINGS_MAX_LINE_LENGTH) {
            return fail(error, file_name, line_number, "line longer than %d characters", BINDINGS_MAX_LINE_LENGTH);
        }
        const char *hash = memchr(line, '#', length);
        const char *content_end = hash != NULL ? hash : end;
        const char *content_start = skip_blanks(line, content_end);
        content_end = trim_end(content_start, content_end);
        if (content_start < content_end &&
            !parse_line(file_name, line_number, content_start, content_end, out, given_on_line, error)) {
            return 0;
        }
        line = next;
    }
    return 1;
}
