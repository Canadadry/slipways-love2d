#include <assert.h>
#include <stdio.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_graphics_capture.bin"
#define STATE "tmp/test_fake_love_graphics_state.txt"
#define STDERR_LOG "tmp/test_fake_love_graphics_stderr.txt"

#define MISUSE FIXTURES "graphics_misuse " FIXTURES "three_frames.json " CAPTURE " " STATE

static void assert_misuse_fails(const char *misuse, const char *const *needles) {
    char environment[128];
    snprintf(environment, sizeof environment, "FAKE_LOVE_MISUSE='%s'", misuse);
    fake_love_cli_assert_failure(environment, MISUSE, CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_graphics_appears_after_conf_and_takes_every_listed_form(void) {
    int status = fake_love_cli_run("", FIXTURES "graphics " FIXTURES "three_frames.json " CAPTURE " " STATE, CAPTURE,
                                   STATE, STDERR_LOG);
    assert(status == 0 && "the graphics game must run");
    fake_love_cli_assert_text(STATE, "conf graphics nil\n"
                                     "main graphics clear newCanvas points setCanvas\n"
                                     "load canvas userdata\n"
                                     "draw\n"
                                     "draw\n"
                                     "draw\n");
}

static void test_new_canvas_takes_only_the_design_size(void) {
    const char *const size[] = {"love.load: ", "main.lua:7: love.graphics.newCanvas: ", "160", NULL};
    assert_misuse_fails("newCanvas size", size);
    const char *const nothing[] = {"main.lua:9: love.graphics.newCanvas: ", NULL};
    assert_misuse_fails("newCanvas nothing", nothing);
    const char *const settings[] = {"main.lua:11: love.graphics.newCanvas: ", NULL};
    assert_misuse_fails("newCanvas settings", settings);
    const char *const strings[] = {"main.lua:13: love.graphics.newCanvas: ", "string", NULL};
    assert_misuse_fails("newCanvas strings", strings);
}

static void test_seventeenth_canvas_fails(void) {
    const char *const needles[] = {"main.lua:16: love.graphics.newCanvas: ", "16 canvases", NULL};
    assert_misuse_fails("newCanvas 17th", needles);
}

static void test_set_canvas_takes_a_canvas_or_nothing(void) {
    const char *const number[] = {"main.lua:19: love.graphics.setCanvas: ", "number", NULL};
    assert_misuse_fails("setCanvas number", number);
    const char *const table[] = {"main.lua:21: love.graphics.setCanvas: ", "table", NULL};
    assert_misuse_fails("setCanvas table", table);
    const char *const mipmap[] = {"main.lua:23: love.graphics.setCanvas: ", NULL};
    assert_misuse_fails("setCanvas mipmap", mipmap);
}

static void test_clear_takes_none_three_or_four_finite_numbers(void) {
    const char *const two[] = {"main.lua:25: love.graphics.clear: ", "2 arguments", NULL};
    assert_misuse_fails("clear two", two);
    const char *const five[] = {"main.lua:27: love.graphics.clear: ", "5 arguments", NULL};
    assert_misuse_fails("clear five", five);
    const char *const string[] = {"main.lua:29: love.graphics.clear: ", "string", NULL};
    assert_misuse_fails("clear string", string);
    const char *const nan[] = {"main.lua:31: love.graphics.clear: ", "finite", NULL};
    assert_misuse_fails("clear nan", nan);
    const char *const infinity[] = {"main.lua:33: love.graphics.clear: ", "finite", NULL};
    assert_misuse_fails("clear infinity", infinity);
    const char *const table[] = {"main.lua:35: love.graphics.clear: ", "table", NULL};
    assert_misuse_fails("clear table", table);
}

static void test_canvas_left_selected_after_draw_is_fatal(void) {
    const char *const draw[] = {"love.draw f0: present cannot be called while a Canvas is active.", NULL};
    assert_misuse_fails("selected in draw", draw);
    const char *const update[] = {"love.draw f0: present cannot be called while a Canvas is active.", NULL};
    assert_misuse_fails("selected in update", update);
}

int main(void) {
    test_graphics_appears_after_conf_and_takes_every_listed_form();
    test_new_canvas_takes_only_the_design_size();
    test_seventeenth_canvas_fails();
    test_set_canvas_takes_a_canvas_or_nothing();
    test_clear_takes_none_three_or_four_finite_numbers();
    test_canvas_left_selected_after_draw_is_fatal();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_graphics: all tests passed\n");
    return 0;
}
