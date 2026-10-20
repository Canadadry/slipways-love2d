#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_failures_capture.bin"
#define STATE "tmp/test_fake_love_failures_state.txt"
#define STDERR_LOG "tmp/test_fake_love_failures_stderr.txt"

/* `needles` ends with NULL; each must appear in stderr. */
static void assert_failure_in(const char *environment, const char *args, int expected_status,
                              const char *const *needles) {
    int status = fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL);
    if (status != expected_status) {
        fprintf(stderr, "fake_love %s: status %d, expected %d\n%s", args, status, expected_status, err);
        assert(!"unexpected exit status");
    }
    for (const char *const *needle = needles; *needle != NULL; needle++)
        if (strstr(err, *needle) == NULL) {
            fprintf(stderr, "fake_love %s: stderr lacks \"%s\":\n%s", args, *needle, err);
            assert(!"stderr must name the cause");
        }
    free(err);
    assert(!fake_love_cli_file_exists(CAPTURE) && "a failed run must remove its capture");
    assert(!fake_love_cli_file_exists(STATE) && "a failed run must remove its state file");
}

static void assert_failure(const char *args, int expected_status, const char *const *needles) {
    assert_failure_in("", args, expected_status, needles);
}

static void test_wrong_argument_count_is_a_usage_error(void) {
    const char *const needles[] = {"usage: fake_love", NULL};
    assert_failure("", 2, needles);
    assert_failure(FIXTURES "counting " FIXTURES "three_frames.json", 2, needles);
    assert_failure(FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE " " STATE " extra", 2, needles);
}

static void test_dash_argument_is_a_usage_error(void) {
    const char *const needles[] = {"usage: fake_love", NULL};
    assert_failure("--verbose " FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE, 2, needles);
    assert_failure(FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE " -", 2, needles);
}

static void test_missing_game_directory_fails(void) {
    const char *const needles[] = {"fake_love: ", FIXTURES "no_such_game", NULL};
    assert_failure(FIXTURES "no_such_game " FIXTURES "three_frames.json " CAPTURE " " STATE, 1, needles);
}

static void test_missing_main_lua_fails(void) {
    const char *const needles[] = {"main.lua", "no_main", NULL};
    assert_failure(FIXTURES "no_main " FIXTURES "three_frames.json " CAPTURE " " STATE, 1, needles);
}

static void test_missing_replay_fails(void) {
    const char *const needles[] = {FIXTURES "no_such_replay.json", NULL};
    assert_failure(FIXTURES "counting " FIXTURES "no_such_replay.json " CAPTURE " " STATE, 1, needles);
}

static void test_unwritable_capture_fails(void) {
    const char *const needles[] = {"tmp/no_such_directory/capture.bin", NULL};
    assert_failure(FIXTURES "counting " FIXTURES "three_frames.json tmp/no_such_directory/capture.bin " STATE, 1,
                   needles);
}

static void test_unwritable_state_fails(void) {
    const char *const needles[] = {"tmp/no_such_directory/state.txt", NULL};
    assert_failure(FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE " tmp/no_such_directory/state.txt", 1,
                   needles);
}

#define RAISES FIXTURES "raises " FIXTURES "three_frames.json " CAPTURE " " STATE

static void test_lua_errors_name_the_phase_the_frame_and_the_line(void) {
    const char *const conf_file[] = {"conf.lua: ", "conf.lua:3: raised in conf.lua", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=conf.lua", RAISES, 1, conf_file);
    const char *const conf[] = {"love.conf: ", "conf.lua:8: raised in love.conf", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=love.conf", RAISES, 1, conf);
    const char *const main_file[] = {"main.lua: ", "main.lua:4: raised in main.lua", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=main.lua", RAISES, 1, main_file);
    const char *const load[] = {"love.load: ", "main.lua:9: raised in love.load", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=love.load", RAISES, 1, load);
    const char *const update[] = {"love.update f1: ", "main.lua:15: raised in love.update", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=love.update", RAISES, 1, update);
    const char *const draw[] = {"love.draw f2: ", "main.lua:21: raised in love.draw", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=love.draw", RAISES, 1, draw);
    const char *const state[] = {"debug_state: ", "main.lua:28: raised in debug_state", "stack traceback", NULL};
    assert_failure_in("FAKE_LOVE_RAISE=debug_state", RAISES, 1, state);
}

static void test_callback_that_is_not_a_function_fails(void) {
    const char *const needles[] = {"love.draw is a number, not a function", NULL};
    assert_failure(FIXTURES "not_function " FIXTURES "three_frames.json " CAPTURE " " STATE, 1, needles);
}

static void test_missing_debug_state_fails_with_a_state_path(void) {
    const char *const needles[] = {"debug_state is a nil, not a function", NULL};
    assert_failure(FIXTURES "no_debug_state " FIXTURES "three_frames.json " CAPTURE " " STATE, 1, needles);
}

static void test_debug_state_returning_no_string_fails(void) {
    const char *const needles[] = {"debug_state returned a number, not a string", NULL};
    assert_failure(FIXTURES "state_not_string " FIXTURES "three_frames.json " CAPTURE " " STATE, 1, needles);
}

int main(void) {
    test_wrong_argument_count_is_a_usage_error();
    test_dash_argument_is_a_usage_error();
    test_missing_game_directory_fails();
    test_missing_main_lua_fails();
    test_missing_replay_fails();
    test_unwritable_capture_fails();
    test_unwritable_state_fails();
    test_lua_errors_name_the_phase_the_frame_and_the_line();
    test_callback_that_is_not_a_function_fails();
    test_missing_debug_state_fails_with_a_state_path();
    test_debug_state_returning_no_string_fails();
    remove(STDERR_LOG);
    printf("test_fake_love_failures: all tests passed\n");
    return 0;
}
