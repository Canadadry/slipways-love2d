#include "devtools/draw_filter.h"

#include "p8/draw_cost.h"
#include "p8/image.h"

#include <assert.h>
#include <string.h>

static const p8_draw_backend_t *wrapped;
static unsigned mask = DRAW_FILTER_ALL;
static int counts[DRAW_FILTER_FAMILY_COUNT];
static p8_draw_cost_window_t window;
static int building_slot = -1;
static unsigned image_families[P8_IMAGE_SLOT_COUNT];
static p8_image_size_t image_sizes[P8_IMAGE_SLOT_COUNT];

static int passes(draw_filter_family_t family) {
    return (mask & DRAW_FILTER_BIT(family)) != 0;
}

static int patterned(const p8_graphics_t *gfx) {
    return (gfx->fillp_pattern & 0xFFFFu) != 0;
}

/* Returns whether the call reaches the wrapped backend: always during a build, whose image must
 * hold its full content. */
static int note(draw_filter_family_t family, p8_draw_cost_t cost) {
    if (building_slot >= 0) {
        if (cost.bytes > 0) image_families[building_slot] |= DRAW_FILTER_BIT(family);
        return 1;
    }
    if (cost.bytes > 0) counts[family]++;
    return passes(family);
}

static int note_fill(draw_filter_family_t shape, const p8_graphics_t *gfx, p8_draw_cost_t cost) {
    int shape_passes = note(shape, cost);
    if (!patterned(gfx)) return shape_passes;
    int pattern_passes = note(DRAW_FILTER_PATTERN, cost);
    return shape_passes && pattern_passes;
}

static void filter_cls(p8_vram_t *mem) {
    counts[DRAW_FILTER_CLS]++;
    wrapped->cls(mem);
}

static void filter_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    if (note(DRAW_FILTER_PSET, p8_draw_cost_pset(gfx, x, y))) wrapped->pset(gfx, mem, x, y, col);
}

static void filter_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    if (note(DRAW_FILTER_LINE, p8_draw_cost_line(gfx, x1, y1, x2, y2))) wrapped->line(gfx, mem, x1, y1, x2, y2, col);
}

static void filter_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    if (note(DRAW_FILTER_RECT, p8_draw_cost_rect(gfx, x1, y1, x2, y2))) wrapped->rect(gfx, mem, x1, y1, x2, y2, col);
}

static void filter_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    p8_draw_cost_t cost = p8_draw_cost_rectfill(&window, gfx, x1, y1, x2, y2, col);
    if (note_fill(DRAW_FILTER_RECTFILL, gfx, cost)) wrapped->rectfill(gfx, mem, x1, y1, x2, y2, col);
}

static void filter_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    if (note(DRAW_FILTER_CIRC, p8_draw_cost_circ(gfx, cx, cy, r))) wrapped->circ(gfx, mem, cx, cy, r, col);
}

static void filter_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    p8_draw_cost_t cost = p8_draw_cost_circfill(&window, gfx, cx, cy, r, col);
    if (note_fill(DRAW_FILTER_CIRCFILL, gfx, cost)) wrapped->circfill(gfx, mem, cx, cy, r, col);
}

static void filter_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    if (note(DRAW_FILTER_SPR, p8_draw_cost_spr(&window, gfx, n, x, y, w, h))) wrapped->spr(gfx, mem, n, x, y, w, h);
}

static void filter_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    if (note(DRAW_FILTER_PRINT, p8_draw_cost_print(&window, gfx, str, x, y))) wrapped->print(gfx, mem, str, x, y, col);
}

static void filter_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    if (note(DRAW_FILTER_PRINT, p8_draw_cost_print_outlined(&window, gfx, str, x, y))) {
        wrapped->print_outlined(gfx, mem, str, x, y, col);
    }
}

static void filter_image_begin(int slot, int w, int h) {
    building_slot = slot;
    image_families[slot] = 0;
    image_sizes[slot].width = w;
    image_sizes[slot].height = h;
    wrapped->image_begin(slot, w, h);
}

static void filter_image_window(int slot, int x, int y) {
    wrapped->image_window(slot, x, y);
}

static void filter_image_end(int slot) {
    building_slot = -1;
    wrapped->image_end(slot);
}

static void filter_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    p8_image_size_t size = image_sizes[slot];
    unsigned families = image_families[slot];
    if (p8_draw_cost_image_draw(&window, gfx, x, y, size.width, size.height).bytes > 0) {
        for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
            if (families & DRAW_FILTER_BIT(family)) counts[family]++;
        }
    }
    if ((mask & families) == families) wrapped->image_draw(gfx, mem, slot, x, y);
}

static void filter_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    int passed = 1;
    for (int i = 0; i < count; i++) {
        passed = note(DRAW_FILTER_LINE, p8_draw_cost_line(gfx, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2));
    }
    if (passed) wrapped->lines(gfx, mem, segments, count, col);
}

static void filter_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    int passed = 1;
    for (int i = 0; i < count; i++) {
        passed = note(DRAW_FILTER_PSET, p8_draw_cost_pset(gfx, points[i].x, points[i].y));
    }
    if (passed) wrapped->psets(gfx, mem, points, count);
}

static const p8_draw_backend_t FILTER = {
    filter_cls, filter_pset, filter_line, filter_rect, filter_rectfill,
    filter_circ, filter_circfill, filter_spr, filter_print, filter_print_outlined,
    filter_image_begin, filter_image_window, filter_image_end, filter_image_draw,
    filter_lines, filter_psets,
};

const p8_draw_backend_t *draw_filter_install(void) {
    const p8_draw_backend_t *active = p8_draw_backend_get();
    assert(active != &FILTER && "the filter would wrap itself");
    wrapped = active;
    mask = DRAW_FILTER_ALL;
    building_slot = -1;
    draw_filter_frame_end();
    p8_draw_backend_set(&FILTER);
    return wrapped;
}

void draw_filter_set_mask(unsigned new_mask) {
    assert((new_mask & DRAW_FILTER_BIT(DRAW_FILTER_CLS)) && "every mask holds cls");
    assert((new_mask & ~DRAW_FILTER_ALL) == 0);
    mask = new_mask;
}

void draw_filter_frame_end(void) {
    memset(counts, 0, sizeof counts);
    p8_draw_cost_frame_begin(&window);
}

int draw_filter_count(draw_filter_family_t family) {
    assert(family >= 0 && family < DRAW_FILTER_FAMILY_COUNT);
    return counts[family];
}
