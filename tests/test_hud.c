#include "game/hud.h"
#include "p8/sprite_sheet.h"
#include "p8/draw_backend.h"
#include "game/image_slot.h"
#include "game/text_image.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const unsigned char block_8[16] = {0, 1, 2, 131, 4, 5, 6, 7, 10, 9, 0, 4, 2, 13, 0, 0};

static void assert_draw_palette_is_block_8(const p8_vram_t *mem) {
    for (int c = 0; c < 16; c++) {
        assert(mem->draw_palette[c] == block_8[c]);
    }
}

/* The cart encodes block n, color c as sprite-sheet pixel (n, c), color 3
 * with the transparency bit. */
static void test_each_block_matches_the_sprite_sheet_pixels(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);
    p8_sprite_sheet_load(&mem);

    for (int n = 0; n < 16; n++) {
        hud_select_palette_block(&mem, p8_fixed_from_int(n));
        for (int c = 0; c < 16; c++) {
            unsigned char expected = (unsigned char)(p8_sget(&mem, n, c) | (c == 3 ? P8_DRAW_PALETTE_TRANSPARENT_BIT : 0));
            if (mem.draw_palette[c] != expected) {
                fprintf(stderr, "block %d, color %d: %d, expected %d\n", n, c,
                        mem.draw_palette[c], expected);
                assert(0);
            }
        }
    }
}

static void test_select_needs_no_sprite_sheet(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    hud_select_palette_block(&mem, p8_fixed_from_int(8));

    assert_draw_palette_is_block_8(&mem);
}

static void test_fractional_index_selects_its_floor(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    hud_select_palette_block(&mem, P8_FIXED_LIT(0x0008e666, 8.9));

    assert_draw_palette_is_block_8(&mem);
}

static void test_only_block_0_in_force_reads_as_the_default_block(void) {
    p8_vram_t mem;
    p8_vram_init(&mem);

    hud_select_palette_block(&mem, p8_fixed_from_int(8));
    assert(!hud_palette_block_default_in_force(&mem));
    hud_select_palette_block_default(&mem);
    assert(hud_palette_block_default_in_force(&mem));
    mem.draw_palette[15] ^= 1;
    assert(!hud_palette_block_default_in_force(&mem) && "one remapped colour is not block 0");
}

static void test_bar_reaches_the_right_edge(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    hud_draw_bar(&gfx, &mem, p8_fixed_from_int(5), p8_fixed_from_int(6), 7);

    for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
        assert(mem.screen[5][x] == 7 && mem.screen[6][x] == 7);
    }
}

static int screen_equal(const p8_vram_t *a, const p8_vram_t *b) {
    return memcmp(a->screen, b->screen, sizeof a->screen) == 0;
}

static int screen_has_ink(const p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (mem->screen[y][x] != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static void test_alignment_zero_prints_as_the_default(void) {
    p8_vram_t aligned, plain;
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_vram_init(&aligned);
    p8_vram_init(&plain);

    hud_print_outlined(&gfx, &aligned, "ore", p8_fixed_from_int(40), p8_fixed_from_int(30), 7, p8_fixed_from_int(0));
    hud_print_outlined_default(&gfx, &plain, "ore", p8_fixed_from_int(40), p8_fixed_from_int(30), 7);

    assert(screen_has_ink(&plain));
    assert(screen_equal(&aligned, &plain));
}

static void test_half_alignment_shifts_by_two_pixels_per_letter(void) {
    p8_vram_t centered, shifted;
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_vram_init(&centered);
    p8_vram_init(&shifted);

    hud_print_outlined(&gfx, &centered, "ore", p8_fixed_from_int(40), p8_fixed_from_int(30), 7, P8_FIXED_LIT(0x00008000, 0.5));
    hud_print_outlined_default(&gfx, &shifted, "ore", p8_fixed_from_int(34), p8_fixed_from_int(30), 7);

    assert(screen_equal(&centered, &shifted));
}

typedef struct {
    int prints;
    int outlined_prints;
    const char *str;
    int x, y, col;
} outlined_record_t;

static outlined_record_t outlined_record;

static void count_print(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    (void)gfx, (void)mem, (void)str, (void)x, (void)y, (void)col;
    outlined_record.prints++;
}

static void record_print_outlined(const p8_graphics_t *gfx, p8_vram_t *mem, const char *str, int x, int y, int col) {
    (void)gfx, (void)mem;
    outlined_record.outlined_prints++;
    outlined_record.str = str;
    outlined_record.x = x;
    outlined_record.y = y;
    outlined_record.col = col;
}

static void test_outlined_text_is_one_outlined_print_at_the_aligned_position(void) {
    static p8_draw_backend_t recording;
    p8_vram_t mem;
    p8_graphics_t gfx;
    p8_gfx_init(&gfx);
    p8_vram_init(&mem);
    recording = *p8_gfx_software_backend();
    recording.print = count_print;
    recording.print_outlined = record_print_outlined;
    memset(&outlined_record, 0, sizeof outlined_record);
    const p8_draw_backend_t *previous = p8_draw_backend_set(&recording);
    static const char text[] = "ore";

    hud_print_outlined(&gfx, &mem, text, p8_fixed_from_int(40), p8_fixed_from_int(30), 0x1b, P8_FIXED_LIT(0x00008000, 0.5));

    p8_draw_backend_set(previous);
    assert(outlined_record.prints == 0);
    assert(outlined_record.outlined_prints == 1);
    assert(outlined_record.str == text);
    assert(outlined_record.x == 34 && outlined_record.y == 30);
    assert(outlined_record.col == 0x0b);
}

static int g_copies;
static const p8_draw_backend_t *g_software;

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    g_copies++;
    g_software->image_draw(gfx, mem, slot, x, y);
}

static void stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)((x / 3 + y / 5) % 16);
        }
    }
}

static void assert_text_image_draws_like_direct(const char *t, p8_fixed_t x, p8_fixed_t y, int c, p8_fixed_t a) {
    static p8_vram_t direct, cached;
    static const int cameras[][2] = {{0, 0}, {-7, 3}};
    for (size_t k = 0; k < sizeof cameras / sizeof cameras[0]; k++) {
        p8_graphics_t direct_gfx, gfx;
        p8_gfx_init(&gfx);
        p8_camera(&gfx, cameras[k][0], cameras[k][1]);
        direct_gfx = gfx;
        p8_vram_init(&direct);
        stripes(&direct);
        hud_select_palette_block(&direct, p8_fixed_from_int(8));
        cached = direct;
        hud_print_outlined(&direct_gfx, &direct, t, x, y, c, a);
        hud_print_outlined_text_image(&gfx, &cached, t, x, y, c, a);
        assert(gfx.camera_x == cameras[k][0] && gfx.camera_y == cameras[k][1]);
        assert(screen_has_ink(&direct));
        assert(screen_equal(&direct, &cached));
        assert(memcmp(direct.draw_palette, cached.draw_palette, sizeof direct.draw_palette) == 0);
    }
}

static void test_text_image_print_draws_like_the_outlined_print(void) {
    static p8_draw_backend_t counting;
    static const p8_fixed_t as[] = {0, P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00010000, 1.0)};
    static const int colors[] = {7, 9, 0x1b, 0};
    static const char too_long[] = "a string far too long for one text slot";
    char label[16];
    g_software = p8_gfx_software_backend();
    counting = *g_software;
    counting.image_draw = count_image_draw;
    const p8_draw_backend_t *previous = p8_draw_backend_set(&counting);

    for (size_t n = 0; n < sizeof as / sizeof as[0]; n++) {
        for (size_t c = 0; c < sizeof colors / sizeof colors[0]; c++) {
            text_image_frame_begin();
            assert_text_image_draws_like_direct("5 years remain", p8_fixed_from_int(64), p8_fixed_from_int(40), colors[c], as[n]);
            assert_text_image_draws_like_direct("ore", P8_FIXED_LIT(0x00284CCD, 40.3), P8_FIXED_LIT(0x001E6000, 30.375), colors[c], as[n]);
        }
    }
    assert(g_copies == 3 * 4 * 2 * 2 && "each print a copy from its text slot");

    g_copies = 0;
    assert(!text_image_cacheable(too_long));
    assert_text_image_draws_like_direct(too_long, p8_fixed_from_int(2), p8_fixed_from_int(60), 7, 0);
    assert(g_copies == 0);

    text_image_frame_begin();
    for (int k = 0; k < IMAGE_SLOT_TEXT_COUNT; k++) {
        snprintf(label, sizeof label, "ITEM %d", k);
        assert_text_image_draws_like_direct(label, p8_fixed_from_int(10), p8_fixed_from_int(k * 3), 7, 0);
    }
    g_copies = 0;
    assert_text_image_draws_like_direct("every slot used", p8_fixed_from_int(64), p8_fixed_from_int(90), 12, P8_FIXED_LIT(0x00008000, 0.5));
    assert(g_copies == 0 && "every slot was copied this frame: the print goes direct");

    p8_draw_backend_set(previous);
}

int main(void) {
    test_each_block_matches_the_sprite_sheet_pixels();
    test_select_needs_no_sprite_sheet();
    test_fractional_index_selects_its_floor();
    test_only_block_0_in_force_reads_as_the_default_block();
    test_bar_reaches_the_right_edge();
    test_alignment_zero_prints_as_the_default();
    test_half_alignment_shifts_by_two_pixels_per_letter();
    test_outlined_text_is_one_outlined_print_at_the_aligned_position();
    test_text_image_print_draws_like_the_outlined_print();

    printf("test_hud: OK\n");
    return 0;
}
