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

image_rules_verdict_t image_rules_check_position(double position);

image_rules_verdict_t image_rules_check_screen_position(double position);

image_rules_verdict_t image_rules_check_screen_scale(double scale);

image_rules_verdict_t image_rules_check_quad(const double values[IMAGE_RULES_QUAD_VALUES], int *refused);

int image_rules_find_translucent_pixel(const unsigned char *pixels, int pixel_count);

#endif
