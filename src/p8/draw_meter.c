#include "p8/draw_meter.h"

#include "p8/draw_cost.h"
#include "p8/gpu_sizes.h"
#include "p8/image.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static const p8_draw_backend_t *wrapped;
static p8_draw_cost_window_t window;
static p8_draw_meter_counts_t frame;
static p8_draw_meter_counts_t last_frame;
static p8_draw_meter_counts_t peak;
static int building;
static p8_image_size_t built[P8_IMAGE_SLOT_COUNT];

enum { CLUT_TABLE = 2 * GPU_CLUT_SLOTS, PATTERN_TABLE = 2 * GPU_PATTERN_SLOTS };

static unsigned char cluts[CLUT_TABLE][16];
static unsigned long pattern_keys[PATTERN_TABLE];

static void count_clut(const p8_vram_t *mem) {
    for (int i = 0; i < frame.cluts; i++) {
        if (memcmp(cluts[i], mem->draw_palette, sizeof cluts[i]) == 0) {
            return;
        }
    }
    if (frame.cluts < CLUT_TABLE) {
        memcpy(cluts[frame.cluts++], mem->draw_palette, sizeof cluts[0]);
    }
}

static void count_pattern(const p8_graphics_t *gfx, int col) {
    if ((gfx->fillp_pattern & 0xFFFFu) == 0) {
        return;
    }
    unsigned long key = p8_draw_cost_pattern_key(gfx, col);
    for (int i = 0; i < frame.patterns; i++) {
        if (pattern_keys[i] == key) {
            return;
        }
    }
    if (frame.patterns < PATTERN_TABLE) {
        pattern_keys[frame.patterns++] = key;
    }
}

static void add(p8_draw_cost_t cost) {
    if (building) {
        frame.build_bytes += cost.bytes;
        return;
    }
    frame.bytes += cost.bytes;
    frame.primitives += cost.primitives;
}

static void frame_begin(void) {
    memset(&frame, 0, sizeof frame);
    add(p8_draw_cost_frame_begin(&window));
}

static void meter_cls(p8_vram_t *mem) {
    add(p8_draw_cost_cls());
    wrapped->cls(mem);
}

static void meter_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    add(p8_draw_cost_pset(gfx, x, y));
    wrapped->pset(gfx, mem, x, y, col);
}

static void meter_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    add(p8_draw_cost_line(gfx, x1, y1, x2, y2));
    wrapped->line(gfx, mem, x1, y1, x2, y2, col);
}

static void meter_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    add(p8_draw_cost_rect(gfx, x1, y1, x2, y2));
    wrapped->rect(gfx, mem, x1, y1, x2, y2, col);
}

static void meter_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    add(p8_draw_cost_rectfill(&window, gfx, x1, y1, x2, y2, col));
    count_pattern(gfx, col);
    wrapped->rectfill(gfx, mem, x1, y1, x2, y2, col);
}

static void meter_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    add(p8_draw_cost_circ(gfx, cx, cy, r));
    wrapped->circ(gfx, mem, cx, cy, r, col);
}

static void meter_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    add(p8_draw_cost_circfill(&window, gfx, cx, cy, r, col));
    count_pattern(gfx, col);
    wrapped->circfill(gfx, mem, cx, cy, r, col);
}

static void meter_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    add(p8_draw_cost_spr(&window, gfx, n, x, y, w, h));
    count_clut(mem);
    wrapped->spr(gfx, mem, n, x, y, w, h);
}

static void meter_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    add(p8_draw_cost_print(&window, gfx, str, x, y));
    wrapped->print(gfx, mem, str, x, y, col);
}

static void meter_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    add(p8_draw_cost_print_outlined(&window, gfx, str, x, y));
    wrapped->print_outlined(gfx, mem, str, x, y, col);
}

static void meter_image_begin(int slot, int w, int h) {
    building = 1;
    built[slot].width = w;
    built[slot].height = h;
    add(p8_draw_cost_image_begin());
    wrapped->image_begin(slot, w, h);
}

static void meter_image_window(int slot, int x, int y) {
    add(p8_draw_cost_image_window());
    wrapped->image_window(slot, x, y);
}

static void meter_image_end(int slot) {
    add(p8_draw_cost_image_end());
    building = 0;
    wrapped->image_end(slot);
}

static void meter_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    add(p8_draw_cost_image_draw(&window, gfx, x, y, built[slot].width, built[slot].height));
    wrapped->image_draw(gfx, mem, slot, x, y);
}

static void meter_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    for (int i = 0; i < count; i++) {
        add(p8_draw_cost_line(gfx, segments[i].x1, segments[i].y1, segments[i].x2, segments[i].y2));
    }
    wrapped->lines(gfx, mem, segments, count, col);
}

static void meter_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    for (int i = 0; i < count; i++) {
        add(p8_draw_cost_pset(gfx, points[i].x, points[i].y));
    }
    wrapped->psets(gfx, mem, points, count);
}

static const p8_draw_backend_t METER = {
    meter_cls, meter_pset, meter_line, meter_rect, meter_rectfill,
    meter_circ, meter_circfill, meter_spr, meter_print, meter_print_outlined,
    meter_image_begin, meter_image_window, meter_image_end, meter_image_draw,
    meter_lines, meter_psets,
};

const p8_draw_backend_t *p8_draw_meter_install(void) {
    const p8_draw_backend_t *active = p8_draw_backend_get();
    assert(active != &METER && "the meter would wrap itself");
    wrapped = active;
    memset(&last_frame, 0, sizeof last_frame);
    memset(&peak, 0, sizeof peak);
    building = 0;
    frame_begin();
    p8_draw_backend_set(&METER);
    return wrapped;
}

static int larger(int a, int b) {
    return a < b ? b : a;
}

void p8_draw_meter_frame_end(void) {
    last_frame = frame;
    peak.bytes = frame.bytes > peak.bytes ? frame.bytes : peak.bytes;
    peak.build_bytes = frame.build_bytes > peak.build_bytes ? frame.build_bytes : peak.build_bytes;
    peak.primitives = larger(peak.primitives, frame.primitives);
    peak.cluts = larger(peak.cluts, frame.cluts);
    peak.patterns = larger(peak.patterns, frame.patterns);
    frame_begin();
}

p8_draw_meter_counts_t p8_draw_meter_last_frame(void) {
    return last_frame;
}

p8_draw_meter_counts_t p8_draw_meter_peak(void) {
    return peak;
}
