#include "p8/draw_backend.h"
#include "p8/draw_cost.h"
#include "p8/draw_meter.h"
#include "p8/gfx.h"
#include "p8/gpu_sizes.h"
#include "p8/image.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { KINDS = 16, KIND_PRINT_OUTLINED = 13, KIND_LINES = 14, KIND_PSETS = 15 };

static int forwarded[KINDS];
static const p8_graphics_t *forwarded_gfx;
static p8_vram_t *forwarded_mem;

static void seen(int kind, const p8_graphics_t *gfx, p8_vram_t *mem) {
    forwarded[kind]++;
    forwarded_gfx = gfx;
    forwarded_mem = mem;
}

static void counting_cls(p8_vram_t *mem) {
    seen(0, NULL, mem);
}

static void counting_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    (void)x, (void)y, (void)col;
    seen(1, gfx, mem);
}

static void counting_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    (void)x1, (void)y1, (void)x2, (void)y2, (void)col;
    seen(2, gfx, mem);
}

static void counting_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    (void)x1, (void)y1, (void)x2, (void)y2, (void)col;
    seen(3, gfx, mem);
}

static void counting_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    (void)x1, (void)y1, (void)x2, (void)y2, (void)col;
    seen(4, gfx, mem);
}

static void counting_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    (void)cx, (void)cy, (void)r, (void)col;
    seen(5, gfx, mem);
}

static void counting_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    (void)cx, (void)cy, (void)r, (void)col;
    seen(6, gfx, mem);
}

static void counting_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    (void)n, (void)x, (void)y, (void)w, (void)h;
    seen(7, gfx, mem);
}

static void counting_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    (void)str, (void)x, (void)y, (void)col;
    seen(8, gfx, mem);
}

static void counting_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    (void)str, (void)x, (void)y, (void)col;
    seen(KIND_PRINT_OUTLINED, gfx, mem);
}

static void counting_image_begin(int slot, int w, int h) {
    (void)slot, (void)w, (void)h;
    forwarded[9]++;
}

static void counting_image_window(int slot, int x, int y) {
    (void)slot, (void)x, (void)y;
    forwarded[10]++;
}

static void counting_image_end(int slot) {
    (void)slot;
    forwarded[11]++;
}

static void counting_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    (void)slot, (void)x, (void)y;
    seen(12, gfx, mem);
}

static void counting_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    (void)segments, (void)count, (void)col;
    seen(KIND_LINES, gfx, mem);
}

static void counting_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    (void)points, (void)count;
    seen(KIND_PSETS, gfx, mem);
}

static const p8_draw_backend_t COUNTING = {
    counting_cls, counting_pset, counting_line, counting_rect, counting_rectfill,
    counting_circ, counting_circfill, counting_spr, counting_print, counting_print_outlined,
    counting_image_begin, counting_image_window, counting_image_end, counting_image_draw,
    counting_lines, counting_psets,
};

static p8_vram_t mem;
static p8_graphics_t gfx;
static const p8_draw_backend_t *before_meter;

static void meter_over_counting(void) {
    memset(forwarded, 0, sizeof forwarded);
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_draw_backend_set(&COUNTING);
    before_meter = p8_draw_meter_install();
    assert(before_meter == &COUNTING);
    assert(p8_draw_backend_get() != &COUNTING);
}

static void meter_removed(void) {
    p8_draw_backend_set(NULL);
}

static void draw_nothing(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)gfx, (void)mem, (void)context;
}

static void draw_one_of_each(void) {
    p8_cls(&mem);
    p8_pset(&gfx, &mem, 1, 2, 3);
    p8_line(&gfx, &mem, 1, 2, 3, 4, 5);
    p8_rect(&gfx, &mem, 1, 2, 3, 4, 5);
    p8_rectfill(&gfx, &mem, 1, 2, 3, 4, 5);
    p8_circ(&gfx, &mem, 10, 10, 3, 5);
    p8_circfill(&gfx, &mem, 10, 10, 3, 5);
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    p8_print(&gfx, &mem, "ab", 0, 0, 7);
    p8_print_outlined(&gfx, &mem, "ab", 0, 0, 7);
    p8_image_build(&gfx, &mem, 0, 16, 16, draw_nothing, NULL);
    p8_image_draw(&gfx, &mem, 0, 0, 0);
    static const p8_segment_t segment = { 1, 2, 3, 4 };
    p8_lines(&gfx, &mem, &segment, 1, 5);
    static const p8_point_t point = { 1, 2, 3 };
    p8_psets(&gfx, &mem, &point, 1);
}

static void test_every_call_reaches_the_wrapped_backend(void) {
    meter_over_counting();
    draw_one_of_each();
    for (int kind = 0; kind < KINDS; kind++) {
        assert(forwarded[kind] == 1);
    }
    assert(forwarded_gfx == &gfx && forwarded_mem == &mem);
    meter_removed();
}


static p8_draw_cost_t overhead(void) {
    p8_draw_cost_window_t window;
    return p8_draw_cost_frame_begin(&window);
}

static void test_frame_end_folds_the_frame_into_the_peaks_and_resets_it(void) {
    meter_over_counting();
    assert(p8_draw_meter_last_frame().bytes == 0 && p8_draw_meter_peak().bytes == 0);
    unsigned long base = overhead().bytes;
    int base_primitives = overhead().primitives;

    p8_cls(&mem);
    p8_pset(&gfx, &mem, 1, 1, 7);
    p8_pset(&gfx, &mem, 500, 1, 7);
    p8_draw_meter_frame_end();
    p8_draw_meter_counts_t first = p8_draw_meter_last_frame();
    assert(first.bytes == base + P8_GPU_FILL_BYTES + P8_GPU_TILE_1_BYTES);
    assert(first.primitives == base_primitives + 2);
    assert(p8_draw_meter_peak().bytes == first.bytes);

    p8_cls(&mem);
    p8_draw_meter_frame_end();
    assert(p8_draw_meter_last_frame().bytes == base + P8_GPU_FILL_BYTES && "the frame was reset");
    assert(p8_draw_meter_peak().bytes == first.bytes && p8_draw_meter_peak().primitives == first.primitives);

    p8_rect(&gfx, &mem, 0, 0, 5, 5, 7);
    p8_rect(&gfx, &mem, 0, 0, 5, 5, 7);
    p8_draw_meter_frame_end();
    assert(p8_draw_meter_peak().bytes == base + 8 * P8_GPU_TILE_BYTES);
    meter_removed();
}

static void test_an_outlined_print_costs_one_sprt_per_glyph_over_its_outlined_box(void) {
    meter_over_counting();
    unsigned long base = overhead().bytes;
    int base_primitives = overhead().primitives;

    p8_print_outlined(&gfx, &mem, "a b", 0, 0, 7);
    p8_print_outlined(&gfx, &mem, "ab", -7, 0, 7);
    p8_print_outlined(&gfx, &mem, "ab", -8, 0, 7);
    p8_draw_meter_frame_end();

    assert(p8_draw_meter_last_frame().bytes == base + 4 * P8_GPU_SPRT_BYTES);
    assert(p8_draw_meter_last_frame().primitives == base_primitives + 4);
    meter_removed();
}

static void test_distinct_palettes_and_patterns_are_counted_once_per_frame(void) {
    meter_over_counting();
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    p8_spr(&gfx, &mem, 2, 50, 0, 1, 1);
    p8_map(&gfx, &mem, 0, 0, 0, 0, 1, 1);
    p8_palt(&mem, 0, 0);
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    mem.draw_palette[5] = 9;
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    p8_circfill(&gfx, &mem, 0, 0, 5, 7);
    p8_fillp(&gfx, 0xA5A5);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 0x15);
    p8_circfill(&gfx, &mem, 0, 0, 5, 0x15);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 0x16);
    p8_fillp_transparent(&gfx, 0xA5A5);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 0x16);
    p8_draw_meter_frame_end();
    assert(p8_draw_meter_last_frame().cluts == 3);
    assert(p8_draw_meter_last_frame().patterns == 3);

    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    p8_draw_meter_frame_end();
    assert(p8_draw_meter_last_frame().cluts == 1 && p8_draw_meter_last_frame().patterns == 0);
    assert(p8_draw_meter_peak().cluts == 3 && p8_draw_meter_peak().patterns == 3);
    meter_removed();
}

static void test_a_count_past_the_table_is_clamped_above_the_limit(void) {
    meter_over_counting();
    int palettes = 2 * GPU_CLUT_SLOTS + 3;
    int patterns = 2 * GPU_PATTERN_SLOTS + 3;
    assert(palettes <= 256 && patterns <= 0xFFFF);
    for (int i = 0; i < palettes; i++) {
        mem.draw_palette[0] = (unsigned char)i;
        p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);
    }
    for (int i = 1; i <= patterns; i++) {
        p8_fillp(&gfx, (unsigned int)i);
        p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    }
    p8_draw_meter_frame_end();
    assert(p8_draw_meter_peak().cluts == 2 * GPU_CLUT_SLOTS && p8_draw_meter_peak().cluts > GPU_CLUT_SLOTS);
    assert(p8_draw_meter_peak().patterns == 2 * GPU_PATTERN_SLOTS && p8_draw_meter_peak().patterns > GPU_PATTERN_SLOTS);
    meter_removed();
}

static void two_sprites_and_a_palette(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_spr(build_gfx, build_mem, 1, 0, 0, 1, 1);
    p8_spr(build_gfx, build_mem, 1, 200, 130, 1, 1);
    p8_spr(build_gfx, build_mem, 1, 500, 0, 1, 1);
}

static void test_build_bytes_go_to_the_build_count_and_its_palettes_to_the_frame(void) {
    meter_over_counting();
    unsigned long base = overhead().bytes;
    mem.draw_palette[5] = 9;

    p8_image_build(&gfx, &mem, 0, 256, 256, two_sprites_and_a_palette, NULL);
    p8_draw_meter_frame_end();

    p8_draw_meter_counts_t built = p8_draw_meter_last_frame();
    unsigned long windows = 6 * p8_draw_cost_image_window().bytes;
    assert(built.bytes == base && "nothing of the build in the frame");
    assert(built.build_bytes ==
           p8_draw_cost_image_begin().bytes + windows + 2 * P8_GPU_SPRT_BYTES + p8_draw_cost_image_end().bytes);
    assert(built.cluts == 1 && "the build's palette is a frame CLUT, as on the GPU");

    p8_draw_meter_frame_end();
    assert(p8_draw_meter_last_frame().build_bytes == 0 && "the build count restarts each frame");
    assert(p8_draw_meter_peak().build_bytes == built.build_bytes);
    meter_removed();
}

static void test_a_copy_costs_frame_bytes_on_screen_and_nothing_off_screen(void) {
    meter_over_counting();
    unsigned long base = overhead().bytes;
    p8_image_build(&gfx, &mem, 0, 64, 32, draw_nothing, NULL);
    p8_draw_meter_frame_end();

    p8_image_draw(&gfx, &mem, 0, 10, 10);
    p8_image_draw(&gfx, &mem, 0, -64, 0);
    p8_draw_meter_frame_end();
    p8_draw_cost_window_t window;
    (void)p8_draw_cost_frame_begin(&window);
    assert(p8_draw_meter_last_frame().bytes == base + p8_draw_cost_image_draw(&window, &gfx, 10, 10, 64, 32).bytes);
    assert(p8_draw_meter_last_frame().build_bytes == 0);
    meter_removed();
}

static unsigned long frame_bytes_of(void (*draw)(void), int *primitives) {
    meter_over_counting();
    p8_camera(&gfx, 20, -9);
    draw();
    p8_draw_meter_frame_end();
    p8_draw_meter_counts_t counts = p8_draw_meter_last_frame();
    meter_removed();
    *primitives = counts.primitives;
    return counts.bytes;
}

static const p8_segment_t METERED_SEGMENTS[] = {
    { 0, 0, 40, 40 }, { -200, 5, -100, 6 }, { 140, 140, 150, 150 }, { 10, -50, 30, 300 }, { 7, 7, 7, 7 },
};

static void draw_segments_batched(void) {
    p8_lines(&gfx, &mem, METERED_SEGMENTS, 5, 7);
}

static void draw_segments_one_by_one(void) {
    for (int i = 0; i < 5; i++) {
        const p8_segment_t *s = &METERED_SEGMENTS[i];
        p8_line(&gfx, &mem, s->x1, s->y1, s->x2, s->y2, 7);
    }
}

static void test_a_batch_of_lines_costs_the_sum_of_its_lines(void) {
    int batched_primitives;
    int single_primitives;
    unsigned long batched = frame_bytes_of(draw_segments_batched, &batched_primitives);
    unsigned long single = frame_bytes_of(draw_segments_one_by_one, &single_primitives);

    assert(batched == single && batched_primitives == single_primitives);
    assert(batched > overhead().bytes && "some segment is on screen");
}

static const p8_point_t METERED_POINTS[] = { { 20, -9, 1 }, { 19, -9, 2 }, { 179, 110, 3 }, { 180, 110, 4 }, { 60, 60, 5 } };

static void draw_points_batched(void) {
    p8_psets(&gfx, &mem, METERED_POINTS, 5);
}

static void draw_points_one_by_one(void) {
    for (int i = 0; i < 5; i++) p8_pset(&gfx, &mem, METERED_POINTS[i].x, METERED_POINTS[i].y, METERED_POINTS[i].col);
}

static void test_a_batch_of_points_costs_the_sum_of_its_psets(void) {
    int batched_primitives;
    int single_primitives;
    unsigned long batched = frame_bytes_of(draw_points_batched, &batched_primitives);
    unsigned long single = frame_bytes_of(draw_points_one_by_one, &single_primitives);

    assert(batched == single && batched_primitives == single_primitives);
    assert(batched == overhead().bytes + 3 * P8_GPU_TILE_1_BYTES && "three points on screen after the camera");
}

int main(void) {
    test_every_call_reaches_the_wrapped_backend();
    test_frame_end_folds_the_frame_into_the_peaks_and_resets_it();
    test_an_outlined_print_costs_one_sprt_per_glyph_over_its_outlined_box();
    test_distinct_palettes_and_patterns_are_counted_once_per_frame();
    test_a_count_past_the_table_is_clamped_above_the_limit();
    test_build_bytes_go_to_the_build_count_and_its_palettes_to_the_frame();
    test_a_copy_costs_frame_bytes_on_screen_and_nothing_off_screen();
    test_a_batch_of_lines_costs_the_sum_of_its_lines();
    test_a_batch_of_points_costs_the_sum_of_its_psets();
    printf("test_draw_meter: ok\n");
    return 0;
}
