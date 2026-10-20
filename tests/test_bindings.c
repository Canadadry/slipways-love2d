#include "game/bindings.h"

#include "p8/input.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_reader_accepts_every_button_number(void) {
    char text[4];
    for (int number = 1; number <= 12; number++) {
        snprintf(text, sizeof text, "%d", number);
        const char *end = NULL;
        assert(bindings_read_button_number(text, &end) == number);
        assert(end == text + strlen(text));
    }
}

static void assert_reads(const char *text, int number, int digits) {
    const char *end = NULL;
    assert(bindings_read_button_number(text, &end) == number);
    assert(end == text + digits);
}

static void test_reader_leaves_what_follows_the_digits_to_the_caller(void) {
    assert_reads("5,6", 5, 1);
    assert_reads("12]", 12, 2);
    assert_reads("5+6", 5, 1);
    assert_reads("1x", 1, 1);
}

static void assert_refused(const char *text) {
    const char *end = NULL;
    assert(bindings_read_button_number(text, &end) == 0);
    assert(end == text);
}

static void test_reader_refuses_anything_but_a_number_from_1_to_12(void) {
    assert_refused("13");
    assert_refused("0");
    assert_refused("05");
    assert_refused("012");
    assert_refused("");
    assert_refused("+5");
    assert_refused("-1");
    assert_refused(" 5");
    assert_refused("cross");
    assert_refused("123");
    assert_refused("12345678901234567890");
}

typedef char button_count_is_12[P8_PAD_BUTTON_COUNT == 12 ? 1 : -1];
typedef char button_count_ends_at_button_12[(1 << (P8_PAD_BUTTON_COUNT - 1)) == P8_PAD_BUTTON_12 ? 1 : -1];

static bindings_t parse_ok(const char *text) {
    bindings_t bindings;
    bindings_error_t error;
    int parsed = bindings_parse("tests/e2e/swap.cfg", text, &bindings, &error);
    if (!parsed) {
        fprintf(stderr, "unexpected error: %s\n", error.text);
    }
    assert(parsed);
    return bindings;
}

static void test_one_line_binds_one_action(void) {
    bindings_t bindings = parse_ok("jump.back = 6\n");
    assert(bindings_mask(&bindings, BINDINGS_MODE_JUMP, BINDINGS_ACTION_BACK) == P8_PAD_BUTTON_6);
}

static void assert_all_unbound(const bindings_t *bindings) {
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            assert(bindings_mask(bindings, (bindings_mode_t)mode, (bindings_action_t)action) == 0);
        }
    }
}

static void test_empty_and_comment_only_texts_leave_everything_unbound(void) {
    bindings_t empty = parse_ok("");
    assert_all_unbound(&empty);
    bindings_t comments = parse_ok("# nothing bound\n\n   # still nothing\n");
    assert_all_unbound(&comments);
}

static void test_several_buttons_on_one_action_are_ored(void) {
    bindings_t bindings = parse_ok("pause.close_pause = 6, 11\n");
    assert(bindings_mask(&bindings, BINDINGS_MODE_PAUSE, BINDINGS_ACTION_CLOSE_PAUSE) ==
           (P8_PAD_BUTTON_6 | P8_PAD_BUTTON_11));
}

static void test_blanks_comments_crlf_and_no_final_newline_are_accepted(void) {
    bindings_t bindings = parse_ok("  \tjump . back\t=  6 ,7 # cancel\r\n\r\naim.leave_aim=7");
    assert(bindings_mask(&bindings, BINDINGS_MODE_JUMP, BINDINGS_ACTION_BACK) == (P8_PAD_BUTTON_6 | P8_PAD_BUTTON_7));
    assert(bindings_mask(&bindings, BINDINGS_MODE_AIM, BINDINGS_ACTION_LEAVE_AIM) == P8_PAD_BUTTON_7);
}

static void test_an_action_without_a_line_is_unbound(void) {
    bindings_t bindings = parse_ok("jump.back = 6\n");
    assert(bindings_mask(&bindings, BINDINGS_MODE_JUMP, BINDINGS_ACTION_ENTER_AIM) == 0);
    assert(bindings_mask(&bindings, BINDINGS_MODE_JUMP, BINDINGS_ACTION_BACK) == 32);
}

static void test_the_same_button_in_two_modes_is_accepted(void) {
    bindings_t bindings = parse_ok("jump.back = 6\naim.back = 6\n");
    assert(bindings_mask(&bindings, BINDINGS_MODE_JUMP, BINDINGS_ACTION_BACK) == P8_PAD_BUTTON_6);
    assert(bindings_mask(&bindings, BINDINGS_MODE_AIM, BINDINGS_ACTION_BACK) == P8_PAD_BUTTON_6);
}

static void test_an_action_the_mode_does_not_have_is_unbound(void) {
    bindings_t bindings = parse_ok("jump.pause = 11\n");
    assert(bindings_mask(&bindings, BINDINGS_MODE_TITLE, BINDINGS_ACTION_PAUSE) == 0);
    assert(bindings_mask(&bindings, BINDINGS_MODE_PAUSE, BINDINGS_ACTION_PAUSE) == 0);
    assert(!bindings_mode_has_action(BINDINGS_MODE_TITLE, BINDINGS_ACTION_PAUSE));
    assert(bindings_mode_has_action(BINDINGS_MODE_JUMP, BINDINGS_ACTION_PAUSE));
}

typedef struct {
    bindings_mode_t mode;
    bindings_action_t action;
    int mask;
} expected_binding_t;

static const expected_binding_t full_table_masks[] = {
    { BINDINGS_MODE_TITLE, BINDINGS_ACTION_MOVE_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_TITLE, BINDINGS_ACTION_MOVE_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_TITLE, BINDINGS_ACTION_MOVE_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_TITLE, BINDINGS_ACTION_MOVE_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_TITLE, BINDINGS_ACTION_CHOOSE_ROW, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_JUMP_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_JUMP_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_JUMP_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_JUMP_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_ACT_ON_HOVERED, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_BACK, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_ENTER_AIM, P8_PAD_BUTTON_7 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_OPEN_MENU, P8_PAD_BUTTON_8 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_PAUSE, P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_JUMP, BINDINGS_ACTION_SELECT_TAP_OR_YEAR, P8_PAD_BUTTON_12 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PAN_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PAN_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PAN_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PAN_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PROBE_OR_FOLD, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_BACK, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_LEAVE_AIM, P8_PAD_BUTTON_7 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_PAUSE, P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_AIM, BINDINGS_ACTION_SELECT_TAP_OR_YEAR, P8_PAD_BUTTON_12 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_JUMP_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_JUMP_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_JUMP_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_JUMP_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_BUILD_ON_TARGET, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_CANCEL_SLIPWAY, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_SLIPWAY, BINDINGS_ACTION_PAUSE, P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_MOVE_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_MOVE_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_MOVE_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_MOVE_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_CHOOSE_ROW, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_CLOSE_MENU, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_OPEN_MENU, P8_PAD_BUTTON_8 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_MENU, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_MOVE_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_MOVE_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_MOVE_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_MOVE_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_CHOOSE_ROW, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_PAUSE, BINDINGS_ACTION_CLOSE_PAUSE, P8_PAD_BUTTON_6 | P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_PICK_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_PICK_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_TAKE_STRUCTURE, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_CLOSE_BAR, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_BUILD_BAR, BINDINGS_ACTION_PAUSE, P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PAN_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PAN_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PAN_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PAN_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PLACE_STRUCTURE, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_CANCEL_PLACEMENT, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_OPEN_TECH_PANEL, P8_PAD_BUTTON_10 },
    { BINDINGS_MODE_PLACEMENT, BINDINGS_ACTION_PAUSE, P8_PAD_BUTTON_11 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_UP, P8_PAD_BUTTON_1 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_DOWN, P8_PAD_BUTTON_2 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_LEFT, P8_PAD_BUTTON_3 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_MOVE_RIGHT, P8_PAD_BUTTON_4 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_CHOOSE_ROW, P8_PAD_BUTTON_5 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_CLOSE_PANEL, P8_PAD_BUTTON_6 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_OPEN_BUILD_BAR, P8_PAD_BUTTON_9 },
    { BINDINGS_MODE_PANELS, BINDINGS_ACTION_NEXT_PANEL, P8_PAD_BUTTON_10 },
};

static int expected_mask(int mode, int action) {
    for (size_t i = 0; i < sizeof full_table_masks / sizeof full_table_masks[0]; i++) {
        if ((int)full_table_masks[i].mode == mode && (int)full_table_masks[i].action == action) {
            return full_table_masks[i].mask;
        }
    }
    return 0;
}

static bindings_t parse_default_file(void) {
    static char text[65536];
    FILE *file = fopen("bindings.default.cfg", "rb");
    assert(file != NULL);
    size_t length = fread(text, 1, sizeof text - 1, file);
    assert(length < sizeof text - 1);
    text[length] = '\0';
    fclose(file);
    bindings_t bindings;
    bindings_error_t error;
    int parsed = bindings_parse("bindings.default.cfg", text, &bindings, &error);
    if (!parsed) {
        fprintf(stderr, "unexpected error: %s\n", error.text);
    }
    assert(parsed);
    return bindings;
}

static void test_the_default_file_binds_plan_12_buttons(void) {
    bindings_t bindings = parse_default_file();
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            int mask = bindings_mask(&bindings, (bindings_mode_t)mode, (bindings_action_t)action);
            assert(mask == expected_mask(mode, action));
            assert((mask != 0) == bindings_mode_has_action((bindings_mode_t)mode, (bindings_action_t)action));
        }
    }
}

static void assert_same_masks(const bindings_t *got, const bindings_t *want) {
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            assert(bindings_mask(got, (bindings_mode_t)mode, (bindings_action_t)action) ==
                   bindings_mask(want, (bindings_mode_t)mode, (bindings_action_t)action));
        }
    }
}

static void test_the_compiled_default_table_is_the_parse_of_the_default_file(void) {
    bindings_t bindings = parse_default_file();
    assert_same_masks(&bindings_compiled, &bindings);
}

static void test_the_core_default_table_is_the_parse_of_the_default_file(void) {
    bindings_t bindings = parse_default_file();
    assert_same_masks(&bindings_default, &bindings);
}

static void test_the_core_and_compiled_tables_link_side_by_side(void) {
    assert(bindings_compiled_personal == 0);
    assert_same_masks(&bindings_default, &bindings_compiled);
}

static void test_a_swap_table_leaves_every_other_mode_unbound(void) {
    bindings_t bindings = parse_ok("jump.enter_aim = 8\njump.open_menu = 7\n");
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            int expected = 0;
            if (mode == BINDINGS_MODE_JUMP && action == BINDINGS_ACTION_ENTER_AIM) {
                expected = P8_PAD_BUTTON_8;
            } else if (mode == BINDINGS_MODE_JUMP && action == BINDINGS_ACTION_OPEN_MENU) {
                expected = P8_PAD_BUTTON_7;
            }
            assert(bindings_mask(&bindings, (bindings_mode_t)mode, (bindings_action_t)action) == expected);
        }
    }
}

static void assert_error(const char *text, int line, const char *expected) {
    bindings_t bindings;
    bindings_error_t error;
    assert(!bindings_parse("tests/e2e/swap.cfg", text, &bindings, &error));
    if (error.line != line || strcmp(error.text, expected) != 0) {
        fprintf(stderr, "expected line %d \"%s\"\n     got line %d \"%s\"\n", line, expected, error.line, error.text);
    }
    assert(error.line == line);
    assert(strcmp(error.text, expected) == 0);
}

static void test_a_button_bound_twice_in_one_mode_is_a_conflict(void) {
    assert_error("# jump\n"
                 "jump.jump_up = 1\n"
                 "jump.jump_down = 2\n"
                 "\n"
                 "jump.jump_left = 3\n"
                 "jump.jump_right = 4\n"
                 "# the rest\n"
                 "jump.act_on_hovered = 5\n"
                 "jump.back = 6\n"
                 "aim.leave_aim = 7\n"
                 "jump.enter_aim = 7\n"
                 "jump.open_menu = 8, 7\n",
                 12, "tests/e2e/swap.cfg:12: button 7 already bound in mode jump");
}

static void test_an_unknown_mode_is_quoted(void) {
    assert_error("jmup.back = 6\n", 1, "tests/e2e/swap.cfg:1: unknown mode \"jmup\"");
}

static void test_an_unknown_action_or_one_of_another_mode_is_refused(void) {
    assert_error("jump.fly = 5\n", 1, "tests/e2e/swap.cfg:1: unknown action \"fly\" in mode jump");
    assert_error("title.pause = 11\n", 1, "tests/e2e/swap.cfg:1: unknown action \"pause\" in mode title");
}

static void test_a_bad_button_is_quoted(void) {
    const char *expected_suffix = "\": expected a number from 1 to 12";
    const char *items[] = { "13", "0", "05", "cross", "5x", "+5", "1234567890123" };
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        char text[64];
        char expected[128];
        snprintf(text, sizeof text, "jump.back = 6, %s\n", items[i]);
        snprintf(expected, sizeof expected, "tests/e2e/swap.cfg:1: bad button \"%s%s", items[i], expected_suffix);
        assert_error(text, 1, expected);
    }
}

static void test_a_pair_given_twice_names_the_first_line(void) {
    assert_error("jump.back = 6\n# again\njump.back = 7\n", 3,
                 "tests/e2e/swap.cfg:3: jump.back already given on line 1");
}

static void test_a_line_without_equal_or_dot_is_refused(void) {
    assert_error("jump.back 6  # half written\n", 1, "tests/e2e/swap.cfg:1: no '=' in \"jump.back 6\"");
    assert_error("jumpback = 6\n", 1, "tests/e2e/swap.cfg:1: expected mode.action, got \"jumpback\"");
    assert_error("jump.back = 6.5\n", 1, "tests/e2e/swap.cfg:1: bad button \"6.5\": expected a number from 1 to 12");
}

static void test_an_empty_button_is_refused(void) {
    const char *expected = "tests/e2e/swap.cfg:1: empty button in the line of jump.back";
    assert_error("jump.back =\n", 1, expected);
    assert_error("jump.back = 5,,6\n", 1, expected);
    assert_error("jump.back = 5, 6,\n", 1, expected);
    assert_error("jump.back = 5, \t, 6\n", 1, expected);
}

static void test_a_button_repeated_on_one_line_is_refused(void) {
    assert_error("pause.close_pause = 6, 11, 6\n", 1,
                 "tests/e2e/swap.cfg:1: button 6 listed twice for pause.close_pause");
}

static void test_a_line_longer_than_127_characters_is_refused(void) {
    char text[160];
    const char *head = "jump.back = 6 #";
    size_t head_length = strlen(head);
    memcpy(text, head, head_length);
    memset(text + head_length, '-', 127 - head_length);
    strcpy(text + 127, "\r\n");
    parse_ok(text);
    text[127] = '-';
    strcpy(text + 128, "\n");
    assert_error(text, 1, "tests/e2e/swap.cfg:1: line longer than 127 characters");
}

static void test_the_first_error_wins_and_lines_count_blanks_and_comments(void) {
    assert_error("# header\n\n   \njump.back = 6\n\njump.fly = 5\njmup.back = 6\n", 6,
                 "tests/e2e/swap.cfg:6: unknown action \"fly\" in mode jump");
}

static void test_every_name_is_unique_and_every_pair_parses(void) {
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        assert(bindings_mode_name((bindings_mode_t)mode)[0] != '\0');
        for (int other = 0; other < mode; other++) {
            assert(strcmp(bindings_mode_name((bindings_mode_t)mode), bindings_mode_name((bindings_mode_t)other)) != 0);
        }
    }
    for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
        assert(bindings_action_name((bindings_action_t)action)[0] != '\0');
        for (int other = 0; other < action; other++) {
            assert(strcmp(bindings_action_name((bindings_action_t)action),
                          bindings_action_name((bindings_action_t)other)) != 0);
        }
    }
    for (int mode = 0; mode < BINDINGS_MODE_COUNT; mode++) {
        for (int action = 0; action < BINDINGS_ACTION_COUNT; action++) {
            if (!bindings_mode_has_action((bindings_mode_t)mode, (bindings_action_t)action)) {
                continue;
            }
            char text[64];
            snprintf(text, sizeof text, "%s.%s = 12\n", bindings_mode_name((bindings_mode_t)mode),
                     bindings_action_name((bindings_action_t)action));
            bindings_t bindings = parse_ok(text);
            assert(bindings_mask(&bindings, (bindings_mode_t)mode, (bindings_action_t)action) == P8_PAD_BUTTON_12);
        }
    }
}

static void test_a_long_quoted_text_never_overflows_the_message(void) {
    char text[160];
    memset(text, 'x', 110);
    strcpy(text + 110, ".back = 6\n");
    bindings_t bindings;
    bindings_error_t error;
    char file_name[300];
    memset(file_name, 'f', sizeof file_name - 1);
    file_name[sizeof file_name - 1] = '\0';
    assert(!bindings_parse(file_name, text, &bindings, &error));
    assert(error.line == 1);
    assert(strlen(error.text) == sizeof error.text - 1);
    assert(!bindings_parse("a.cfg", text, &bindings, &error));
    assert(strncmp(error.text, "a.cfg:1: unknown mode \"xxx", 26) == 0);
    assert(strlen(error.text) < sizeof error.text);
}

int main(void) {
    test_reader_accepts_every_button_number();
    test_reader_leaves_what_follows_the_digits_to_the_caller();
    test_reader_refuses_anything_but_a_number_from_1_to_12();
    test_one_line_binds_one_action();
    test_empty_and_comment_only_texts_leave_everything_unbound();
    test_several_buttons_on_one_action_are_ored();
    test_blanks_comments_crlf_and_no_final_newline_are_accepted();
    test_an_action_without_a_line_is_unbound();
    test_the_same_button_in_two_modes_is_accepted();
    test_an_action_the_mode_does_not_have_is_unbound();
    test_the_default_file_binds_plan_12_buttons();
    test_the_compiled_default_table_is_the_parse_of_the_default_file();
    test_the_core_default_table_is_the_parse_of_the_default_file();
    test_the_core_and_compiled_tables_link_side_by_side();
    test_a_swap_table_leaves_every_other_mode_unbound();
    test_a_button_bound_twice_in_one_mode_is_a_conflict();
    test_an_unknown_mode_is_quoted();
    test_an_unknown_action_or_one_of_another_mode_is_refused();
    test_a_bad_button_is_quoted();
    test_a_pair_given_twice_names_the_first_line();
    test_a_line_without_equal_or_dot_is_refused();
    test_an_empty_button_is_refused();
    test_a_button_repeated_on_one_line_is_refused();
    test_a_line_longer_than_127_characters_is_refused();
    test_the_first_error_wins_and_lines_count_blanks_and_comments();
    test_every_name_is_unique_and_every_pair_parses();
    test_a_long_quoted_text_never_overflows_the_message();
    puts("test_bindings: ok");
    return 0;
}
