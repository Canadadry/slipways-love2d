#ifndef FAKE_LOVE_SCREEN_H
#define FAKE_LOVE_SCREEN_H

#include <stdbool.h>

#include "canvas.h"

#define SCREEN_MAX_SIDE 4096
#define SCREEN_MAX_SCALE 1024.0

typedef struct screen screen_t;

/* NULL when memory runs out; the caller releases the result with
 * screen_free. A new screen holds no frame. */
screen_t *screen_create(void);
void screen_free(screen_t *screen);

/* Starts a frame of `width` x `height`. A recorded frame is all 000000FF;
 * an unrecorded one touches no pixel buffer, and none is allocated until a
 * frame is recorded. False only when memory runs out, the frame then
 * unrecorded. Asserts sides of 1 to SCREEN_MAX_SIDE. */
bool screen_begin_frame(screen_t *screen, int width, int height, bool recorded);

/* Draws the `quad` texels of `source` with the quad's top-left corner at
 * (x, y), stretched by `scale_x` and `scale_y`, tinted by `tint` and
 * blended under design.md's rule "The screen": nothing on an unrecorded
 * frame. Asserts the quad inside the source and not empty, a finite
 * position of magnitude below 2^20 and finite scales in
 * (0, SCREEN_MAX_SCALE]. */
void screen_draw(screen_t *screen, const canvas_source_t *source, const canvas_area_t *quad, double x, double y,
                 double scale_x, double scale_y, const unsigned char tint[4]);

/* The current frame's row-major RGBA8 pixels and size, valid until the
 * next screen_begin_frame or screen_free; NULL, sizes left untouched, when
 * the frame is not recorded or none was begun. */
const unsigned char *screen_read_pixels(const screen_t *screen, int *width, int *height);

#endif
