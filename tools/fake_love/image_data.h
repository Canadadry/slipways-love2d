#ifndef FAKE_LOVE_IMAGE_DATA_H
#define FAKE_LOVE_IMAGE_DATA_H

#include <stdbool.h>
#include <stddef.h>

#define IMAGE_DATA_MAX_SIDE 1024

typedef struct image_data {
    int width;
    int height;
    unsigned char pixels[]; /* row-major RGBA8 */
} image_data_t;

bool image_data_accepts_side(double side);

/* The bytes image_data_init needs for this size. Asserts accepted sides. */
size_t image_data_byte_count(int width, int height);

/* Over caller-owned memory of image_data_byte_count bytes: sets the size
 * and every pixel to 00000000. Asserts accepted sides. */
void image_data_init(image_data_t *data, int width, int height);

/* Whether pixel (floor(x), floor(y)) is inside, as LÖVE 11.5 floors both
 * methods' coordinates; if so it is *column, *row, which are left untouched
 * otherwise. NaN and infinities are outside. */
bool image_data_find_pixel(const image_data_t *data, double x, double y, int *column, int *row);

/* Stores the four bytes as given, alpha 0 included: nothing is blended.
 * Asserts (column, row) is inside. */
void image_data_write_pixel(image_data_t *data, int column, int row, const unsigned char rgba[4]);

/* Asserts (column, row) is inside. */
void image_data_read_pixel(const image_data_t *data, int column, int row, unsigned char rgba[4]);

/* setPixel's rule in LÖVE 11.5: clamped to [0, 1], times 255 plus 0.5 in
 * double, floored, so 0.5 gives 128 and 2.5/255 gives 3. Not
 * canvas_convert_clear_component (float, ties to even: 2) nor
 * canvas_convert_draw_component (float, truncated: 127). Asserts not NaN. */
unsigned char image_data_convert_component(double component);

/* getPixel's value in LÖVE 11.5: byte / 255 in double, the Lua expression's
 * value, not its float. */
double image_data_read_component(unsigned char byte);

#endif
