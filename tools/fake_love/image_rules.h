#ifndef FAKE_LOVE_IMAGE_RULES_H
#define FAKE_LOVE_IMAGE_RULES_H

#define IMAGE_RULES_QUAD_VALUES 6
#define IMAGE_RULES_NONE (-1)

typedef enum image_rules_verdict {
    IMAGE_RULES_ACCEPTED,
    IMAGE_RULES_NOT_FINITE,
    IMAGE_RULES_NOT_INTEGER,
    IMAGE_RULES_OUT_OF_RANGE,
} image_rules_verdict_t;

/* A draw position: an integer of magnitude below POINT_COORDINATE_LIMIT
 * (2^20). A fraction is refused because LÖVE's default linear filter blends
 * texels from 1/512 of a pixel on. */
image_rules_verdict_t image_rules_check_position(double position);

/* A position of the present, draw(canvas, x, y, 0, s, s): finite, of
 * magnitude below POINT_COORDINATE_LIMIT (2^20), fractions accepted. */
image_rules_verdict_t image_rules_check_present_position(double position);

/* A scale of the present: finite, greater than 0, at most
 * SCREEN_MAX_SCALE (1024). */
image_rules_verdict_t image_rules_check_present_scale(double scale);

/* newQuad's values in argument order: x, y, width, height, reference width,
 * reference height. All integers; reference sides 1 ... IMAGE_DATA_MAX_SIDE;
 * x and y from 0, width and height from 1, inside the reference size. On a
 * refusal, *refused is the 0-based index of the value blamed; it is left
 * untouched when accepted. */
image_rules_verdict_t image_rules_check_quad(const double values[IMAGE_RULES_QUAD_VALUES], int *refused);

/* The index of the first pixel of the row-major RGBA8 `pixels` whose alpha
 * is neither 00 nor FF, or IMAGE_RULES_NONE. */
int image_rules_find_translucent_pixel(const unsigned char *pixels, int pixel_count);

#endif
