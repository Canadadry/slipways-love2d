#include "devtools/pad_metrics.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static pad_metrics_flags_t game_with_nothing_open(void) {
    pad_metrics_flags_t flags;
    memset(&flags, 0, sizeof flags);
    flags.has_economy = 1;
    return flags;
}

static pad_metrics_flags_t all_flags(void) {
    pad_metrics_flags_t flags = {1, 1, 1, 1, 1, 1, 1, 1, 1};
    return flags;
}

static void test_no_economy_is_the_title_whatever_else_is_set(void) {
    pad_metrics_flags_t flags = all_flags();
    flags.has_economy = 0;
    assert(pad_metrics_classify(&flags) == PAD_METRICS_MODE_TITLE);
}

static void test_a_game_with_nothing_open_is_jump(void) {
    pad_metrics_flags_t flags = game_with_nothing_open();
    assert(pad_metrics_classify(&flags) == PAD_METRICS_MODE_JUMP);
}

enum { RUNGS = 8 };

static const pad_metrics_mode_t RUNG_MODES[RUNGS] = {
    PAD_METRICS_MODE_PAUSE, PAD_METRICS_MODE_PLACEMENT, PAD_METRICS_MODE_BAR, PAD_METRICS_MODE_TECH,
    PAD_METRICS_MODE_SCORE, PAD_METRICS_MODE_MENU,      PAD_METRICS_MODE_AIM, PAD_METRICS_MODE_SLIPWAY,
};

static int *rung_flag(pad_metrics_flags_t *flags, int rung) {
    int *order[RUNGS] = {&flags->pause_shown, &flags->placing,    &flags->build_bar_open, &flags->tech_open,
                         &flags->score_open,  &flags->panel_open, &flags->aim_mode,       &flags->slipway_started};
    return order[rung];
}

static void test_each_rung_alone_gives_its_mode(void) {
    for (int rung = 0; rung < RUNGS; rung++) {
        pad_metrics_flags_t flags = game_with_nothing_open();
        *rung_flag(&flags, rung) = 1;
        assert(pad_metrics_classify(&flags) == RUNG_MODES[rung]);
    }
}

/* With every flag set, clearing them one by one from the top walks the modes in order. */
static void test_the_first_matching_rung_wins(void) {
    pad_metrics_flags_t flags = all_flags();
    for (int rung = 0; rung < RUNGS; rung++) {
        assert(pad_metrics_classify(&flags) == RUNG_MODES[rung]);
        *rung_flag(&flags, rung) = 0;
    }
    assert(pad_metrics_classify(&flags) == PAD_METRICS_MODE_JUMP);
}

static void test_mode_names_keep_the_mode_log_spelling(void) {
    const char *names[PAD_METRICS_MODE_COUNT] = {
        "title", "pause", "placement", "bar", "tech", "score", "menu", "jump", "aim", "slipway",
    };
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        assert(strcmp(pad_metrics_mode_name((pad_metrics_mode_t)m), names[m]) == 0);
    }
}

static pad_metrics_view_t still_view(void) {
    pad_metrics_view_t view;
    memset(&view, 0, sizeof view);
    view.mode = PAD_METRICS_MODE_JUMP;
    view.hovered = (entity_id_t){3, 1};
    view.camera_x = 40;
    view.camera_y = -8;
    view.credits = p8_fixed_from_int(12);
    view.year = p8_fixed_from_int(2);
    view.nodes = 30;
    return view;
}

static void test_a_view_against_itself_changes_nothing(void) {
    pad_metrics_view_t a = still_view(), b = still_view();
    assert(!pad_metrics_navigation_changed(&a, &b));
    assert(!pad_metrics_view_changed(&a, &b));
}

static void test_each_navigation_field_moves_both_change_tests(void) {
    for (int field = 0; field < 12; field++) {
        pad_metrics_view_t a = still_view(), b = still_view();
        switch (field) {
        case 0: b.mode = PAD_METRICS_MODE_AIM; break;
        case 1: b.hovered.generation++; break;
        case 2: b.slipway_start = (entity_id_t){5, 1}; break;
        case 3: b.slipway_target = (entity_id_t){6, 1}; break;
        case 4: b.build_icon = (entity_id_t){7, 1}; break;
        case 5: b.placing = (entity_id_t){8, 1}; break;
        case 6: b.focus = (entity_id_t){9, 1}; break;
        case 7: b.camera_x++; break;
        case 8: b.camera_y--; break;
        case 9: b.panel_open = 1; break;
        case 10: b.box_open = 1; break;
        default: b.message_box = 1; break;
        }
        assert(pad_metrics_navigation_changed(&a, &b));
        assert(pad_metrics_view_changed(&a, &b));
    }
}

static void test_each_game_field_moves_only_the_view_change_test(void) {
    for (int field = 0; field < 6; field++) {
        pad_metrics_view_t a = still_view(), b = still_view();
        switch (field) {
        case 0: b.credits -= p8_fixed_from_int(1); break;
        case 1: b.year += p8_fixed_from_int(1); break;
        case 2: b.year_ticks = 1; break;
        case 3: b.nodes++; break;
        case 4: b.probes = 1; break;
        default: b.techs = 1; break;
        }
        assert(!pad_metrics_navigation_changed(&a, &b));
        assert(pad_metrics_view_changed(&a, &b));
    }
}

static void test_each_direction_maps_to_its_dpad_button(void) {
    assert(pad_metrics_direction_button(PAD_DIRECTION_UP) == P8_PAD_BUTTON_1);
    assert(pad_metrics_direction_button(PAD_DIRECTION_DOWN) == P8_PAD_BUTTON_2);
    assert(pad_metrics_direction_button(PAD_DIRECTION_LEFT) == P8_PAD_BUTTON_3);
    assert(pad_metrics_direction_button(PAD_DIRECTION_RIGHT) == P8_PAD_BUTTON_4);
    assert(pad_metrics_direction_button(PAD_DIRECTION_NONE) == 0);
}

static void test_the_first_direction_button_follows_up_down_left_right(void) {
    const int dpad = P8_PAD_BUTTON_1 | P8_PAD_BUTTON_2 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4;
    assert(pad_metrics_first_direction_button(dpad) == P8_PAD_BUTTON_1);
    assert(pad_metrics_first_direction_button(P8_PAD_BUTTON_2 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4) == P8_PAD_BUTTON_2);
    assert(pad_metrics_first_direction_button(P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4) == P8_PAD_BUTTON_3);
    assert(pad_metrics_first_direction_button(P8_PAD_BUTTON_4) == P8_PAD_BUTTON_4);
}

static void test_the_first_direction_button_ignores_other_buttons(void) {
    assert(pad_metrics_first_direction_button(P8_PAD_BUTTON_5 | P8_PAD_BUTTON_7) == 0);
    assert(pad_metrics_first_direction_button(P8_PAD_BUTTON_5 | P8_PAD_BUTTON_3) == P8_PAD_BUTTON_3);
    assert(pad_metrics_first_direction_button(0) == 0);
}

/* Feeds the same pad frames to the counter in mode and to pad_jump; returns how many frames of frame_count both
 * report a repeat on, and asserts they never disagree past a press frame. */
static int repeats_against_pad_jump(const int *buttons, int frame_count, pad_metrics_mode_t mode) {
    pad_metrics_hold_t hold;
    memset(&hold, 0, sizeof hold);
    pad_jump_t jump;
    pad_jump_init(&jump);
    int previous = 0;
    int repeats = 0;
    for (int f = 0; f < frame_count; f++) {
        int pressed = buttons[f] & ~previous;
        int counted = pad_metrics_hold_step(&hold, mode, buttons[f]);
        pad_direction_t jumped =
            pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, buttons[f], pressed);
        if (pressed != 0) {
            assert(counted == 0);
        } else {
            assert(counted == pad_metrics_direction_button(jumped));
        }
        repeats += counted != 0;
        previous = buttons[f];
    }
    return repeats;
}

static void test_a_held_direction_repeats_on_the_frames_pad_jump_does(void) {
    enum { FRAMES = 40 };
    int buttons[FRAMES];
    for (int f = 0; f < FRAMES; f++) buttons[f] = P8_PAD_BUTTON_3;
    pad_metrics_hold_t hold;
    memset(&hold, 0, sizeof hold);
    for (int f = 0; f < FRAMES; f++) {
        int fired = pad_metrics_hold_step(&hold, PAD_METRICS_MODE_JUMP, buttons[f]);
        assert(fired == ((f == 18 || f == 26 || f == 34) ? P8_PAD_BUTTON_3 : 0));
    }
    assert(repeats_against_pad_jump(buttons, FRAMES, PAD_METRICS_MODE_JUMP) == 3);
}

static void test_a_fresh_press_restarts_the_count(void) {
    enum { FRAMES = 60 };
    int buttons[FRAMES];
    for (int f = 0; f < FRAMES; f++) buttons[f] = f == 22 ? 0 : P8_PAD_BUTTON_2;
    assert(repeats_against_pad_jump(buttons, FRAMES, PAD_METRICS_MODE_JUMP) == 4);
    pad_metrics_hold_t hold;
    memset(&hold, 0, sizeof hold);
    for (int f = 0; f < FRAMES; f++) {
        int fired = pad_metrics_hold_step(&hold, PAD_METRICS_MODE_JUMP, buttons[f]);
        assert(fired == ((f == 18 || f == 23 + 18 || f == 23 + 26 || f == 23 + 34) ? P8_PAD_BUTTON_2 : 0));
    }
}

static void test_holds_repeat_in_slipway_mode_and_nowhere_else(void) {
    enum { FRAMES = 30 };
    int buttons[FRAMES];
    for (int f = 0; f < FRAMES; f++) buttons[f] = P8_PAD_BUTTON_4;
    assert(repeats_against_pad_jump(buttons, FRAMES, PAD_METRICS_MODE_SLIPWAY) == 2);
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        if (m == PAD_METRICS_MODE_JUMP || m == PAD_METRICS_MODE_SLIPWAY) continue;
        pad_metrics_hold_t hold;
        memset(&hold, 0, sizeof hold);
        for (int f = 0; f < FRAMES; f++) {
            assert(pad_metrics_hold_step(&hold, (pad_metrics_mode_t)m, buttons[f]) == 0);
        }
    }
}

BUMP_ZONE_STORAGE(unused_cells, 64);
static bump_zone_t unused_zone;
static pad_metrics_t metrics;
static pad_metrics_view_t current;

static void start(pad_metrics_mode_t mode) {
    bump_zone_init(&unused_zone, "unused", unused_cells, 64);
    pad_metrics_init_judging(&metrics, &unused_zone);
    current = still_view();
    current.mode = mode;
    pad_metrics_begin(&metrics, &current);
}

/* One frame with buttons down; the camera moves by one pixel in it when moved. */
static void run(int buttons, int moved) {
    if (moved) current.camera_x++;
    pad_metrics_step(&metrics, buttons, 0, &current);
}

static void run_idle(int frames) {
    for (int i = 0; i < frames; i++) run(0, 0);
}

static int count(pad_metrics_kind_t kind, pad_metrics_mode_t mode, int button_bit, pad_metrics_verdict_t verdict) {
    int b = 0;
    while (!(button_bit & (1 << b))) b++;
    return pad_metrics_verdict_count(&metrics, kind, mode, b, verdict);
}

static void test_a_press_that_moves_nothing_is_dead(void) {
    start(PAD_METRICS_MODE_AIM);
    run(P8_PAD_BUTTON_8, 0);
    run_idle(5);
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_DEAD_OTHER) == 1);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_LIVE) == 0);
}

/* Pressed at f0, released at f3: the window runs from f0 to f5. */
static void test_the_window_runs_to_two_frames_after_the_release(void) {
    for (int moved_at = 0; moved_at <= 6; moved_at++) {
        start(PAD_METRICS_MODE_AIM);
        for (int f = 0; f < 10; f++) run(f < 3 ? P8_PAD_BUTTON_8 : 0, f == moved_at);
        pad_metrics_finish(&metrics);
        pad_metrics_verdict_t expected = moved_at <= 5 ? PAD_METRICS_LIVE : PAD_METRICS_DEAD_OTHER;
        assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, expected) == 1);
    }
}

static void test_a_window_reaching_the_end_of_the_file_is_judged_on_the_frames_seen(void) {
    start(PAD_METRICS_MODE_AIM);
    run(P8_PAD_BUTTON_8, 0);
    run(P8_PAD_BUTTON_8, 0);
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_DEAD_OTHER) == 1);

    start(PAD_METRICS_MODE_AIM);
    run(P8_PAD_BUTTON_8, 0);
    run(P8_PAD_BUTTON_8, 1);
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_LIVE) == 1);
}

/* Held from f0 to f29, released at f30: repeats at f18 and f26. */
static void hold_left(int moved_at) {
    start(PAD_METRICS_MODE_JUMP);
    for (int f = 0; f < 40; f++) run(f < 30 ? P8_PAD_BUTTON_3 : 0, f == moved_at);
    pad_metrics_finish(&metrics);
}

static void test_a_press_window_is_cut_before_its_first_repeat(void) {
    hold_left(17);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 1);
    hold_left(18);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_DEAD_OTHER) == 1);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 1);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_DEAD_OTHER) == 1);
}

static void test_a_repeat_window_runs_to_the_next_repeat_or_the_release(void) {
    hold_left(25);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 1);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_DEAD_OTHER) == 1);
    hold_left(32);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 1);
    hold_left(33);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 0);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_DEAD_OTHER) == 2);
}

static void test_a_chord_is_counted_apart_and_never_judged(void) {
    start(PAD_METRICS_MODE_JUMP);
    run(P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6, 0);
    run_idle(5);
    pad_metrics_finish(&metrics);
    assert(pad_metrics_chord_count(&metrics, PAD_METRICS_MODE_JUMP, 4) == 1);
    assert(pad_metrics_chord_count(&metrics, PAD_METRICS_MODE_JUMP, 5) == 1);
    for (int v = 0; v < PAD_METRICS_VERDICT_COUNT; v++) {
        assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_5, (pad_metrics_verdict_t)v) == 0);
        assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_6, (pad_metrics_verdict_t)v) == 0);
    }
}

/* A dead tap of button_bit in mode on a snapshot changed by set. */
static pad_metrics_verdict_t dead_press_group(pad_metrics_mode_t mode, int button_bit,
                                              void (*set)(pad_metrics_view_t *view)) {
    start(mode);
    set(&current);
    pad_metrics_begin(&metrics, &current);
    run(button_bit, 0);
    run_idle(4);
    pad_metrics_finish(&metrics);
    if (count(PAD_METRICS_PRESS, mode, button_bit, PAD_METRICS_DEAD_REQUIRED) == 1) return PAD_METRICS_DEAD_REQUIRED;
    assert(count(PAD_METRICS_PRESS, mode, button_bit, PAD_METRICS_DEAD_OTHER) == 1);
    return PAD_METRICS_DEAD_OTHER;
}

static pad_metrics_verdict_t dead_circle_group(pad_metrics_mode_t mode, void (*set)(pad_metrics_view_t *view)) {
    return dead_press_group(mode, P8_PAD_BUTTON_6, set);
}

static void nothing_recorded(pad_metrics_view_t *view) {
    view->last_acted = ENTITY_ID_NONE;
}

static void last_is_hovered(pad_metrics_view_t *view) {
    view->last_acted = view->hovered;
}

static void last_elsewhere(pad_metrics_view_t *view) {
    view->last_acted = (entity_id_t){11, 1};
}

static void message_box_shown(pad_metrics_view_t *view) {
    view->message_box = 1;
}

static void test_a_dead_circle_at_the_root_is_required(void) {
    assert(dead_circle_group(PAD_METRICS_MODE_JUMP, nothing_recorded) == PAD_METRICS_DEAD_REQUIRED);
    assert(dead_circle_group(PAD_METRICS_MODE_JUMP, last_is_hovered) == PAD_METRICS_DEAD_REQUIRED);
    assert(pad_metrics_section4(&metrics).ok);
}

static void test_any_other_dead_circle_is_in_the_other_group(void) {
    assert(dead_circle_group(PAD_METRICS_MODE_JUMP, last_elsewhere) == PAD_METRICS_DEAD_OTHER);
    assert(dead_circle_group(PAD_METRICS_MODE_JUMP, message_box_shown) == PAD_METRICS_DEAD_OTHER);
    assert(dead_circle_group(PAD_METRICS_MODE_AIM, nothing_recorded) == PAD_METRICS_DEAD_OTHER);
    assert(!pad_metrics_section4(&metrics).ok);
    assert(pad_metrics_section4(&metrics).button_6_dead_other == 1);
}

static void nothing_to_jump_to(pad_metrics_view_t *view) {
    view->no_candidate = 1;
}

static void something_to_jump_to(pad_metrics_view_t *view) {
    view->no_candidate = 0;
}

static void test_a_dead_square_in_aim_mode_with_nothing_to_jump_to_is_required(void) {
    assert(dead_press_group(PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_7, nothing_to_jump_to) == PAD_METRICS_DEAD_REQUIRED);
    assert(dead_press_group(PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_7, something_to_jump_to) == PAD_METRICS_DEAD_OTHER);
    assert(dead_press_group(PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_7, nothing_to_jump_to) == PAD_METRICS_DEAD_OTHER);
}

static void nothing_to_jump_to_with_a_message_box(pad_metrics_view_t *view) {
    view->no_candidate = 1;
    view->message_box = 1;
}

static void nothing_to_jump_to_with_a_box(pad_metrics_view_t *view) {
    view->no_candidate = 1;
    view->box_open = 1;
}

static void test_a_dead_circle_in_aim_mode_with_nothing_to_jump_to_is_required(void) {
    assert(dead_circle_group(PAD_METRICS_MODE_AIM, nothing_to_jump_to) == PAD_METRICS_DEAD_REQUIRED);
    assert(pad_metrics_section4(&metrics).button_6_dead_other == 0);
    assert(dead_circle_group(PAD_METRICS_MODE_AIM, nothing_to_jump_to_with_a_message_box) == PAD_METRICS_DEAD_OTHER);
    assert(dead_circle_group(PAD_METRICS_MODE_AIM, nothing_to_jump_to_with_a_box) == PAD_METRICS_DEAD_OTHER);
    assert(dead_circle_group(PAD_METRICS_MODE_AIM, something_to_jump_to) == PAD_METRICS_DEAD_OTHER);
    assert(!pad_metrics_section4(&metrics).ok);
}

static void test_a_dead_direction_into_an_empty_cone_is_required_in_jump_and_slipway_mode(void) {
    pad_metrics_mode_t modes[] = {PAD_METRICS_MODE_JUMP, PAD_METRICS_MODE_SLIPWAY};
    for (int i = 0; i < 2; i++) {
        start(modes[i]);
        current.cone_empty = P8_PAD_BUTTON_1;
        pad_metrics_begin(&metrics, &current);
        run(P8_PAD_BUTTON_1, 0);
        run_idle(4);
        run(P8_PAD_BUTTON_2, 0);
        run_idle(4);
        pad_metrics_finish(&metrics);
        assert(count(PAD_METRICS_PRESS, modes[i], P8_PAD_BUTTON_1, PAD_METRICS_DEAD_REQUIRED) == 1);
        assert(count(PAD_METRICS_PRESS, modes[i], P8_PAD_BUTTON_2, PAD_METRICS_DEAD_OTHER) == 1);
    }
}

static void test_a_diagonals_second_direction_is_never_required(void) {
    start(PAD_METRICS_MODE_JUMP);
    current.cone_empty = P8_PAD_BUTTON_1 | P8_PAD_BUTTON_4;
    pad_metrics_begin(&metrics, &current);
    run(P8_PAD_BUTTON_1 | P8_PAD_BUTTON_4, 0);
    run_idle(4);
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_1, PAD_METRICS_DEAD_REQUIRED) == 1);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_4, PAD_METRICS_DEAD_OTHER) == 1);
}

static void test_a_dead_repeat_into_an_empty_cone_is_required(void) {
    start(PAD_METRICS_MODE_JUMP);
    current.cone_empty = P8_PAD_BUTTON_3;
    pad_metrics_begin(&metrics, &current);
    for (int f = 0; f < 20; f++) run(P8_PAD_BUTTON_3, f == 1);
    run_idle(4);
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_LIVE) == 1);
    assert(count(PAD_METRICS_REPEAT, PAD_METRICS_MODE_JUMP, P8_PAD_BUTTON_3, PAD_METRICS_DEAD_REQUIRED) == 1);
}

static void test_the_group_facts_move_neither_change_test(void) {
    pad_metrics_view_t a = still_view(), b = still_view();
    b.last_acted = (entity_id_t){4, 2};
    b.cone_empty = P8_PAD_BUTTON_1 | P8_PAD_BUTTON_3;
    b.no_candidate = 1;
    assert(!pad_metrics_navigation_changed(&a, &b));
    assert(!pad_metrics_view_changed(&a, &b));
}

static char report[1 << 16];

/* The report of the run, in report; asserts the line is there. */
static void write_report(void) {
    FILE *out = tmpfile();
    assert(out != NULL);
    pad_metrics_write_report(&metrics, out);
    rewind(out);
    size_t n = fread(report, 1, sizeof report - 1, out);
    report[n] = '\0';
    fclose(out);
}

static int report_has(const char *line) {
    return strstr(report, line) != NULL;
}

static void test_shares_print_two_decimals_and_the_aim_test_is_exact(void) {
    start(PAD_METRICS_MODE_AIM);
    for (int f = 0; f < 1487; f++) {
        if (f == 1486) current.mode = PAD_METRICS_MODE_JUMP;
        run(0, 0);
    }
    run_idle(5955 - 1487);
    pad_metrics_finish(&metrics);
    assert(pad_metrics_aim_ok(&metrics));
    write_report();
    assert(report_has("section4 aim share 24.97% (1487 of 5955 frames) under 25% ok\n"));
    assert(report_has("aim share all frames 24.97% (1487 of 5955 frames)\n"));
    assert(report_has("total aim share 25.0% (1487 of 5955 frames)\n"));
    assert(report_has("section4 verdict ok\n"));

    start(PAD_METRICS_MODE_AIM);
    for (int f = 0; f < 1489; f++) {
        if (f == 1488) current.mode = PAD_METRICS_MODE_JUMP;
        run(0, 0);
    }
    run_idle(5956 - 1489);
    pad_metrics_finish(&metrics);
    assert(!pad_metrics_aim_ok(&metrics));
}

static void test_title_and_pause_are_outside_the_aim_denominator(void) {
    start(PAD_METRICS_MODE_TITLE);
    run_idle(9);
    current.mode = PAD_METRICS_MODE_PAUSE;
    run(0, 0);
    run_idle(9);
    current.mode = PAD_METRICS_MODE_AIM;
    run(0, 0);
    run_idle(4);
    current.mode = PAD_METRICS_MODE_JUMP;
    run(0, 0);
    run_idle(17);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("in-game frames 22 (without title and pause)\n"));
    assert(report_has("in-game frames with pause 32\n"));
    assert(report_has("section4 aim share 22.73% (5 of 22 frames) under 25% ok\n"));
    assert(report_has("aim share in-game with pause 15.63% (5 of 32 frames)\n"));
    assert(report_has("mode title 23.81% (10 of 42 frames)\n"));
}

static void test_the_verdict_fails_on_a_required_dead_dpad_press_in_jump_mode(void) {
    start(PAD_METRICS_MODE_JUMP);
    current.cone_empty = P8_PAD_BUTTON_2;
    pad_metrics_begin(&metrics, &current);
    run(P8_PAD_BUTTON_2, 0);
    run_idle(4);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("dead f0 press 2 jump required\n"));
    assert(report_has("section4 dead d-pad presses in jump mode 1 fail (lenient, other group only: 0)\n"));
    assert(report_has("section4 verdict fail\n"));
}

static void test_the_verdict_passes_with_required_circles_and_dead_repeats_only(void) {
    start(PAD_METRICS_MODE_JUMP);
    run(P8_PAD_BUTTON_6, 0);
    run_idle(4);
    for (int f = 0; f < 20; f++) run(P8_PAD_BUTTON_3, f == 1);
    run_idle(4);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("dead f0 press 6 jump required\n"));
    assert(report_has("repeat f23 3 jump dead-other\n"));
    assert(report_has("presses jump 6 1 chords 0 live 0 dead-required 1 dead-other 0\n"));
    assert(report_has("repeats jump 3 1 live 0 dead-required 0 dead-other 1\n"));
    assert(report_has("section4 dead hold repeats required 0 other 1 (not deciding)\n"));
    assert(report_has("total required no-ops 1\n"));
    assert(report_has("section4 verdict ok\n"));
}

static void test_the_verdict_passes_with_a_required_circle_in_aim_mode(void) {
    start(PAD_METRICS_MODE_AIM);
    current.no_candidate = 1;
    pad_metrics_begin(&metrics, &current);
    run(P8_PAD_BUTTON_6, 0);
    run(P8_PAD_BUTTON_7, 0);
    run_idle(4);
    current.mode = PAD_METRICS_MODE_JUMP;
    current.no_candidate = 0;
    run(0, 0);
    run_idle(30);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("dead f0 press 6 aim required\n"));
    assert(report_has("dead f1 press 7 aim required\n"));
    assert(report_has("total required no-ops 1\n"));
    assert(report_has("section4 verdict ok\n"));
}

static void test_the_lenient_count_leaves_out_the_required_group(void) {
    start(PAD_METRICS_MODE_JUMP);
    current.cone_empty = P8_PAD_BUTTON_1;
    pad_metrics_begin(&metrics, &current);
    run(P8_PAD_BUTTON_1 | P8_PAD_BUTTON_4, 0);
    run_idle(4);
    pad_metrics_finish(&metrics);
    pad_metrics_section4_t section4 = pad_metrics_section4(&metrics);
    assert(section4.jump_dpad_dead_strict == 2 && section4.jump_dpad_dead_lenient == 1 && !section4.ok);
}

static void test_the_report_numbers_buttons_from_1_for_the_lowest_bit_to_12_for_the_highest(void) {
    start(PAD_METRICS_MODE_JUMP);
    run(P8_PAD_BUTTON_1, 1);
    run_idle(4);
    run(P8_PAD_BUTTON_12, 1);
    run_idle(4);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("presses jump 1 1 chords 0 live 1 dead-required 0 dead-other 0\n"));
    assert(report_has("presses jump 12 1 chords 0 live 1 dead-required 0 dead-other 0\n"));
    assert(report_has("total presses jump 1 1\n"));
    assert(report_has("total presses jump 12 1\n"));
}

static void test_cheat_activations_fail_the_verdict(void) {
    start(PAD_METRICS_MODE_JUMP);
    pad_metrics_step(&metrics, 0, 1, &current);
    pad_metrics_finish(&metrics);
    write_report();
    assert(report_has("section4 cheat activations 1 fail\n"));
    assert(report_has("section4 verdict fail\n"));
}

/* △ pressed at f0, released at f1; then other at f_other, held 1 frame; the camera moves at f_other only. */
static pad_metrics_verdict_t triangle_then(int f_other, int other) {
    start(PAD_METRICS_MODE_AIM);
    for (int f = 0; f < 10; f++) {
        int buttons = (f == 0 ? P8_PAD_BUTTON_8 : 0) | (f == f_other ? other : 0);
        run(buttons, f == f_other);
    }
    pad_metrics_finish(&metrics);
    return count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_LIVE) == 1 ? PAD_METRICS_LIVE
                                                                                                     : PAD_METRICS_DEAD_OTHER;
}

static void test_a_later_separate_press_cuts_a_released_press_window(void) {
    assert(triangle_then(2, P8_PAD_BUTTON_7) == PAD_METRICS_DEAD_OTHER);
    assert(triangle_then(3, P8_PAD_BUTTON_7) == PAD_METRICS_DEAD_OTHER);
}

static void test_a_press_on_the_release_frame_is_a_roll_and_does_not_cut(void) {
    assert(triangle_then(1, P8_PAD_BUTTON_7) == PAD_METRICS_LIVE);
}

static void test_a_new_press_of_the_same_button_does_not_cut(void) {
    triangle_then(2, P8_PAD_BUTTON_8);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_8, PAD_METRICS_LIVE) == 2);
}

static void test_another_press_while_still_held_does_not_cut(void) {
    start(PAD_METRICS_MODE_AIM);
    for (int f = 0; f < 10; f++) {
        int buttons = (f < 4 ? P8_PAD_BUTTON_5 : 0) | (f == 2 ? P8_PAD_BUTTON_1 : 0);
        run(buttons, f == 5);
    }
    pad_metrics_finish(&metrics);
    assert(count(PAD_METRICS_PRESS, PAD_METRICS_MODE_AIM, P8_PAD_BUTTON_5, PAD_METRICS_LIVE) == 1);
}

int main(void) {
    test_no_economy_is_the_title_whatever_else_is_set();
    test_a_game_with_nothing_open_is_jump();
    test_each_rung_alone_gives_its_mode();
    test_the_first_matching_rung_wins();
    test_mode_names_keep_the_mode_log_spelling();
    test_a_view_against_itself_changes_nothing();
    test_each_navigation_field_moves_both_change_tests();
    test_each_game_field_moves_only_the_view_change_test();
    test_each_direction_maps_to_its_dpad_button();
    test_the_first_direction_button_follows_up_down_left_right();
    test_the_first_direction_button_ignores_other_buttons();
    test_a_held_direction_repeats_on_the_frames_pad_jump_does();
    test_a_fresh_press_restarts_the_count();
    test_holds_repeat_in_slipway_mode_and_nowhere_else();
    test_a_press_that_moves_nothing_is_dead();
    test_the_window_runs_to_two_frames_after_the_release();
    test_a_window_reaching_the_end_of_the_file_is_judged_on_the_frames_seen();
    test_a_press_window_is_cut_before_its_first_repeat();
    test_a_repeat_window_runs_to_the_next_repeat_or_the_release();
    test_a_chord_is_counted_apart_and_never_judged();
    test_a_dead_circle_at_the_root_is_required();
    test_any_other_dead_circle_is_in_the_other_group();
    test_a_dead_square_in_aim_mode_with_nothing_to_jump_to_is_required();
    test_a_dead_circle_in_aim_mode_with_nothing_to_jump_to_is_required();
    test_a_dead_direction_into_an_empty_cone_is_required_in_jump_and_slipway_mode();
    test_a_diagonals_second_direction_is_never_required();
    test_a_dead_repeat_into_an_empty_cone_is_required();
    test_the_group_facts_move_neither_change_test();
    test_shares_print_two_decimals_and_the_aim_test_is_exact();
    test_title_and_pause_are_outside_the_aim_denominator();
    test_the_verdict_fails_on_a_required_dead_dpad_press_in_jump_mode();
    test_the_verdict_passes_with_required_circles_and_dead_repeats_only();
    test_the_verdict_passes_with_a_required_circle_in_aim_mode();
    test_the_lenient_count_leaves_out_the_required_group();
    test_the_report_numbers_buttons_from_1_for_the_lowest_bit_to_12_for_the_highest();
    test_cheat_activations_fail_the_verdict();
    test_a_later_separate_press_cuts_a_released_press_window();
    test_a_press_on_the_release_frame_is_a_roll_and_does_not_cut();
    test_a_new_press_of_the_same_button_does_not_cut();
    test_another_press_while_still_held_does_not_cut();
    printf("test_pad_metrics: ok\n");
    return 0;
}
