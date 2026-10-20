#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define FAKE_LOVE "./tmp/fake_love"
#define FIXTURES "tests/fixtures/fake_love/"
#define CAPTURE "tmp/test_fake_love_capture.bin"
#define STATE "tmp/test_fake_love_state.txt"
#define STDERR_LOG "tmp/test_fake_love_stderr.txt"

/* Allocated buffer, NUL-terminated, freed by the caller; NULL when the file
 * does not exist. */
static char *read_whole_file(const char *path, long *size_out) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return NULL;
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    buf[size] = '\0';
    fclose(file);
    if (size_out != NULL)
        *size_out = size;
    return buf;
}

/* Returns the exit status of `fake_love <args>`; stderr goes to STDERR_LOG. */
static int run_fake_love(const char *args) {
    char cmd[512];
    remove(CAPTURE);
    remove(STATE);
    snprintf(cmd, sizeof cmd, FAKE_LOVE " %s > /dev/null 2> " STDERR_LOG, args);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    return WEXITSTATUS(status);
}

static void assert_state(const char *expected) {
    char *state = read_whole_file(STATE, NULL);
    assert(state != NULL && "the state file must be written");
    if (strcmp(state, expected) != 0) {
        fprintf(stderr, "state:\n%s\nexpected:\n%s\n", state, expected);
        assert(!"unexpected state");
    }
    free(state);
}

static void assert_capture_empty(void) {
    long size = -1;
    char *capture = read_whole_file(CAPTURE, &size);
    assert(capture != NULL && "the capture file must be created");
    assert(size == 0 && "nothing draws yet: the capture stays empty");
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
    assert_capture_empty();
}

static void test_zero_frames_only_loads(void) {
    int status = run_fake_love(FIXTURES "counting " FIXTURES "zero_frames.json " CAPTURE " " STATE);
    assert(status == 0);
    assert_state("love empty\n"
                 "conf\n"
                 "love.conf\n"
                 "main\n"
                 "load table\n");
    assert_capture_empty();
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
    int status = system("cd " FIXTURES " && ../../../" FAKE_LOVE " outside zero_frames.json ../../../" CAPTURE
                        " ../../../" STATE " > /dev/null 2> ../../../" STDERR_LOG);
    assert(status != -1 && WIFEXITED(status));
    assert(WEXITSTATUS(status) == 1);
    char *err = read_whole_file(STDERR_LOG, NULL);
    assert(err != NULL && strstr(err, "outside_module") != NULL);
    free(err);
}

static void test_no_state_path_leaves_debug_state_uncalled(void) {
    int status = run_fake_love(FIXTURES "state_raises " FIXTURES "three_frames.json " CAPTURE);
    assert(status == 0);
    assert_capture_empty();
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
