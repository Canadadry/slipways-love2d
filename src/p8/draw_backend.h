#ifndef P8_DRAW_BACKEND_H
#define P8_DRAW_BACKEND_H

#include "p8/gfx.h"

/* One entry per draw call, with the arguments the API received: world coordinates (a backend
 * applies gfx's camera itself), the colour byte high nibble included. A backend reads gfx and
 * mem's draw state and never writes it. */
typedef struct {
    void (*cls)(p8_vram_t *mem);
    void (*pset)(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col);
    void (*line)(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col);
    void (*rect)(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col);
    void (*rectfill)(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col);
    void (*circ)(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col);
    void (*circfill)(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col);
    void (*spr)(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h);
    void (*print)(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col);
    void (*print_outlined)(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col);
    /* Clears the slot's w x h, then routes the draw calls to it until image_end. */
    void (*image_begin)(int slot, int w, int h);
    /* The next calls draw at image origin (x, y): clipped to the screen size, then to the image. */
    void (*image_window)(int slot, int x, int y);
    void (*image_end)(int slot);
    void (*image_draw)(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y);
    /* Each segment exactly as line draws it, in list order, colour looked up as line does. A count
     * of 0 draws nothing; segments is only read during the call. */
    void (*lines)(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col);
    /* Each point exactly as pset draws it, in list order, in its own colour. A count of 0 draws
     * nothing; points is only read during the call. */
    void (*psets)(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count);
} p8_draw_backend_t;

/* Process-wide, because p8_cls takes no graphics state. The table is static, never freed, and
 * must outlive its installation. NULL installs the software backend. Returns the backend active
 * before the call, never NULL. */
const p8_draw_backend_t *p8_draw_backend_set(const p8_draw_backend_t *backend);

/* Never NULL: the software backend when none was installed. */
const p8_draw_backend_t *p8_draw_backend_get(void);

/* Rasterises into mem->screen. Static table, never freed. */
const p8_draw_backend_t *p8_gfx_software_backend(void);

#endif
