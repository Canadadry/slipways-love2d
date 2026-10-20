#include "game/demo_data.h"
#include "game/demo_overlay.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { SENTINEL = 3, BAND_TOP = 96, BAND_BOTTOM = 108, LINE_ONE_Y = 97, LINE_TWO_Y = 103, SINGLE_LINE_Y = 100 };

typedef struct {
    const char *text;
    int x;
    int y;
} expected_line_t;

static p8_vram_t g_actual;
static p8_vram_t g_expected;

static void fill_sentinel(p8_vram_t *mem) {
    p8_vram_init(mem);
    memset(mem->screen, SENTINEL, sizeof mem->screen);
}

static void expect_tag(void) {
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_rectfill(&gfx, &g_expected, 143, 11, 159, 17, 8);
    p8_print(&gfx, &g_expected, "demo", 144, 12, 7);
}

static void expect_band(const expected_line_t *lines, int count) {
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_rectfill(&gfx, &g_expected, 0, BAND_TOP, P8_SCREEN_WIDTH - 1, BAND_BOTTOM, 1);
    for (int i = 0; i < count; i++) {
        p8_print(&gfx, &g_expected, lines[i].text, lines[i].x, lines[i].y, 7);
    }
}

static void draw_frame(const p8_graphics_t *gfx, int frame) {
    fill_sentinel(&g_actual);
    demo_overlay_draw(gfx, &g_actual, frame);
}

static void draw_frame_default(int frame) {
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    draw_frame(&gfx, frame);
}

static void assert_screens_equal(const char *what) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (g_actual.screen[y][x] != g_expected.screen[y][x]) {
                fprintf(stderr, "%s: pixel (%d, %d) is %d, expected %d\n", what, x, y, g_actual.screen[y][x],
                        g_expected.screen[y][x]);
                assert(0 && "the overlay's pixels are the rule's");
            }
        }
    }
}

static void test_each_compiled_caption_covers_its_range(void) {
    for (int i = 0; i < demo_data_caption_count; i++) {
        const demo_data_caption_t *caption = &demo_data_captions[i];
        assert(demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, caption->from) == i);
        assert(demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, caption->to - 1) == i);
    }
    assert(demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, -1) == -1);
    assert(demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, demo_data_frame_count) == -1);
}

static void test_a_gap_between_captions_finds_none(void) {
    static const demo_data_caption_t gapped[] = {{0, 10, "a"}, {20, 30, "b"}};
    assert(demo_overlay_find_caption(gapped, 2, 9) == 0);
    assert(demo_overlay_find_caption(gapped, 2, 10) == -1);
    assert(demo_overlay_find_caption(gapped, 2, 19) == -1);
    assert(demo_overlay_find_caption(gapped, 2, 20) == 1);
}

static void test_first_frame_draws_two_centred_lines_on_the_band_and_the_tag(void) {
    draw_frame_default(0);
    fill_sentinel(&g_expected);
    const expected_line_t lines[] = {{"build a trading empire", 36, LINE_ONE_Y}, {"across the stars", 48, LINE_TWO_Y}};
    expect_band(lines, 2);
    expect_tag();
    assert_screens_equal("demo frame 0");
}

static void test_the_tech_caption_is_one_line_centred_on_the_same_band(void) {
    int tech_frame = 2120;
    assert(demo_overlay_find_caption(demo_data_captions, demo_data_caption_count, tech_frame) == 5);
    draw_frame_default(tech_frame);
    fill_sentinel(&g_expected);
    const expected_line_t line = {"spend science on new tech", 30, SINGLE_LINE_Y};
    expect_band(&line, 1);
    expect_tag();
    assert_screens_equal("tech caption");
    for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
        for (int y = BAND_TOP; y < SINGLE_LINE_Y; y++) {
            assert(g_actual.screen[y][x] == 1 && "4 empty band rows above the single line");
        }
        for (int y = SINGLE_LINE_Y + 6; y <= BAND_BOTTOM; y++) {
            assert(g_actual.screen[y][x] == 1 && "the band's last 3 rows are empty");
        }
    }
}

static void test_the_last_frame_shows_the_closing_caption(void) {
    draw_frame_default(demo_data_frame_count - 1);
    fill_sentinel(&g_expected);
    const expected_line_t lines[] = {{"slipways", 64, LINE_ONE_Y}, {"press any button", 48, LINE_TWO_Y}};
    expect_band(lines, 2);
    expect_tag();
    assert_screens_equal("closing caption");
}

static void test_no_demo_frame_draws_nothing(void) {
    draw_frame_default(-1);
    fill_sentinel(&g_expected);
    assert_screens_equal("no demo frame");
}

static void test_the_caller_camera_and_fill_pattern_neither_move_nor_change(void) {
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_camera(&gfx, 5, 7);
    p8_fillp(&gfx, 0x5a5a);
    p8_graphics_t before = gfx;
    draw_frame(&gfx, 0);
    assert(memcmp(&before, &gfx, sizeof gfx) == 0 && "the caller's graphics state is untouched");
    fill_sentinel(&g_expected);
    const expected_line_t lines[] = {{"build a trading empire", 36, LINE_ONE_Y}, {"across the stars", 48, LINE_TWO_Y}};
    expect_band(lines, 2);
    expect_tag();
    assert_screens_equal("shifted camera and fill pattern");
}

int main(void) {
    test_each_compiled_caption_covers_its_range();
    test_a_gap_between_captions_finds_none();
    test_first_frame_draws_two_centred_lines_on_the_band_and_the_tag();
    test_the_tech_caption_is_one_line_centred_on_the_same_band();
    test_the_last_frame_shows_the_closing_caption();
    test_no_demo_frame_draws_nothing();
    test_the_caller_camera_and_fill_pattern_neither_move_nor_change();
    printf("test_demo_overlay: ok\n");
    return 0;
}
