#include "p8/draw_cost.h"

#include "p8/vram.h"

static const p8_draw_cost_t FREE = { 0, 0 };

static p8_draw_cost_t cost_of(unsigned long bytes_each, int primitives) {
    p8_draw_cost_t cost = { bytes_each * (unsigned long)primitives, primitives };
    return cost;
}

static int on_screen(const p8_graphics_t *gfx, int xmin, int ymin, int xmax, int ymax) {
    xmin -= gfx->camera_x;
    xmax -= gfx->camera_x;
    ymin -= gfx->camera_y;
    ymax -= gfx->camera_y;
    return xmax >= 0 && xmin < P8_SCREEN_WIDTH && ymax >= 0 && ymin < P8_SCREEN_HEIGHT;
}

static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a < b ? b : a;
}

static int box_on_screen(const p8_graphics_t *gfx, int x1, int y1, int x2, int y2) {
    return on_screen(gfx, min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2));
}

static p8_draw_cost_t cost_add(p8_draw_cost_t a, p8_draw_cost_t b) {
    a.bytes += b.bytes;
    a.primitives += b.primitives;
    return a;
}

static p8_draw_cost_t window_pair(void) {
    p8_draw_cost_t pair = { P8_GPU_DR_TPAGE_BYTES + P8_GPU_DR_TWIN_BYTES, 2 };
    return pair;
}

static p8_draw_cost_t window_switch(p8_draw_cost_window_t *window, int patterned, unsigned long pattern_key) {
    if (window->patterned == patterned && window->pattern_key == pattern_key) {
        return FREE;
    }
    window->patterned = patterned;
    window->pattern_key = pattern_key;
    return window_pair();
}

static p8_draw_cost_t unpatterned_window(p8_draw_cost_window_t *window) {
    return window_switch(window, 0, 0);
}

p8_draw_cost_t p8_draw_cost_frame_begin(p8_draw_cost_window_t *window) {
    window->patterned = 0;
    window->pattern_key = 0;
    p8_draw_cost_t overhead = cost_add(cost_of(P8_GPU_DR_ENV_BYTES, 2), window_pair());
    return cost_add(overhead, cost_of(P8_GPU_POLY_FT4_BYTES, 1));
}

unsigned long p8_draw_cost_pattern_key(const p8_graphics_t *gfx, int col) {
    return (gfx->fillp_pattern & 0xFFFFul) | ((unsigned long)(col & 0xFF) << 16) | ((unsigned long)(gfx->fillp_transparent != 0) << 24);
}

static p8_draw_cost_t fill_spans(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int col, int spans) {
    if ((gfx->fillp_pattern & 0xFFFFu) == 0) {
        return cost_of(P8_GPU_TILE_BYTES, spans);
    }
    p8_draw_cost_t pair = window_switch(window, 1, p8_draw_cost_pattern_key(gfx, col));
    return cost_add(pair, cost_of(P8_GPU_SPRT_BYTES, spans));
}

p8_draw_cost_t p8_draw_cost_cls(void) {
    return cost_of(P8_GPU_FILL_BYTES, 1);
}

p8_draw_cost_t p8_draw_cost_pset(const p8_graphics_t *gfx, int x, int y) {
    return on_screen(gfx, x, y, x, y) ? cost_of(P8_GPU_TILE_1_BYTES, 1) : FREE;
}

p8_draw_cost_t p8_draw_cost_line(const p8_graphics_t *gfx, int x1, int y1, int x2, int y2) {
    return box_on_screen(gfx, x1, y1, x2, y2) ? cost_of(P8_GPU_LINE_F2_BYTES, 1) : FREE;
}

p8_draw_cost_t p8_draw_cost_rect(const p8_graphics_t *gfx, int x1, int y1, int x2, int y2) {
    return box_on_screen(gfx, x1, y1, x2, y2) ? cost_of(P8_GPU_TILE_BYTES, 4) : FREE;
}

p8_draw_cost_t p8_draw_cost_rectfill(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int x1, int y1, int x2, int y2,
                                     int col) {
    return box_on_screen(gfx, x1, y1, x2, y2) ? fill_spans(window, gfx, col, 1) : FREE;
}

/* Must count the iterations of gfx_software.c's midpoint loop, or circles go undercounted. */
static int circle_iterations(int r) {
    int x = r;
    int y = 0;
    int err = 0;
    int iterations = 0;
    while (x >= y) {
        iterations++;
        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        }
        if (err > 0) {
            x--;
            err -= 2 * x + 1;
        }
    }
    return iterations;
}

static int circle_on_screen(const p8_graphics_t *gfx, int cx, int cy, int r) {
    return r >= 0 && on_screen(gfx, cx - r, cy - r, cx + r, cy + r);
}

p8_draw_cost_t p8_draw_cost_circ(const p8_graphics_t *gfx, int cx, int cy, int r) {
    return circle_on_screen(gfx, cx, cy, r) ? cost_of(P8_GPU_TILE_1_BYTES, 8 * circle_iterations(r)) : FREE;
}

p8_draw_cost_t p8_draw_cost_circfill(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int cx, int cy, int r, int col) {
    return circle_on_screen(gfx, cx, cy, r) ? fill_spans(window, gfx, col, 4 * circle_iterations(r)) : FREE;
}

p8_draw_cost_t p8_draw_cost_spr(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int n, int x, int y, int w, int h) {
    (void)n;
    if (w <= 0 || h <= 0 || !on_screen(gfx, x, y, x + w * P8_SPRITE_SIZE - 1, y + h * P8_SPRITE_SIZE - 1)) {
        return FREE;
    }
    return cost_add(unpatterned_window(window), cost_of(P8_GPU_SPRT_BYTES, 1));
}

/* Only the space glyph has no lit row. margin widens the box on every side. */
static p8_draw_cost_t text_cost(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, const char *str, int x, int y, int margin) {
    int length = 0;
    int glyphs = 0;
    for (const char *p = str; *p != '\0'; p++) {
        length++;
        glyphs += *p != ' ';
    }
    int right = x + (length - 1) * P8_FONT_CELL_WIDTH + P8_FONT_GLYPH_WIDTH - 1;
    if (glyphs == 0 || !on_screen(gfx, x - margin, y - margin, right + margin, y + P8_FONT_GLYPH_HEIGHT - 1 + margin)) {
        return FREE;
    }
    return cost_add(unpatterned_window(window), cost_of(P8_GPU_SPRT_BYTES, glyphs));
}

p8_draw_cost_t p8_draw_cost_print(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, const char *str, int x, int y) {
    return text_cost(window, gfx, str, x, y, 0);
}

p8_draw_cost_t p8_draw_cost_print_outlined(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, const char *str, int x, int y) {
    return text_cost(window, gfx, str, x, y, 1);
}

p8_draw_cost_t p8_draw_cost_image_begin(void) {
    p8_draw_cost_t cost = { P8_GPU_FILL_BYTES + P8_GPU_DR_STP_BYTES, 2 };
    return cost;
}

p8_draw_cost_t p8_draw_cost_image_window(void) {
    p8_draw_cost_t cost = { P8_GPU_DR_AREA_BYTES + P8_GPU_DR_OFFSET_BYTES, 2 };
    return cost;
}

p8_draw_cost_t p8_draw_cost_image_end(void) {
    p8_draw_cost_t cost = { P8_GPU_DR_AREA_BYTES + P8_GPU_DR_OFFSET_BYTES + P8_GPU_DR_STP_BYTES + P8_GPU_COMMAND_BYTES, 4 };
    return cost;
}

p8_draw_cost_t p8_draw_cost_image_draw(p8_draw_cost_window_t *window, const p8_graphics_t *gfx, int x, int y, int w, int h) {
    if (w <= 0 || h <= 0 || !on_screen(gfx, x, y, x + w - 1, y + h - 1)) {
        return FREE;
    }
    window->patterned = 0;
    window->pattern_key = 0;
    p8_draw_cost_t cost = { P8_GPU_DR_TWIN_BYTES + 2 * P8_GPU_DR_TPAGE_BYTES + P8_GPU_SPRT_BYTES, 4 };
    return cost;
}
