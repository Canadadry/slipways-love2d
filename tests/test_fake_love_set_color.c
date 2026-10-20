#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "../tools/fake_love/canvas.h"
#include "fake_love_cli.h"
#include "fake_love_png.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_set_color_capture.bin"
#define STATE "tmp/test_fake_love_set_color_state.txt"
#define STDERR_LOG "tmp/test_fake_love_set_color_stderr.txt"
#define PNG "tmp/test_fake_love_set_color.png"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"

static void test_set_color_misuse_is_refused_with_a_message_and_opaque_colours_succeed(void) {
    int status = fake_love_cli_run("", FIXTURES "set_color_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the setColor log game must run");
    fake_love_cli_assert_text(
        STATE,
        "graphics clear newCanvas points rectangle setCanvas setColor\n"
        "none: love.graphics.setColor: takes 3 numbers or 4 with alpha 1, not 0 arguments\n"
        "two: love.graphics.setColor: takes 3 numbers or 4 with alpha 1, not 2 arguments\n"
        "five: love.graphics.setColor: takes 3 numbers or 4 with alpha 1, not 5 arguments\n"
        "table: love.graphics.setColor: argument 1 is a table, not a number (the table form is not supported)\n"
        "string: love.graphics.setColor: argument 1 is a string, not a number\n"
        "nil: love.graphics.setColor: argument 2 is a nil, not a number\n"
        "boolean: love.graphics.setColor: argument 3 is a boolean, not a number\n"
        "nan: love.graphics.setColor: argument 1 is not finite\n"
        "infinity: love.graphics.setColor: argument 2 is not finite\n"
        "alpha 0: love.graphics.setColor: alpha is 0, only 1 is supported (translucent colours are not modelled)\n"
        "alpha half: love.graphics.setColor: alpha is 0.5, only 1 is supported (translucent colours are not "
        "modelled)\n"
        "alpha 2: love.graphics.setColor: alpha is 2, only 1 is supported (translucent colours are not modelled)\n"
        "alpha nil: love.graphics.setColor: argument 4 is a nil, not a number\n"
        "clamped: ok\n"
        "alpha 1: ok\n");
}

static void test_an_uncaught_translucent_colour_is_fatal_and_names_set_color_and_the_frame(void) {
    const char *const needles[] = {"love.draw f0: ", "main.lua:2: love.graphics.setColor: ", "alpha is 0.5", NULL};
    fake_love_cli_assert_failure("", FIXTURES "set_color_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_refused_calls_keep_the_colour_set_at_load_time_without_a_canvas(void) {
    unsigned char *pixels = calloc(CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    assert(pixels != NULL);
    static const unsigned char red[4] = {0xFF, 0x00, 0x00, 0xFF};
    for (int row = 0; row < 10; row++)
        for (int column = 0; column < 10; column++)
            for (int channel = 0; channel < 4; channel++)
                pixels[4 * (row * CANVAS_WIDTH + column) + channel] = red[channel];
    fake_love_png_write(PNG, pixels, CANVAS_WIDTH, CANVAS_HEIGHT);
    free(pixels);
    if (system("./tmp/love_capture_check tools/fake_love/harness " PROGRAMS "set_color_refused.lua " PNG
               " 2> " STDERR_LOG) != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "set_color_refused.lua:\n%s", err != NULL ? err : "");
        free(err);
        assert(!"the square must stay red");
    }
}

int main(void) {
    test_set_color_misuse_is_refused_with_a_message_and_opaque_colours_succeed();
    test_an_uncaught_translucent_colour_is_fatal_and_names_set_color_and_the_frame();
    test_refused_calls_keep_the_colour_set_at_load_time_without_a_canvas();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_set_color: all tests passed\n");
    return 0;
}
