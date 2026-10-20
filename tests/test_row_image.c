#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/dispatch_context.h"
#include "engine/game_zone.h"
#include "engine/pointer_list.h"
#include "game/cart.h"
#include "game/layout.h"
#include "game/map_tables.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/visual_random.h"
#include "game/row_image.h"
#include "p8/sprite_sheet.h"
#include "p8/draw_backend.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "heap_allocator.h"

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static int g_builds;
static int g_building;
static int g_other_calls_in_builds;
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
    g_software->spr(gfx, mem, n, x, y, w, h);
}

static void count_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    g_other_calls_in_builds += g_building;
    g_software->print(gfx, mem, str, x, y, col);
}

static void count_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    g_other_calls_in_builds += g_building;
    g_software->print_outlined(gfx, mem, str, x, y, col);
}

static void count_image_begin(int slot, int w, int h) {
    g_builds++;
    g_building = 1;
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

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static void stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)(1 + (x * 3 + y) % 15);
        }
    }
}

static void entry_palette(p8_vram_t *mem) {
    for (int col = 0; col < 16; col++) {
        mem->draw_palette[col] = (unsigned char)((col * 7 + 2) % 16);
    }
}

/* Draws the row both ways from the same screen and entry palette, and compares what they leave. */
static void assert_draws_like_direct(const layout_row_t *row, vec_t p, p8_fixed_t a, int camera_x, int camera_y) {
    static p8_vram_t direct;
    p8_graphics_t direct_gfx;
    direct = memory;
    p8_camera(&graphics, camera_x, camera_y);
    direct_gfx = graphics;
    stripes(&memory);
    entry_palette(&memory);
    stripes(&direct);
    entry_palette(&direct);

    layout_row_draw_at(&direct_gfx, &direct, row, p, a);
    row_image_draw_at(&graphics, &memory, row, p, a);

    assert(graphics.camera_x == camera_x && graphics.camera_y == camera_y && "the camera is back");
    if (memcmp(direct.screen, memory.screen, sizeof direct.screen) != 0) {
        fprintf(stderr, "screens differ: row at (%d, %d) a %d camera (%d, %d)\n", p.x, p.y, a, camera_x, camera_y);
        assert(0);
    }
    assert(memcmp(direct.draw_palette, memory.draw_palette, sizeof direct.draw_palette) == 0 && "the same palette after");
}

static const pointer_list_t *largest_inputs(void) {
    const pointer_list_t *inputs = NULL;
    for (int k = 1; research_projects_at(k) != NULL; k++) {
        const research_project_t *project = research_projects_at(k);
        if (inputs == NULL || project->inputs.count > inputs->count) {
            inputs = &project->inputs;
        }
    }
    assert(inputs != NULL);
    return inputs;
}

static int resource(const char *code) {
    int number;
    assert(resource_code_lookup(code, &number));
    return number;
}

static layout_row_t *planet_row(const pointer_list_t *inputs, int output, int amount, int used) {
    layout_row_keys_t keys;
    assert(research_display_planet_resource_row_keys(inputs, 1, p8_fixed_from_int(output), p8_fixed_from_int(amount),
                                                     p8_fixed_from_int(used), 0, &keys));
    return layout_row_new(&heap_allocator, &keys);
}

static layout_row_t *largest_row(void) {
    return planet_row(largest_inputs(), resource("w"), 5, 0);
}

static void start(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_flush_exit_queue();
    cart_start_game(NULL);
    p8_gfx_init(&graphics);
    g_software = p8_draw_backend_set(&COUNTING);
    g_builds = 0;
}

static void test_the_largest_planet_row_is_cached_and_draws_like_the_direct_row(void) {
    start();
    layout_row_t *row = largest_row();
    assert(row_image_cacheable(row));

    assert_draws_like_direct(row, vec_xy(64, 71), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 1);
    assert(g_other_calls_in_builds == 0 && "a row image holds sprite texels only");
    assert_draws_like_direct(row, vec_xy(80, 30), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 1 && "the second draw copies the image");
}

static const pointer_list_t *three_wants_one_grey_one_empty(void) {
    static pointer_list_t wants;
    need_t *ore = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(21)});
    need_t *grey = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(18)});
    need_t *empty = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
    grey->has_satisfied = 1;
    grey->satisfied = 0;
    pointer_list_init(&wants, &heap_allocator);
    pointer_list_add(&wants, ore);
    pointer_list_add(&wants, grey);
    pointer_list_add(&wants, empty);
    return &wants;
}

static void test_planet_rows_draw_like_the_direct_rows_at_every_edge_and_shake(void) {
    static const p8_fixed_t xs[] = {
        P8_FIXED_LIT(0x00400000, 64), P8_FIXED_LIT(0xFFE20000, -30), P8_FIXED_LIT(0xFFF60000, -10),
        P8_FIXED_LIT(0x00960000, 150), P8_FIXED_LIT(0x00AA0000, 170), P8_FIXED_LIT(0x00404CCD, 64.3),
        P8_FIXED_LIT(0x0046C000, 70.75), P8_FIXED_LIT(0xFFFF8000, -0.5),
    };
    static const p8_fixed_t ys[] = {
        P8_FIXED_LIT(0x003C0000, 60), P8_FIXED_LIT(0xFFFD0000, -3), P8_FIXED_LIT(0x007C0000, 124),
        P8_FIXED_LIT(0x007E0000, 126), P8_FIXED_LIT(0x003C6000, 60.375),
    };
    static const p8_fixed_t aligns[] = {0, P8_FIXED_LIT(0x00008000, 0.5)};
    static const int cameras[][2] = {{0, 0}, {-13, 21}};
    start();
    layout_row_t *rows[] = {
        largest_row(),
        planet_row(three_wants_one_grey_one_empty(), resource("w"), 3, 1),
        planet_row(largest_inputs(), 21, 2, 2),
        planet_row(NULL, 18, 1, 0),
    };
    for (size_t r = 0; r < sizeof rows / sizeof rows[0]; r++) {
        assert(row_image_cacheable(rows[r]));
        for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
            for (size_t al = 0; al < sizeof aligns / sizeof aligns[0]; al++) {
                for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
                    for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                        vec_t p = vec_make(xs[i] + p8_fixed_from_int(cameras[c][0]), ys[j] + p8_fixed_from_int(cameras[c][1]));
                        assert_draws_like_direct(rows[r], p, aligns[al], cameras[c][0], cameras[c][1]);
                    }
                }
            }
        }
    }
    assert(g_other_calls_in_builds == 0 && "a row image holds sprite texels only");
}

static layout_row_t *row_with_a_half_pixel_spacing(void) {
    layout_row_keys_t keys = {0};
    layout_row_tuple_t first = {.count = 4,
                                .scalars = {layout_row_scalar_fixed(p8_fixed_from_int(17)), layout_row_scalar_fixed(p8_fixed_from_int(6)),
                                            layout_row_scalar_fixed(0), layout_row_scalar_fixed(P8_FIXED_LIT(0x00008000, 0.5))}};
    layout_row_keys_append(&keys, layout_row_cell_tuple(first));
    layout_row_keys_append(&keys, layout_row_cell_fixed(p8_fixed_from_int(33)));
    return layout_row_new(&heap_allocator, &keys);
}

static void test_rows_it_cannot_copy_exactly_are_drawn_directly_without_a_build(void) {
    start();
    layout_row_t *with_text = planet_row(largest_inputs(), 21, 6, 0);
    layout_row_t *half_pixel = row_with_a_half_pixel_spacing();
    assert(!row_image_cacheable(with_text) && !row_image_cacheable(half_pixel));
    assert_draws_like_direct(with_text, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert_draws_like_direct(with_text, vec_xy(-5, 2), 0, 3, 4);
    assert_draws_like_direct(half_pixel, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert_draws_like_direct(half_pixel, vec_xy(155, 117), 0, 0, 0);
    assert(g_builds == 0);
}

static void test_the_same_content_in_another_row_is_not_rebuilt_and_a_changed_item_is(void) {
    start();
    layout_row_t *first = largest_row();
    layout_row_t *same = largest_row();
    assert(first != same);
    assert_draws_like_direct(first, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 1);
    assert_draws_like_direct(same, vec_xy(30, 90), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 1 && "content-keyed, not keyed by the row object");

    int greyed = -1;
    for (int i = 0; i < same->placed_items_count && greyed < 0; i++) {
        if (same->placed_items[i].kind == LAYOUT_ROW_ITEM_SPRITE && same->placed_items[i].sprite_palette_block == 0) {
            greyed = i;
        }
    }
    assert(greyed >= 0);
    same->placed_items[greyed].sprite_palette_block = p8_fixed_from_int(7);
    assert_draws_like_direct(same, vec_xy(30, 90), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 2);
    assert_draws_like_direct(first, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 2 && "the first content is still held");
}

enum { DISTINCT_ROWS = 45 };

static void test_more_distinct_rows_than_slots_still_draw_like_the_direct_rows(void) {
    static layout_row_t *rows[DISTINCT_ROWS];
    start();
    for (int k = 0; k < DISTINCT_ROWS; k++) {
        rows[k] = planet_row(k % 3 == 0 ? NULL : largest_inputs(), 1 + k, 1 + k % 3, k % 2);
        assert(row_image_cacheable(rows[k]));
    }
    for (int round = 0; round < 2; round++) {
        for (int k = 0; k < DISTINCT_ROWS; k++) {
            assert_draws_like_direct(rows[k], vec_xy(k * 7 % 170 - 20, k * 11 % 130 - 4), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
        }
    }
    assert(g_builds > DISTINCT_ROWS && "the second round rebuilds evicted rows");
}

static void test_a_fill_pattern_in_force_changes_nothing(void) {
    start();
    layout_row_t *row = largest_row();
    p8_fillp(&graphics, 0x5A5A);
    assert_draws_like_direct(row, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    p8_fillp(&graphics, 0xA5A5);
    assert_draws_like_direct(row, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    p8_fillp_reset(&graphics);
    assert_draws_like_direct(row, vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
    assert(g_builds == 1);
}

static void test_a_building_draw_touches_neither_random_stream(void) {
    start();
    uint32_t game = p8_fixed_rnd_state();
    uint32_t visual = visual_random_state();
    assert_draws_like_direct(largest_row(), vec_xy(64, 60), P8_FIXED_LIT(0x00008000, 0.5), 0, 0);
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
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();

    in_fresh_child(test_the_largest_planet_row_is_cached_and_draws_like_the_direct_row);
    in_fresh_child(test_planet_rows_draw_like_the_direct_rows_at_every_edge_and_shake);
    in_fresh_child(test_rows_it_cannot_copy_exactly_are_drawn_directly_without_a_build);
    in_fresh_child(test_the_same_content_in_another_row_is_not_rebuilt_and_a_changed_item_is);
    in_fresh_child(test_more_distinct_rows_than_slots_still_draw_like_the_direct_rows);
    in_fresh_child(test_a_fill_pattern_in_force_changes_nothing);
    in_fresh_child(test_a_building_draw_touches_neither_random_stream);
    printf("test_row_image: all tests passed\n");
    return 0;
}
