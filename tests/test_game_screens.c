#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/png_file.h"
#include "expected_screen.h"
#include "fake_love_cli.h"

#define CAPTURE "tmp/test_game_screens_capture.bin"
#define STATE "tmp/test_game_screens_state.txt"
#define STDERR_LOG "tmp/test_game_screens_stderr.txt"
#define RUN "game tests/fixtures/game_screens.json " CAPTURE " " STATE

#define CANVAS_WIDTH 160
#define CANVAS_HEIGHT 120
#define CAPTURE_FRAME_BYTES (CANVAS_WIDTH * CANVAS_HEIGHT / 2)
#define FRAME_COUNT 8
#define NO_ROW (-1)
#define PAD_FLAT "tests/fixtures/pad_flat.png"
#define PAD_SHEET_WIDTH 240
#define PAD_SHEET_HEIGHT 72
#define BUTTON_COUNT 8
#define BUTTON_SIZE 24
#define STICK_BASE_SIZE 48
#define STICK_KNOB_SIZE 24

/* The buttons in the sheet's order: bit k of pad_case_t.pressed is the enum's k-th. */
enum { A, B, X, Y, LEFTSHOULDER, RIGHTSHOULDER, START, BACK };
#define PRESSED(button) (1u << (button))

/* The test's own copies of design.md's pad layout table (reference units)
 * and sheet table, so that a wrong table of love.pad_layout or
 * love.pad_sheet is caught rather than copied. */
static const int BUTTON_REFERENCE[BUTTON_COUNT][2] = {
    {108, 84}, {132, 60}, {84, 60}, {108, 36}, {4, 4}, {132, 4}, {82, 94}, {54, 94},
};
static const int STICK_BASE_SPRITE[4] = {0, 0, STICK_BASE_SIZE, STICK_BASE_SIZE};
static const int STICK_KNOB_SPRITE[4] = {0, 48, STICK_KNOB_SIZE, STICK_KNOB_SIZE};
#define FLAT_STICK_BASE 0
#define FLAT_STICK_KNOB 1
#define FLAT_BUTTON(k, pressed) (2 + 2 * (k) + ((pressed) ? 1 : 0))

typedef struct {
    double x;
    double y;
} point_t;

/* Literal, from the layout of each window size: x, y and scale place the
 * pad's reference rectangle, base and knob are centres (held is false
 * without a stick). */
typedef struct {
    bool shown;
    double x;
    double y;
    double scale;
    unsigned pressed;
    bool held;
    point_t base;
    point_t knob;
} pad_case_t;

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
    pad_case_t pad;
} screen_case_t;

static const screen_case_t CASES[] = {
    {0, 390, 844, 0, 64.75, 2.4375, {64, 357}, {false, 0, 486.75, 2.4375, 0, false, {0, 0}, {0, 0}}},
    {1, 640, 480, 0, 0, 4, {NO_ROW, NO_ROW}, {false, 0, 0, 4, 0, false, {0, 0}, {0, 0}}},
    /* Pad at y 181.5: every button's top edge on a pixel centre. */
    {2, 160, 322, 0, 20.5, 1, {19, 160}, {true, 0, 181.5, 1, 0, false, {0, 0}, {0, 0}}},
    /* Y, not A: A on the title starts a game, about 35 ms more for this check. */
    {4, 160, 322, 0, 20.5, 1, {19, 160}, {true, 0, 181.5, 1, PRESSED(Y), false, {0, 0}, {0, 0}}},
    /* Pushed from (40, 240) to (70, 200): the knob capped at (54.4, 220.8), over the base. */
    {6, 160, 322, 0, 20.5, 1, {19, 160}, {true, 0, 181.5, 1, 0, true, {40, 240}, {54.4, 220.8}}},
    {7, 160, 120, 0, 0, 1, {NO_ROW, NO_ROW}, {true, 0, 0, 1, 0, false, {0, 0}, {0, 0}}},
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

static unsigned char *pad_flat = NULL;

static void pad_flat_texel(int i, int j, unsigned char rgba[4]) {
    memcpy(rgba, pad_flat + 4 * ((size_t)j * PAD_SHEET_WIDTH + (size_t)i), 4);
}

static void load_pad_flat(void) {
    int width = 0, height = 0;
    const char *reason = png_file_load(PAD_FLAT, &pad_flat, &width, &height);
    if (reason != NULL)
        fprintf(stderr, PAD_FLAT ": %s\n", reason);
    assert(reason == NULL && width == PAD_SHEET_WIDTH && height == PAD_SHEET_HEIGHT && "pad_flat.png must load");
}

/* design.md's flat colour of sprite n, never read from pad_flat.png: the
 * landmarks catch an expected frame that left a sprite out. */
static void assert_flat_pixel(const expected_screen_t *screen, point_t centre, int sprite) {
    expected_screen_assert_pixel(screen, (int)centre.x, (int)centre.y, 0x20u + 0x0Bu * (unsigned)sprite,
                                 0xE0u - 0x0Bu * (unsigned)sprite, sprite % 2 == 0 ? 0x40u : 0xC0u);
}

static const unsigned char WHITE[4] = {0xFF, 0xFF, 0xFF, 0xFF};

/* Draws `sprite` stretched into the square of side `side` centred on `centre`. */
static void draw_centred(expected_screen_t *screen, const int sprite[4], point_t centre, double side) {
    expected_screen_draw(screen, pad_flat_texel, sprite, centre.x - side / 2, centre.y - side / 2, side / sprite[2],
                         side / sprite[3], WHITE);
}

static point_t button_centre(const pad_case_t *pad, int k) {
    double side = BUTTON_SIZE * pad->scale;
    return (point_t){pad->x + BUTTON_REFERENCE[k][0] * pad->scale + side / 2,
                     pad->y + BUTTON_REFERENCE[k][1] * pad->scale + side / 2};
}

static void draw_pad(expected_screen_t *screen, const pad_case_t *pad) {
    for (int k = 0; k < BUTTON_COUNT; k++) {
        bool pressed = (pad->pressed & PRESSED(k)) != 0;
        const int sprite[4] = {48 + BUTTON_SIZE * k, pressed ? BUTTON_SIZE : 0, BUTTON_SIZE, BUTTON_SIZE};
        draw_centred(screen, sprite, button_centre(pad, k), BUTTON_SIZE * pad->scale);
    }
    if (pad->held) {
        draw_centred(screen, STICK_BASE_SPRITE, pad->base, STICK_BASE_SIZE * pad->scale);
        draw_centred(screen, STICK_KNOB_SPRITE, pad->knob, STICK_KNOB_SIZE * pad->scale);
    }
    for (int k = 0; k < BUTTON_COUNT; k++)
        assert_flat_pixel(screen, button_centre(pad, k), FLAT_BUTTON(k, (pad->pressed & PRESSED(k)) != 0));
    if (pad->held) {
        assert_flat_pixel(screen, pad->base, FLAT_STICK_BASE);
        assert_flat_pixel(screen, pad->knob, FLAT_STICK_KNOB);
    }
}

/* A hidden pad: A's centre holds the game's colour there, or black outside the game. */
static void assert_no_pad(const expected_screen_t *screen, const screen_case_t *screen_case) {
    point_t centre = button_centre(&screen_case->pad, A);
    int i = (int)centre.x, j = (int)centre.y;
    double column = (i + 0.5 - screen_case->x) / screen_case->scale;
    double row = (j + 0.5 - screen_case->y) / screen_case->scale;
    unsigned char rgba[4] = {0, 0, 0, 0xFF};
    if (column >= 0 && column < CANVAS_WIDTH && row >= 0 && row < CANVAS_HEIGHT)
        capture_texel((int)column, (int)row, rgba);
    expected_screen_assert_pixel(screen, i, j, rgba[0], rgba[1], rgba[2]);
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
        expected_screen_draw(&expected, capture_texel, whole, screen_case->x, screen_case->y, screen_case->scale,
                             screen_case->scale, WHITE);
    }
    if (screen_case->pad.shown)
        draw_pad(&expected, &screen_case->pad);
    else
        assert_no_pad(&expected, screen_case);
    for (int k = 0; k < 2; k++)
        if (screen_case->bar_rows[k] != NO_ROW)
            expected_screen_assert_pixel(&expected, 0, screen_case->bar_rows[k], 0, 0, 0);
    expected_screen_assert_file(screen_path(screen_case->frame), &expected);
    expected_screen_free(&expected);
}

static void test_each_listed_frame_presents_the_game_then_the_shown_pad(void) {
    remove_screens();
    load_pad_flat();
    int status = fake_love_cli_run("", RUN, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love " RUN ": status %d\n%s", status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
    long size = 0;
    unsigned char *capture = (unsigned char *)fake_love_cli_read_file(CAPTURE, &size);
    assert(capture != NULL && size == (long)FRAME_COUNT * CAPTURE_FRAME_BYTES &&
           "the capture must hold the replay's eight frames");
    for (int k = 0; k < CASE_COUNT; k++)
        assert_screen(&CASES[k], capture);
    free(capture);
    free(pad_flat);
    remove_screens();
}

int main(void) {
    test_each_listed_frame_presents_the_game_then_the_shown_pad();
    printf("test_game_screens: ok\n");
    return 0;
}
