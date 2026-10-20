#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "game/image_slot.h"
#include "game/text_image.h"
#include "game/visual_random.h"
#include "p8/draw_backend.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/vram.h"

static p8_vram_t memory;
static p8_graphics_t graphics;

static int g_builds;
static int g_building;
static int g_build_slot;
static int g_other_calls_in_builds;
static int g_direct_prints;
static int g_copies;
static int g_rebuilt_after_copy;
static int g_copied[P8_IMAGE_SLOT_COUNT];
static const p8_draw_backend_t *g_software;

static void count_cls(p8_vram_t *mem) {
    g_other_calls_in_builds += g_building;
    g_software->cls(mem);
}

static void count_pset(const p8_graphics_t *gfx, p8_vram_t *mem, int x, int y, int col) {
    g_other_calls_in_builds += g_building;
    g_software->pset(gfx, mem, x, y, col);
}

static void count_line(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    g_other_calls_in_builds += g_building;
    g_software->line(gfx, mem, x1, y1, x2, y2, col);
}

static void count_rect(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    g_other_calls_in_builds += g_building;
    g_software->rect(gfx, mem, x1, y1, x2, y2, col);
}

static void count_rectfill(const p8_graphics_t *gfx, p8_vram_t *mem, int x1, int y1, int x2, int y2, int col) {
    g_other_calls_in_builds += g_building;
    g_software->rectfill(gfx, mem, x1, y1, x2, y2, col);
}

static void count_circ(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    g_other_calls_in_builds += g_building;
    g_software->circ(gfx, mem, cx, cy, r, col);
}

static void count_circfill(const p8_graphics_t *gfx, p8_vram_t *mem, int cx, int cy, int r, int col) {
    g_other_calls_in_builds += g_building;
    g_software->circfill(gfx, mem, cx, cy, r, col);
}

static void count_spr(const p8_graphics_t *gfx, p8_vram_t *mem, int n, int x, int y, int w, int h) {
    g_other_calls_in_builds += g_building;
    g_software->spr(gfx, mem, n, x, y, w, h);
}

static void count_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    g_other_calls_in_builds += g_building;
    g_software->print(gfx, mem, str, x, y, col);
}

static void count_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    g_direct_prints += !g_building;
    g_software->print_outlined(gfx, mem, str, x, y, col);
}

static void count_image_begin(int slot, int w, int h) {
    g_builds++;
    g_building = 1;
    g_build_slot = slot;
    g_rebuilt_after_copy += g_copied[slot];
    g_software->image_begin(slot, w, h);
}

static void count_image_window(int slot, int x, int y) {
    g_software->image_window(slot, x, y);
}

static void count_image_end(int slot) {
    g_building = 0;
    g_software->image_end(slot);
}

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    assert(slot >= IMAGE_SLOT_TEXT_FIRST && slot < IMAGE_SLOT_TEXT_FIRST + IMAGE_SLOT_TEXT_COUNT && "a text slot");
    g_copies++;
    g_copied[slot] = 1;
    g_software->image_draw(gfx, mem, slot, x, y);
}

static void count_lines(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_segment_t *segments, int count, int col) {
    g_other_calls_in_builds += g_building;
    g_software->lines(gfx, mem, segments, count, col);
}

static void count_psets(const p8_graphics_t *gfx, p8_vram_t *mem, const p8_point_t *points, int count) {
    g_other_calls_in_builds += g_building;
    g_software->psets(gfx, mem, points, count);
}

static const p8_draw_backend_t COUNTING = {
    count_cls, count_pset, count_line, count_rect, count_rectfill,
    count_circ, count_circfill, count_spr, count_print, count_print_outlined,
    count_image_begin, count_image_window, count_image_end, count_image_draw,
    count_lines, count_psets,
};

static void new_frame(void) {
    text_image_frame_begin();
    memset(g_copied, 0, sizeof g_copied);
}

static void start(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    g_software = p8_draw_backend_set(&COUNTING);
    g_builds = 0;
    g_direct_prints = 0;
    g_copies = 0;
    g_rebuilt_after_copy = 0;
    new_frame();
}

static void stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)(1 + (x * 3 + y) % 15);
        }
    }
}

static void remapped_palette(p8_vram_t *mem) {
    for (int col = 0; col < 16; col++) {
        mem->draw_palette[col] = (unsigned char)((col * 7 + 2) % 16);
    }
}

/* Prints both ways from the same screen, camera and draw palette, and compares what they leave. */
static void assert_prints_like_direct(const char *str, int x, int y, int col, int camera_x, int camera_y) {
    static p8_vram_t direct;
    p8_graphics_t direct_gfx;
    p8_camera(&graphics, camera_x, camera_y);
    direct_gfx = graphics;
    stripes(&memory);
    direct = memory;

    int direct_before = g_direct_prints;
    g_software->print_outlined(&direct_gfx, &direct, str, x, y, col);
    text_image_print_outlined(&graphics, &memory, str, x, y, col);

    assert(g_direct_prints - direct_before <= 1);
    assert(graphics.camera_x == camera_x && graphics.camera_y == camera_y && "the camera is back");
    if (memcmp(direct.screen, memory.screen, sizeof direct.screen) != 0) {
        fprintf(stderr, "screens differ: \"%s\" at (%d, %d) col %d camera (%d, %d)\n", str, x, y, col, camera_x, camera_y);
        assert(0);
    }
    assert(memcmp(direct.draw_palette, memory.draw_palette, sizeof direct.draw_palette) == 0 && "the same palette after");
}

static void test_a_label_is_built_once_then_copied_like_the_direct_print(void) {
    start();
    remapped_palette(&memory);
    assert(text_image_cacheable("TECH 12"));
    assert_prints_like_direct("TECH 12", 30, 40, 7, 0, 0);
    assert(g_builds == 1 && g_copies == 1 && g_direct_prints == 0);
    assert(g_other_calls_in_builds == 0 && "a text image holds print texels only");
    new_frame();
    assert_prints_like_direct("TECH 12", 70, 101, 7, 0, 0);
    assert(g_builds == 1 && g_copies == 2 && "the second draw copies the image");
}

static void test_labels_print_like_the_direct_print_at_every_edge_and_camera(void) {
    static const int xs[] = {-40, -9, -1, 0, 1, 37, 100, 120, 126, 127, 128, 140};
    static const int ys[] = {-7, -6, -1, 0, 1, 60, 121, 122, 127, 128};
    static const int cameras[][2] = {{0, 0}, {-13, 21}, {200, -64}};
    static const char *labels[] = {"0", "W", "SKIP 10Y", "$ 1,234 + 5%", "ABCDEFGHIJKLMNOPQRSTUVWXYZ01234"};
    start();
    for (size_t l = 0; l < sizeof labels / sizeof labels[0]; l++) {
        assert(text_image_cacheable(labels[l]));
        for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
            for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
                for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                    assert_prints_like_direct(labels[l], xs[i] + cameras[c][0], ys[j] + cameras[c][1], (int)(l * 3 + 5) % 16,
                                              cameras[c][0], cameras[c][1]);
                }
            }
        }
    }
    assert(g_direct_prints == 0 && g_builds == 5);
    assert(g_other_calls_in_builds == 0 && "a text image holds print texels only");
}

static void test_a_remapped_draw_palette_or_a_fill_pattern_changes_nothing(void) {
    start();
    remapped_palette(&memory);
    assert_prints_like_direct("FADED 7", 20, 30, 9, 0, 0);
    p8_fillp(&graphics, 0x5A5A);
    assert_prints_like_direct("FADED 7", 21, 33, 9, 0, 0);
    p8_fillp_reset(&graphics);
    p8_vram_init(&memory);
    assert_prints_like_direct("FADED 7", 22, 35, 9, 0, 0);
    assert(g_builds == 1 && g_direct_prints == 0);
}

static void test_equal_labels_share_a_slot_and_a_changed_label_or_colour_rebuilds(void) {
    char counter[8];
    start();
    assert_prints_like_direct("12 CR", 10, 10, 7, 0, 0);
    assert_prints_like_direct("12 CR", 50, 70, 7, 0, 0);
    assert(g_builds == 1 && "content-keyed, not keyed by the call");
    assert_prints_like_direct("12 CR", 50, 70, 8, 0, 0);
    assert(g_builds == 2 && "another colour is another image");
    snprintf(counter, sizeof counter, "%d CR", 13);
    assert_prints_like_direct(counter, 10, 10, 7, 0, 0);
    assert(g_builds == 3 && "a changed label rebuilds on the frame it changes");
    snprintf(counter, sizeof counter, "%d CR", 12);
    assert_prints_like_direct(counter, 10, 10, 7, 0, 0);
    assert(g_builds == 3 && "keyed on the bytes, not the pointer");
}

static void test_a_label_too_long_for_a_slot_or_empty_prints_directly(void) {
    static const char too_long[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";
    start();
    assert(strlen(too_long) == 32 && !text_image_cacheable(too_long) && !text_image_cacheable(""));
    assert_prints_like_direct(too_long, 0, 60, 7, 0, 0);
    assert_prints_like_direct(too_long, -3, 2, 12, 4, 5);
    assert_prints_like_direct("", 10, 10, 7, 0, 0);
    assert(g_builds == 0 && g_copies == 0 && g_direct_prints == 3);
}

enum { MORE_THAN_SLOTS = IMAGE_SLOT_TEXT_COUNT + 9 };

static void test_a_slot_copied_this_frame_is_never_rebuilt_in_it(void) {
    char label[16];
    start();
    for (int k = 0; k < MORE_THAN_SLOTS; k++) {
        snprintf(label, sizeof label, "ITEM %d", k);
        assert_prints_like_direct(label, k * 7 % 140 - 10, k * 11 % 130 - 3, k % 16, 0, 0);
    }
    assert(g_builds == IMAGE_SLOT_TEXT_COUNT && g_direct_prints == MORE_THAN_SLOTS - IMAGE_SLOT_TEXT_COUNT);
    assert(g_rebuilt_after_copy == 0);

    new_frame();
    for (int k = MORE_THAN_SLOTS - 1; k >= 0; k--) {
        snprintf(label, sizeof label, "ITEM %d", k);
        assert_prints_like_direct(label, k * 5 % 140 - 10, k * 3 % 130 - 3, k % 16, 0, 0);
    }
    assert(g_rebuilt_after_copy == 0);
    assert(g_builds > IMAGE_SLOT_TEXT_COUNT && "the next frame evicts what it needs");
}

static void test_a_building_print_touches_neither_random_stream(void) {
    start();
    uint32_t game = p8_fixed_rnd_state();
    uint32_t visual = visual_random_state();
    assert_prints_like_direct("RANDOM 1", 64, 60, 7, 0, 0);
    assert(g_builds == 1);
    assert(p8_fixed_rnd_state() == game && visual_random_state() == visual);
}

static void in_fresh_child(void (*test)(void)) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

int main(void) {
    in_fresh_child(test_a_label_is_built_once_then_copied_like_the_direct_print);
    in_fresh_child(test_labels_print_like_the_direct_print_at_every_edge_and_camera);
    in_fresh_child(test_a_remapped_draw_palette_or_a_fill_pattern_changes_nothing);
    in_fresh_child(test_equal_labels_share_a_slot_and_a_changed_label_or_colour_rebuilds);
    in_fresh_child(test_a_label_too_long_for_a_slot_or_empty_prints_directly);
    in_fresh_child(test_a_slot_copied_this_frame_is_never_rebuilt_in_it);
    in_fresh_child(test_a_building_print_touches_neither_random_stream);
    printf("test_text_image: all tests passed\n");
    return 0;
}
