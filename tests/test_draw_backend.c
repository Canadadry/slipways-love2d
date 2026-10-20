#include "p8/draw_backend.h"
#include "p8/draw_cost.h"
#include "p8/gfx.h"
#include "p8/image.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum call_kind {
    CALL_CLS, CALL_PSET, CALL_LINE, CALL_RECT, CALL_RECTFILL, CALL_CIRC, CALL_CIRCFILL, CALL_SPR, CALL_PRINT, CALL_PRINT_OUTLINED,
    CALL_IMAGE_BEGIN, CALL_IMAGE_WINDOW, CALL_IMAGE_END, CALL_IMAGE_DRAW, CALL_LINES, CALL_PSETS
};

#define MAX_CALLS 1024

typedef struct {
    enum call_kind kind;
    const p8_graphics_t *gfx;
    p8_vram_t *mem;
    int args[6];
    const char *str;
    const p8_segment_t *segments;
    const p8_point_t *points;
    unsigned char draw_palette[16];
} recorded_call_t;

static recorded_call_t calls[MAX_CALLS];
static int call_count;

static recorded_call_t *record(enum call_kind kind, const p8_graphics_t *gfx, p8_vram_t *mem) {
    assert(call_count < MAX_CALLS && "recording backend: raise MAX_CALLS");
    recorded_call_t *call = &calls[call_count++];
    memset(call, 0, sizeof(*call));
    call->kind = kind;
    call->gfx = gfx;
    call->mem = mem;
    memcpy(call->draw_palette, mem->draw_palette, sizeof(call->draw_palette));
    return call;
}

static void record_cls(p8_vram_t *mem) {
    record(CALL_CLS, NULL, mem);
}

static void record_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    recorded_call_t *call = record(CALL_PSET, gfx, mem);
    call->args[0] = x;
    call->args[1] = y;
    call->args[2] = col;
}

static void record_box(enum call_kind kind, const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    recorded_call_t *call = record(kind, gfx, mem);
    call->args[0] = x1;
    call->args[1] = y1;
    call->args[2] = x2;
    call->args[3] = y2;
    call->args[4] = col;
}

static void record_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    record_box(CALL_LINE, gfx, mem, x1, y1, x2, y2, col);
}

static void record_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    record_box(CALL_RECT, gfx, mem, x1, y1, x2, y2, col);
}

static void record_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    record_box(CALL_RECTFILL, gfx, mem, x1, y1, x2, y2, col);
}

static void record_circle(enum call_kind kind, const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    recorded_call_t *call = record(kind, gfx, mem);
    call->args[0] = cx;
    call->args[1] = cy;
    call->args[2] = r;
    call->args[3] = col;
}

static void record_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    record_circle(CALL_CIRC, gfx, mem, cx, cy, r, col);
}

static void record_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    record_circle(CALL_CIRCFILL, gfx, mem, cx, cy, r, col);
}

static void record_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    recorded_call_t *call = record(CALL_SPR, gfx, mem);
    call->args[0] = n;
    call->args[1] = x;
    call->args[2] = y;
    call->args[3] = w;
    call->args[4] = h;
}

static void record_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    recorded_call_t *call = record(CALL_PRINT, gfx, mem);
    call->str = str;
    call->args[0] = x;
    call->args[1] = y;
    call->args[2] = col;
}

static void record_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    recorded_call_t *call = record(CALL_PRINT_OUTLINED, gfx, mem);
    call->str = str;
    call->args[0] = x;
    call->args[1] = y;
    call->args[2] = col;
}

static recorded_call_t *record_image(enum call_kind kind, int slot, int a, int b) {
    assert(call_count < MAX_CALLS && "recording backend: raise MAX_CALLS");
    recorded_call_t *call = &calls[call_count++];
    memset(call, 0, sizeof(*call));
    call->kind = kind;
    call->args[0] = slot;
    call->args[1] = a;
    call->args[2] = b;
    return call;
}

static void record_image_begin(int slot, int w, int h) {
    record_image(CALL_IMAGE_BEGIN, slot, w, h);
}

static void record_image_window(int slot, int x, int y) {
    record_image(CALL_IMAGE_WINDOW, slot, x, y);
}

static void record_image_end(int slot) {
    record_image(CALL_IMAGE_END, slot, 0, 0);
}

static void record_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    recorded_call_t *call = record_image(CALL_IMAGE_DRAW, slot, x, y);
    call->gfx = gfx;
    call->mem = mem;
}

static void record_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    recorded_call_t *call = record(CALL_LINES, gfx, mem);
    call->segments = segments;
    call->args[0] = count;
    call->args[1] = col;
}

static void record_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    recorded_call_t *call = record(CALL_PSETS, gfx, mem);
    call->points = points;
    call->args[0] = count;
}

static const p8_draw_backend_t RECORDING_BACKEND = {
    record_cls, record_pset, record_line, record_rect, record_rectfill,
    record_circ, record_circfill, record_spr, record_print, record_print_outlined,
    record_image_begin, record_image_window, record_image_end, record_image_draw,
    record_lines, record_psets,
};

static p8_vram_t mem;
static p8_graphics_t gfx;

static const p8_draw_backend_t *start_recording(void) {
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    call_count = 0;
    return p8_draw_backend_set(&RECORDING_BACKEND);
}

static void stop_recording(const p8_draw_backend_t *previous) {
    p8_draw_backend_set(previous);
}

static void assert_call(int index, enum call_kind kind, int a0, int a1, int a2, int a3, int a4) {
    const recorded_call_t *call = &calls[index];
    assert(call->kind == kind);
    assert(call->mem == &mem);
    assert(call->gfx == (kind == CALL_CLS ? NULL : &gfx));
    assert(call->args[0] == a0);
    assert(call->args[1] == a1);
    assert(call->args[2] == a2);
    assert(call->args[3] == a3);
    assert(call->args[4] == a4);
}

static void test_pset_reaches_its_entry_once_with_its_arguments(void) {
    const p8_draw_backend_t *previous = start_recording();

    p8_pset(&gfx, &mem, -3, 200, 0x1c);

    assert(call_count == 1);
    assert_call(0, CALL_PSET, -3, 200, 0x1c, 0, 0);
    stop_recording(previous);
}

static void test_each_draw_call_reaches_its_own_entry_once(void) {
    const p8_draw_backend_t *previous = start_recording();
    static const char text[] = "HI";

    p8_cls(&mem);
    p8_line(&gfx, &mem, 1, 2, 3, 4, 0x25);
    p8_rect(&gfx, &mem, 5, 6, 7, 8, 9);
    p8_rectfill(&gfx, &mem, 10, 11, -12, 13, 0xa5);
    p8_circ(&gfx, &mem, 14, 15, 16, 7);
    p8_circfill(&gfx, &mem, 17, 18, 19, 0x3c);
    p8_spr(&gfx, &mem, 20, 21, 22, 2, 3);
    p8_print(&gfx, &mem, text, 23, 24, 0x18);

    assert(call_count == 8);
    assert_call(0, CALL_CLS, 0, 0, 0, 0, 0);
    assert_call(1, CALL_LINE, 1, 2, 3, 4, 0x25);
    assert_call(2, CALL_RECT, 5, 6, 7, 8, 9);
    assert_call(3, CALL_RECTFILL, 10, 11, -12, 13, 0xa5);
    assert_call(4, CALL_CIRC, 14, 15, 16, 7, 0);
    assert_call(5, CALL_CIRCFILL, 17, 18, 19, 0x3c, 0);
    assert_call(6, CALL_SPR, 20, 21, 22, 2, 3);
    assert_call(7, CALL_PRINT, 23, 24, 0x18, 0, 0);
    assert(calls[7].str == text);
    stop_recording(previous);
}

static void test_print_outlined_reaches_its_own_entry_once(void) {
    const p8_draw_backend_t *previous = start_recording();
    static const char text[] = "A B";
    p8_camera(&gfx, 5, -6);

    p8_print_outlined(&gfx, &mem, text, -2, 117, 0x1b);

    assert(call_count == 1);
    assert_call(0, CALL_PRINT_OUTLINED, -2, 117, 0x1b, 0, 0);
    assert(calls[0].str == text);
    stop_recording(previous);
}

static void test_lines_reaches_its_own_entry_once_with_its_list(void) {
    const p8_draw_backend_t *previous = start_recording();
    static const p8_segment_t segments[] = { { 1, 2, 3, 4 }, { -5, 6, 7, 200 }, { 8, 9, 8, 9 } };
    p8_camera(&gfx, 7, -3);

    p8_lines(&gfx, &mem, segments, 3, 0x2b);

    assert(call_count == 1);
    assert_call(0, CALL_LINES, 3, 0x2b, 0, 0, 0);
    assert(calls[0].segments == segments);
    assert(calls[0].gfx->camera_x == 7);
    stop_recording(previous);
}

static void test_psets_reaches_its_own_entry_once_with_its_list(void) {
    const p8_draw_backend_t *previous = start_recording();
    static const p8_point_t points[] = { { 1, 2, 3 }, { -5, 200, 0x1c } };
    p8_camera(&gfx, -4, 9);

    p8_psets(&gfx, &mem, points, 2);

    assert(call_count == 1);
    assert_call(0, CALL_PSETS, 2, 0, 0, 0, 0);
    assert(calls[0].points == points);
    assert(calls[0].gfx->camera_y == 9);
    stop_recording(previous);
}

static void test_calls_reach_the_backend_in_world_coordinates(void) {
    const p8_draw_backend_t *previous = start_recording();
    p8_camera(&gfx, 40, -8);

    p8_pset(&gfx, &mem, 50, 0, 7);

    assert(call_count == 1);
    assert_call(0, CALL_PSET, 50, 0, 7, 0, 0);
    assert(calls[0].gfx->camera_x == 40);
    stop_recording(previous);
}

static void test_map_gives_one_spr_per_non_empty_cell(void) {
    const p8_draw_backend_t *previous = start_recording();
    p8_mset(&mem, 3, 4, 17);
    p8_mset(&mem, 5, 4, 18);
    p8_mset(&mem, 4, 5, 19);

    p8_map(&gfx, &mem, 3, 4, 100, 50, 3, 2);

    assert(call_count == 3);
    assert_call(0, CALL_SPR, 17, 100, 50, 1, 1);
    assert_call(1, CALL_SPR, 18, 100 + 2 * P8_SPRITE_SIZE, 50, 1, 1);
    assert_call(2, CALL_SPR, 19, 100 + P8_SPRITE_SIZE, 50 + P8_SPRITE_SIZE, 1, 1);
    stop_recording(previous);
}

static void fill_map(void) {
    for (int r = 0; r < 32; r++) {
        for (int c = 0; c < 32; c++) {
            p8_mset(&mem, c, r, 1 + (r * 32 + c) % 255);
        }
    }
}

static void test_map_skips_cells_off_screen_after_the_camera(void) {
    const p8_draw_backend_t *previous = start_recording();
    fill_map();
    p8_camera(&gfx, -(P8_SCREEN_WIDTH - 2 * P8_SPRITE_SIZE), -(P8_SCREEN_HEIGHT - 2 * P8_SPRITE_SIZE));

    p8_map(&gfx, &mem, 0, 0, 0, 0, 32, 32);

    assert(call_count == 4);
    assert_call(0, CALL_SPR, 1, 0, 0, 1, 1);
    assert_call(1, CALL_SPR, 2, P8_SPRITE_SIZE, 0, 1, 1);
    assert_call(2, CALL_SPR, 33, 0, P8_SPRITE_SIZE, 1, 1);
    assert_call(3, CALL_SPR, 34, P8_SPRITE_SIZE, P8_SPRITE_SIZE, 1, 1);
    stop_recording(previous);
}

static unsigned char map_cell_or_zero(int x, int y) {
    if (x < 0 || x >= 32 || y < 0 || y >= 32) {
        return 0;
    }
    return mem.map_cells[y][x];
}

/* The pre-culling loop, filtered by the cost model: the calls p8_map must give, in order. */
static void assert_map_matches_the_old_loop_filtered_by_cost(int cel_x, int cel_y, int sx, int sy, int cel_w, int cel_h) {
    call_count = 0;
    p8_map(&gfx, &mem, cel_x, cel_y, sx, sy, cel_w, cel_h);
    int expected = 0;
    for (int r = 0; r < cel_h; r++) {
        for (int c = 0; c < cel_w; c++) {
            int n = map_cell_or_zero(cel_x + c, cel_y + r);
            if (n == 0) {
                continue;
            }
            int x = sx + c * P8_SPRITE_SIZE;
            int y = sy + r * P8_SPRITE_SIZE;
            p8_draw_cost_window_t window = {0, 0};
            if (p8_draw_cost_spr(&window, &gfx, n, x, y, 1, 1).bytes == 0) {
                continue;
            }
            assert(expected < call_count);
            assert_call(expected, CALL_SPR, n, x, y, 1, 1);
            expected++;
        }
    }
    assert(call_count == expected);
}

static void assert_map_matches_at_cameras(const int *xs, int x_count, const int *ys, int y_count) {
    static const int origins[][4] = {{0, 0, 0, 0}, {0, 0, -13, 21}, {20, 25, 5, -3}};
    for (int o = 0; o < 3; o++) {
        for (int i = 0; i < x_count; i++) {
            for (int j = 0; j < y_count; j++) {
                p8_camera(&gfx, xs[i], ys[j]);
                assert_map_matches_the_old_loop_filtered_by_cost(origins[o][0], origins[o][1], origins[o][2], origins[o][3], 32, 32);
            }
        }
    }
}

static int camera_sweep(int *out, int extent) {
    const int map_end = 32 * P8_SPRITE_SIZE;
    const int centres[] = {-extent - P8_SPRITE_SIZE, -P8_SPRITE_SIZE, map_end - extent, map_end};
    int count = 0;
    for (int k = 0; k < 4; k++) {
        for (int d = -10; d <= 10; d++) {
            out[count++] = centres[k] + d;
        }
    }
    out[count++] = -5000;
    out[count++] = 5000;
    return count;
}

static void test_map_gives_the_old_loops_on_screen_calls_at_every_edge(void) {
    const p8_draw_backend_t *previous = start_recording();
    fill_map();
    p8_mset(&mem, 4, 3, 0);
    int xs[90];
    int ys[90];
    int x_count = camera_sweep(xs, P8_SCREEN_WIDTH);
    int y_count = camera_sweep(ys, P8_SCREEN_HEIGHT);
    const int fixed[] = {0, -5, 60};

    assert_map_matches_at_cameras(xs, x_count, fixed, 3);
    assert_map_matches_at_cameras(fixed, 3, ys, y_count);
    stop_recording(previous);
}

static void test_map_with_no_cell_on_screen_gives_no_call(void) {
    const p8_draw_backend_t *previous = start_recording();
    fill_map();

    p8_map(&gfx, &mem, 0, 0, 0, 0, 0, 32);
    p8_map(&gfx, &mem, 0, 0, 0, 0, 32, -1);
    p8_map(&gfx, &mem, 0, 0, P8_SCREEN_WIDTH, 0, 32, 32);
    p8_map(&gfx, &mem, 0, 0, 0, -32 * P8_SPRITE_SIZE, 32, 32);
    p8_camera(&gfx, 0, 32 * P8_SPRITE_SIZE);
    p8_map(&gfx, &mem, 0, 0, 0, 0, 32, 32);

    assert(call_count == 0);
    stop_recording(previous);
}

static void test_spr_default_gives_a_one_by_one_spr(void) {
    const p8_draw_backend_t *previous = start_recording();

    p8_spr_default(&gfx, &mem, 33, -4, 9);

    assert(call_count == 1);
    assert_call(0, CALL_SPR, 33, -4, 9, 1, 1);
    stop_recording(previous);
}

static void test_state_calls_reach_no_entry(void) {
    const p8_draw_backend_t *previous = start_recording();

    p8_camera(&gfx, 3, 4);
    p8_camera_reset(&gfx);
    p8_fillp(&gfx, 0xa5a5);
    p8_fillp_transparent(&gfx, 0xaaaa);
    p8_fillp_reset(&gfx);
    p8_palt(&mem, 0, 0);
    p8_mset(&mem, 1, 1, 5);
    (void)p8_sget(&mem, 2, 2);
    (void)p8_gfx_is_transparent(&mem, 0);

    assert(call_count == 0);
    stop_recording(previous);
}

static void test_spr_sees_a_draw_palette_copied_with_memcpy(void) {
    const p8_draw_backend_t *previous = start_recording();
    unsigned char block[16];
    for (int i = 0; i < 16; i++) {
        block[i] = (unsigned char)(15 - i);
    }
    block[2] |= P8_DRAW_PALETTE_TRANSPARENT_BIT;

    memcpy(mem.draw_palette, block, sizeof(block));
    p8_spr(&gfx, &mem, 1, 0, 0, 1, 1);

    assert(call_count == 1);
    assert(memcmp(calls[0].draw_palette, block, sizeof(block)) == 0);
    stop_recording(previous);
}

static void test_set_returns_the_previous_backend_and_null_restores_software(void) {
    assert(p8_draw_backend_get() == p8_gfx_software_backend());
    assert(p8_draw_backend_set(&RECORDING_BACKEND) == p8_gfx_software_backend());
    assert(p8_draw_backend_get() == &RECORDING_BACKEND);
    assert(p8_draw_backend_set(NULL) == &RECORDING_BACKEND);
    assert(p8_draw_backend_get() == p8_gfx_software_backend());

    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    call_count = 0;
    p8_pset(&gfx, &mem, 10, 20, 7);

    assert(call_count == 0);
    assert(mem.screen[20][10] == 7);
}

static void spr_at_origin(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_spr(build_gfx, build_mem, 9, 0, 0, 1, 1);
}

static void assert_image_call(int index, enum call_kind kind, int slot, int a, int b) {
    const recorded_call_t *call = &calls[index];
    assert(call->kind == kind && call->args[0] == slot && call->args[1] == a && call->args[2] == b);
}

static void test_a_build_frames_its_windowed_calls_and_a_copy_reaches_its_entry(void) {
    const p8_draw_backend_t *previous = start_recording();
    p8_camera(&gfx, 7, 9);

    p8_image_build(&gfx, &mem, 0, 200, 100, spr_at_origin, NULL);
    p8_image_draw(&gfx, &mem, 0, -3, 4);

    assert(call_count == 7);
    assert_image_call(0, CALL_IMAGE_BEGIN, 0, 200, 100);
    assert_image_call(1, CALL_IMAGE_WINDOW, 0, 0, 0);
    assert_call(2, CALL_SPR, 9, 0, 0, 1, 1);
    assert_image_call(3, CALL_IMAGE_WINDOW, 0, 160, 0);
    assert_call(4, CALL_SPR, 9, 0, 0, 1, 1);
    assert_image_call(5, CALL_IMAGE_END, 0, 0, 0);
    assert_image_call(6, CALL_IMAGE_DRAW, 0, -3, 4);
    assert(calls[6].gfx == &gfx && calls[6].mem == &mem && gfx.camera_x == 7 && gfx.camera_y == 9);
    assert(p8_draw_backend_get() == &RECORDING_BACKEND && "the build gave the backend back");
    stop_recording(previous);
}

static const p8_segment_t BUILD_SEGMENTS[] = { { 0, 0, 3, 0 }, { 0, 1, 0, 5 } };

static void lines_at_origin(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_lines(build_gfx, build_mem, BUILD_SEGMENTS, 2, 6);
}

static const p8_point_t BUILD_POINTS[] = { { 0, 0, 7 }, { 3, 4, 8 } };

static void points_at_origin(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_psets(build_gfx, build_mem, BUILD_POINTS, 2);
}

static void test_psets_in_a_build_reach_their_entry_through_the_guard(void) {
    const p8_draw_backend_t *previous = start_recording();

    p8_image_build(&gfx, &mem, 0, 100, 100, points_at_origin, NULL);

    assert(call_count == 4);
    assert(calls[2].kind == CALL_PSETS && calls[2].points == BUILD_POINTS && calls[2].args[0] == 2);
    assert_image_call(3, CALL_IMAGE_END, 0, 0, 0);
    stop_recording(previous);
}

static void test_lines_in_a_build_reach_their_entry_through_the_guard(void) {
    const p8_draw_backend_t *previous = start_recording();

    p8_image_build(&gfx, &mem, 0, 100, 100, lines_at_origin, NULL);

    assert(call_count == 4);
    assert_image_call(0, CALL_IMAGE_BEGIN, 0, 100, 100);
    assert_image_call(1, CALL_IMAGE_WINDOW, 0, 0, 0);
    assert(calls[2].kind == CALL_LINES && calls[2].segments == BUILD_SEGMENTS);
    assert(calls[2].args[0] == 2 && calls[2].args[1] == 6);
    assert_image_call(3, CALL_IMAGE_END, 0, 0, 0);
    stop_recording(previous);
}

int main(void) {
    test_pset_reaches_its_entry_once_with_its_arguments();
    test_each_draw_call_reaches_its_own_entry_once();
    test_print_outlined_reaches_its_own_entry_once();
    test_lines_reaches_its_own_entry_once_with_its_list();
    test_psets_reaches_its_own_entry_once_with_its_list();
    test_calls_reach_the_backend_in_world_coordinates();
    test_map_gives_one_spr_per_non_empty_cell();
    test_map_skips_cells_off_screen_after_the_camera();
    test_map_gives_the_old_loops_on_screen_calls_at_every_edge();
    test_map_with_no_cell_on_screen_gives_no_call();
    test_spr_default_gives_a_one_by_one_spr();
    test_state_calls_reach_no_entry();
    test_spr_sees_a_draw_palette_copied_with_memcpy();
    test_set_returns_the_previous_backend_and_null_restores_software();
    test_a_build_frames_its_windowed_calls_and_a_copy_reaches_its_entry();
    test_lines_in_a_build_reach_their_entry_through_the_guard();
    test_psets_in_a_build_reach_their_entry_through_the_guard();
    printf("test_draw_backend: ok\n");
    return 0;
}
