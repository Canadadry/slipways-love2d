#ifndef GAME_BUTTON_IMAGE_H
#define GAME_BUTTON_IMAGE_H

#include <stdint.h>

#include "p8/gfx.h"
#include "p8/image.h"
#include "p8/vram.h"

enum { BUTTON_IMAGE_HEIGHT = 11, BUTTON_IMAGE_ICON_WIDTH = 16, BUTTON_IMAGE_WIDE_WIDTH = 112 };

/* The caller owns its meaning: two equal keys must mean the same pixels. */
typedef struct {
    uint32_t words[4];
} button_image_key_t;

/* Draws key's image, w x h, top-left at world (x, y) under gfx's camera, built through draw on the
 * first frame it is seen (image pixel (0, 0) is the image's top-left; gfx's camera moves during the
 * build and is restored). An image at most BUTTON_IMAGE_ICON_WIDTH wide never evicts a wider one,
 * nor the reverse. Returns 0, drawing nothing, when w x h is over BUTTON_IMAGE_WIDE_WIDTH x
 * BUTTON_IMAGE_HEIGHT or every slot of its pool was copied this frame. */
int button_image_draw(p8_graphics_t *gfx, p8_vram_t *mem, const button_image_key_t *key, int x, int y, int w, int h,
                      p8_image_draw_t draw, void *context);

/* Starts a draw frame: a slot copied earlier in the frame is never rebuilt before the next call. */
void button_image_frame_begin(void);

#endif
