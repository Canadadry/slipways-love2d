#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/canvas.h"
#include "fake_love_cli.h"
#include "fake_love_png.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_points_capture.bin"
#define STATE "tmp/test_fake_love_points_state.txt"
#define STDERR_LOG "tmp/test_fake_love_points_stderr.txt"
#define PNG "tmp/test_fake_love_points.png"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"

static void test_points_misuse_is_refused_with_a_message_and_empty_calls_succeed(void) {
    int status = fake_love_cli_run("", FIXTURES "points_log " FIXTURES "three_frames.json " CAPTURE " " STATE, CAPTURE,
                                   STATE, STDERR_LOG);
    assert(status == 0 && "the points log game must run");
    fake_love_cli_assert_text(
        STATE, "graphics clear draw newCanvas newImage newQuad points pop push rectangle setCanvas setColor "
               "setDefaultFilter translate\n"
               "odd arguments: love.graphics.points: odd number of coordinates (3)\n"
               "odd table: love.graphics.points: odd number of coordinates (3)\n"
               "table then numbers: love.graphics.points: a table must be the only argument, not one of 3\n"
               "strings: love.graphics.points: argument 1 is a string, not a number\n"
               "boolean: love.graphics.points: argument 1 is a boolean, not a number\n"
               "nil: love.graphics.points: argument 1 is a nil, not a number\n"
               "pairs: love.graphics.points: element 1 is a table, not a number\n"
               "coloured: love.graphics.points: element 1 is a table, not a number\n"
               "hole: love.graphics.points: element 2 is a nil, not a number\n"
               "nan: love.graphics.points: argument 1 is not finite\n"
               "infinity: love.graphics.points: argument 1 is not finite\n"
               "table nan: love.graphics.points: element 2 is not finite\n"
               "too far: love.graphics.points: argument 1 is out of range (|v| >= 2^20)\n"
               "too far below: love.graphics.points: argument 2 is out of range (|v| >= 2^20)\n"
               "table too far: love.graphics.points: element 1 is out of range (|v| >= 2^20)\n"
               "nothing: ok\n"
               "empty table: ok\n"
               "largest: ok\n"
               "no canvas: ok\n");
}

static void test_an_uncaught_misuse_is_fatal_and_names_points_and_the_frame(void) {
    const char *const needles[] = {"love.draw f0: ", "main.lua:2: love.graphics.points: ", "odd number of coordinates (3)",
                                   NULL};
    fake_love_cli_assert_failure("", FIXTURES "points_uncaught " FIXTURES "three_frames.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

/* Writes PNG: transparent black but for `lit`, {x, y} pairs, opaque white. */
static void write_expected(const int (*lit)[2], int lit_count) {
    unsigned char *pixels = calloc(CANVAS_WIDTH * CANVAS_HEIGHT, 4);
    assert(pixels != NULL);
    for (int point = 0; point < lit_count; point++)
        memset(pixels + 4 * (lit[point][1] * CANVAS_WIDTH + lit[point][0]), 0xFF, 4);
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

static void test_a_refused_call_and_points_on_the_screen_leave_the_canvas_transparent(void) {
    write_expected(NULL, 0);
    assert_program_draws("points_refused.lua");
}

static void test_both_forms_light_the_pixels_of_the_point_rule(void) {
    static const int lit[][2] = {{10, 20}, {9, 9}, {30, 40}, {159, 119}, {20, 2}};
    write_expected(lit, sizeof lit / sizeof lit[0]);
    assert_program_draws("points_forms.lua");
}

int main(void) {
    test_points_misuse_is_refused_with_a_message_and_empty_calls_succeed();
    test_an_uncaught_misuse_is_fatal_and_names_points_and_the_frame();
    test_a_refused_call_and_points_on_the_screen_leave_the_canvas_transparent();
    test_both_forms_light_the_pixels_of_the_point_rule();
    remove(PNG);
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_points: all tests passed\n");
    return 0;
}
