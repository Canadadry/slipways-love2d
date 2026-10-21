#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../tools/fake_love/canvas.h"
#include "fake_love_cli.h"
#include "fake_love_png.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_image_draw_capture.bin"
#define STATE "tmp/test_fake_love_image_draw_state.txt"
#define STDERR_LOG "tmp/test_fake_love_image_draw_stderr.txt"
#define PNG "tmp/test_fake_love_image_draw.png"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"
#define IMAGE_WIDTH 4
#define IMAGE_HEIGHT 3

static void test_image_misuse_is_refused_with_a_message_and_valid_calls_succeed(void) {
    int status = fake_love_cli_run("", FIXTURES "image_draw_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the image draw log game must run");
    fake_love_cli_assert_text(STATE, "conf graphics nil\n"
                                     "graphics clear draw newCanvas newImage newQuad points pop push rectangle "
                                     "setCanvas setColor setDefaultFilter translate\n"
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
        "image translucent: love.graphics.newImage: pixel (2, 1) has alpha 128, not 0 or 255 (translucent texels "
        "are not modelled)\n"
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
        "draw three: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, not 3\n"
        "draw five: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, not 5\n"
        "draw trailing nil: love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, not 5\n"
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
    fake_love_png_write(PNG, expected, CANVAS_WIDTH, CANVAS_HEIGHT);
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

int main(void) {
    test_image_misuse_is_refused_with_a_message_and_valid_calls_succeed();
    test_an_uncaught_fractional_position_is_fatal_and_names_draw_and_the_frame();
    test_a_draw_copies_opaque_texels_clipped_and_tinted_and_skips_transparent_ones();
    test_refused_draws_a_draw_on_the_screen_and_a_later_image_data_change_leave_the_canvas();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_image_draw: all tests passed\n");
    return 0;
}
