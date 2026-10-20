#include "devtools/draw_filter.h"

#include "p8/draw_backend.h"
#include "p8/gfx.h"
#include "p8/image.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { KINDS = 12, KIND_PRINT = 8, KIND_PRINT_OUTLINED = 9, KIND_LINES = 10, KIND_PSETS = 11 };

typedef struct {
    int calls;
    const p8_graphics_t *gfx;
    p8_vram_t *mem;
    int args[6];
    const char *str;
} received_t;

static received_t received[KINDS];

static void seen(int kind, const p8_graphics_t *gfx, p8_vram_t *mem, int a, int b, int c, int d, int e, int f) {
    received_t *r = &received[kind];
    r->calls++;
    r->gfx = gfx;
    r->mem = mem;
    int args[6] = { a, b, c, d, e, f };
    memcpy(r->args, args, sizeof args);
}

static void recording_cls(p8_vram_t *mem) {
    seen(0, NULL, mem, 0, 0, 0, 0, 0, 0);
}

static void recording_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    seen(1, gfx, mem, x, y, col, 0, 0, 0);
}

static void recording_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    seen(2, gfx, mem, x1, y1, x2, y2, col, 0);
}

static void recording_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    seen(3, gfx, mem, x1, y1, x2, y2, col, 0);
}

static void recording_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    seen(4, gfx, mem, x1, y1, x2, y2, col, 0);
}

static void recording_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    seen(5, gfx, mem, cx, cy, r, col, 0, 0);
}

static void recording_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    seen(6, gfx, mem, cx, cy, r, col, 0, 0);
}

static void recording_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    seen(7, gfx, mem, n, x, y, w, h, 0);
}

static void recording_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    seen(8, gfx, mem, x, y, col, 0, 0, 0);
    received[8].str = str;
}

static void recording_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    seen(KIND_PRINT_OUTLINED, gfx, mem, x, y, col, 0, 0, 0);
    received[KIND_PRINT_OUTLINED].str = str;
}

enum { IMAGE_BEGIN, IMAGE_WINDOW, IMAGE_END, IMAGE_DRAW, IMAGE_KINDS };

static int image_calls[IMAGE_KINDS];

static void recording_image_begin(int slot, int w, int h) {
    (void)slot, (void)w, (void)h;
    image_calls[IMAGE_BEGIN]++;
}

static void recording_image_window(int slot, int x, int y) {
    (void)slot, (void)x, (void)y;
    image_calls[IMAGE_WINDOW]++;
}

static void recording_image_end(int slot) {
    (void)slot;
    image_calls[IMAGE_END]++;
}

static void recording_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    (void)gfx, (void)mem, (void)slot, (void)x, (void)y;
    image_calls[IMAGE_DRAW]++;
}

static void recording_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    seen(KIND_LINES, gfx, mem, count, col, segments[0].x1, segments[0].y1, segments[0].x2, segments[0].y2);
}

static void recording_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    seen(KIND_PSETS, gfx, mem, count, points[0].x, points[0].y, points[0].col, 0, 0);
}

static const p8_draw_backend_t RECORDING = {
    recording_cls, recording_pset, recording_line, recording_rect, recording_rectfill,
    recording_circ, recording_circfill, recording_spr, recording_print, recording_print_outlined,
    recording_image_begin, recording_image_window, recording_image_end, recording_image_draw,
    recording_lines, recording_psets,
};

static p8_vram_t mem;
static p8_graphics_t gfx;

static void filter_over_recording(void) {
    memset(received, 0, sizeof received);
    memset(image_calls, 0, sizeof image_calls);
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_draw_backend_set(&RECORDING);
    const p8_draw_backend_t *wrapped = draw_filter_install();
    assert(wrapped == &RECORDING);
    assert(p8_draw_backend_get() != &RECORDING);
}

static void filter_removed(void) {
    p8_draw_backend_set(NULL);
}

static const char TEXT[] = "ab";
static const p8_segment_t SEGMENT = { 14, 15, 16, 17 };
static const p8_point_t POINT = { 18, 19, 11 };

static void draw_one_of_each_on_screen(void) {
    p8_cls(&mem);
    p8_pset(&gfx, &mem, 1, 2, 3);
    p8_line(&gfx, &mem, 1, 2, 3, 4, 5);
    p8_rect(&gfx, &mem, 1, 2, 3, 4, 6);
    p8_rectfill(&gfx, &mem, 1, 2, 3, 4, 7);
    p8_circ(&gfx, &mem, 10, 11, 3, 8);
    p8_circfill(&gfx, &mem, 12, 13, 4, 9);
    p8_spr(&gfx, &mem, 5, 6, 7, 1, 2);
    p8_print(&gfx, &mem, TEXT, 8, 9, 10);
    p8_print_outlined(&gfx, &mem, TEXT, 11, 12, 13);
    p8_lines(&gfx, &mem, &SEGMENT, 1, 14);
    p8_psets(&gfx, &mem, &POINT, 1);
}

static void test_all_forwards_every_call_with_its_arguments_and_pointers(void) {
    filter_over_recording();
    draw_one_of_each_on_screen();
    for (int kind = 0; kind < KINDS; kind++) {
        assert(received[kind].calls == 1);
        assert(received[kind].mem == &mem);
        assert(kind == 0 || received[kind].gfx == &gfx);
    }
    assert(received[1].args[0] == 1 && received[1].args[1] == 2 && received[1].args[2] == 3);
    assert(received[4].args[0] == 1 && received[4].args[3] == 4 && received[4].args[4] == 7);
    assert(received[6].args[0] == 12 && received[6].args[2] == 4 && received[6].args[3] == 9);
    assert(received[7].args[0] == 5 && received[7].args[3] == 1 && received[7].args[4] == 2);
    assert(received[8].str == TEXT && received[8].args[2] == 10);
    assert(received[KIND_PRINT_OUTLINED].str == TEXT && received[KIND_PRINT_OUTLINED].args[0] == 11);
    assert(received[KIND_PRINT_OUTLINED].args[1] == 12 && received[KIND_PRINT_OUTLINED].args[2] == 13);
    assert(received[KIND_LINES].args[0] == 1 && received[KIND_LINES].args[1] == 14);
    assert(received[KIND_LINES].args[2] == 14 && received[KIND_LINES].args[5] == 17);
    assert(received[KIND_PSETS].args[0] == 1 && received[KIND_PSETS].args[1] == 18 && received[KIND_PSETS].args[3] == 11);
    filter_removed();
}

static const draw_filter_family_t KIND_FAMILY[KINDS] = {
    DRAW_FILTER_CLS, DRAW_FILTER_PSET, DRAW_FILTER_LINE, DRAW_FILTER_RECT, DRAW_FILTER_RECTFILL,
    DRAW_FILTER_CIRC, DRAW_FILTER_CIRCFILL, DRAW_FILTER_SPR, DRAW_FILTER_PRINT, DRAW_FILTER_PRINT,
    DRAW_FILTER_LINE, DRAW_FILTER_PSET,
};

static void test_a_mask_passes_cls_and_its_families_only(void) {
    for (int kind = 1; kind < KINDS; kind++) {
        filter_over_recording();
        draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(KIND_FAMILY[kind]));
        draw_one_of_each_on_screen();
        for (int other = 0; other < KINDS; other++) {
            int expected = other == 0 || KIND_FAMILY[other] == KIND_FAMILY[kind];
            assert(received[other].calls == expected);
        }
        filter_removed();
    }
}

static void test_a_patterned_fill_needs_its_shape_and_the_pattern_bit(void) {
    unsigned shapes = DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL)
        | DRAW_FILTER_BIT(DRAW_FILTER_CIRCFILL);
    filter_over_recording();
    draw_filter_set_mask(shapes);
    p8_fillp(&gfx, 0xA5A5);
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    p8_circfill(&gfx, &mem, 10, 10, 3, 7);
    assert(received[4].calls == 0 && received[6].calls == 0);

    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PATTERN));
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    assert(received[4].calls == 0 && "the pattern bit alone is not enough");

    draw_filter_set_mask(shapes | DRAW_FILTER_BIT(DRAW_FILTER_PATTERN));
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    p8_circfill(&gfx, &mem, 10, 10, 3, 7);
    assert(received[4].calls == 1 && received[6].calls == 1);
    filter_removed();
}

static void test_a_plain_fill_ignores_the_pattern_bit(void) {
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL));
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    assert(received[4].calls == 1);
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PATTERN));
    p8_rectfill(&gfx, &mem, 0, 0, 5, 5, 7);
    assert(received[4].calls == 1);
    filter_removed();
}

static void test_counts_are_on_screen_calls_before_the_mask(void) {
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS));
    draw_one_of_each_on_screen();
    for (int kind = 0; kind < KINDS; kind++) {
        int same_family = 0;
        for (int other = 0; other < KINDS; other++) same_family += KIND_FAMILY[other] == KIND_FAMILY[kind];
        assert(draw_filter_count(KIND_FAMILY[kind]) == same_family);
    }
    assert(draw_filter_count(DRAW_FILTER_PATTERN) == 0);

    p8_pset(&gfx, &mem, 500, 2, 3);
    p8_rectfill(&gfx, &mem, -20, -20, -10, -10, 7);
    p8_print(&gfx, &mem, TEXT, 300, 9, 10);
    assert(draw_filter_count(DRAW_FILTER_PSET) == 2 && "an off-screen call counts 0");
    assert(draw_filter_count(DRAW_FILTER_RECTFILL) == 1);
    assert(draw_filter_count(DRAW_FILTER_PRINT) == 2);

    p8_print_outlined(&gfx, &mem, TEXT, -8, 9, 10);
    assert(draw_filter_count(DRAW_FILTER_PRINT) == 2 && "the outline column is off screen too");
    p8_print_outlined(&gfx, &mem, TEXT, -7, 9, 10);
    assert(draw_filter_count(DRAW_FILTER_PRINT) == 3 && "the outline column alone is on screen");

    p8_camera(&gfx, 400, 0);
    p8_pset(&gfx, &mem, 401, 2, 3);
    assert(draw_filter_count(DRAW_FILTER_PSET) == 3 && "on screen after the camera");
    filter_removed();
}

static void test_a_patterned_fill_counts_under_its_shape_and_pattern(void) {
    filter_over_recording();
    p8_fillp_transparent(&gfx, 0xAAAA);
    p8_circfill(&gfx, &mem, 10, 10, 3, 7);
    p8_rectfill(&gfx, &mem, 0, 0, 3, 3, 7);
    assert(draw_filter_count(DRAW_FILTER_CIRCFILL) == 1);
    assert(draw_filter_count(DRAW_FILTER_RECTFILL) == 1);
    assert(draw_filter_count(DRAW_FILTER_PATTERN) == 2);
    filter_removed();
}

static void test_frame_end_resets_the_counts_and_keeps_the_mask(void) {
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PSET));
    draw_one_of_each_on_screen();
    draw_filter_frame_end();
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        assert(draw_filter_count((draw_filter_family_t)family) == 0);
    }
    p8_line(&gfx, &mem, 1, 2, 3, 4, 5);
    assert(received[2].calls == 0);
    filter_removed();
}

static void test_the_filter_writes_no_graphics_or_vram_state(void) {
    filter_over_recording();
    p8_fillp(&gfx, 0x0F0F);
    p8_camera(&gfx, 3, 4);
    p8_graphics_t gfx_before = gfx;
    static p8_vram_t mem_before;
    mem_before = mem;
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS));
    draw_one_of_each_on_screen();
    draw_filter_frame_end();
    assert(memcmp(&gfx, &gfx_before, sizeof gfx) == 0);
    assert(memcmp(&mem, &mem_before, sizeof mem) == 0);
    filter_removed();
}

static void sprite_and_patterned_fill(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_spr(build_gfx, build_mem, 1, 0, 0, 1, 1);
    p8_fillp(build_gfx, 0xA5A5);
    p8_rectfill(build_gfx, build_mem, 0, 0, 5, 5, 7);
    p8_fillp_reset(build_gfx);
    p8_line(build_gfx, build_mem, 1000, 1000, 1001, 1001, 7);
}

static void test_a_build_passes_every_call_whatever_the_mask_and_counts_none(void) {
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS));
    p8_image_build(&gfx, &mem, 0, 256, 256, sprite_and_patterned_fill, NULL);
    assert(image_calls[IMAGE_BEGIN] == 1 && image_calls[IMAGE_WINDOW] == 6 && image_calls[IMAGE_END] == 1);
    assert(received[7].calls == 6 && received[4].calls == 6 && received[2].calls == 6);
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        assert(draw_filter_count((draw_filter_family_t)family) == 0 && "a build adds no frame bytes");
    }
    filter_removed();
}

static void test_a_copy_passes_only_when_the_mask_holds_every_family_drawn_into_its_image(void) {
    unsigned image_families = DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_SPR)
        | DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL) | DRAW_FILTER_BIT(DRAW_FILTER_PATTERN);
    filter_over_recording();
    p8_image_build(&gfx, &mem, 0, 256, 256, sprite_and_patterned_fill, NULL);

    unsigned short_of_one[] = {
        image_families & ~DRAW_FILTER_BIT(DRAW_FILTER_SPR),
        image_families & ~DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL),
        image_families & ~DRAW_FILTER_BIT(DRAW_FILTER_PATTERN),
    };
    for (size_t i = 0; i < sizeof short_of_one / sizeof short_of_one[0]; i++) {
        draw_filter_set_mask(short_of_one[i]);
        p8_image_draw(&gfx, &mem, 0, 0, 0);
        assert(image_calls[IMAGE_DRAW] == 0);
    }
    draw_filter_set_mask(image_families);
    p8_image_draw(&gfx, &mem, 0, 0, 0);
    assert(image_calls[IMAGE_DRAW] == 1 && "the off-screen line drew nothing into the image");
    filter_removed();
}

static void test_an_on_screen_copy_counts_once_under_each_family_of_its_image(void) {
    filter_over_recording();
    p8_image_build(&gfx, &mem, 0, 256, 256, sprite_and_patterned_fill, NULL);
    p8_image_draw(&gfx, &mem, 0, 0, 0);
    p8_image_draw(&gfx, &mem, 0, 0, -256);
    assert(draw_filter_count(DRAW_FILTER_SPR) == 1);
    assert(draw_filter_count(DRAW_FILTER_RECTFILL) == 1);
    assert(draw_filter_count(DRAW_FILTER_PATTERN) == 1);
    assert(draw_filter_count(DRAW_FILTER_LINE) == 0);
    filter_removed();
}

static void test_a_batch_of_lines_counts_each_on_screen_segment_and_passes_whole_under_line(void) {
    static const p8_segment_t segments[] = { { 0, 0, 5, 5 }, { -50, 0, -40, 3 }, { 150, 110, 170, 130 } };
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_LINE));
    p8_lines(&gfx, &mem, segments, 3, 7);
    assert(received[KIND_LINES].calls == 1 && received[KIND_LINES].args[0] == 3);
    assert(draw_filter_count(DRAW_FILTER_LINE) == 2 && "the off-screen segment counts 0");

    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PSET));
    p8_lines(&gfx, &mem, segments, 3, 7);
    assert(received[KIND_LINES].calls == 1 && "dropped whole");
    assert(draw_filter_count(DRAW_FILTER_LINE) == 4 && "counted before the mask");
    filter_removed();
}

static void test_a_batch_of_points_counts_each_on_screen_point_and_passes_whole_under_pset(void) {
    static const p8_point_t points[] = { { 0, 0, 1 }, { -1, 0, 2 }, { 159, 119, 3 } };
    filter_over_recording();
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PSET));
    p8_psets(&gfx, &mem, points, 3);
    assert(received[KIND_PSETS].calls == 1 && received[KIND_PSETS].args[0] == 3);
    assert(draw_filter_count(DRAW_FILTER_PSET) == 2 && "the off-screen point counts 0");

    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_LINE));
    p8_psets(&gfx, &mem, points, 3);
    assert(received[KIND_PSETS].calls == 1 && "dropped whole");
    assert(draw_filter_count(DRAW_FILTER_PSET) == 4 && "counted before the mask");
    filter_removed();
}

static void lines_into_the_image(p8_graphics_t *build_gfx, p8_vram_t *build_mem, void *context) {
    (void)context;
    p8_lines(build_gfx, build_mem, &SEGMENT, 1, 7);
}

static void test_a_batch_of_lines_in_a_build_marks_its_image_with_line(void) {
    filter_over_recording();
    p8_image_build(&gfx, &mem, 1, 48, 8, lines_into_the_image, NULL);
    assert(received[KIND_LINES].calls == 1);
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_PSET));
    p8_image_draw(&gfx, &mem, 1, 0, 0);
    assert(image_calls[IMAGE_DRAW] == 0);
    draw_filter_set_mask(DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_LINE));
    p8_image_draw(&gfx, &mem, 1, 0, 0);
    assert(image_calls[IMAGE_DRAW] == 1);
    filter_removed();
}

int main(void) {
    test_all_forwards_every_call_with_its_arguments_and_pointers();
    test_a_mask_passes_cls_and_its_families_only();
    test_a_patterned_fill_needs_its_shape_and_the_pattern_bit();
    test_a_plain_fill_ignores_the_pattern_bit();
    test_counts_are_on_screen_calls_before_the_mask();
    test_a_patterned_fill_counts_under_its_shape_and_pattern();
    test_frame_end_resets_the_counts_and_keeps_the_mask();
    test_the_filter_writes_no_graphics_or_vram_state();
    test_a_build_passes_every_call_whatever_the_mask_and_counts_none();
    test_a_copy_passes_only_when_the_mask_holds_every_family_drawn_into_its_image();
    test_an_on_screen_copy_counts_once_under_each_family_of_its_image();
    test_a_batch_of_lines_counts_each_on_screen_segment_and_passes_whole_under_line();
    test_a_batch_of_lines_in_a_build_marks_its_image_with_line();
    test_a_batch_of_points_counts_each_on_screen_point_and_passes_whole_under_pset();
    printf("test_draw_filter: ok\n");
    return 0;
}
