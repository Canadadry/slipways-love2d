#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/canvas.h"
#include "fake_love_cli.h"
#include "fake_love_png.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_rectangle_capture.bin"
#define STATE "tmp/test_fake_love_rectangle_state.txt"
#define STDERR_LOG "tmp/test_fake_love_rectangle_stderr.txt"
#define PNG "tmp/test_fake_love_rectangle.png"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"

static void test_rectangle_misuse_is_refused_with_a_message_and_valid_calls_succeed(void) {
    int status = fake_love_cli_run("", FIXTURES "rectangle_log " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                   CAPTURE, STATE, STDERR_LOG);
    assert(status == 0 && "the rectangle log game must run");
    fake_love_cli_assert_text(
        STATE, "graphics clear draw newCanvas newImage newQuad points rectangle setCanvas setColor\n"
               "line: love.graphics.rectangle: mode 'line' is not supported, only 'fill'\n"
               "full: love.graphics.rectangle: mode 'full' is not supported, only 'fill'\n"
               "number mode: love.graphics.rectangle: argument 1 is a number, not a string\n"
               "table mode: love.graphics.rectangle: argument 1 is a table, not a string\n"
               "four: love.graphics.rectangle: takes 5 arguments, not 4\n"
               "rx: love.graphics.rectangle: takes 5 arguments, not 6\n"
               "trailing nil: love.graphics.rectangle: takes 5 arguments, not 6\n"
               "string: love.graphics.rectangle: argument 2 is a string, not a number\n"
               "nil: love.graphics.rectangle: argument 3 is a nil, not a number\n"
               "boolean: love.graphics.rectangle: argument 4 is a boolean, not a number\n"
               "nan: love.graphics.rectangle: argument 2 is not finite\n"
               "infinity: love.graphics.rectangle: argument 5 is not finite\n"
               "too far: love.graphics.rectangle: argument 2 is out of range (|v| >= 2^20)\n"
               "too wide below: love.graphics.rectangle: argument 4 is out of range (|v| >= 2^20)\n"
               "zero width: ok\n"
               "largest: ok\n"
               "no canvas: ok\n");
}

static void test_an_uncaught_misuse_is_fatal_and_names_rectangle_and_the_frame(void) {
    const char *const needles[] = {"love.draw f0: ", "main.lua:2: love.graphics.rectangle: ",
                                   "mode 'line' is not supported", NULL};
    fake_love_cli_assert_failure("", FIXTURES "rectangle_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

/* Writes PNG: transparent black but for `filled`, inclusive {left, top,
 * right, bottom} ranges, opaque white. */
static void write_expected(const int (*filled)[4], int filled_count) {
    unsigned char *pixels = calloc(CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    assert(pixels != NULL);
    for (int range = 0; range < filled_count; range++)
        for (int row = filled[range][1]; row <= filled[range][3]; row++)
            for (int column = filled[range][0]; column <= filled[range][2]; column++)
                memset(pixels + 4 * (row * CANVAS_WIDTH + column), 0xFF, 4);
    fake_love_png_write(PNG, pixels, CANVAS_WIDTH, CANVAS_HEIGHT);
    free(pixels);
}

static void assert_program_draws(const char *program) {
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

static void test_a_refused_call_and_a_rectangle_on_the_screen_leave_the_canvas_transparent(void) {
    write_expected(NULL, 0);
    assert_program_draws("rectangle_refused.lua");
}

static void test_a_rectangle_fills_the_pixels_of_the_span_rule_on_both_axes(void) {
    static const int filled[][4] = {
        {10, 10, 14, 12}, {20, 7, 24, 9}, {0, 2, 4, 3}, {41, 11, 45, 13}, {159, 119, 159, 119}};
    write_expected(filled, sizeof filled / sizeof filled[0]);
    assert_program_draws("rectangle_draws.lua");
}

int main(void) {
    test_rectangle_misuse_is_refused_with_a_message_and_valid_calls_succeed();
    test_an_uncaught_misuse_is_fatal_and_names_rectangle_and_the_frame();
    test_a_refused_call_and_a_rectangle_on_the_screen_leave_the_canvas_transparent();
    test_a_rectangle_fills_the_pixels_of_the_span_rule_on_both_axes();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_rectangle: all tests passed\n");
    return 0;
}
