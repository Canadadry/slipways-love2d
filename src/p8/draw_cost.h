#ifndef P8_DRAW_COST_H
#define P8_DRAW_COST_H

#include "p8/gfx.h"

/* PSn00bSDK packet sizes on MIPS, tag included. */
enum {
    P8_GPU_FILL_BYTES = 16,
    P8_GPU_TILE_1_BYTES = 12,
    P8_GPU_TILE_BYTES = 16,
    P8_GPU_LINE_F2_BYTES = 16,
    P8_GPU_SPRT_BYTES = 20,
    P8_GPU_DR_TPAGE_BYTES = 8,
    P8_GPU_DR_TWIN_BYTES = 8,
    P8_GPU_DR_ENV_BYTES = 36,
    P8_GPU_POLY_FT4_BYTES = 40,
    P8_GPU_DR_AREA_BYTES = 12,
    P8_GPU_DR_OFFSET_BYTES = 8,
    P8_GPU_DR_STP_BYTES = 8,
    P8_GPU_COMMAND_BYTES = 8,
};

typedef struct {
    unsigned long bytes;
    int primitives;
} p8_draw_cost_t;

/* The texture window of the frame's last textured call: none for spr and print, the pattern key
 * for a patterned fill. */
typedef struct {
    int patterned;
    unsigned long pattern_key;
} p8_draw_cost_window_t;

/* An upper bound of what the GPU backend may emit for each call, 0 when the call's screen bounding
 * box misses 0..159 x 0..119. Coordinates are the API's, before gfx's camera. */

/* Resets *window to the frame's initial state and returns the fixed per-frame overhead
 * (draw environments, texture state, x2 upscale quad). */
p8_draw_cost_t p8_draw_cost_frame_begin(p8_draw_cost_window_t *window);

/* What tells two pattern slots apart: the pattern, the colour byte and the transparent flag. */
unsigned long p8_draw_cost_pattern_key(const p8_graphics_t *gfx, int col);

p8_draw_cost_t p8_draw_cost_cls(void);
p8_draw_cost_t p8_draw_cost_pset(const p8_graphics_t *gfx, int x, int y);
p8_draw_cost_t p8_draw_cost_line(const p8_graphics_t *gfx, int x1, int y1, int x2, int y2);
p8_draw_cost_t p8_draw_cost_rect(const p8_graphics_t *gfx, int x1, int y1, int x2, int y2);
p8_draw_cost_t p8_draw_cost_rectfill(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int x1, int y1, int x2, int y2,
                                     int col);
p8_draw_cost_t p8_draw_cost_circ(const p8_graphics_t *gfx, int cx, int cy, int r);
p8_draw_cost_t p8_draw_cost_circfill(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int cx, int cy, int r, int col);
p8_draw_cost_t p8_draw_cost_spr(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int n, int x, int y, int w, int h);
p8_draw_cost_t p8_draw_cost_print(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, const char *str, int x, int y);
/* One outlined sprite per non-space glyph; the box reaches one pixel further than print's on each side. */
p8_draw_cost_t p8_draw_cost_print_outlined(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, const char *str, int x, int y);

/* A build's own packets, all in the build buffer: the clear and the mask bit on, each window's draw
 * area and offset, then the frame's area and offset, the mask bit off and the texture cache flush. */
p8_draw_cost_t p8_draw_cost_image_begin(void);
p8_draw_cost_t p8_draw_cost_image_window(void);
p8_draw_cost_t p8_draw_cost_image_end(void);

/* The copy of a w x h image at world (x, y); it leaves no texture window. */
p8_draw_cost_t p8_draw_cost_image_draw(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int x, int y, int w, int h);

#endif
