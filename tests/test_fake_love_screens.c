#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expected_screen.h"
#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_screens_capture.bin"
#define STATE "tmp/test_fake_love_screens_state.txt"
#define STDERR_LOG "tmp/test_fake_love_screens_stderr.txt"
#define BAD_REPLAY "tmp/test_fake_love_screens_bad.json"
#define RUN FIXTURES "screens " FIXTURES "screens.json " CAPTURE " " STATE
#define FRAME_COUNT 5

#define SOURCE_WIDTH 160
#define SOURCE_HEIGHT 120

static char *screen_path(int frame) {
    static char path[128];
    snprintf(path, sizeof path, CAPTURE ".screen-f%d.png", frame);
    return path;
}

static void remove_screens(void) {
    for (int frame = 0; frame < FRAME_COUNT; frame++)
        remove(screen_path(frame));
}

static void assert_no_screen(void) {
    for (int frame = 0; frame < FRAME_COUNT; frame++)
        assert(!fake_love_cli_file_exists(screen_path(frame)) && "no screen PNG may be left");
}

/* The fixture's canvases: texel (i, j) of the gradient is (i, j, 80, FF);
 * the sparse one is transparent but for three texels. */
static void gradient_texel(int i, int j, unsigned char rgba[4]) {
    rgba[0] = (unsigned char)i;
    rgba[1] = (unsigned char)j;
    rgba[2] = 0x80;
    rgba[3] = 0xFF;
}

static void sparse_texel(int i, int j, unsigned char rgba[4]) {
    static const unsigned char TRANSPARENT[4] = {0, 0, 0, 0};
    static const unsigned char RED[4] = {0xFF, 0, 0, 0xFF};
    static const unsigned char GREEN[4] = {0, 0xFF, 0, 0xFF};
    static const unsigned char BLUE[4] = {0, 0, 0xFF, 0xFF};
    const unsigned char *texel = TRANSPARENT;
    if (i == 0 && j == 0)
        texel = RED;
    else if (i == 5 && j == 3)
        texel = GREEN;
    else if (i == 159 && j == 119)
        texel = BLUE;
    memcpy(rgba, texel, 4);
}

static void expect_present(expected_screen_t *frame, expected_screen_texel_t texel_of, double x, double y,
                           double scale) {
    static const int whole[4] = {0, 0, SOURCE_WIDTH, SOURCE_HEIGHT};
    static const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    expected_screen_draw(frame, texel_of, whole, x, y, scale, scale, white);
}

static void assert_frame(int frame, const expected_screen_t *expected) {
    expected_screen_assert_file(screen_path(frame), expected);
}

static void assert_runs(const char *environment, const char *args) {
    int status = fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

static void test_each_listed_frame_writes_its_whole_screen(void) {
    remove_screens();
    assert_runs("", RUN);
    assert(!fake_love_cli_file_exists(screen_path(1)) && "an unlisted frame writes no PNG");

    expected_screen_t tall = expected_screen_black(390, 844);
    expect_present(&tall, gradient_texel, 0, 64.75, 2.4375);
    expected_screen_assert_pixel(&tall, 18, 100, 7, 14, 0x80);
    expected_screen_assert_pixel(&tall, 19, 100, 8, 14, 0x80);
    expected_screen_assert_pixel(&tall, 0, 64, 0, 0, 0);
    expected_screen_assert_pixel(&tall, 0, 65, 0, 0, 0x80);
    expected_screen_assert_pixel(&tall, 0, 74, 0, 4, 0x80);
    expected_screen_assert_pixel(&tall, 389, 356, 159, 119, 0x80);
    expected_screen_assert_pixel(&tall, 389, 357, 0, 0, 0);
    assert_frame(0, &tall);
    expected_screen_free(&tall);

    expected_screen_t resized = expected_screen_black(640, 480);
    expect_present(&resized, gradient_texel, -10.5, 7.25, 3.1);
    assert_frame(2, &resized);
    expected_screen_free(&resized);

    expected_screen_t empty = expected_screen_black(640, 480);
    assert_frame(3, &empty);

    expect_present(&empty, gradient_texel, 0, 0, 1);
    expect_present(&empty, sparse_texel, 100.5, 50.5, 0.75);
    expected_screen_assert_pixel(&empty, 100, 50, 0xFF, 0, 0);
    expected_screen_assert_pixel(&empty, 101, 50, 101, 50, 0x80);
    assert_frame(4, &empty);
    expected_screen_free(&empty);
    remove_screens();
}

static void assert_failure(const char *environment, const char *args, const char *const *needles) {
    remove_screens();
    fake_love_cli_assert_failure(environment, args, CAPTURE, STATE, STDERR_LOG, needles);
    assert_no_screen();
}

static void test_a_run_failing_after_a_written_screen_leaves_no_png(void) {
    assert_failure("FAKE_LOVE_SCREENS_FAIL=1", RUN, (const char *const[]){"fails at f2", NULL});
}

static void test_a_translucent_canvas_presented_is_refused_naming_the_pixel(void) {
    assert_failure("FAKE_LOVE_SCREENS_TRANSLUCENT=1", RUN,
                   (const char *const[]){"love.graphics.draw: the canvas's pixel (0, 0) is FF000080, translucent", NULL});
}

/* Each refusal's message is checked by test_fake_love_replay. */
static void test_a_malformed_screens_is_refused_before_the_game_runs(void) {
    FILE *file = fopen(BAD_REPLAY, "wb");
    assert(file != NULL);
    fputs("{\"input\": \"pad\", \"screens\": [2, 1], \"frames\": [{}, {}, {}]}", file);
    assert(fclose(file) == 0);
    assert_failure("", FIXTURES "screens " BAD_REPLAY " " CAPTURE " " STATE,
                   (const char *const[]){BAD_REPLAY ": offset ", "\"screens\" entry 1 is not after 2", NULL});
    remove(BAD_REPLAY);
}

int main(void) {
    test_each_listed_frame_writes_its_whole_screen();
    test_a_run_failing_after_a_written_screen_leaves_no_png();
    test_a_translucent_canvas_presented_is_refused_naming_the_pixel();
    test_a_malformed_screens_is_refused_before_the_game_runs();
    printf("test_fake_love_screens: ok\n");
    return 0;
}
