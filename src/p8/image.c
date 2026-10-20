#include "p8/image.h"

#include "p8/draw_backend.h"
#include "p8/fatal.h"

#include <stdio.h>

#define SLOT_SIZE(width, height) {(width), (height)},
static const p8_image_size_t SLOT_SIZES[P8_IMAGE_SLOT_COUNT] = {P8_IMAGE_SLOTS(SLOT_SIZE)};

static int built[P8_IMAGE_SLOT_COUNT];
static int building_slot = -1;
static const p8_draw_backend_t *guarded;

static void slot_fatal(int slot, const char *what) {
    char message[96];
    snprintf(message, sizeof message, "image slot %d: %s", slot, what);
    p8_fatal(message);
}

static void check_slot(int slot) {
    if (slot < 0 || slot >= P8_IMAGE_SLOT_COUNT) slot_fatal(slot, "no such slot");
}

p8_image_size_t p8_image_size(int slot) {
    check_slot(slot);
    return SLOT_SIZES[slot];
}

static void guard_cls(p8_vram_t *mem) {
    (void)mem;
    slot_fatal(building_slot, "cls during a build");
}

static void guard_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    guarded->pset(gfx, mem, x, y, col);
}

static void guard_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    guarded->line(gfx, mem, x1, y1, x2, y2, col);
}

static void guard_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    guarded->rect(gfx, mem, x1, y1, x2, y2, col);
}

static void guard_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    guarded->rectfill(gfx, mem, x1, y1, x2, y2, col);
}

static void guard_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    guarded->circ(gfx, mem, cx, cy, r, col);
}

static void guard_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    guarded->circfill(gfx, mem, cx, cy, r, col);
}

static void guard_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    guarded->spr(gfx, mem, n, x, y, w, h);
}

static void guard_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    guarded->print(gfx, mem, str, x, y, col);
}

static void guard_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    guarded->print_outlined(gfx, mem, str, x, y, col);
}

static void guard_image_begin(int slot, int w, int h) {
    (void)w, (void)h;
    slot_fatal(slot, "build inside a build");
}

static void guard_image_window(int slot, int x, int y) {
    (void)x, (void)y;
    slot_fatal(slot, "window outside its build");
}

static void guard_image_end(int slot) {
    slot_fatal(slot, "end outside its build");
}

static void guard_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    (void)gfx, (void)mem, (void)x, (void)y;
    slot_fatal(slot, "draw during a build");
}

static void guard_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    guarded->lines(gfx, mem, segments, count, col);
}

static void guard_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    guarded->psets(gfx, mem, points, count);
}

/* Stands in front of the backend during a build, which p8_cls cannot otherwise see. */
static const p8_draw_backend_t GUARD = {
    guard_cls, guard_pset, guard_line, guard_rect, guard_rectfill,
    guard_circ, guard_circfill, guard_spr, guard_print, guard_print_outlined,
    guard_image_begin, guard_image_window, guard_image_end, guard_image_draw,
    guard_lines, guard_psets,
};

void p8_image_build(p8_graphics_t *gfx, p8_vram_t *mem, int slot, int w, int h, p8_image_draw_t draw, void *context) {
    check_slot(slot);
    if (building_slot >= 0) slot_fatal(slot, "build inside a build");
    p8_image_size_t capacity = SLOT_SIZES[slot];
    if (w < 0 || h < 0 || w > capacity.width || h > capacity.height) {
        char what[64];
        snprintf(what, sizeof what, "%dx%d over its %dx%d", w, h, capacity.width, capacity.height);
        slot_fatal(slot, what);
    }
    int camera_x = gfx->camera_x;
    int camera_y = gfx->camera_y;
    guarded = p8_draw_backend_set(&GUARD);
    building_slot = slot;
    guarded->image_begin(slot, w, h);
    for (int y = 0; y < h; y += P8_IMAGE_WINDOW_HEIGHT) {
        for (int x = 0; x < w; x += P8_IMAGE_WINDOW_WIDTH) {
            guarded->image_window(slot, x, y);
            p8_camera(gfx, x, y);
            draw(gfx, mem, context);
        }
    }
    guarded->image_end(slot);
    building_slot = -1;
    p8_draw_backend_set(guarded);
    built[slot] = 1;
    p8_camera(gfx, camera_x, camera_y);
}

void p8_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    check_slot(slot);
    if (building_slot >= 0) slot_fatal(slot, "draw during a build");
    if (!built[slot]) slot_fatal(slot, "drawn before any build");
    p8_draw_backend_get()->image_draw(gfx, mem, slot, x, y);
}
