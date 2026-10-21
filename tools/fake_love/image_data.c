#include "image_data.h"

#include <assert.h>
#include <float.h>
#include <math.h>
#include <string.h>

_Static_assert(FLT_EVAL_METHOD == 0, "setPixel's product and sum must each round to double, as LÖVE's do");

unsigned char image_data_convert_component(double component) {
    assert(!isnan(component) && "image_data_convert_component: NaN has no byte");
    double clamped = fmin(fmax(component, 0.0), 1.0);
    return (unsigned char)floor(clamped * 255.0 + 0.5);
}

double image_data_read_component(unsigned char byte) {
    return byte / 255.0;
}

bool image_data_accepts_side(double side) {
    return side >= 1 && side <= IMAGE_DATA_MAX_SIDE && side == floor(side);
}

size_t image_data_byte_count(int width, int height) {
    assert(image_data_accepts_side(width) && image_data_accepts_side(height));
    return sizeof(image_data_t) + (size_t)width * (size_t)height * 4;
}

void image_data_init(image_data_t *data, int width, int height) {
    assert(image_data_accepts_side(width) && image_data_accepts_side(height));
    data->width = width;
    data->height = height;
    memset(data->pixels, 0, (size_t)width * (size_t)height * 4);
}

bool image_data_find_pixel(const image_data_t *data, double x, double y, int *column, int *row) {
    double floored_x = floor(x);
    double floored_y = floor(y);
    /* Compared in double first: an int conversion would wrap 2^32 + 3 onto
     * pixel 3, and NaN fails every comparison. */
    if (!(floored_x >= 0 && floored_x < data->width && floored_y >= 0 && floored_y < data->height)) {
        return false;
    }
    *column = (int)floored_x;
    *row = (int)floored_y;
    return true;
}

static size_t pixel_offset(const image_data_t *data, int column, int row) {
    assert(column >= 0 && column < data->width && row >= 0 && row < data->height);
    return ((size_t)row * (size_t)data->width + (size_t)column) * 4;
}

void image_data_write_pixel(image_data_t *data, int column, int row, const unsigned char rgba[4]) {
    memcpy(data->pixels + pixel_offset(data, column, row), rgba, 4);
}

void image_data_read_pixel(const image_data_t *data, int column, int row, unsigned char rgba[4]) {
    memcpy(rgba, data->pixels + pixel_offset(data, column, row), 4);
}
