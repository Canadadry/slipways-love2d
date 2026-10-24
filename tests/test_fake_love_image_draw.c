#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/canvas.h"
#include "../tools/fake_love/png_file.h"
#include "expected_screen.h"
#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_image_draw_capture.bin"
#define STATE "tmp/test_fake_love_image_draw_state.txt"
#define STDERR_LOG "tmp/test_fake_love_image_draw_stderr.txt"
#define PNG "tmp/test_fake_love_image_draw.png"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"
#define IMAGE_WIDTH 4
#define IMAGE_HEIGHT 3
#define SCREENS_GAME FIXTURES "image_draw_screens "
#define WINDOW_WIDTH 390
#define WINDOW_HEIGHT 844
#define PAD_SHEET_GAME "tests/fixtures/pad_sheet"
#define PAD_SHEET_EXPECTED "tests/fixtures/pad_sheet_expected.png"
#define PAD_FLAT "tests/fixtures/pad_flat.png"
#define PAD_SPRITES 18

static void test_image_misuse_is_refused_with_a_message_and_valid_calls_succeed(void) {
    int status = fake_love_cli_run("", FIXTURES "image_draw_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the image draw log game must run");
    fake_love_cli_assert_text(STATE, "conf graphics nil\n"
                                     "graphics clear draw getDimensions newCanvas newImage newQuad points pop push "
                                     "rectangle setCanvas setColor setDefaultFilter translate\n"
                                     "filter before images: ok\n"
                                     "image userdata getWidth Image:getWidth is not in the function list of "
                                     "docs/love-port/design.md\n"
                                     "quad userdata setViewport Quad:setViewport is not in the function list of "
                                     "docs/love-port/design.md\n"
                                     "filter after images: ok\n"
        "image none: love.graphics.newImage: takes 1 argument, an ImageData, not 0 arguments\n"
        "image file: love.graphics.newImage: argument 1 is a string, not an ImageData\n"
        "image settings: love.graphics.newImage: takes 1 argument, an ImageData, not 2 arguments\n"
        "image canvas: love.graphics.newImage: argument 1 is a Canvas, not an ImageData\n"
        "image image: love.graphics.newImage: argument 1 is an Image, not an ImageData\n"
        "image table: love.graphics.newImage: argument 1 is a table, not an ImageData\n"
        "image translucent: ok\n"
        "image accepted: ok\n"
        "quad four: love.graphics.newQuad: takes 6 numbers, x, y, w, h, sw and sh, not 4 arguments\n"
        "quad five: love.graphics.newQuad: takes 6 numbers, x, y, w, h, sw and sh, not 5 arguments\n"
        "quad seven: love.graphics.newQuad: takes 6 numbers, x, y, w, h, sw and sh, not 7 arguments\n"
        "quad string: love.graphics.newQuad: argument 1 is a string, not a number\n"
        "quad nan: love.graphics.newQuad: argument 2 is not finite\n"
        "quad infinity: love.graphics.newQuad: argument 5 is not finite\n"
        "quad fractional: love.graphics.newQuad: argument 1 is not an integer\n"
        "quad empty: love.graphics.newQuad: argument 3 is out of range (w from 1 to sw - x)\n"
        "quad negative width: love.graphics.newQuad: argument 3 is out of range (w from 1 to sw - x)\n"
        "quad negative y: love.graphics.newQuad: argument 2 is out of range (y from 0 to sh - 1)\n"
        "quad no reference: love.graphics.newQuad: argument 5 is out of range (sw from 1 to 1024)\n"
        "quad wide reference: love.graphics.newQuad: argument 5 is out of range (sw from 1 to 1024)\n"
        "quad past the reference: love.graphics.newQuad: argument 3 is out of range (w from 1 to sw - x)\n"
        "quad accepted: ok\n"
        "filter on a canvas: ok\n"
        "draw three: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not 3\n"
        "draw five: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not 5\n"
        "draw trailing nil: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not 5\n"
        "draw image data: love.graphics.draw: argument 1 is an ImageData, not an Image\n"
        "draw canvas: love.graphics.draw: argument 1 is a Canvas, not an Image\n"
        "draw table: love.graphics.draw: argument 1 is a table, not an Image\n"
        "draw string: love.graphics.draw: argument 1 is a string, not an Image\n"
        "draw table quad: love.graphics.draw: argument 2 is a table, not a Quad\n"
        "draw image quad: love.graphics.draw: argument 2 is an Image, not a Quad\n"
        "draw half x: love.graphics.draw: argument 3 is not an integer\n"
        "draw fractional y: love.graphics.draw: argument 4 is not an integer\n"
        "draw far x: love.graphics.draw: argument 3 is out of range (|v| >= 2^20)\n"
        "draw nan: love.graphics.draw: argument 4 is not finite\n"
        "draw string x: love.graphics.draw: argument 3 is a string, not a number\n"
        "draw other reference: love.graphics.draw: the quad's reference size 128x128 differs from the image's "
        "48x30\n"
        "draw accepted: ok\n"
        "draw translucent image: love.graphics.draw: the image's texel (2, 1) is FFFFFF80, translucent (an Image "
        "with translucent texels is drawn only onto the screen: blending on a canvas is not modelled)\n"
        "draw translucent colour and image: love.graphics.draw: the colour's alpha is 7F, not FF (a translucent "
        "colour is drawn only onto the screen: blending on a canvas is not modelled)\n"
        "present canvas selected: love.graphics.draw: a canvas is selected, a Canvas is drawn only onto the screen "
        "(draws between canvases are not modelled)\n"
        "present accepted: ok\n"
        "draw translucent image on the screen: ok\n"
        "present far and unscaled: ok\n"
        "present sixteen: ok\n"
        "present tall: ok\n"
        "present far and fractional: ok\n"
        "present below one: ok\n"
        "present 1024: ok\n"
        "present five: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not 5\n"
        "present seven: love.graphics.draw: argument 1 is a Canvas, not an Image\n"
        "present trailing nil: love.graphics.draw: argument 6 is a nil, not a number\n"
        "present image: love.graphics.draw: argument 1 is an Image, not a Canvas\n"
        "present quad: love.graphics.draw: argument 2 is a userdata, not a number\n"
        "present string x: love.graphics.draw: argument 2 is a string, not a number\n"
        "present half y: ok\n"
        "present far x: love.graphics.draw: argument 2 is out of range (|v| >= 2^20)\n"
        "present nan rotation: love.graphics.draw: argument 4 is not finite\n"
        "present rotated: love.graphics.draw: argument 4 is 1.5707963267949, only 0 is supported (rotation is not modelled)\n"
        "present string scale: love.graphics.draw: argument 5 is a string, not a number\n"
        "present infinite scale: love.graphics.draw: argument 5 is not finite\n"
        "present fractional scale: ok\n"
        "present zero scale: love.graphics.draw: argument 5 is out of range (s greater than 0, at most 1024)\n"
        "present mirrored: love.graphics.draw: argument 5 is out of range (s greater than 0, at most 1024)\n"
        "present seventeen: ok\n"
        "present above 1024: love.graphics.draw: argument 6 is out of range (s greater than 0, at most 1024)\n"
        "present unequal scales: love.graphics.draw: the scales 4 and 3 differ (unequal scales distort the window)\n"
        "present unequal fractional scales: love.graphics.draw: the scales 2.5 and 2.4375 differ (unequal scales "
        "distort the window)\n"
        "present tinted: love.graphics.draw: the colour is FF004DFF, not white FFFFFFFF (it would tint the window)\n"
        "present translated: love.graphics.draw: the offset is (0, -3), not (0, 0) (it would shift the window)\n"
        "present untranslated: ok\n"
        "present translucent: love.graphics.draw: the canvas's pixel (0, 0) is FF000080, translucent (a translucent "
        "canvas is not modelled on the screen)\n"
        "scaled accepted: ok\n"
        "scaled unequal below one: ok\n"
        "scaled far: ok\n"
        "scaled 1024: ok\n"
        "scaled eight: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not 8\n"
        "scaled trailing nil: love.graphics.draw: argument 7 is a nil, not a number\n"
        "scaled canvas: love.graphics.draw: argument 1 is a Canvas, not an Image\n"
        "scaled image data: love.graphics.draw: argument 1 is an ImageData, not an Image\n"
        "scaled number quad: love.graphics.draw: argument 2 is a number, not a Quad\n"
        "scaled string x: love.graphics.draw: argument 3 is a string, not a number\n"
        "scaled nan x: love.graphics.draw: argument 3 is not finite\n"
        "scaled far y: love.graphics.draw: argument 4 is out of range (|v| >= 2^20)\n"
        "scaled nan rotation: love.graphics.draw: argument 5 is not finite\n"
        "scaled rotated: love.graphics.draw: argument 5 is 1.5707963267949, only 0 is supported (rotation is not "
        "modelled)\n"
        "scaled string sx: love.graphics.draw: argument 6 is a string, not a number\n"
        "scaled zero sx: love.graphics.draw: argument 6 is out of range (s greater than 0, at most 1024)\n"
        "scaled above 1024: love.graphics.draw: argument 6 is out of range (s greater than 0, at most 1024)\n"
        "scaled mirrored sy: love.graphics.draw: argument 7 is out of range (s greater than 0, at most 1024)\n"
        "scaled infinite sy: love.graphics.draw: argument 7 is not finite\n"
        "scaled other reference: love.graphics.draw: the quad's reference size 128x128 differs from the image's "
        "48x30\n"
        "scaled translucent image: ok\n"
        "scaled translucent colour: ok\n"
        "scaled translated: love.graphics.draw: the offset is (0, -3), not (0, 0) (a scaled draw onto the screen is "
        "not translated)\n"
        "draw translated on the screen: ok\n"
        "scaled canvas selected: love.graphics.draw: a canvas is selected, a scaled Image is drawn only onto the "
        "screen (scaled draws onto a canvas are not modelled)\n"
        "scaled canvas selected and translated: love.graphics.draw: a canvas is selected, a scaled Image is drawn "
        "only onto the screen (scaled draws onto a canvas are not modelled)\n"
        "filter none: love.graphics.setDefaultFilter: takes 2 arguments, \"nearest\" and \"nearest\", not 0\n"
        "filter one: love.graphics.setDefaultFilter: takes 2 arguments, \"nearest\" and \"nearest\", not 1\n"
        "filter anisotropy: love.graphics.setDefaultFilter: takes 2 arguments, \"nearest\" and \"nearest\", not 3\n"
        "filter trailing nil: love.graphics.setDefaultFilter: takes 2 arguments, \"nearest\" and \"nearest\", not "
        "3\n"
        "filter number: love.graphics.setDefaultFilter: argument 1 is a number, not a string\n"
        "filter table: love.graphics.setDefaultFilter: argument 1 is a table, not a string\n"
        "filter nil: love.graphics.setDefaultFilter: argument 1 is a nil, not a string\n"
        "filter boolean: love.graphics.setDefaultFilter: argument 2 is a boolean, not a string\n"
        "filter bogus: love.graphics.setDefaultFilter: Invalid filter mode 'bogus', expected one of: 'none', "
        "'linear', 'nearest'\n"
        "filter upper case: love.graphics.setDefaultFilter: Invalid filter mode 'NEAREST', expected one of: 'none', "
        "'linear', 'nearest'\n"
        "filter linear: love.graphics.setDefaultFilter: argument 1 is \"linear\", only \"nearest\" is supported (the "
        "filter is not modelled)\n"
        "filter mixed: love.graphics.setDefaultFilter: argument 2 is \"linear\", only \"nearest\" is supported (the "
        "filter is not modelled)\n"
        "filter none name: love.graphics.setDefaultFilter: argument 1 is \"none\", only \"nearest\" is supported "
        "(the filter is not modelled)\n"
        "filter then draw: ok\n");
}

static void test_an_uncaught_fractional_position_is_fatal_and_names_draw_and_the_frame(void) {
    const char *const needles[] = {"love.draw f0: ", "main.lua:9: love.graphics.draw: ", "integer", NULL};
    fake_love_cli_assert_failure("", FIXTURES "image_draw_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

static void assert_program_draws(const char *program, const unsigned char *expected) {
    bool written = png_file_write(PNG, expected, CANVAS_WIDTH, CANVAS_HEIGHT);
    assert(written);
    char command[512];
    int length = snprintf(command, sizeof command,
                          "./tmp/love_capture_check tools/fake_love/harness " PROGRAMS "%s " PNG " 2> " STDERR_LOG,
                          program);
    assert(length > 0 && (size_t)length < sizeof command && "the command must fit");
    if (system(command) != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "%s:\n%s", program, err != NULL ? err : "");
        free(err);
        assert(!"the program must draw the expected canvas");
    }
}

static unsigned char *new_transparent_canvas(void) {
    unsigned char *pixels = calloc(CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    assert(pixels != NULL);
    return pixels;
}

/* image_draw_draws.lua's 4x3 image: alpha 0 at (1, 1) and (3, 0). */
static bool texel_is_opaque(int column, int row) {
    return !(column == 1 && row == 1) && !(column == 3 && row == 0);
}

/* Paints texels (column ... column + width - 1, row ... row + height - 1)
 * at (x, y), each channel tinted round(color x texel / 255). */
static void paint(unsigned char *pixels, const int area[4], int x, int y, const unsigned char color[3]) {
    for (int j = 0; j < area[3]; j++)
        for (int i = 0; i < area[2]; i++) {
            int column = area[0] + i, row = area[1] + j;
            int target_x = x + i, target_y = y + j;
            if (!texel_is_opaque(column, row) || target_x < 0 || target_y < 0 || target_x >= CANVAS_WIDTH ||
                target_y >= CANVAS_HEIGHT)
                continue;
            const int texel[3] = {column * 60 + 10, row * 80 + 20, (column + row) * 30 + 5};
            unsigned char *pixel = pixels + 4 * (target_y * CANVAS_WIDTH + target_x);
            for (int channel = 0; channel < 3; channel++)
                pixel[channel] = (unsigned char)((2 * color[channel] * texel[channel] + 255) / 510);
            pixel[3] = 0xFF;
        }
}

static void test_a_draw_copies_opaque_texels_clipped_and_tinted_and_skips_transparent_ones(void) {
    static const int whole[4] = {0, 0, IMAGE_WIDTH, IMAGE_HEIGHT};
    static const int corner[4] = {2, 1, 2, 2};
    static const unsigned char white[3] = {255, 255, 255};
    static const unsigned char tint[3] = {127, 255, 63};
    unsigned char *pixels = new_transparent_canvas();
    paint(pixels, whole, -1, -1, white);
    paint(pixels, whole, 10, 10, white);
    paint(pixels, whole, 158, 118, white);
    paint(pixels, corner, 30, 10, white);
    paint(pixels, whole, 20, 10, tint);
    assert(pixels[4 * (10 * CANVAS_WIDTH + 20)] == 5 && "texel 10 tinted by 127 must round to 5, not floor to 4");
    assert_program_draws("image_draw_draws.lua", pixels);
    free(pixels);
}

static void test_refused_draws_a_draw_on_the_screen_and_a_later_image_data_change_leave_the_canvas(void) {
    unsigned char *pixels = new_transparent_canvas();
    assert_program_draws("image_draw_refused.lua", pixels);
    free(pixels);
}

static char *screen_path(int frame) {
    static char path[128];
    snprintf(path, sizeof path, CAPTURE ".screen-f%d.png", frame);
    return path;
}

/* image_draw_screens's generated 6x4 image. */
static void generated_texel(int i, int j, unsigned char rgba[4]) {
    const unsigned char texel[4] = {(unsigned char)(i * 40 + 10), (unsigned char)(j * 50 + 20),
                                    (unsigned char)(i * 7 + j * 30 + 3), 0xFF};
    memcpy(rgba, texel, 4);
}

/* image_draw_sheet.png, 8x2: row 0 holds the alphas below, row 1 is opaque.
 * The game directory's pad.png is 770077FF throughout. */
static void sheet_texel(int i, int j, unsigned char rgba[4]) {
    static const unsigned char ALPHAS[8] = {0x01, 0x40, 0x7F, 0x80, 0xC0, 0xFE, 0x00, 0xFF};
    const unsigned char row0[4] = {(unsigned char)(0xE0 - 0x10 * i), (unsigned char)(0x30 + 0x18 * i), 0xA5, ALPHAS[i]};
    const unsigned char row1[4] = {(unsigned char)(0x20 + 0x14 * i), 0xC8, (unsigned char)(0x40 + 0x11 * i), 0xFF};
    memcpy(rgba, j == 0 ? row0 : row1, 4);
}

static void backdrop_texel(int i, int j, unsigned char rgba[4]) {
    (void)i;
    (void)j;
    static const unsigned char NAVY[4] = {0x1D, 0x2B, 0x53, 0xFF};
    memcpy(rgba, NAVY, 4);
}

static void test_scaled_tinted_and_blended_draws_reach_the_screen_with_the_replay_s_pad_sheet(void) {
    static const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    static const unsigned char tint[4] = {0x7F, 0xFF, 0x3F, 0x7F};
    static const int generated_part[4] = {1, 1, 3, 2};
    static const int generated_whole[4] = {0, 0, 6, 4};
    static const int canvas_whole[4] = {0, 0, CANVAS_WIDTH, CANVAS_HEIGHT};
    static const int sheet_whole[4] = {0, 0, 8, 2};
    static const int sheet_row[4] = {0, 1, 4, 1};
    int status = fake_love_cli_run("", SCREENS_GAME FIXTURES "image_draw_screens.json " CAPTURE " " STATE, CAPTURE,
                                   STATE, STDERR_LOG);
    assert(status == 0 && "the screen draw game must run");

    expected_screen_t frame = expected_screen_black(WINDOW_WIDTH, WINDOW_HEIGHT);
    expected_screen_draw(&frame, generated_texel, generated_part, 10.25, 7.6, 2.4375, 1.7, white);
    expected_screen_draw(&frame, generated_texel, generated_whole, -3.5, 20.5, 0.75, 3, white);
    expected_screen_assert_pixel(&frame, 9, 8, 0, 0, 0);
    expected_screen_assert_pixel(&frame, 10, 8, 50, 70, 40);
    expected_screen_assert_pixel(&frame, 17, 10, 130, 120, 84);
    expected_screen_assert_pixel(&frame, 18, 10, 0, 0, 0);
    expected_screen_assert_pixel(&frame, 17, 11, 0, 0, 0);
    expected_screen_assert_pixel(&frame, 0, 19, 0, 0, 0);
    expected_screen_assert_pixel(&frame, 0, 20, 210, 20, 38);
    expected_screen_assert_pixel(&frame, 1, 20, 0, 0, 0);
    expected_screen_assert_pixel(&frame, 0, 31, 210, 170, 128);
    expected_screen_assert_pixel(&frame, 0, 32, 0, 0, 0);
    expected_screen_assert_file(screen_path(0), &frame);
    expected_screen_free(&frame);

    frame = expected_screen_black(WINDOW_WIDTH, WINDOW_HEIGHT);
    expected_screen_draw(&frame, backdrop_texel, canvas_whole, 0, 64.75, 2.4375, 2.4375, white);
    expected_screen_draw(&frame, sheet_texel, sheet_whole, 5.5, 70.5, 10, 10, white);
    expected_screen_draw(&frame, sheet_texel, sheet_row, 100.25, 70.5, 10, 10, tint);
    expected_screen_draw(&frame, sheet_texel, sheet_whole, 203, 104, 1, 1, tint);
    expected_screen_assert_pixel(&frame, 4, 75, 0x1D, 0x2B, 0x53);
    expected_screen_assert_pixel(&frame, 5, 75, 0x1E, 0x2B, 0x54);
    expected_screen_assert_pixel(&frame, 25, 75, 0x6F, 0x46, 0x7C);
    expected_screen_assert_pixel(&frame, 55, 75, 0x8F, 0xA7, 0xA4);
    expected_screen_assert_pixel(&frame, 65, 75, 0x1D, 0x2B, 0x53);
    expected_screen_assert_pixel(&frame, 75, 75, 0x70, 0xD8, 0xA5);
    expected_screen_assert_pixel(&frame, 85, 75, 0x1D, 0x2B, 0x53);
    expected_screen_assert_pixel(&frame, 5, 85, 0x20, 0xC8, 0x40);
    expected_screen_assert_pixel(&frame, 100, 75, 0x17, 0x7A, 0x32);
    expected_screen_assert_pixel(&frame, 203, 104, 0x1D, 0x2B, 0x53);
    expected_screen_assert_pixel(&frame, 210, 104, 0x2B, 0x82, 0x3E);
    expected_screen_assert_pixel(&frame, 211, 104, 0x1D, 0x2B, 0x53);
    expected_screen_assert_file(screen_path(1), &frame);
    expected_screen_free(&frame);
    remove(screen_path(0));
    remove(screen_path(1));
}

static void test_a_missing_pad_sheet_is_refused_before_the_game_runs(void) {
    const char *const needles[] = {"fake_love: " FIXTURES "image_draw_missing_sheet.json: \"pad_sheet\": " FIXTURES
                                   "image_draw_absent.png cannot be read",
                                   NULL};
    fake_love_cli_assert_failure("", SCREENS_GAME FIXTURES "image_draw_missing_sheet.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
    assert(!fake_love_cli_file_exists(screen_path(0)) && !fake_love_cli_file_exists(screen_path(1)) &&
           "a refused replay writes no screen PNG");
}

static unsigned char *load_png(const char *path, int *width, int *height) {
    unsigned char *pixels = NULL;
    const char *reason = png_file_load(path, &pixels, width, height);
    if (reason != NULL)
        fprintf(stderr, "%s: %s\n", path, reason);
    assert(reason == NULL && "the PNG must load");
    return pixels;
}

typedef struct {
    int x, y, width, height;
} pad_rectangle_t;

/* Parses the fixture's state: the sheet size, then one line per sprite in
 * the flat order of design.md. */
static void read_pad_state(int *sheet_width, int *sheet_height, pad_rectangle_t rectangles[PAD_SPRITES]) {
    static const char *const names[PAD_SPRITES] = {
        "stick_base",             "stick_knob",
        "a released",             "a pressed",
        "b released",             "b pressed",
        "x released",             "x pressed",
        "y released",             "y pressed",
        "leftshoulder released",  "leftshoulder pressed",
        "rightshoulder released", "rightshoulder pressed",
        "start released",         "start pressed",
        "back released",          "back pressed",
    };
    char *text = fake_love_cli_read_file(STATE, NULL);
    assert(text != NULL && "the pad sheet game must write its state");
    char *line = strtok(text, "\n");
    int consumed = 0;
    assert(line != NULL && sscanf(line, "%d %d%n", sheet_width, sheet_height, &consumed) == 2 &&
           line[consumed] == '\0' && "the state's first line must be the sheet size");
    for (int n = 0; n < PAD_SPRITES; n++) {
        line = strtok(NULL, "\n");
        size_t length = strlen(names[n]);
        pad_rectangle_t *r = &rectangles[n];
        if (line == NULL || strncmp(line, names[n], length) != 0 ||
            sscanf(line + length, " %d %d %d %d%n", &r->x, &r->y, &r->width, &r->height, &consumed) != 4 ||
            line[length + (size_t)consumed] != '\0') {
            fprintf(stderr, "state line %d is \"%s\", not \"%s <x> <y> <width> <height>\"\n", n + 2,
                    line != NULL ? line : "(none)", names[n]);
            assert(!"every sprite line must name its sprite, in the flat order");
        }
    }
    assert(strtok(NULL, "\n") == NULL && "the state must hold exactly 18 sprite lines");
    free(text);
}

static void assert_flat_sprites_are_solid_and_distinct(int width, int height,
                                                        const pad_rectangle_t rectangles[PAD_SPRITES]) {
    int flat_width = 0, flat_height = 0;
    unsigned char *flat = load_png(PAD_FLAT, &flat_width, &flat_height);
    assert(flat_width == width && flat_height == height && "the flat sheet must have the real sheet's size");
    unsigned char colors[PAD_SPRITES][4];
    for (int n = 0; n < PAD_SPRITES; n++) {
        const pad_rectangle_t *r = &rectangles[n];
        assert(r->x >= 0 && r->y >= 0 && r->width > 0 && r->height > 0 && r->x + r->width <= width &&
               r->y + r->height <= height && "every sprite must lie inside the sheet");
        memcpy(colors[n], flat + 4 * ((size_t)r->y * (size_t)width + (size_t)r->x), 4);
        for (int j = r->y; j < r->y + r->height; j++)
            for (int i = r->x; i < r->x + r->width; i++)
                if (memcmp(flat + 4 * ((size_t)j * (size_t)width + (size_t)i), colors[n], 4) != 0) {
                    fprintf(stderr, PAD_FLAT ": sprite %d's texel (%d, %d) differs from its first\n", n, i, j);
                    assert(!"every flat sprite must be one solid colour");
                }
        assert(colors[n][3] == 0xFF && "every flat sprite must be opaque");
        assert((colors[n][0] | colors[n][1] | colors[n][2]) != 0 && "no flat sprite may be black");
        for (int m = 0; m < n; m++)
            if (memcmp(colors[m], colors[n], 3) == 0) {
                fprintf(stderr, PAD_FLAT ": sprites %d and %d share a colour\n", m, n);
                assert(!"the flat colours must be pairwise distinct");
            }
    }
    free(flat);
}

static void test_the_quad_table_cuts_the_real_sheet_as_the_generator_drew_it_and_the_flat_sheet_is_solid(void) {
    int status = fake_love_cli_run("", PAD_SHEET_GAME " tests/fixtures/pad_sheet.json " CAPTURE " " STATE, CAPTURE,
                                   STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "%s", err != NULL ? err : "");
        free(err);
    }
    assert(status == 0 && "the pad sheet game must run");

    expected_screen_t frame = {NULL, 0, 0};
    frame.pixels = load_png(PAD_SHEET_EXPECTED, &frame.width, &frame.height);
    int width = 0, height = 0;
    unsigned char *screen = load_png(screen_path(0), &width, &height);
    if (width != frame.width || height != frame.height ||
        memcmp(screen, frame.pixels, (size_t)width * (size_t)height * 4) != 0)
        fprintf(stderr, "the pad sheet's static frame differs: if the art changed on purpose, run make pad-art\n");
    free(screen);
    expected_screen_assert_file(screen_path(0), &frame);
    expected_screen_free(&frame);

    pad_rectangle_t rectangles[PAD_SPRITES];
    read_pad_state(&width, &height, rectangles);
    assert_flat_sprites_are_solid_and_distinct(width, height, rectangles);
    remove(screen_path(0));
}

int main(void) {
    test_image_misuse_is_refused_with_a_message_and_valid_calls_succeed();
    test_an_uncaught_fractional_position_is_fatal_and_names_draw_and_the_frame();
    test_a_draw_copies_opaque_texels_clipped_and_tinted_and_skips_transparent_ones();
    test_refused_draws_a_draw_on_the_screen_and_a_later_image_data_change_leave_the_canvas();
    test_scaled_tinted_and_blended_draws_reach_the_screen_with_the_replay_s_pad_sheet();
    test_a_missing_pad_sheet_is_refused_before_the_game_runs();
    test_the_quad_table_cuts_the_real_sheet_as_the_generator_drew_it_and_the_flat_sheet_is_solid();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_image_draw: all tests passed\n");
    return 0;
}
