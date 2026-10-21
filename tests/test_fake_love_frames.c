#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fake_love_cli.h"

#define FIXTURES FAKE_LOVE_CLI_FIXTURES
#define CAPTURE "tmp/test_fake_love_frames_capture.bin"
#define STATE "tmp/test_fake_love_frames_state.txt"
#define STDERR_LOG "tmp/test_fake_love_frames_stderr.txt"
#define FRAME_SIZE 9600
#define ROW_BYTES 80

static void assert_runs(const char *args) {
    int status = fake_love_cli_run("", args, CAPTURE, STATE, STDERR_LOG);
    if (status != 0) {
        char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
        fprintf(stderr, "fake_love %s: status %d\n%s", args, status, err != NULL ? err : "");
        free(err);
        assert(!"the run must succeed");
    }
}

static void assert_capture(const unsigned char *expected, long size) {
    long actual_size = -1;
    char *capture = fake_love_cli_read_file(CAPTURE, &actual_size);
    assert(capture != NULL && "the capture file must be written");
    assert(actual_size == size && "one capture frame of 9 600 bytes per frame");
    for (long i = 0; i < size; i++) {
        if ((unsigned char)capture[i] != expected[i]) {
            fprintf(stderr, "capture byte %ld (frame %ld, offset %ld): %02X, expected %02X\n", i, i / FRAME_SIZE,
                    i % FRAME_SIZE, (unsigned char)capture[i], expected[i]);
            assert(!"unexpected capture byte");
        }
    }
    free(capture);
}

static void build_colours_capture(unsigned char capture[3 * FRAME_SIZE]) {
    unsigned char *bands = capture;
    for (int y = 0; y < 120; y++)
        for (int byte = 0; byte < ROW_BYTES; byte++) {
            int colour = byte / 5;
            bands[y * ROW_BYTES + byte] = (unsigned char)(colour | colour << 4);
        }
    unsigned char *single_pixels = capture + FRAME_SIZE;
    memset(single_pixels, 0, FRAME_SIZE);
    single_pixels[0] = 0x21;
    single_pixels[ROW_BYTES + 1] = 0xA9;
    single_pixels[FRAME_SIZE - 1] = 0xF0;
    unsigned char *elsewhere = capture + 2 * FRAME_SIZE;
    memcpy(elsewhere, single_pixels, FRAME_SIZE);
}

static void test_capture_holds_canvas_zero_of_every_frame(void) {
    assert_runs(FIXTURES "capture_colours " FIXTURES "three_frames.json " CAPTURE " " STATE);
    static unsigned char expected[3 * FRAME_SIZE];
    build_colours_capture(expected);
    assert_capture(expected, sizeof expected);
    fake_love_cli_assert_text(STATE, "frames 3\n");
}

static void set_pixel(unsigned char frame[FRAME_SIZE], int x, int y, int colour) {
    unsigned char *byte = frame + y * ROW_BYTES + x / 2;
    *byte = (unsigned char)(x % 2 == 0 ? (*byte & 0xF0) | colour : (*byte & 0x0F) | colour << 4);
}

static void draw_expected_scene(unsigned char frame[FRAME_SIZE], int offset) {
    for (int y = 10; y <= 11; y++)
        for (int x = 0; x < 4; x++)
            set_pixel(frame, offset + x, y, 11);
    set_pixel(frame, offset + 5, 10, 10);
    set_pixel(frame, offset, 20, 8);
    set_pixel(frame, offset + 1, 21, 12);
    set_pixel(frame, offset, 25, 8);
    set_pixel(frame, offset + 2, 25, 8);
}

static void test_a_game_following_the_drawing_rules_runs_without_error(void) {
    assert_runs(FIXTURES "drawing_rules " FIXTURES "three_frames.json " CAPTURE " " STATE);
    static unsigned char expected[3 * FRAME_SIZE];
    unsigned char *frame = expected;
    memset(frame, 0x11, FRAME_SIZE);
    for (int index = 0; index < 3; index++, frame += FRAME_SIZE) {
        if (index > 0)
            memcpy(frame, frame - FRAME_SIZE, FRAME_SIZE);
        draw_expected_scene(frame, 20 * index);
    }
    assert(expected[FRAME_SIZE + 20 * ROW_BYTES + 10] == 0x18 && "frame 1: the sprite texel of colour 8 at (20, 20)");
    assert(expected[2 * FRAME_SIZE + 25 * ROW_BYTES + 21] == 0x18 && "frame 2: a tinted letter texel at (42, 25)");
    assert(expected[2 * FRAME_SIZE + 10 * ROW_BYTES + 0] == 0xBB && "frame 2: the frame-0 rectangle is kept");
    assert_capture(expected, sizeof expected);
    fake_love_cli_assert_text(STATE, "frames 3\n");
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL && err[0] == '\0' && "a game following the rules writes nothing on stderr");
    free(err);
}

static void test_foreign_pixel_on_canvas_zero_is_fatal(void) {
    const char *const needles[] = {
        "fake_love: frame f3: pixel (0, 0) of canvas 0 is FF0000FF, not one of the 16 palette colours (1 such pixels "
        "in the frame)\n",
        NULL};
    fake_love_cli_assert_failure("", FIXTURES "capture_colours " FIXTURES "capture_colours.json " CAPTURE " " STATE,
                                 CAPTURE, STATE, STDERR_LOG, needles);
}

static void test_frame_without_canvas_is_fatal(void) {
    const char *const needles[] = {
        "fake_love: frame f0: no canvas exists at the end of the frame (create the game canvas first, in love.load)\n",
        NULL};
    fake_love_cli_assert_failure("", FIXTURES "no_canvas " FIXTURES "three_frames.json " CAPTURE " " STATE, CAPTURE,
                                 STATE, STDERR_LOG, needles);
}

static void test_zero_frames_give_an_empty_capture(void) {
    assert_runs(FIXTURES "counting " FIXTURES "zero_frames.json " CAPTURE " " STATE);
    assert_capture(NULL, 0);
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL && err[0] == '\0' && "no frame, nothing on stderr");
    free(err);
}

int main(void) {
    test_capture_holds_canvas_zero_of_every_frame();
    test_a_game_following_the_drawing_rules_runs_without_error();
    test_foreign_pixel_on_canvas_zero_is_fatal();
    test_frame_without_canvas_is_fatal();
    test_zero_frames_give_an_empty_capture();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_frames: all tests passed\n");
    return 0;
}
