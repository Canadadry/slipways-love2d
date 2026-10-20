#ifndef P8_IMAGE_H
#define P8_IMAGE_H

#include "p8/gfx.h"

/* One SLOT(width, height) per slot, in slot order. Each width is a multiple of 16,
 * each side at most 256. */
#define P8_IMAGE_SLOTS(SLOT) SLOT(256, 256) P8_IMAGE_SMALL_SLOTS_8(SLOT) P8_IMAGE_SMALL_SLOTS_8(SLOT) \
    P8_IMAGE_SMALL_SLOTS_8(SLOT) P8_IMAGE_SMALL_SLOTS_8(SLOT) P8_IMAGE_WIDE_SLOTS_8(SLOT) \
    P8_IMAGE_WIDE_SLOTS_8(SLOT) P8_IMAGE_WIDE_SLOTS_8(SLOT) P8_IMAGE_WIDE_SLOTS_8(SLOT) \
    SLOT(16, 16) SLOT(16, 16) SLOT(16, 16) SLOT(16, 16) SLOT(16, 16) \
    SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) SLOT(16, 11) \
    SLOT(112, 11) SLOT(112, 11) SLOT(112, 11) SLOT(112, 11) SLOT(112, 11) SLOT(112, 11) SLOT(112, 11) \
    SLOT(112, 11) SLOT(112, 11) SLOT(112, 11)
#define P8_IMAGE_SMALL_SLOTS_8(SLOT) \
    SLOT(48, 8) SLOT(48, 8) SLOT(48, 8) SLOT(48, 8) SLOT(48, 8) SLOT(48, 8) SLOT(48, 8) SLOT(48, 8)
#define P8_IMAGE_WIDE_SLOTS_8(SLOT) \
    SLOT(128, 8) SLOT(128, 8) SLOT(128, 8) SLOT(128, 8) SLOT(128, 8) SLOT(128, 8) SLOT(128, 8) SLOT(128, 8)

#define P8_IMAGE_SLOT_COUNT_EACH(width, height) +1
#define P8_IMAGE_SLOT_COUNT (0 P8_IMAGE_SLOTS(P8_IMAGE_SLOT_COUNT_EACH))
#define P8_IMAGE_PIXELS_EACH(width, height) +(width) * (height)
#define P8_IMAGE_PIXELS (0 P8_IMAGE_SLOTS(P8_IMAGE_PIXELS_EACH))

#define P8_IMAGE_WINDOW_WIDTH P8_SCREEN_WIDTH
#define P8_IMAGE_WINDOW_HEIGHT P8_SCREEN_HEIGHT

typedef struct {
    int width;
    int height;
} p8_image_size_t;

/* Called once per window, so it must be deterministic and touch no game state nor visual_random. */
typedef void (*p8_image_draw_t)(p8_graphics_t *gfx, p8_vram_t *mem, void *context);

p8_image_size_t p8_image_size(int slot);

/* Clears the slot, then calls draw once per window of at most P8_IMAGE_WINDOW_WIDTH x
 * P8_IMAGE_WINDOW_HEIGHT covering w x h, row-major, with gfx's camera at the window's origin: image
 * pixel (x, y) is world (x, y). Restores the camera. The image bakes in the draw palette, the sprite
 * sheet and the fill pattern phase in force (image coordinates, world ones only when drawn at
 * (0, 0)); the runtime never rebuilds it by itself. A p8_fatal on a size over capacity, a cls, a
 * nested build or a p8_image_draw during the build. */
void p8_image_build(p8_graphics_t *gfx, p8_vram_t *mem, int slot, int w, int h, p8_image_draw_t draw, void *context);

/* The slot's last build, top-left at world (x, y) under the camera, clipped to the screen; pixels
 * the build never wrote are transparent. A p8_fatal on a slot never built. */
void p8_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y);

#endif
