#include "devtools/dump.h"
#include "p8/gfx.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DUMP_HEADER "160 120\n"
#define DUMP_ROW_CHARS (P8_SCREEN_WIDTH)
#define DUMP_LINE_LEN (DUMP_ROW_CHARS + 1)
#define DUMP_SIZE (sizeof(DUMP_HEADER) - 1 + (size_t)DUMP_LINE_LEN * P8_SCREEN_HEIGHT)

static char *dump_to_buffer(const p8_vram_t *mem, size_t *out_len) {
    FILE *tmp = tmpfile();
    assert(tmp != NULL);

    int ok = dump_screen(mem, tmp);
    assert(ok);

    long size = ftell(tmp);
    assert(size >= 0);
    rewind(tmp);

    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    size_t got = fread(buf, 1, (size_t)size, tmp);
    fclose(tmp);
    assert(got == (size_t)size);
    buf[got] = '\0';

    *out_len = got;
    return buf;
}

static void set_expected_pixel(char *expected, int x, int y, char hex_digit) {
    size_t line_start = strlen(DUMP_HEADER) + (size_t)y * DUMP_LINE_LEN;
    expected[line_start + (size_t)x] = hex_digit;
}

static void test_dump_of_cleared_screen_is_all_zero_grid(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);
    p8_cls(&mem);

    size_t actual_len;
    char *actual = dump_to_buffer(&mem, &actual_len);

    char *expected = malloc(DUMP_SIZE + 1);
    assert(expected != NULL);
    memcpy(expected, DUMP_HEADER, strlen(DUMP_HEADER));
    size_t offset = strlen(DUMP_HEADER);
    for (int row = 0; row < P8_SCREEN_HEIGHT; row++) {
        memset(expected + offset, '0', DUMP_ROW_CHARS);
        offset += DUMP_ROW_CHARS;
        expected[offset++] = '\n';
    }
    expected[DUMP_SIZE] = '\0';

    assert(actual_len == DUMP_SIZE);
    assert(memcmp(actual, expected, DUMP_SIZE) == 0);

    free(actual);
    free(expected);
}

static void test_dump_of_hand_drawn_pattern_matches_exact_text(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_cls(&mem);
    p8_pset(&gfx, &mem, 0, 0, 10);
    p8_pset(&gfx, &mem, 159, 0, 1);
    p8_pset(&gfx, &mem, 64, 64, 15);
    p8_pset(&gfx, &mem, 0, 119, 9);

    size_t actual_len;
    char *actual = dump_to_buffer(&mem, &actual_len);

    char *expected = malloc(DUMP_SIZE + 1);
    assert(expected != NULL);
    memcpy(expected, DUMP_HEADER, strlen(DUMP_HEADER));
    size_t offset = strlen(DUMP_HEADER);
    for (int row = 0; row < P8_SCREEN_HEIGHT; row++) {
        memset(expected + offset, '0', DUMP_ROW_CHARS);
        offset += DUMP_ROW_CHARS;
        expected[offset++] = '\n';
    }
    expected[DUMP_SIZE] = '\0';

    set_expected_pixel(expected, 0, 0, 'a');
    set_expected_pixel(expected, 159, 0, '1');
    set_expected_pixel(expected, 64, 64, 'f');
    set_expected_pixel(expected, 0, 119, '9');

    assert(actual_len == DUMP_SIZE);
    assert(memcmp(actual, expected, DUMP_SIZE) == 0);

    free(actual);
    free(expected);
}

/* The text `write` puts in a temporary file: an allocated buffer, with
 * no terminating NUL, that the caller frees. */
static char *capture_text(int (*write)(const void *, FILE *), const void *src, size_t *out_len) {
    FILE *tmp = tmpfile();
    assert(tmp != NULL);
    assert(write(src, tmp));
    long size = ftell(tmp);
    assert(size > 0);
    rewind(tmp);
    char *buf = malloc((size_t)size);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, tmp) == (size_t)size);
    fclose(tmp);
    *out_len = (size_t)size;
    return buf;
}

static int write_screen(const void *mem, FILE *out) {
    return dump_screen(mem, out);
}

static int write_pixels(const void *pixels, FILE *out) {
    return dump_pixels(pixels, out);
}

static void test_dump_of_pixel_buffer_matches_dump_of_same_screen(void) {
    static unsigned char pixels[P8_SCREEN_WIDTH * P8_SCREEN_HEIGHT];
    p8_vram_t mem;
    p8_vram_init(&mem);
    for (int i = 0; i < (int)sizeof(pixels); i++) {
        pixels[i] = (unsigned char)((i * 7 + i / P8_SCREEN_WIDTH) % 16);
    }
    memcpy(mem.screen, pixels, sizeof(mem.screen));

    size_t from_screen_len;
    char *from_screen = capture_text(write_screen, &mem, &from_screen_len);
    size_t from_pixels_len;
    char *from_pixels = capture_text(write_pixels, pixels, &from_pixels_len);

    assert(from_pixels_len == DUMP_SIZE);
    assert(from_pixels_len == from_screen_len);
    assert(memcmp(from_pixels, from_screen, from_screen_len) == 0);

    free(from_screen);
    free(from_pixels);
}

int main(void) {
    test_dump_of_cleared_screen_is_all_zero_grid();
    test_dump_of_hand_drawn_pattern_matches_exact_text();
    test_dump_of_pixel_buffer_matches_dump_of_same_screen();
    printf("test_dump: all tests passed\n");
    return 0;
}
