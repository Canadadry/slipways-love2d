#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_touch_capture.bin"
#define STATE "tmp/test_fake_love_touch_state.txt"
#define STDERR_LOG "tmp/test_fake_love_touch_stderr.txt"

#define NOTHING "touches mouse 0,0 up"

static void assert_runs(const char *environment, const char *args) {
    int status = fake_love_cli_run(environment, args, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

static void test_a_replay_without_touches_or_mouse_holds_nothing_anywhere(void) {
    assert_runs("", FIXTURES "touch_log " FIXTURES "three_frames.json " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "conf touch nil mouse nil\n"
                                     "main new list true " NOTHING "\n"
                                     "load " NOTHING "\n"
                                     "f0 update " NOTHING "\n"
                                     "f0 draw " NOTHING "\n"
                                     "f1 update " NOTHING "\n"
                                     "f1 draw " NOTHING "\n"
                                     "f2 update " NOTHING "\n"
                                     "f2 draw " NOTHING "\n"
                                     "end " NOTHING "\n");
}

#define TOUCH_FRAMES "tests/fixtures/touch_frames.json"

static void test_touches_and_mouse_follow_the_replay_beside_the_pad(void) {
    assert_runs("", FIXTURES "touch_log " TOUCH_FRAMES " " CAPTURE " " STATE);
    fake_love_cli_assert_text(STATE, "conf touch nil mouse nil\n"
                                     "main new list true " NOTHING "\n"
                                     "load " NOTHING "\n"
                                     "f0 update " NOTHING "\n"
                                     "f0 draw " NOTHING "\n"
                                     "new A userdata adds false\n"
                                     "f1 update touches A 100,700 mouse 0,0 up\n"
                                     "f1 draw touches A 100,700 mouse 0,0 up\n"
                                     "f2 update touches A 120,690 mouse 0,0 up\n"
                                     "f2 draw touches A 120,690 mouse 0,0 up\n"
                                     "new B userdata adds false\n"
                                     "f3 pressed a touches A 120,690 B 300,750 mouse 0,0 up\n"
                                     "f3 update touches A 120,690 B 300,750 mouse 0,0 up\n"
                                     "f3 draw touches A 120,690 B 300,750 mouse 0,0 up\n"
                                     "f4 released a touches B 300,750 mouse 0,0 up\n"
                                     "f4 update touches B 300,750 mouse 0,0 up\n"
                                     "f4 draw touches B 300,750 mouse 0,0 up\n"
                                     "f5 update " NOTHING "\n"
                                     "f5 draw " NOTHING "\n"
                                     "f6 update touches mouse 200,400 down\n"
                                     "f6 draw touches mouse 200,400 down\n"
                                     "f7 pressed dpup touches mouse 200,400 down\n"
                                     "f7 update touches mouse 200,400 down\n"
                                     "f7 draw touches mouse 200,400 down\n"
                                     "f8 released dpup touches A 10,20 mouse 200,400 up\n"
                                     "f8 update touches A 10,20 mouse 200,400 up\n"
                                     "f8 draw touches A 10,20 mouse 200,400 up\n"
                                     "f9 resize touches A 10,20 mouse 200,400 up\n"
                                     "new C userdata adds false\n"
                                     "f9 update touches C 843,389 mouse 200,400 up\n"
                                     "f9 draw touches C 843,389 mouse 200,400 up\n"
                                     "f10 update touches mouse 200,400 up\n"
                                     "f10 draw touches mouse 200,400 up\n"
                                     "f11 pressed dpdown touches mouse 200,400 up\n"
                                     "f11 update touches mouse 200,400 up\n"
                                     "f11 draw touches mouse 200,400 up\n"
                                     "end touches mouse 200,400 up\n");
}

#define MISUSE FIXTURES "touch_misuse " TOUCH_FRAMES " " CAPTURE " " STATE

static void assert_failure(const char *environment, const char *args, const char *const *needles) {
    fake_love_cli_assert_failure(environment, args, CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_a_lifted_touch_id_is_invalid(void) {
    const char *const needles[] = {"love.update f4: ", "main.lua:7: Invalid active touch ID", NULL};
    assert_failure("FAKE_LOVE_MISUSE=lifted", MISUSE, needles);
}

static void test_any_other_form_of_the_four_functions_fails_naming_it(void) {
    const char *const number_id[] = {"main.lua:11: love.touch.getPosition takes one touch id, not a number", NULL};
    assert_failure("FAKE_LOVE_MISUSE='number id'", MISUSE, number_id);
    const char *const touches_argument[] = {"main.lua:13: love.touch.getTouches takes no argument", NULL};
    assert_failure("FAKE_LOVE_MISUSE='touches argument'", MISUSE, touches_argument);
    const char *const right_button[] = {
        "main.lua:15: love.mouse.isDown: only isDown(1), the left button, is in the function list", NULL};
    assert_failure("FAKE_LOVE_MISUSE='right button'", MISUSE, right_button);
    const char *const no_button[] = {"main.lua:17: love.mouse.isDown: only isDown(1)", NULL};
    assert_failure("FAKE_LOVE_MISUSE='no button'", MISUSE, no_button);
    const char *const mouse_argument[] = {"main.lua:19: love.mouse.getPosition takes no argument", NULL};
    assert_failure("FAKE_LOVE_MISUSE='mouse argument'", MISUSE, mouse_argument);
}

static void test_touch_callbacks_and_other_touch_names_are_not_in_the_function_list(void) {
    const char *const callback[] = {
        "main.lua:21: love.touchpressed is not in the function list of docs/love-port/design.md", NULL};
    assert_failure("FAKE_LOVE_MISUSE=callback", MISUSE, callback);
    const char *const pressure[] = {
        "main.lua:23: love.touch.getPressure is not in the function list of docs/love-port/design.md", NULL};
    assert_failure("FAKE_LOVE_MISUSE=pressure", MISUSE, pressure);
}

static void test_a_touch_outside_the_window_runs_nothing(void) {
    const char *const needles[] = {FIXTURES "touch_invalid.json", "f1", "touch 4 at (390, 10)", "390x844", NULL};
    assert_failure("", FIXTURES "touch_log " FIXTURES "touch_invalid.json " CAPTURE " " STATE, needles);
}

int main(void) {
    test_a_replay_without_touches_or_mouse_holds_nothing_anywhere();
    test_touches_and_mouse_follow_the_replay_beside_the_pad();
    test_a_lifted_touch_id_is_invalid();
    test_any_other_form_of_the_four_functions_fails_naming_it();
    test_touch_callbacks_and_other_touch_names_are_not_in_the_function_list();
    test_a_touch_outside_the_window_runs_nothing();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_touch: all tests passed\n");
    return 0;
}
