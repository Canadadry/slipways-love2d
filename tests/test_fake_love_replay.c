#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/replay.h"

#define DEFAULT_WINDOW 640, 480
#define PHONE_WINDOW 390, 844

/* Counted with `jq '.frames | length'`, not by the reader under test. */
static const struct {
    const char *path;
    size_t frames;
    int width;
    int height;
} E2E_REPLAYS[] = {
    {"tests/e2e/attract.json", 176, DEFAULT_WINDOW},
    {"tests/e2e/pad_bankrupt.json", 95, DEFAULT_WINDOW},
    {"tests/e2e/pad_build.json", 86, DEFAULT_WINDOW},
    {"tests/e2e/pad_colonise.json", 447, DEFAULT_WINDOW},
    {"tests/e2e/pad_empty_planet.json", 150, DEFAULT_WINDOW},
    {"tests/e2e/pad_end_turn.json", 156, DEFAULT_WINDOW},
    {"tests/e2e/pad_game_end.json", 114, DEFAULT_WINDOW},
    {"tests/e2e/pad_message_box.json", 225, DEFAULT_WINDOW},
    {"tests/e2e/pad_pause_restart.json", 80, DEFAULT_WINDOW},
    {"tests/e2e/pad_planet_projects.json", 524, DEFAULT_WINDOW},
    {"tests/e2e/pad_probe.json", 79, DEFAULT_WINDOW},
    {"tests/e2e/pad_research.json", 502, DEFAULT_WINDOW},
    {"tests/e2e/pad_restart_resume.json", 157, DEFAULT_WINDOW},
    {"tests/e2e/pad_resume_at_start.json", 48, DEFAULT_WINDOW},
    {"tests/e2e/pad_score.json", 98, DEFAULT_WINDOW},
    {"tests/e2e/pad_slipway.json", 320, DEFAULT_WINDOW},
    {"tests/e2e/pad_swap_aim_menu.json", 177, DEFAULT_WINDOW},
    {"tests/e2e/pad_tech_costs.json", 558, DEFAULT_WINDOW},
    {"tests/e2e/pad_title.json", 32, DEFAULT_WINDOW},
    {"tests/e2e/pad_twin_aim_diagonal.json", 65, DEFAULT_WINDOW},
    {"tests/e2e/pad_twin_first_slipway.json", 36, DEFAULT_WINDOW},
    {"tests/e2e/pad_twin_show_hide.json", 121, DEFAULT_WINDOW},
    {"tests/e2e/pad_twin_year_hold.json", 54, DEFAULT_WINDOW},
    {"tests/e2e/pad_unbound_enter_aim.json", 48, DEFAULT_WINDOW},
    {"tests/e2e/touch_aim_diagonal.json", 65, PHONE_WINDOW},
    {"tests/e2e/touch_first_slipway.json", 36, PHONE_WINDOW},
    {"tests/e2e/touch_show_hide.json", 121, PHONE_WINDOW},
    {"tests/e2e/touch_year_hold.json", 54, PHONE_WINDOW},
};

static void test_reads_every_e2e_replay_with_its_frame_count_window_and_no_resize(void) {
    for (size_t i = 0; i < sizeof(E2E_REPLAYS) / sizeof(E2E_REPLAYS[0]); i++) {
        char error[256] = "";
        replay_t *replay = replay_read(E2E_REPLAYS[i].path, error, sizeof(error));
        if (replay == NULL) {
            fprintf(stderr, "%s\n", error);
        }
        assert(replay != NULL);
        assert(replay_frame_count(replay) == E2E_REPLAYS[i].frames);
        int width = 0;
        int height = 0;
        replay_window(replay, &width, &height);
        assert(width == E2E_REPLAYS[i].width && height == E2E_REPLAYS[i].height);
        for (size_t frame = 0; frame < E2E_REPLAYS[i].frames; frame++) {
            assert(!replay_frame_resize(replay, frame, &width, &height));
        }
        replay_free(replay);
    }
}

/* Transcribed from the file by hand, not by the reader under test. */
static void test_reads_the_held_buttons_of_pad_title(void) {
    char error[256] = "";
    replay_t *replay = replay_read("tests/e2e/pad_title.json", error, sizeof(error));
    assert(replay != NULL);
    assert(replay_frame_count(replay) == 32);
    for (size_t frame = 0; frame < 32; frame++) {
        unsigned expected = 0;
        if (frame == 10 || frame == 12) {
            expected = 1u << 1;
        } else if (frame == 14) {
            expected = 1u << 3;
        } else if (frame == 16) {
            expected = 1u << 0;
        } else if (frame >= 18 && frame <= 20) {
            expected = 1u << 4;
        }
        assert(replay_frame_held(replay, frame) == expected);
    }
    replay_free(replay);
}

/* Read with `jq`, not by the reader under test. */
static void test_reads_the_start_state_members_of_e2e_replays(void) {
    static const struct {
        const char *path;
        int attract_after;
        size_t save_count;
        const char *bindings;
    } EXPECTED[] = {
        {"tests/e2e/attract.json", 3, 3921, NULL},
        {"tests/e2e/pad_swap_aim_menu.json", 0, 0, "swap_aim_menu.cfg"},
        {"tests/e2e/pad_unbound_enter_aim.json", 0, 4186, "unbound_enter_aim.cfg"},
        {"tests/e2e/pad_title.json", 0, 0, NULL},
    };
    for (size_t i = 0; i < sizeof(EXPECTED) / sizeof(EXPECTED[0]); i++) {
        char error[256] = "";
        replay_t *replay = replay_read(EXPECTED[i].path, error, sizeof(error));
        assert(replay != NULL);
        assert(replay_attract_after(replay) == EXPECTED[i].attract_after);
        size_t save_count = 99;
        const unsigned char *save = replay_initial_save(replay, &save_count);
        assert(save_count == EXPECTED[i].save_count);
        if (save_count == 0) {
            assert(save == NULL);
        } else {
            assert(memcmp(save, "SLWY", 4) == 0);
        }
        const char *bindings = replay_bindings(replay);
        if (EXPECTED[i].bindings == NULL) {
            assert(bindings == NULL);
        } else {
            assert(bindings != NULL && strcmp(bindings, EXPECTED[i].bindings) == 0);
        }
        assert(replay_pad_sheet(replay) == NULL);
        replay_free(replay);
    }
}

static void test_every_e2e_replay_is_listed(void) {
    glob_t found;
    assert(glob("tests/e2e/*.json", 0, NULL, &found) == 0);
    assert(found.gl_pathc == sizeof(E2E_REPLAYS) / sizeof(E2E_REPLAYS[0]) && "a new e2e replay goes into E2E_REPLAYS");
    globfree(&found);
}

static size_t frame_count_of(const char *text) {
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    if (replay == NULL) {
        fprintf(stderr, "%s\n", error);
    }
    assert(replay != NULL);
    size_t count = replay_frame_count(replay);
    replay_free(replay);
    return count;
}

static void assert_refused(const char *text, const char *problem) {
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    if (replay != NULL || strstr(error, problem) == NULL || strncmp(error, "memory.json: offset ", 20) != 0) {
        fprintf(stderr, "text %s\n  expected: %s\n  got: %s\n", text, problem, error);
    }
    assert(replay == NULL);
    assert(strncmp(error, "memory.json: offset ", 20) == 0);
    assert(strstr(error, problem) != NULL);
}

static void test_an_empty_frames_array_has_no_frame(void) {
    assert(frame_count_of("{\"input\": \"pad\", \"frames\": []}") == 0);
}

static void test_unknown_keys_and_every_value_kind_are_accepted(void) {
    assert(frame_count_of("{ \"input\": \"pad\", \"after\": -3, \"x\": [true, false, null, 0, {}],\n"
                          "  \"save\": {}, \"binding\": \"\",\n"
                          "  \"escapes\": \"\\\" \\\\ \\/ \\b \\f \\n \\r \\t \\u00e9 \\ud83d\\ude00\",\n"
                          "  \"frames\": [ {\"pad\": [1, 12]}, {\"pad\": []} ] }\n") == 2);
}

static unsigned held_of_frame_1(const char *pad) {
    char text[256];
    snprintf(text, sizeof(text), "{\"input\": \"pad\", \"frames\": [{\"pad\": []}, {\"pad\": %s}]}", pad);
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    if (replay == NULL) {
        fprintf(stderr, "%s\n", error);
    }
    assert(replay != NULL);
    assert(replay_frame_held(replay, 0) == 0);
    unsigned held = replay_frame_held(replay, 1);
    replay_free(replay);
    return held;
}

static void test_the_buttons_of_a_frame_are_read_in_any_order(void) {
    assert(held_of_frame_1("[1, 5]") == 0x11);
    assert(held_of_frame_1("[5, 1]") == 0x11);
    assert(held_of_frame_1("[12, 3, 7]") == 0x844);
    assert(held_of_frame_1("[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12]") == 0xFFF);
}

static void assert_frame_1_refused(const char *frame, const char *problem) {
    char text[256];
    snprintf(text, sizeof(text), "{\"input\": \"pad\", \"frames\": [{\"pad\": []}, %s]}", frame);
    assert_refused(text, "f1: ");
    assert_refused(text, problem);
}

static void test_a_replay_without_pad_input_is_refused(void) {
    assert_refused("{\"frames\": []}", "\"input\"");
    assert_refused("{\"input\": \"mouse\", \"frames\": []}", "\"mouse\"");
    assert_refused("{\"input\": 1, \"frames\": []}", "\"input\"");
}

static void test_a_frame_with_wrong_buttons_is_refused(void) {
    assert_frame_1_refused("{\"pad\": 5}", "\"pad\"");
    assert_frame_1_refused("{\"pad\": {}}", "\"pad\"");
    assert_frame_1_refused("{\"pad\": [0]}", " 0 ");
    assert_frame_1_refused("{\"pad\": [1, 13]}", " 13 ");
    assert_frame_1_refused("{\"pad\": [-1]}", " -1 ");
    assert_frame_1_refused("{\"pad\": [\"a\"]}", "\"a\"");
    assert_frame_1_refused("{\"pad\": [null]}", "null");
    assert_frame_1_refused("{\"pad\": [5, 2, 5]}", " 5 ");
    assert_frame_1_refused("{\"pad\": [5, 2, 5]}", "twice");
}

static void test_a_frame_with_another_key_is_refused(void) {
    assert_frame_1_refused("{\"pad\": [], \"mouse_x\": 3}", "\"mouse_x\"");
    assert_frame_1_refused("{\"dpad\": 1, \"pad\": []}", "\"dpad\"");
    assert_frame_1_refused("{\"pad\": [], \"touch\": []}",
                           "unexpected key \"touch\": a frame holds only \"pad\", \"resize\", \"touches\" and \"mouse\"");
}

static void test_a_frame_key_given_twice_is_refused(void) {
    assert_frame_1_refused("{\"pad\": [1], \"pad\": [2]}", "\"pad\" given twice");
    assert_frame_1_refused("{\"resize\": [390, 844], \"pad\": [], \"resize\": [844, 390]}", "\"resize\" given twice");
    assert_frame_1_refused("{\"touches\": [], \"touches\": [[1, 2, 3]]}", "\"touches\" given twice");
    assert_frame_1_refused("{\"mouse\": [1, 2, 0], \"mouse\": [1, 2, 1]}", "\"mouse\" given twice");
}

static void assert_member_refused(const char *member, const char *problem) {
    char text[256];
    snprintf(text, sizeof(text), "{\"input\": \"pad\", %s, \"frames\": []}", member);
    assert_refused(text, problem);
}

static void test_initial_save_is_decoded_in_either_case(void) {
    const char *text = "{\"input\": \"pad\", \"initial_save\": \"aBcD\", \"frames\": []}";
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    assert(replay != NULL);
    size_t count = 0;
    const unsigned char *save = replay_initial_save(replay, &count);
    assert(count == 2 && save[0] == 0xab && save[1] == 0xcd);
    replay_free(replay);
}

static void test_a_bad_initial_save_is_refused(void) {
    assert_member_refused("\"initial_save\": 12", "\"initial_save\"");
    assert_member_refused("\"initial_save\": 12", "12");
    assert_member_refused("\"initial_save\": \"\"", "\"initial_save\" \"\"");
    assert_member_refused("\"initial_save\": \"abc\"", "\"initial_save\" \"abc\"");
    assert_member_refused("\"initial_save\": \"zz\"", "\"initial_save\" \"zz\"");
    assert_member_refused("\"initial_save\": \"ab\", \"initial_save\": \"cd\"", "\"initial_save\"");
    assert_member_refused("\"initial_save\": \"ab\", \"initial_save\": \"cd\"", "twice");
}

static void test_a_bad_bindings_name_is_refused(void) {
    static const char *const BAD[] = {"personal", "../x.cfg", "bindings.cfg", ".x.cfg", "x.txt", "a b.cfg", ".cfg"};
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        char member[128];
        char problem[128];
        snprintf(member, sizeof(member), "\"bindings\": \"%s\"", BAD[i]);
        snprintf(problem, sizeof(problem), "\"bindings\" \"%s\"", BAD[i]);
        assert_member_refused(member, problem);
    }
    assert_member_refused("\"bindings\": \"personal\"", "personal bindings.cfg");
    char long_name[66];
    memset(long_name, 'a', 61);
    memcpy(long_name + 61, ".cfg", 5);
    char member[128];
    snprintf(member, sizeof(member), "\"bindings\": \"%s\"", long_name);
    assert_member_refused(member, "\"bindings\" \"aaaa");
    assert_member_refused(member, "64");
    assert_member_refused("\"bindings\": 3", "\"bindings\" 3");
    assert_member_refused("\"bindings\": \"a.cfg\", \"bindings\": \"b.cfg\"", "\"bindings\" given twice");
}

static void test_a_fixture_name_of_64_characters_is_accepted(void) {
    char text[256];
    char name[65];
    memset(name, 'a', 60);
    memcpy(name + 60, ".cfg", 5);
    snprintf(text, sizeof(text), "{\"input\": \"pad\", \"bindings\": \"%s\", \"frames\": []}", name);
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    assert(replay != NULL && strcmp(replay_bindings(replay), name) == 0);
    replay_free(replay);
}

static replay_t *parsed(const char *text);

static void test_pad_sheet_names_a_png_next_to_the_replay(void) {
    char text[256];
    char name[65];
    memset(name, 'a', 60);
    memcpy(name + 60, ".png", 5);
    static const char *const GOOD[] = {"a.png", "pad.png", "pad_flat-2.x.png"};
    for (size_t i = 0; i < sizeof(GOOD) / sizeof(GOOD[0]) + 1; i++) {
        const char *sheet = i < sizeof(GOOD) / sizeof(GOOD[0]) ? GOOD[i] : name;
        snprintf(text, sizeof(text), "{\"input\": \"pad\", \"pad_sheet\": \"%s\", \"frames\": []}", sheet);
        replay_t *replay = parsed(text);
        assert(strcmp(replay_pad_sheet(replay), sheet) == 0);
        replay_free(replay);
    }
    replay_t *replay = parsed("{\"input\": \"pad\", \"frames\": []}");
    assert(replay_pad_sheet(replay) == NULL);
    replay_free(replay);
}

static void test_a_bad_pad_sheet_is_refused(void) {
    static const char *const BAD[] = {"../x.png", ".x.png", "x.cfg", "a b.png", ".png", "x.PNG", "x.png/"};
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        char member[128];
        char problem[160];
        snprintf(member, sizeof(member), "\"pad_sheet\": \"%s\"", BAD[i]);
        snprintf(problem, sizeof(problem),
                 "\"pad_sheet\" \"%s\" is not a .png file name of 5-64 characters A-Za-z0-9_.- next to the replay", BAD[i]);
        assert_member_refused(member, problem);
    }
    char long_name[66];
    memset(long_name, 'a', 61);
    memcpy(long_name + 61, ".png", 5);
    char member[128];
    snprintf(member, sizeof(member), "\"pad_sheet\": \"%s\"", long_name);
    assert_member_refused(member, "\"pad_sheet\" \"aaaa");
    assert_member_refused("\"pad_sheet\": 3", "\"pad_sheet\" 3 is not a .png file name");
    assert_member_refused("\"pad_sheet\": null", "\"pad_sheet\" null is not a .png file name");
    assert_member_refused("\"pad_sheet\": \"a.png\", \"pad_sheet\": \"b.png\"", "\"pad_sheet\" given twice");
}

static void test_attract_after_is_read_up_to_the_largest_int(void) {
    const char *text = "{\"input\": \"pad\", \"attract_after\": 2147483647, \"frames\": []}";
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    assert(replay != NULL && replay_attract_after(replay) == 2147483647);
    replay_free(replay);
}

static void test_a_bad_attract_after_is_refused(void) {
    assert_member_refused("\"attract_after\": 0", "\"attract_after\" 0 ");
    assert_member_refused("\"attract_after\": -1", "\"attract_after\" -1 ");
    assert_member_refused("\"attract_after\": 2147483648", "\"attract_after\" 2147483648 ");
    assert_member_refused("\"attract_after\": \"3\"", "\"attract_after\" \"3\"");
    assert_member_refused("\"attract_after\": 3, \"attract_after\": 4", "\"attract_after\" given twice");
}

static void test_a_replay_with_start_is_refused(void) {
    assert_member_refused("\"start\": \"resume\"", "\"start\"");
    assert_member_refused("\"start\": \"resume\"", "not supported");
}

static void test_a_malformed_replay_is_refused_with_its_problem_named(void) {
    assert_refused("", "unexpected end of text");
    assert_refused("frames", "not JSON");
    assert_refused("[]", "not an object");
    assert_refused("{\"pad\": []}", "\"frames\" missing");
    assert_refused("{\"input\": \"pad\"}", "\"frames\" missing");
    assert_refused("{\"frames\": {}}", "\"frames\" is not an array");
    assert_refused("{\"frames\": [{}, 3]}", "frame 1 is not an object");
    assert_refused("{\"frames\": [], \"x\": 1.5}", "fraction");
    assert_refused("{\"frames\": [], \"x\": 1e3}", "exponent");
    assert_refused("{\"frames\": [], \"x\": \"abc}", "unterminated string");
    assert_refused("{\"frames\": [], \"x\": \"a\\qb\"}", "bad escape");
    assert_refused("{\"frames\": []} []", "trailing text");
}

static void test_the_error_gives_the_byte_offset_of_the_problem(void) {
    assert_refused("{\"frames\": [], \"x\": 1.5}", "offset 20:");
    assert_refused("{\"frames\": [{}, 3]}", "offset 16:");
}

static void window_of(const char *text, int *width, int *height) {
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    if (replay == NULL) {
        fprintf(stderr, "%s\n", error);
    }
    assert(replay != NULL);
    replay_window(replay, width, height);
    replay_free(replay);
}

static void test_a_replay_without_window_runs_at_640_by_480(void) {
    int width = 0;
    int height = 0;
    window_of("{\"input\": \"pad\", \"frames\": []}", &width, &height);
    assert(width == 640 && height == 480);
}

static void test_window_is_read(void) {
    int width = 0;
    int height = 0;
    window_of("{\"input\": \"pad\", \"window\": [390, 844], \"frames\": []}", &width, &height);
    assert(width == 390 && height == 844);
    window_of("{\"input\": \"pad\", \"window\": [4096, 1], \"frames\": []}", &width, &height);
    assert(width == 4096 && height == 1);
}

static void test_a_bad_window_is_refused(void) {
    static const char *const BAD[] = {
        "3", "\"390x844\"", "{}", "[]", "[390]", "[390, 844, 1]", "[390, \"844\"]", "[390, null]", "[[390], 844]",
        "[0, 844]", "[390, 0]", "[4097, 844]", "[390, 4097]", "[-1, 844]",
    };
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        char member[128];
        snprintf(member, sizeof(member), "\"window\": %s", BAD[i]);
        assert_member_refused(member, "\"window\" ");
        assert_member_refused(member, " is not [width, height] with integers 1-4096");
    }
    assert_member_refused("\"window\": [390, 844], \"window\": [640, 480]", "\"window\" given twice");
}

static replay_t *parsed(const char *text) {
    char error[256] = "";
    replay_t *replay = replay_parse("memory.json", text, strlen(text), error, sizeof(error));
    if (replay == NULL) {
        fprintf(stderr, "%s\n", error);
    }
    assert(replay != NULL);
    return replay;
}

static void assert_no_resize(const replay_t *replay, size_t frame) {
    int width = -1;
    int height = -1;
    assert(!replay_frame_resize(replay, frame, &width, &height));
}

static void assert_resize(const replay_t *replay, size_t frame, int expected_width, int expected_height) {
    int width = -1;
    int height = -1;
    assert(replay_frame_resize(replay, frame, &width, &height));
    assert(width == expected_width && height == expected_height);
}

static void test_the_resizes_of_a_replay_are_read(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"window\": [390, 844], \"frames\": [\n"
                              "  {\"resize\": [844, 390], \"pad\": [1]},\n"
                              "  {\"pad\": []},\n"
                              "  {\"pad\": [], \"resize\": [390, 844]},\n"
                              "  {\"pad\": [2], \"resize\": [640, 480]},\n"
                              "  {\"pad\": []}]}");
    assert(replay_frame_count(replay) == 5);
    assert_resize(replay, 0, 844, 390);
    assert_no_resize(replay, 1);
    assert_resize(replay, 2, 390, 844);
    assert_resize(replay, 3, 640, 480);
    assert_no_resize(replay, 4);
    assert(replay_frame_held(replay, 0) == 0x1 && replay_frame_held(replay, 3) == 0x2);
    int width = 0;
    int height = 0;
    replay_window(replay, &width, &height);
    assert(width == 390 && height == 844);
    replay_free(replay);
}

static void test_a_replay_without_resize_has_none_on_any_frame(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"frames\": [{\"pad\": []}, {\"pad\": [3]}]}");
    assert_no_resize(replay, 0);
    assert_no_resize(replay, 1);
    replay_free(replay);
}

static void test_a_bad_resize_is_refused(void) {
    static const char *const BAD[] = {
        "3", "{}", "[]", "[390]", "[390, 844, 1]", "[390, \"844\"]", "[390, null]", "[0, 844]", "[4097, 844]", "[390, -2]",
    };
    for (size_t i = 0; i < sizeof(BAD) / sizeof(BAD[0]); i++) {
        char frame[128];
        snprintf(frame, sizeof(frame), "{\"pad\": [], \"resize\": %s}", BAD[i]);
        assert_frame_1_refused(frame, "\"resize\" ");
        assert_frame_1_refused(frame, " is not [width, height] with integers 1-4096");
    }
    assert_frame_1_refused("{\"pad\": [], \"resize\": [390, 844, 1]}", "[390, 844, 1]");
}

static void test_a_resize_to_the_size_in_force_is_refused(void) {
    static const char *const ALREADY = "is already the window size: LÖVE calls love.resize only on a change";
    assert_refused("{\"input\": \"pad\", \"frames\": [{\"pad\": [], \"resize\": [640, 480]}]}", "f0: \"resize\" [640, 480] ");
    assert_refused("{\"input\": \"pad\", \"frames\": [{\"pad\": [], \"resize\": [640, 480]}]}", ALREADY);
    assert_refused("{\"input\": \"pad\", \"window\": [390, 844], \"frames\": [{\"pad\": []}, {\"pad\": [], \"resize\": [390, 844]}]}",
                   "f1: \"resize\" [390, 844] ");
    const char *twice = "{\"input\": \"pad\", \"window\": [390, 844], \"frames\": [{\"pad\": [], \"resize\": [844, 390]},\n"
                        "  {\"pad\": []}, {\"pad\": [], \"resize\": [844, 390]}]}";
    assert_refused(twice, "f2: \"resize\" [844, 390] ");
    assert_refused(twice, ALREADY);
}

static void test_a_frame_without_pad_holds_no_button(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"frames\": [{\"pad\": [3]}, {}, {\"resize\": [390, 844]}]}");
    assert(replay_frame_held(replay, 0) == 0x4);
    assert(replay_frame_held(replay, 1) == 0);
    assert(replay_frame_held(replay, 2) == 0);
    assert_resize(replay, 2, 390, 844);
    replay_free(replay);
}

static void assert_touches(const replay_t *replay, size_t frame, size_t expected_count, const replay_touch_t *expected) {
    size_t count = 99;
    const replay_touch_t *touches = replay_frame_touches(replay, frame, &count);
    assert(count == expected_count);
    for (size_t i = 0; i < count; i++) {
        assert(touches[i].id == expected[i].id && touches[i].x == expected[i].x && touches[i].y == expected[i].y);
    }
}

static void assert_mouse(const replay_t *replay, size_t frame, int x, int y, int left) {
    replay_mouse_t mouse = replay_frame_mouse(replay, frame);
    assert(mouse.x == x && mouse.y == y && mouse.left == left);
}

/* Transcribed from the file by hand, not by the reader under test. */
static void test_reads_the_touches_of_the_touch_fixture(void) {
    char error[256] = "";
    replay_t *replay = replay_read("tests/fixtures/touch_frames.json", error, sizeof(error));
    if (replay == NULL) {
        fprintf(stderr, "%s\n", error);
    }
    assert(replay != NULL);
    assert(replay_frame_count(replay) == 12);
    assert_touches(replay, 0, 0, NULL);
    assert_touches(replay, 1, 1, (const replay_touch_t[]){{7, 100, 700}});
    assert_touches(replay, 2, 1, (const replay_touch_t[]){{7, 120, 690}});
    assert_touches(replay, 3, 2, (const replay_touch_t[]){{7, 120, 690}, {3, 300, 750}});
    assert_touches(replay, 4, 1, (const replay_touch_t[]){{3, 300, 750}});
    assert_touches(replay, 5, 0, NULL);
    assert_touches(replay, 6, 0, NULL);
    assert_touches(replay, 7, 0, NULL);
    assert_touches(replay, 8, 1, (const replay_touch_t[]){{7, 10, 20}});
    assert_touches(replay, 9, 1, (const replay_touch_t[]){{1, 843, 389}});
    assert_touches(replay, 10, 0, NULL);
    assert_touches(replay, 11, 0, NULL);
    replay_free(replay);
}

/* Transcribed from the file by hand; the kept mouse of f9 is outside the
 * window of its resize, which LÖVE allows. */
static void test_reads_the_mouse_and_the_buttons_of_the_touch_fixture(void) {
    char error[256] = "";
    replay_t *replay = replay_read("tests/fixtures/touch_frames.json", error, sizeof(error));
    assert(replay != NULL);
    for (size_t frame = 0; frame < 6; frame++) {
        assert_mouse(replay, frame, 0, 0, 0);
    }
    assert_mouse(replay, 6, 200, 400, 1);
    assert_mouse(replay, 7, 200, 400, 1);
    for (size_t frame = 8; frame < 12; frame++) {
        assert_mouse(replay, frame, 200, 400, 0);
    }
    for (size_t frame = 0; frame < 12; frame++) {
        unsigned expected = frame == 3 ? 1u << 4 : frame == 7 ? 1u << 0 : frame == 11 ? 1u << 1 : 0;
        assert(replay_frame_held(replay, frame) == expected);
    }
    assert_resize(replay, 9, 844, 390);
    replay_free(replay);
}

static void test_touches_are_checked_against_the_window_after_the_frames_resize(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"frames\": [{\"resize\": [800, 600], \"touches\": [[1, 799, 599]]},\n"
                              "  {\"touches\": [[2, 0, 0]], \"resize\": [100, 50], \"mouse\": [99, 49, 1]}]}");
    assert_touches(replay, 0, 1, (const replay_touch_t[]){{1, 799, 599}});
    assert_mouse(replay, 1, 99, 49, 1);
    replay_free(replay);
    assert_refused("{\"input\": \"pad\", \"frames\": [{\"touches\": [[1, 700, 10]], \"resize\": [700, 480]}]}",
                   "f0: touch 1 at (700, 10) is outside the 700x480 window");
    assert_refused("{\"input\": \"pad\", \"window\": [390, 844], \"frames\": [{}, {\"touches\": [[4, 390, 10]]}]}",
                   "f1: touch 4 at (390, 10) is outside the 390x844 window");
    assert_refused("{\"input\": \"pad\", \"frames\": [{\"resize\": [100, 50], \"mouse\": [10, 50, 0]}]}",
                   "f0: mouse at (10, 50) is outside the 100x50 window");
}

static void test_touch_ids_up_to_the_largest_int_are_read(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"frames\": [{\"touches\": [[2147483647, 0, 0], [1, 639, 479]]}]}");
    assert_touches(replay, 0, 2, (const replay_touch_t[]){{2147483647, 0, 0}, {1, 639, 479}});
    replay_free(replay);
}

static void test_bad_touches_are_refused(void) {
    assert_frame_1_refused("{\"touches\": 3}", "\"touches\" is an integer, not an array of [id, x, y]");
    assert_frame_1_refused("{\"touches\": {}}", "\"touches\" is an object, not an array of [id, x, y]");
    assert_frame_1_refused("{\"touches\": [[1, 2]]}", "touch [1, 2] is not [id, x, y] with integers");
    assert_frame_1_refused("{\"touches\": [[1, 2, 3, 4]]}", "touch [1, 2, 3, ...] is not [id, x, y] with integers");
    assert_frame_1_refused("{\"touches\": [[1, \"2\", 3]]}", "touch [1, \"2\", 3] is not [id, x, y] with integers");
    assert_frame_1_refused("{\"touches\": [5]}", "touch 5 is not [id, x, y] with integers");
    assert_frame_1_refused("{\"touches\": [[0, 2, 3]]}", "touch id 0 is not 1-2147483647");
    assert_frame_1_refused("{\"touches\": [[2147483648, 2, 3]]}", "touch id 2147483648 is not 1-2147483647");
    assert_frame_1_refused("{\"touches\": [[4, 2, 3], [5, 6, 7], [4, 8, 9]]}", "touch id 4 listed twice");
    assert_frame_1_refused("{\"touches\": [[1,0,0],[2,0,0],[3,0,0],[4,0,0],[5,0,0],[6,0,0],[7,0,0],[8,0,0],[9,0,0],"
                           "[10,0,0],[11,0,0]]}",
                           "11 touches, more than 10");
    assert_frame_1_refused("{\"touches\": [[1, 640, 0]]}", "touch 1 at (640, 0) is outside the 640x480 window");
    assert_frame_1_refused("{\"touches\": [[1, 0, 480]]}", "touch 1 at (0, 480) is outside the 640x480 window");
    assert_frame_1_refused("{\"touches\": [[1, -1, 0]]}", "touch 1 at (-1, 0) is outside the 640x480 window");
    assert_frame_1_refused("{\"touches\": [[1, 0, -1]]}", "touch 1 at (0, -1) is outside the 640x480 window");
}

static void test_a_bad_mouse_is_refused(void) {
    assert_frame_1_refused("{\"mouse\": [1, 2]}", "\"mouse\" [1, 2] is not [x, y, left] with integers, left 0 or 1");
    assert_frame_1_refused("{\"mouse\": [1, 2, 2]}", "\"mouse\" [1, 2, 2] is not [x, y, left] with integers, left 0 or 1");
    assert_frame_1_refused("{\"mouse\": [1, 2, true]}", "\"mouse\" [1, 2, true] is not [x, y, left]");
    assert_frame_1_refused("{\"mouse\": 3}", "\"mouse\" 3 is not [x, y, left]");
    assert_frame_1_refused("{\"mouse\": [640, 2, 0]}", "mouse at (640, 2) is outside the 640x480 window");
    assert_frame_1_refused("{\"mouse\": [0, -1, 1]}", "mouse at (0, -1) is outside the 640x480 window");
}

static void test_screens_lists_the_frames_to_record(void) {
    replay_t *replay = parsed("{\"input\": \"pad\", \"screens\": [0, 2, 3], \"frames\": [{}, {\"pad\": []}, {}, {}]}");
    static const int LISTED[] = {1, 0, 1, 1};
    for (size_t frame = 0; frame < 4; frame++) {
        assert(replay_frame_screen_listed(replay, frame) == LISTED[frame]);
    }
    replay_free(replay);
    replay = parsed("{\"input\": \"pad\", \"screens\": [], \"frames\": [{}]}");
    assert(!replay_frame_screen_listed(replay, 0));
    replay_free(replay);
    replay = parsed("{\"input\": \"pad\", \"frames\": [{}]}");
    assert(!replay_frame_screen_listed(replay, 0));
    replay_free(replay);
    replay = parsed("{\"input\": \"pad\", \"screens\": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15], "
                    "\"frames\": [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}]}");
    assert(replay_frame_screen_listed(replay, 15));
    replay_free(replay);
}

static void assert_screens_refused(const char *screens, const char *problem) {
    char text[512];
    snprintf(text, sizeof(text), "{\"input\": \"pad\", %s, \"frames\": [{}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, "
                                 "{}, {}, {}, {}, {}, {}, {}, {}]}", screens);
    assert_refused(text, problem);
}

static void test_a_bad_screens_is_refused(void) {
    assert_screens_refused("\"screens\": 3", "\"screens\" is an integer, not an array of frame numbers");
    assert_screens_refused("\"screens\": {}", "\"screens\" is an object, not an array of frame numbers");
    assert_screens_refused("\"screens\": [1, \"2\"]", "\"screens\" entry \"2\" is not an integer");
    assert_screens_refused("\"screens\": [null]", "\"screens\" entry null is not an integer");
    assert_screens_refused("\"screens\": [20]", "\"screens\" entry 20 is not a frame of this replay (0-19)");
    assert_screens_refused("\"screens\": [-1]", "\"screens\" entry -1 is not a frame of this replay (0-19)");
    assert_screens_refused("\"screens\": [3, 3]", "\"screens\" entry 3 is not after 3");
    assert_screens_refused("\"screens\": [4, 2]", "\"screens\" entry 2 is not after 4");
    assert_screens_refused("\"screens\": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]",
                           "\"screens\" lists 17 frames, more than 16");
    assert_screens_refused("\"screens\": [0], \"screens\": [1]", "\"screens\" given twice");
    assert_refused("{\"input\": \"pad\", \"screens\": [0], \"frames\": []}",
                   "\"screens\" entry 0 is not a frame of this replay (the replay has no frame)");
}

static void test_a_missing_file_is_refused_with_its_path(void) {
    char error[256] = "";
    assert(replay_read("tests/e2e/no_such_replay.json", error, sizeof(error)) == NULL);
    assert(strstr(error, "tests/e2e/no_such_replay.json") != NULL);
}

int main(void) {
    test_reads_every_e2e_replay_with_its_frame_count_window_and_no_resize();
    test_every_e2e_replay_is_listed();
    test_reads_the_held_buttons_of_pad_title();
    test_reads_the_start_state_members_of_e2e_replays();
    test_an_empty_frames_array_has_no_frame();
    test_unknown_keys_and_every_value_kind_are_accepted();
    test_the_buttons_of_a_frame_are_read_in_any_order();
    test_a_replay_without_pad_input_is_refused();
    test_a_frame_with_wrong_buttons_is_refused();
    test_a_frame_with_another_key_is_refused();
    test_initial_save_is_decoded_in_either_case();
    test_a_bad_initial_save_is_refused();
    test_a_bad_bindings_name_is_refused();
    test_a_fixture_name_of_64_characters_is_accepted();
    test_pad_sheet_names_a_png_next_to_the_replay();
    test_a_bad_pad_sheet_is_refused();
    test_attract_after_is_read_up_to_the_largest_int();
    test_a_bad_attract_after_is_refused();
    test_a_replay_with_start_is_refused();
    test_a_malformed_replay_is_refused_with_its_problem_named();
    test_the_error_gives_the_byte_offset_of_the_problem();
    test_a_replay_without_window_runs_at_640_by_480();
    test_window_is_read();
    test_a_bad_window_is_refused();
    test_the_resizes_of_a_replay_are_read();
    test_a_replay_without_resize_has_none_on_any_frame();
    test_a_bad_resize_is_refused();
    test_a_resize_to_the_size_in_force_is_refused();
    test_a_frame_without_pad_holds_no_button();
    test_a_frame_key_given_twice_is_refused();
    test_reads_the_touches_of_the_touch_fixture();
    test_reads_the_mouse_and_the_buttons_of_the_touch_fixture();
    test_touches_are_checked_against_the_window_after_the_frames_resize();
    test_touch_ids_up_to_the_largest_int_are_read();
    test_bad_touches_are_refused();
    test_a_bad_mouse_is_refused();
    test_screens_lists_the_frames_to_record();
    test_a_bad_screens_is_refused();
    test_a_missing_file_is_refused_with_its_path();
    printf("test_fake_love_replay: ok\n");
    return 0;
}
