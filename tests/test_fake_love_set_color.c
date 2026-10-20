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
        "graphics clear draw newCanvas newImage newQuad points pop push rectangle setCanvas setColor translate\n"
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

static void test_transform_misuse_is_refused_with_a_message_and_the_offset_resets_before_each_draw(void) {
    int status = fake_love_cli_run("", FIXTURES "transform_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the transform log game must run");
    fake_love_cli_assert_text(
        STATE,
        "conf graphics nil\n"
        "graphics clear draw newCanvas newImage newQuad points pop push rectangle setCanvas setColor translate\n"
        "push nil: love.graphics.push: takes no arguments, not 1 (push(\"all\") and push(\"transform\") are not "
        "supported)\n"
        "push all: love.graphics.push: takes no arguments, not 1 (push(\"all\") and push(\"transform\") are not "
        "supported)\n"
        "push transform: love.graphics.push: takes no arguments, not 1 (push(\"all\") and push(\"transform\") are not "
        "supported)\n"
        "pop argument: love.graphics.pop: takes no arguments, not 1\n"
        "translate none: love.graphics.translate: takes 2 numbers, dx and dy, not 0 arguments\n"
        "translate one: love.graphics.translate: takes 2 numbers, dx and dy, not 1 arguments\n"
        "translate three: love.graphics.translate: takes 2 numbers, dx and dy, not 3 arguments\n"
        "translate strings: love.graphics.translate: argument 1 is a string, not a number\n"
        "translate nil: love.graphics.translate: argument 2 is a nil, not a number\n"
        "translate half: love.graphics.translate: argument 1 is not an integer\n"
        "translate nan: love.graphics.translate: argument 2 is not finite\n"
        "translate infinity: love.graphics.translate: argument 1 is not finite\n"
        "translate 2^20: love.graphics.translate: argument 1 is out of range (|v| >= 2^20)\n"
        "offset after refusals: 0 0\n"
        "total x: love.graphics.translate: the offset would reach 2^20 (x: 1048576)\n"
        "total y: love.graphics.translate: the offset would reach 2^20 (y: -1048576)\n"
        "offset after totals: 0 0\n"
        "pushed: 128\n"
        "push 129: love.graphics.push: Maximum stack depth reached (more pushes than pops?)\n"
        "popped: 128\n"
        "pop empty: love.graphics.pop: Minimum stack depth reached (more pops than pushes?)\n"
        "points x in range: ok\n"
        "points x: love.graphics.points: argument 1 is out of range after translate (|v| >= 2^20)\n"
        "points table: love.graphics.points: element 3 is out of range after translate (|v| >= 2^20)\n"
        "rectangle x in range: ok\n"
        "rectangle x: love.graphics.rectangle: argument 2 is out of range after translate (|v| >= 2^20)\n"
        "draw x in range: ok\n"
        "draw x: love.graphics.draw: argument 3 is out of range after translate (|v| >= 2^20)\n"
        "points y: love.graphics.points: argument 2 is out of range after translate (|v| >= 2^20)\n"
        "rectangle y: love.graphics.rectangle: argument 3 is out of range after translate (|v| >= 2^20)\n"
        "draw y: love.graphics.draw: argument 4 is out of range after translate (|v| >= 2^20)\n"
        "offset on the canvas: 3 4\n"
        "offset after pop: 0 0\n"
        "f0 update offset: 7 9\n"
        "f0 draw offset: 0 0, free depth: 127\n"
        "f1 update offset: 2 3\n"
        "f1 draw offset: 0 0, free depth: 126\n"
        "f2 update offset: 2 3\n"
        "f2 draw offset: 0 0, free depth: 125\n");
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
    test_transform_misuse_is_refused_with_a_message_and_the_offset_resets_before_each_draw();
    test_an_uncaught_translucent_colour_is_fatal_and_names_set_color_and_the_frame();
    test_refused_calls_keep_the_colour_set_at_load_time_without_a_canvas();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_set_color: all tests passed\n");
    return 0;
}
