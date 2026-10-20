#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_capture.bin"
#define STATE "tmp/test_fake_love_state.txt"
#define STDERR_LOG "tmp/test_fake_love_stderr.txt"

static int run_fake_love(const char *args) {
    return fake_love_cli_run("", args, CAPTURE, STATE, STDERR_LOG);
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

/* These games create no canvas: every frame is written as colour 0. */
static void assert_capture_black_frames(long frames) {
    long size = -1;
    char *capture = fake_love_cli_read_file(CAPTURE, &size);
    assert(capture != NULL && "the capture file must be created");
    assert(size == frames * 9600 && "one capture frame of 9 600 bytes per frame");
    for (long i = 0; i < size; i++)
        assert(capture[i] == 0);
    free(capture);
}

static void test_counting_game_runs_three_frames(void) {
    int status = run_fake_love(FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE " " STATE);
    assert(status == 0);
    assert_state("love empty\n"
                 "conf\n"
                 "love.conf\n"
                 "main\n"
                 "load table\n"
                 "update 1/60\n"
                 "draw\n"
                 "update 1/60\n"
                 "draw\n"
                 "update 1/60\n"
                 "draw\n");
    assert_capture_black_frames(3);
}

static void test_zero_frames_only_loads(void) {
    int status = run_fake_love(FIXTURES "counting " FIXTURES "zero_frames.json " CAPTURE " " STATE);
    assert(status == 0);
    assert_state("love empty\n"
                 "conf\n"
                 "love.conf\n"
                 "main\n"
                 "load table\n");
    assert_capture_black_frames(0);
}

static void test_game_without_conf_nor_draw_runs(void) {
    int status = run_fake_love(FIXTURES "minimal " FIXTURES "three_frames.json " CAPTURE " " STATE);
    assert(status == 0);
    assert_state("updates 3\n");
}

static void test_require_resolves_in_the_game_directory(void) {
    int status = run_fake_love(FIXTURES "modules " FIXTURES "zero_frames.json " CAPTURE " " STATE);
    assert(status == 0);
    assert_state("helper widgets\n");
}

/* Run from the fixtures directory, where Lua's default `./?.lua` would find
 * outside_module.lua. */
static void test_require_ignores_modules_outside_the_game_directory(void) {
    remove(STDERR_LOG);
    int status = system("cd " FIXTURES " && ../../../tmp/fake_love outside zero_frames.json ../../../" CAPTURE
                        " ../../../" STATE " > /dev/null 2> ../../../" STDERR_LOG);
    assert(status != -1 && WIFEXITED(status));
    assert(WEXITSTATUS(status) == 1);
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL && strstr(err, "outside_module") != NULL);
    free(err);
}

static void test_no_state_path_leaves_debug_state_uncalled(void) {
    int status = run_fake_love(FIXTURES "state_raises " FIXTURES "three_frames.json " CAPTURE);
    assert(status == 0);
    assert_capture_black_frames(3);
}

int main(void) {
    test_counting_game_runs_three_frames();
    test_zero_frames_only_loads();
    test_game_without_conf_nor_draw_runs();
    test_require_resolves_in_the_game_directory();
    test_require_ignores_modules_outside_the_game_directory();
    test_no_state_path_leaves_debug_state_uncalled();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love: all tests passed\n");
    return 0;
}
