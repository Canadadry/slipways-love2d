#include <stdio.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_unlisted_capture.bin"
#define STATE "tmp/test_fake_love_unlisted_state.txt"
#define STDERR_LOG "tmp/test_fake_love_unlisted_stderr.txt"

#define UNLISTED FIXTURES "unlisted " FIXTURES "three_frames.json " CAPTURE " " STATE
#define NOT_LISTED " is not in the function list of docs/love-port/design.md"

/* `where` is the callback and frame, `line` the file and line, `name` the
 * name the message must give. */
static void assert_unlisted_fails(const char *name, const char *where, const char *line) {
    char environment[128];
    snprintf(environment, sizeof environment, "FAKE_LOVE_UNLISTED='%s'", name);
    char message[256];
    snprintf(message, sizeof message, "%s%s%s", line, name, NOT_LISTED);
    const char *const needles[] = {where, message, NULL};
    fake_love_cli_assert_failure(environment, UNLISTED, CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_an_unlisted_love_field_fails_in_main_and_conf(void) {
    assert_unlisted_fails("love.timer", "main.lua: ", "main.lua:3: ");
    assert_unlisted_fails("love.audio", "conf.lua: ", "conf.lua:3: ");
}

static void test_an_unlisted_module_function_fails_naming_it_in_full(void) {
    assert_unlisted_fails("love.graphics.getWidth", "love.load: ", "main.lua:15: ");
    assert_unlisted_fails("love.filesystem.lines", "love.load: ", "main.lua:17: ");
}

static void test_an_unlisted_method_fails_naming_the_object_kind(void) {
    assert_unlisted_fails("ImageData:mapPixel", "love.load: ", "main.lua:19: ");
    assert_unlisted_fails("Quad:getViewport", "love.load: ", "main.lua:21: ");
    assert_unlisted_fails("Joystick:getName", "love.update f0: ", "main.lua:27: ");
    assert_unlisted_fails("Canvas:getWidth", "love.draw f0: ", "main.lua:33: ");
}

static void test_a_new_field_in_love_or_a_module_fails(void) {
    assert_unlisted_fails("love.keypressed", "main.lua: ", "main.lua:5: ");
    assert_unlisted_fails("love.graphics.extra", "main.lua: ", "main.lua:7: ");
}

int main(void) {
    test_an_unlisted_love_field_fails_in_main_and_conf();
    test_an_unlisted_module_function_fails_naming_it_in_full();
    test_an_unlisted_method_fails_naming_the_object_kind();
    test_a_new_field_in_love_or_a_module_fails();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_unlisted: all tests passed\n");
    return 0;
}
