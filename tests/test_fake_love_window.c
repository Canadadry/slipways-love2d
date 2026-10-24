#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_window_capture.bin"
#define STATE "tmp/test_fake_love_window_state.txt"
#define STDERR_LOG "tmp/test_fake_love_window_stderr.txt"
#define ARGUMENT                                                                                                       \
    "argument false " FIXTURES "window_log/main.lua:13: love.graphics.getDimensions: takes no argument, not 1\n"

static void assert_runs(const char *environment, const char *args) {
    int status = fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

static void test_a_replay_without_window_runs_at_640x480_with_no_resize_and_get_dimensions_takes_no_argument(void) {
    assert_runs("", FIXTURES "window_log " FIXTURES "three_frames.json " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "main 640x480\n"
                                     ARGUMENT
                                     "load 640x480\n"
                                     "f0 update 640x480\n"
                                     "f0 draw 640x480\n"
                                     "f1 update 640x480\n"
                                     "f1 draw 640x480\n"
                                     "f2 update 640x480\n"
                                     "f2 draw 640x480\n"
                                     "end 640x480\n");
}

static void test_resize_comes_first_in_its_frame_and_the_size_changes_at_that_frame(void) {
    assert_runs("", FIXTURES "window_log " FIXTURES "window_resizes.json " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "main 390x844\n"
                                     ARGUMENT
                                     "load 390x844\n"
                                     "f0 update 390x844\n"
                                     "f0 draw 390x844\n"
                                     "f1 resize number 844 390 844x390\n"
                                     "f1 pressed a 844x390\n"
                                     "f1 update 844x390\n"
                                     "f1 draw 844x390\n"
                                     "f2 update 844x390\n"
                                     "f2 draw 844x390\n"
                                     "f3 resize number 390 844 390x844\n"
                                     "f3 released a 390x844\n"
                                     "f3 update 390x844\n"
                                     "f3 draw 390x844\n"
                                     "f4 update 390x844\n"
                                     "f4 draw 390x844\n"
                                     "end 390x844\n");
}

static void test_a_resize_on_frame_0_precedes_the_first_update(void) {
    assert_runs("", FIXTURES "window_log " FIXTURES "window_resize_first.json " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "main 640x480\n"
                                     ARGUMENT
                                     "load 640x480\n"
                                     "f0 resize number 390 844 390x844\n"
                                     "f0 update 390x844\n"
                                     "f0 draw 390x844\n"
                                     "f1 update 390x844\n"
                                     "f1 draw 390x844\n"
                                     "end 390x844\n");
}

static void test_a_game_without_love_resize_still_sees_the_new_size(void) {
    assert_runs("FAKE_LOVE_NO_RESIZE=1", FIXTURES "window_log " FIXTURES "window_resize_first.json " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "main 640x480\n"
                                     ARGUMENT
                                     "load 640x480\n"
                                     "f0 update 390x844\n"
                                     "f0 draw 390x844\n"
                                     "f1 update 390x844\n"
                                     "f1 draw 390x844\n"
                                     "end 390x844\n");
}

static void assert_failure(const char *environment, const char *args, const char *const *needles) {
    fake_love_cli_assert_failure(environment, args, CAPTURE, STATE, STDERR_LOG, needles);
}

#define MISUSE FIXTURES "window_misuse " FIXTURES "window_resize_later.json " CAPTURE " " STATE

static void test_love_resize_that_is_not_a_function_fails(void) {
    const char *const needles[] = {"love.resize is a number, not a function (f2)", NULL};
    assert_failure("FAKE_LOVE_MISUSE='not function'", MISUSE, needles);
}

static void test_an_error_in_love_resize_names_the_callback_the_frame_and_the_line(void) {
    const char *const needles[] = {"love.resize f2: ", "main.lua:7: raised in love.resize", "stack traceback", NULL};
    assert_failure("FAKE_LOVE_MISUSE=raise", MISUSE, needles);
}

int main(void) {
    test_a_replay_without_window_runs_at_640x480_with_no_resize_and_get_dimensions_takes_no_argument();
    test_resize_comes_first_in_its_frame_and_the_size_changes_at_that_frame();
    test_a_resize_on_frame_0_precedes_the_first_update();
    test_a_game_without_love_resize_still_sees_the_new_size();
    test_love_resize_that_is_not_a_function_fails();
    test_an_error_in_love_resize_names_the_callback_the_frame_and_the_line();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_window: all tests passed\n");
    return 0;
}
