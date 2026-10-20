#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/replay.h"

/* Counted with `jq '.frames | length'`, not by the reader under test. */
static const struct {
    const char *path;
    size_t frames;
} E2E_REPLAYS[] = {
    {"tests/e2e/attract.json", 176},
    {"tests/e2e/pad_bankrupt.json", 95},
    {"tests/e2e/pad_build.json", 86},
    {"tests/e2e/pad_colonise.json", 447},
    {"tests/e2e/pad_empty_planet.json", 150},
    {"tests/e2e/pad_end_turn.json", 156},
    {"tests/e2e/pad_game_end.json", 114},
    {"tests/e2e/pad_message_box.json", 225},
    {"tests/e2e/pad_pause_restart.json", 80},
    {"tests/e2e/pad_planet_projects.json", 524},
    {"tests/e2e/pad_probe.json", 79},
    {"tests/e2e/pad_research.json", 502},
    {"tests/e2e/pad_restart_resume.json", 157},
    {"tests/e2e/pad_resume_at_start.json", 48},
    {"tests/e2e/pad_score.json", 98},
    {"tests/e2e/pad_slipway.json", 320},
    {"tests/e2e/pad_swap_aim_menu.json", 177},
    {"tests/e2e/pad_tech_costs.json", 558},
    {"tests/e2e/pad_title.json", 32},
    {"tests/e2e/pad_unbound_enter_aim.json", 48},
};

static void test_reads_the_frame_count_of_every_e2e_replay(void) {
    for (size_t i = 0; i < sizeof(E2E_REPLAYS) / sizeof(E2E_REPLAYS[0]); i++) {
        char error[256] = "";
        replay_t *replay = replay_read(E2E_REPLAYS[i].path, error, sizeof(error));
        if (replay == NULL) {
            fprintf(stderr, "%s\n", error);
        }
        assert(replay != NULL);
        assert(replay_frame_count(replay) == E2E_REPLAYS[i].frames);
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
    assert(frame_count_of("{ \"input\": \"pad\", \"attract_after\": -3, \"x\": [true, false, null, 0, {}],\n"
                          "  \"initial_save\": {}, \"bindings\": \"\",\n"
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
    assert_frame_1_refused("{}", "\"pad\"");
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

static void test_a_missing_file_is_refused_with_its_path(void) {
    char error[256] = "";
    assert(replay_read("tests/e2e/no_such_replay.json", error, sizeof(error)) == NULL);
    assert(strstr(error, "tests/e2e/no_such_replay.json") != NULL);
}

int main(void) {
    test_reads_the_frame_count_of_every_e2e_replay();
    test_every_e2e_replay_is_listed();
    test_reads_the_held_buttons_of_pad_title();
    test_an_empty_frames_array_has_no_frame();
    test_unknown_keys_and_every_value_kind_are_accepted();
    test_the_buttons_of_a_frame_are_read_in_any_order();
    test_a_replay_without_pad_input_is_refused();
    test_a_frame_with_wrong_buttons_is_refused();
    test_a_frame_with_another_key_is_refused();
    test_a_malformed_replay_is_refused_with_its_problem_named();
    test_the_error_gives_the_byte_offset_of_the_problem();
    test_a_missing_file_is_refused_with_its_path();
    printf("test_fake_love_replay: ok\n");
    return 0;
}
