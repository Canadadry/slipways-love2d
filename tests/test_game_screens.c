#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "expected_screen.h"
#include "fake_love_cli.h"

#define CAPTURE "tmp/test_game_screens_capture.bin"
#define STATE "tmp/test_game_screens_state.txt"
#define STDERR_LOG "tmp/test_game_screens_stderr.txt"
#define RUN "game tests/fixtures/game_screens.json " CAPTURE " " STATE

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 120
#define CAPTURE_FRAME_BYTES (CANVAS_WIDTH * CANVAS_HEIGHT / 2)
#define NO_ROW (-1)

/* The rectangles are literal, from design.md's window layout, never computed by
 * love.window_layout: the test checks the layout and the present together. */
typedef struct {
    int frame;
    int width;
    int height;
    double x;
    double y;
    double scale;
    int bar_rows[2];
} screen_case_t;

static const screen_case_t CASES[] = {
    {0, 390, 844, 0, 64.75, 2.4375, {64, 357}},
    {1, 640, 480, 0, 0, 4, {NO_ROW, NO_ROW}},
};

#define CASE_COUNT ((int)(sizeof CASES / sizeof CASES[0]))

/* The test's own copy, so that a palette bug of the fake's capture is not
 * copied into the expected frame. */
static const unsigned char PALETTE[16][3] = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
};

/* expected_screen_texel_t takes no context: the frame being expected. */
static const unsigned char *current_frame = NULL;

static int palette_number(const unsigned char *frame, int i, int j) {
    unsigned char packed = frame[(j * CANVAS_WIDTH + i) / 2];
    return i % 2 == 0 ? packed & 0x0F : packed >> 4;
}

static void capture_texel(int i, int j, unsigned char rgba[4]) {
    const unsigned char *rgb = PALETTE[palette_number(current_frame, i, j)];
    memcpy(rgba, rgb, 3);
    rgba[3] = 0xFF;
}

static char *screen_path(int frame) {
    static char path[128];
    snprintf(path, sizeof path, CAPTURE ".screen-f%d.png", frame);
    return path;
}

static void remove_screens(void) {
    for (int k = 0; k < CASE_COUNT; k++)
        remove(screen_path(CASES[k].frame));
}

static void assert_not_all_colour_0(const unsigned char *frame) {
    for (int k = 0; k < CAPTURE_FRAME_BYTES; k++)
        if (frame[k] != 0)
            return;
    assert(!"the captured frame must hold a colour other than 0, or a misplaced present could pass");
}

/* At an integer scale from (0, 0), the rule of design.md's "The screen"
 * gives pixel (i, j) texel (i / scale, j / scale): one pass, no rasteriser. */
static void fill_integer_scale(expected_screen_t *screen, int scale) {
    for (int j = 0; j < screen->height && j / scale < CANVAS_HEIGHT; j++)
        for (int i = 0; i < screen->width && i / scale < CANVAS_WIDTH; i++)
            capture_texel(i / scale, j / scale, screen->pixels + 4 * ((size_t)j * (size_t)screen->width + (size_t)i));
}

static void assert_screen(const screen_case_t *screen_case, const unsigned char *capture) {
    current_frame = capture + (size_t)screen_case->frame * CAPTURE_FRAME_BYTES;
    assert_not_all_colour_0(current_frame);
    expected_screen_t expected = expected_screen_black(screen_case->width, screen_case->height);
    int integer_scale = (int)screen_case->scale;
    if (screen_case->x == 0 && screen_case->y == 0 && integer_scale == screen_case->scale) {
        fill_integer_scale(&expected, integer_scale);
    } else {
        static const int whole[4] = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
        static const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
        expected_screen_draw(&expected, capture_texel, whole, screen_case->x, screen_case->y, screen_case->scale,
                             screen_case->scale, white);
    }
    for (int k = 0; k < 2; k++)
        if (screen_case->bar_rows[k] != NO_ROW)
            expected_screen_assert_pixel(&expected, 0, screen_case->bar_rows[k], 0, 0, 0);
    expected_screen_assert_file(screen_path(screen_case->frame), &expected);
    expected_screen_free(&expected);
}

static void test_each_listed_frame_presents_the_game_in_its_rectangle(void) {
    remove_screens();
    int status = fake_love_cli_run("", RUN, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love " RUN ": status %d\n%s", status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
    long size = 0;
    unsigned char *capture = (unsigned char *)fake_love_cli_read_file(CAPTURE, &size);
    assert(capture != NULL && size == 2L * CAPTURE_FRAME_BYTES && "the capture must hold the replay's two frames");
    for (int k = 0; k < CASE_COUNT; k++)
        assert_screen(&CASES[k], capture);
    free(capture);
    remove_screens();
}

int main(void) {
    test_each_listed_frame_presents_the_game_in_its_rectangle();
    printf("test_game_screens: ok\n");
    return 0;
}
