#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "../tools/fake_love/png_file.h"
#include "fake_love_cli.h"
#include "fake_love_png.h"

#define WIDTH 160
#define HEIGHT 120
#define PNG "tmp/test_fake_love_capture.png"
#define STDERR_LOG "tmp/test_fake_love_capture_stderr.txt"
#define PROGRAMS FAKE_LOVE_CLI_FIXTURES "capture/"
#define NAVY 0x1D2B53FFUL

static unsigned char *fill(int width, int height, unsigned long rgba) {
    unsigned char *pixels = malloc((size_t)width * height * 4);
    assert(pixels != NULL);
    for (int i = 0; i < width * height * 4; i++)
        pixels[i] = (unsigned char)(rgba >> (24 - 8 * (i % 4)));
    return pixels;
}

/* `*err` is the command's stderr, freed by the caller. */
static int run_check(const char *args, char **err) {
    char command[1024];
    int length = snprintf(command, sizeof command, "./tmp/love_capture_check %s > /dev/null 2> %s", args, STDERR_LOG);
    assert(length > 0 && (size_t)length < sizeof command && "the command must fit");
    int status = system(command);
    assert(status != -1 && WIFEXITED(status));
    *err = fake_love_cli_read_file(STDERR_LOG, NULL);
    assert(*err != NULL);
    return WEXITSTATUS(status);
}

static int check_pair(const char *program, const char *png, char **err) {
    char args[512];
    snprintf(args, sizeof args, "tools/fake_love/harness %s%s %s", PROGRAMS, program, png);
    return run_check(args, err);
}

static int check_program(const char *program, char **err) {
    return check_pair(program, PNG, err);
}

static void write_uniform(unsigned long rgba) {
    unsigned char *pixels = fill(WIDTH, HEIGHT, rgba);
    fake_love_png_write(PNG, pixels, WIDTH, HEIGHT);
    free(pixels);
}

static void test_matching_canvas_passes_silently(void) {
    write_uniform(NAVY);
    char *err;
    int status = check_program("clear_navy.lua", &err);
    if (status != 0 || err[0] != '\0') {
        fprintf(stderr, "status %d, stderr:\n%s", status, err);
        assert(!"a matching canvas must pass with nothing on stderr");
    }
    free(err);
}

static void assert_pair_fails(const char *program, const char *png, const char *const *needles) {
    char *err;
    int status = check_pair(program, png, &err);
    if (status != 1) {
        fprintf(stderr, "%s: status %d, expected 1\n%s", program, status, err);
        assert(!"unexpected exit status");
    }
    for (const char *const *needle = needles; *needle != NULL; needle++)
        if (strstr(err, *needle) == NULL) {
            fprintf(stderr, "%s: stderr lacks \"%s\":\n%s", program, *needle, err);
            assert(!"stderr must name the cause");
        }
    free(err);
}

static void assert_fails(const char *program, const char *const *needles) {
    assert_pair_fails(program, PNG, needles);
}

static void write_uniform_except(unsigned long rgba, int x, int y, unsigned long other) {
    unsigned char *pixels = fill(WIDTH, HEIGHT, rgba);
    for (int channel = 0; channel < 4; channel++)
        pixels[(y * WIDTH + x) * 4 + channel] = (unsigned char)(other >> (24 - 8 * channel));
    fake_love_png_write(PNG, pixels, WIDTH, HEIGHT);
    free(pixels);
}

static void test_one_different_pixel_is_named(void) {
    write_uniform_except(NAVY, 37, 81, 0x1D2B54FFUL);
    const char *const needles[] = {"love_capture_check: " PROGRAMS "clear_navy.lua: ", "pixel (37, 81)", "1D2B53FF",
                                   "1D2B54FF", "1 pixels differ", NULL};
    assert_fails("clear_navy.lua", needles);
}

static void test_alpha_is_compared(void) {
    write_uniform_except(NAVY, 0, 0, 0x1D2B5300UL);
    const char *const needles[] = {"pixel (0, 0)", "1D2B53FF", "1D2B5300", NULL};
    assert_fails("clear_navy.lua", needles);
}

static void test_png_of_another_size_is_refused(void) {
    unsigned char *pixels = fill(16, 12, NAVY);
    fake_love_png_write(PNG, pixels, 16, 12);
    free(pixels);
    const char *const needles[] = {"clear_navy.lua: ", PNG " is 16x12, the canvas is 160x120", NULL};
    assert_fails("clear_navy.lua", needles);
}

static void test_unreadable_png_is_refused_with_its_path(void) {
    const char *const missing[] = {"clear_navy.lua: ", "tmp/test_fake_love_capture_missing.png: ", NULL};
    remove("tmp/test_fake_love_capture_missing.png");
    assert_pair_fails("clear_navy.lua", "tmp/test_fake_love_capture_missing.png", missing);
    FILE *file = fopen(PNG, "w");
    assert(file != NULL);
    fputs("not a PNG\n", file);
    assert(fclose(file) == 0);
    const char *const text[] = {"clear_navy.lua: ", PNG ": ", NULL};
    assert_fails("clear_navy.lua", text);
}

static void test_program_failures_carry_the_host_error(void) {
    write_uniform(NAVY);
    const char *const raises[] = {"raises.lua: ", "love.draw f0: ", PROGRAMS "raises.lua:2: boom", NULL};
    assert_fails("raises.lua", raises);
    const char *const number[] = {"returns_number.lua: ", "love.load: ", "returned a number, not a function", NULL};
    assert_fails("returns_number.lua", number);
    const char *const selected[] = {"leaves_canvas.lua: ", "present cannot be called while a Canvas is active.",
                                    NULL};
    assert_fails("leaves_canvas.lua", selected);
}

static void test_wrong_argument_count_is_a_usage_error(void) {
    char *err;
    assert(run_check("tools/fake_love/harness", &err) == 2);
    assert(strstr(err, "usage: love_capture_check") != NULL);
    free(err);
}

static void test_encoder_output_reads_back(void) {
    unsigned char *expected = fill(WIDTH, HEIGHT, 0x1D2B53FFUL);
    expected[(81 * WIDTH + 37) * 4 + 3] = 0x10;
    fake_love_png_write(PNG, expected, WIDTH, HEIGHT);
    unsigned char *pixels;
    int width, height;
    const char *reason = png_file_load(PNG, &pixels, &width, &height);
    assert(reason == NULL && "the encoder's PNG must decode");
    assert(width == WIDTH && height == HEIGHT);
    assert(memcmp(pixels, expected, WIDTH * HEIGHT * 4) == 0 && "every byte must survive");
    free(pixels);
    free(expected);
}

int main(void) {
    test_encoder_output_reads_back();
    test_matching_canvas_passes_silently();
    test_one_different_pixel_is_named();
    test_alpha_is_compared();
    test_png_of_another_size_is_refused();
    test_unreadable_png_is_refused_with_its_path();
    test_program_failures_carry_the_host_error();
    test_wrong_argument_count_is_a_usage_error();
    printf("test_fake_love_capture: OK\n");
    return 0;
}
