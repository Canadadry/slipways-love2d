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

static void assert_stderr_holds(const char *const *needles) {
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL);
    for (const char *const *needle = needles; *needle != NULL; needle++) {
        if (strstr(err, *needle) == NULL) {
            fprintf(stderr, "stderr:\n%s\nmissing: %s\n", err, *needle);
            assert(!"stderr must hold the warning");
        }
    }
    free(err);
}

static void build_colours_capture(unsigned char capture[4 * FRAME_SIZE]) {
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
    unsigned char *foreign_pixel = capture + 3 * FRAME_SIZE;
    memcpy(foreign_pixel, single_pixels, FRAME_SIZE);
    foreign_pixel[0] = 0x20;
}

static void test_capture_holds_canvas_zero_of_every_frame(void) {
    assert_runs(FIXTURES "capture_colours " FIXTURES "capture_colours.json " CAPTURE " " STATE);
    static unsigned char expected[4 * FRAME_SIZE];
    build_colours_capture(expected);
    assert_capture(expected, sizeof expected);
    fake_love_cli_assert_text(STATE, "frames 4\n");
    const char *const needles[] = {"warning: 1 pixels", "first f3 pixel (0, 0) 4C0000FF\n", NULL};
    assert_stderr_holds(needles);
}

static void test_frames_without_canvas_are_black_and_warned(void) {
    assert_runs(FIXTURES "counting " FIXTURES "three_frames.json " CAPTURE " " STATE);
    static unsigned char expected[3 * FRAME_SIZE];
    assert_capture(expected, sizeof expected);
    const char *const needles[] = {"warning: 57600 pixels", "first f0 no canvas\n", NULL};
    assert_stderr_holds(needles);
}

static void test_zero_frames_give_an_empty_capture(void) {
    assert_runs(FIXTURES "counting " FIXTURES "zero_frames.json " CAPTURE " " STATE);
    assert_capture(NULL, 0);
    char *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(err != NULL && strstr(err, "warning") == NULL && "no frame, no warning");
    free(err);
}

int main(void) {
    test_capture_holds_canvas_zero_of_every_frame();
    test_frames_without_canvas_are_black_and_warned();
    test_zero_frames_give_an_empty_capture();
    remove(CAPTURE);
    remove(STATE);
    remove(STDERR_LOG);
    printf("test_fake_love_frames: all tests passed\n");
    return 0;
}
