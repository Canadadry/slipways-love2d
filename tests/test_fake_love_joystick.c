#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_joystick_capture.bin"
#define STATE "tmp/test_fake_love_joystick_state.txt"
#define STDERR_LOG "tmp/test_fake_love_joystick_stderr.txt"

#define ALL_HELD "held dpup dpdown dpleft dpright a b x y leftshoulder rightshoulder start back"

static int run_fake_love(const char *environment, const char *args) {
    return fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
}

static void assert_state(const char *expected) {
    char *state = fake_love_cli_read_file(STATE, NULL);
    assert(state != NULL && "the state file must be written");
    if (strcmp(state, expected) != 0) {
        fprintf(stderr, "state:\n%s\nexpected:\n%s\n", state, expected);
        assert(!"unexpected state");
    }
    free(state);
}

static void assert_runs(const char *environment, const char *args) {
    int status = run_fake_love(environment, args);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

/* `needles` ends with NULL; each must appear in stderr. */
static void assert_failure(const char *environment, const char *args, const char *const *needles) {
    int status = run_fake_love(environment, args);
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL);
    if (status != 1) {
        fprintf(stderr, "fake_love %s %s: status %d, expected 1\n%s", environment, args, status, err);
        assert(!"unexpected exit status");
    }
    for (const char *const *needle = needles; *needle != NULL; needle++)
        if (strstr(err, *needle) == NULL) {
            fprintf(stderr, "fake_love %s %s: stderr lacks \"%s\":\n%s", environment, args, *needle, err);
            assert(!"stderr must name the cause");
        }
    free(err);
    assert(!fake_love_cli_file_exists(CAPTURE) && "a failed run must remove its capture");
    assert(!fake_love_cli_file_exists(STATE) && "a failed run must remove its state file");
}

static void test_callbacks_follow_the_replay_buttons(void) {
    assert_runs("", FIXTURES "pad_log " FIXTURES "pad_log.json " CAPTURE " " STATE);
    assert_state("load joysticks 1 a false\n"
                 "f0 pressed dpup down true same true\n"
                 "f0 pressed a down true same true\n"
                 "f0 update held dpup a\n"
                 "f0 draw held dpup a\n"
                 "f1 update held dpup a\n"
                 "f1 draw held dpup a\n"
                 "f2 released dpup down false same true\n"
                 "f2 update held a\n"
                 "f2 draw held a\n"
                 "f3 released a down false same true\n"
                 "f3 pressed dpdown down true same true\n"
                 "f3 pressed b down true same true\n"
                 "f3 update held dpdown b\n"
                 "f3 draw held dpdown b\n"
                 "f4 pressed dpup down true same true\n"
                 "f4 pressed dpleft down true same true\n"
                 "f4 pressed dpright down true same true\n"
                 "f4 pressed a down true same true\n"
                 "f4 pressed x down true same true\n"
                 "f4 pressed y down true same true\n"
                 "f4 pressed leftshoulder down true same true\n"
                 "f4 pressed rightshoulder down true same true\n"
                 "f4 pressed start down true same true\n"
                 "f4 pressed back down true same true\n"
                 "f4 update " ALL_HELD "\n"
                 "f4 draw " ALL_HELD "\n"
                 "f5 released dpup down false same true\n"
                 "f5 released dpdown down false same true\n"
                 "f5 released dpleft down false same true\n"
                 "f5 released dpright down false same true\n"
                 "f5 released a down false same true\n"
                 "f5 released b down false same true\n"
                 "f5 released x down false same true\n"
                 "f5 released y down false same true\n"
                 "f5 released leftshoulder down false same true\n"
                 "f5 released rightshoulder down false same true\n"
                 "f5 released start down false same true\n"
                 "f5 released back down false same true\n"
                 "f5 update held\n"
                 "f5 draw held\n"
                 "f6 update held\n"
                 "f6 draw held\n"
                 "f7 pressed start down true same true\n"
                 "f7 pressed back down true same true\n"
                 "f7 update held start back\n"
                 "f7 draw held start back\n"
                 "f8 update held start back\n"
                 "f8 draw held start back\n"
                 "end held start back\n");
}

static void test_polling_game_needs_no_callbacks(void) {
    assert_runs("", FIXTURES "pad_poll " FIXTURES "pad_log.json " CAPTURE " " STATE);
    assert_state("love.joystick getJoysticks, new list true, same joystick true, count 1\n"
                 "f0 dpup or b true, guide false, leftstick or rightstick false\n"
                 "f1 dpup or b true, guide false, leftstick or rightstick false\n"
                 "f2 dpup or b false, guide false, leftstick or rightstick false\n"
                 "f3 dpup or b true, guide false, leftstick or rightstick false\n"
                 "f4 dpup or b true, guide false, leftstick or rightstick false\n"
                 "f5 dpup or b false, guide false, leftstick or rightstick false\n"
                 "f6 dpup or b false, guide false, leftstick or rightstick false\n"
                 "f7 dpup or b false, guide false, leftstick or rightstick false\n"
                 "f8 dpup or b false, guide false, leftstick or rightstick false\n");
}

static void test_pad_callback_that_is_not_a_function_fails(void) {
    const char *const needles[] = {"love.gamepadpressed is a number, not a function (f0)", NULL};
    assert_failure("", FIXTURES "pad_not_function " FIXTURES "pad_log.json " CAPTURE " " STATE, needles);
}

#define MISUSE FIXTURES "pad_misuse " FIXTURES "pad_log.json " CAPTURE " " STATE

static void test_pad_callback_error_names_the_callback_the_frame_and_the_line(void) {
    const char *const needles[] = {"love.gamepadreleased f2: ", "main.lua:6: raised in love.gamepadreleased",
                                   "stack traceback", NULL};
    assert_failure("FAKE_LOVE_MISUSE=raise", MISUSE, needles);
}

static void test_is_gamepad_down_misuse_fails(void) {
    const char *const unknown[] = {"love.update f0: ", "main.lua:12: Invalid gamepad button: select", NULL};
    assert_failure("FAKE_LOVE_MISUSE='unknown name'", MISUSE, unknown);
    const char *const dot[] = {"main.lua:14: ", "isGamepadDown", "colon", NULL};
    assert_failure("FAKE_LOVE_MISUSE=dot", MISUSE, dot);
    const char *const no_name[] = {"main.lua:16: ", "isGamepadDown", NULL};
    assert_failure("FAKE_LOVE_MISUSE='no name'", MISUSE, no_name);
    const char *const number[] = {"main.lua:18: ", "isGamepadDown", "number", NULL};
    assert_failure("FAKE_LOVE_MISUSE=number", MISUSE, number);
}

static void test_invalid_replay_runs_nothing(void) {
    const char *const needles[] = {FIXTURES "pad_invalid.json", "f1", "13", NULL};
    assert_failure("", FIXTURES "pad_log " FIXTURES "pad_invalid.json " CAPTURE " " STATE, needles);
}

int main(void) {
    test_callbacks_follow_the_replay_buttons();
    test_polling_game_needs_no_callbacks();
    test_pad_callback_that_is_not_a_function_fails();
    test_pad_callback_error_names_the_callback_the_frame_and_the_line();
    test_is_gamepad_down_misuse_fails();
    test_invalid_replay_runs_nothing();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_joystick: all tests passed\n");
    return 0;
}
