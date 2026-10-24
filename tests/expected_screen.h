#ifndef TESTS_EXPECTED_SCREEN_H
#define TESTS_EXPECTED_SCREEN_H

typedef struct {
    unsigned char *pixels;
    int width;
    int height;
} expected_screen_t;

typedef void (*expected_screen_texel_t)(int column, int row, unsigned char rgba[4]);

/* A frame's start, every pixel 000000FF; freed by expected_screen_free. */
expected_screen_t expected_screen_black(int width, int height);

void expected_screen_free(expected_screen_t *screen);

/* Draws quad {column, row, width, height} of `texel_of` at (x, y), by the
 * rule and the tint and blend formulas of design.md's "The screen", pixel
 * by pixel. */
void expected_screen_draw(expected_screen_t *screen, expected_screen_texel_t texel_of, const int quad[4], double x,
                          double y, double scale_x, double scale_y, const unsigned char tint[4]);

void expected_screen_assert_pixel(const expected_screen_t *screen, int i, int j, unsigned r, unsigned g, unsigned b);

/* Asserts that the PNG at `path` holds exactly `expected`, all four
 * channels, naming the first differing pixel. */
void expected_screen_assert_file(const char *path, const expected_screen_t *expected);

#endif
