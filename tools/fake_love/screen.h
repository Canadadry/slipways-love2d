#ifndef FAKE_LOVE_SCREEN_H
#define FAKE_LOVE_SCREEN_H

#include <stdbool.h>

#include "canvas.h"

#define SCREEN_MAX_SIDE 4096
#define SCREEN_MAX_SCALE 1024.0

typedef struct screen screen_t;

screen_t *screen_create(void);
void screen_free(screen_t *screen);

bool screen_begin_frame(screen_t *screen, int width, int height, bool recorded);

void screen_draw(screen_t *screen, const canvas_source_t *source, const canvas_area_t *quad, double x, double y,
                 double scale_x, double scale_y, const unsigned char tint[4]);

const unsigned char *screen_read_pixels(const screen_t *screen, int *width, int *height);

#endif
