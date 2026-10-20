#include "p8/gfx.h"
#include "p8/image.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#define STDERR_LOG "tmp/test_image.stderr"

static void sheet_with_patterns(p8_vram_t *mem) {
    for (int y = 0; y < P8_SPRITE_SHEET_HEIGHT; y++) {
        for (int x = 0; x < P8_SPRITE_SHEET_WIDTH; x++) {
            mem->sprite_sheet[y][x] = (unsigned char)((x * 7 + y * 3 + (x ^ y)) & 0x0f);
        }
    }
}

static void map_with_holes(p8_vram_t *mem) {
    for (int y = 0; y < P8_MAP_HEIGHT; y++) {
        for (int x = 0; x < P8_MAP_WIDTH; x++) {
            int cell = (x * 5 + y * 11) % 23;
            p8_mset(mem, x, y, cell < 4 ? 0 : cell);
        }
    }
}

static void draw_map(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    int *calls = context;
    if (calls != NULL) (*calls)++;
    p8_map(gfx, mem, 0, 0, 0, 0, P8_MAP_WIDTH, P8_MAP_HEIGHT);
}

static void fill_screen(p8_vram_t *mem, int col) {
    memset(mem->screen, col, sizeof mem->screen);
}

static const int CAMERAS[][2] = {
    {0, 0}, {-153, -113}, {-7, -7}, {3, 5}, {97, 137}, {251, 249}, {-160, 120}, {96, 136}, {48, 68}, {255, 255}, {300, -40},
};

static void test_a_built_map_copies_like_the_map_drawn_directly(void) {
    static p8_vram_t direct, cached;
    p8_graphics_t gfx;
    p8_vram_init(&direct);
    sheet_with_patterns(&direct);
    map_with_holes(&direct);
    p8_palt(&direct, 3, 1);
    cached = direct;
    p8_gfx_init(&gfx);

    p8_image_build(&gfx, &cached, 0, 256, 256, draw_map, NULL);

    for (size_t i = 0; i < sizeof CAMERAS / sizeof CAMERAS[0]; i++) {
        p8_camera(&gfx, CAMERAS[i][0], CAMERAS[i][1]);
        fill_screen(&direct, 5);
        fill_screen(&cached, 5);
        p8_map(&gfx, &direct, 0, 0, 0, 0, P8_MAP_WIDTH, P8_MAP_HEIGHT);
        p8_image_draw(&gfx, &cached, 0, 0, 0);
        assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "the copy matches p8_map");
    }
}

typedef struct {
    int calls;
    int cameras[8][2];
} window_log_t;

static void log_window(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)mem;
    window_log_t *log = context;
    assert(log->calls < 8);
    log->cameras[log->calls][0] = gfx->camera_x;
    log->cameras[log->calls][1] = gfx->camera_y;
    log->calls++;
}

static void test_a_full_build_runs_six_windows_row_major_and_restores_the_camera(void) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_camera(&gfx, -12, 34);
    window_log_t log = {0};

    p8_image_build(&gfx, &mem, 0, 256, 256, log_window, &log);

    static const int expected[6][2] = {{0, 0}, {160, 0}, {0, 120}, {160, 120}, {0, 240}, {160, 240}};
    assert(log.calls == 6);
    for (int i = 0; i < 6; i++) {
        assert(log.cameras[i][0] == expected[i][0] && log.cameras[i][1] == expected[i][1]);
    }
    assert(gfx.camera_x == -12 && gfx.camera_y == 34 && "the caller's camera is back");
}

static void fill_everything(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_rectfill(gfx, mem, -1000, -1000, 1000, 1000, 9);
}

static void test_a_partial_image_clips_at_its_edge_and_leaves_the_rest_transparent(void) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);

    p8_image_build(&gfx, &mem, 0, 170, 130, fill_everything, NULL);
    fill_screen(&mem, 5);
    p8_camera(&gfx, 100, 100);
    p8_image_draw(&gfx, &mem, 0, 0, 0);

    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            int inside = x + 100 < 170 && y + 100 < 130;
            assert(mem.screen[y][x] == (inside ? 9 : 5));
        }
    }
}

static void test_empty_cells_and_transparent_colours_show_what_was_underneath(void) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    memset(mem.sprite_sheet, 0, sizeof mem.sprite_sheet);
    mem.sprite_sheet[8][9] = 7;
    p8_mset(&mem, 0, 0, 17);

    p8_image_build(&gfx, &mem, 0, 256, 256, draw_map, NULL);
    fill_screen(&mem, 5);
    p8_image_draw(&gfx, &mem, 0, 0, 0);

    assert(mem.screen[0][1] == 7);
    assert(mem.screen[0][0] == 5 && "colour 0 is transparent");
    assert(mem.screen[20][20] == 5 && "an empty cell");
}

static void outlined_labels(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_print_outlined(gfx, mem, "ab9", 156, 118, 7);
    p8_print_outlined(gfx, mem, "x y", 1, 1, 12);
}

static void test_an_outlined_print_in_a_build_copies_like_the_print_drawn_directly(void) {
    static p8_vram_t direct, cached;
    p8_graphics_t gfx;
    p8_vram_init(&direct);
    cached = direct;
    p8_gfx_init(&gfx);

    p8_image_build(&gfx, &cached, 0, 256, 256, outlined_labels, NULL);

    for (size_t i = 0; i < sizeof CAMERAS / sizeof CAMERAS[0]; i++) {
        p8_camera(&gfx, CAMERAS[i][0], CAMERAS[i][1]);
        fill_screen(&direct, 5);
        fill_screen(&cached, 5);
        outlined_labels(&gfx, &direct, NULL);
        p8_image_draw(&gfx, &cached, 0, 0, 0);
        assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "the copy matches the outlined prints");
    }
}

typedef struct {
    int x, y;
} origin_t;

static const int ROW_SPRITES[][3] = { {17, 0, 0}, {33, 1, 6}, {42, 0, 9}, {5, 1, 18}, {60, 1, 21}, {77, 0, 36} };

static void use_palette(p8_vram_t *mem, int variant) {
    for (int col = 0; col < 16; col++) {
        mem->draw_palette[col] = (unsigned char)(variant ? (col * 5 + 3) % 16 : col);
    }
    p8_palt(mem, 0, !variant);
    p8_palt(mem, 9, variant);
}

static void draw_sprite_row(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    const origin_t *origin = context;
    for (size_t i = 0; i < sizeof ROW_SPRITES / sizeof ROW_SPRITES[0]; i++) {
        use_palette(mem, ROW_SPRITES[i][1]);
        p8_spr_default(gfx, mem, ROW_SPRITES[i][0], origin->x + ROW_SPRITES[i][2], origin->y);
        use_palette(mem, 0);
    }
}

static void stripes(p8_vram_t *mem) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)(1 + (x + y) % 15);
        }
    }
}

static void test_a_48_by_8_slot_of_overlapping_sprites_copies_like_the_sprites_drawn_directly(void) {
    static p8_vram_t direct, cached;
    static const int xs[] = {-44, -43, -20, -1, 0, 58, 116, 117, 140, 159, 160};
    static const int ys[] = {-8, -7, -3, 0, 60, 112, 113, 117, 120};
    static const int slots[] = {1, 32};
    p8_graphics_t gfx;
    p8_vram_init(&direct);
    sheet_with_patterns(&direct);
    use_palette(&direct, 0);
    cached = direct;
    p8_gfx_init(&gfx);
    origin_t image_origin = {0, 0};

    for (size_t s = 0; s < sizeof slots / sizeof slots[0]; s++) {
        assert(p8_image_size(slots[s]).width == 48 && p8_image_size(slots[s]).height == 8);
        p8_image_build(&gfx, &cached, slots[s], 44, 8, draw_sprite_row, &image_origin);
        for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
            for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                origin_t at = {xs[i] + 7, ys[j] - 5};
                p8_camera(&gfx, 7, -5);
                stripes(&direct);
                stripes(&cached);
                draw_sprite_row(&gfx, &direct, &at);
                p8_image_draw(&gfx, &cached, slots[s], at.x, at.y);
                assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "the copy matches the sprites");
            }
        }
        p8_camera(&gfx, 0, 0);
    }
}

enum { FIRST_SQUARE_SLOT = 1 + 32 + 32, SQUARE_SLOT_COUNT = 5, FIRST_BUTTON_SLOT = FIRST_SQUARE_SLOT + SQUARE_SLOT_COUNT };

static void test_the_slots_after_the_48_by_8_ones_are_128_by_8(void) {
    assert(P8_IMAGE_SLOT_COUNT == FIRST_BUTTON_SLOT + 8 + 10);
    for (int slot = 1 + 32; slot < FIRST_SQUARE_SLOT; slot++) {
        assert(p8_image_size(slot).width == 128 && p8_image_size(slot).height == 8);
    }
}

static void test_the_button_slots_follow_the_square_ones_eight_16_by_11_then_ten_112_by_11(void) {
    for (int k = 0; k < 8 + 10; k++) {
        p8_image_size_t size = p8_image_size(FIRST_BUTTON_SLOT + k);
        assert(size.width == (k < 8 ? 16 : 112) && size.height == 11);
    }
}

typedef struct {
    int filled;
    int col;
} circle_t;

static void draw_circle(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    const circle_t *circle = context;
    if (circle->filled) {
        p8_circfill(gfx, mem, 7, 7, 7, circle->col);
    } else {
        p8_circ(gfx, mem, 6, 6, 6, circle->col);
    }
}

static void test_the_16_by_16_slots_hold_circles_that_copy_like_the_circles_drawn_directly(void) {
    static p8_vram_t direct, cached;
    static const circle_t circles[SQUARE_SLOT_COUNT] = {{1, 0}, {0, 2}, {0, 3}, {0, 9}, {0, 12}};
    static const int xs[] = {-8, -7, -1, 60, 153, 159, 160};
    static const int ys[] = {-8, -7, -1, 50, 113, 119, 120};
    int count = (int)(sizeof xs / sizeof xs[0]);
    p8_graphics_t gfx;
    p8_vram_init(&direct);
    sheet_with_patterns(&direct);
    map_with_holes(&direct);
    cached = direct;
    p8_gfx_init(&gfx);

    p8_image_build(&gfx, &cached, 0, 256, 256, draw_map, NULL);
    for (int c = 0; c < SQUARE_SLOT_COUNT; c++) {
        int side = circles[c].filled ? 15 : 13;
        int slot = FIRST_SQUARE_SLOT + c;
        assert(p8_image_size(slot).width == 16 && p8_image_size(slot).height == 16);
        p8_image_build(&gfx, &cached, slot, side, side, draw_circle, (void *)&circles[c]);
    }

    for (int c = 0; c < SQUARE_SLOT_COUNT; c++) {
        int radius = circles[c].filled ? 7 : 6;
        for (int i = 0; i < count; i++) {
            for (int j = 0; j < count; j++) {
                int x = xs[i];
                int y = ys[j];
                p8_camera(&gfx, 0, 0);
                stripes(&direct);
                stripes(&cached);
                if (circles[c].filled) {
                    p8_circfill(&gfx, &direct, x, y, radius, circles[c].col);
                } else {
                    p8_circ(&gfx, &direct, x, y, radius, circles[c].col);
                }
                p8_image_draw(&gfx, &cached, FIRST_SQUARE_SLOT + c, x - radius, y - radius);
                assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "the copy matches the circle");
            }
        }
    }
    p8_camera(&gfx, 0, 0);
    fill_screen(&direct, 5);
    fill_screen(&cached, 5);
    p8_map(&gfx, &direct, 0, 0, 0, 0, P8_MAP_WIDTH, P8_MAP_HEIGHT);
    p8_image_draw(&gfx, &cached, 0, 0, 0);
    assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "the small builds left the map's slot alone");
}

static void test_every_map_write_on_any_vram_gives_a_map_version_never_seen(void) {
    static p8_vram_t first, second;
    p8_vram_init(&first);
    p8_vram_init(&second);
    unsigned long seen[4] = {first.map_version, second.map_version};
    assert(seen[0] != seen[1] && "two fresh vrams");

    p8_mset(&first, 3, 4, 17);
    seen[2] = first.map_version;
    p8_mset(&second, 3, 4, 17);
    seen[3] = second.map_version;
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) assert(seen[i] != seen[j]);
    }

    unsigned long before = first.map_version;
    p8_palt(&first, 5, 1);
    p8_mset(&first, -1, 0, 9);
    assert(first.map_version == before && "only a map write changes it");
}

static void clear_screen(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)gfx, (void)context;
    p8_cls(mem);
}

static void build_again(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_image_build(gfx, mem, 0, 16, 16, fill_everything, NULL);
}

static void copy_inside(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    (void)context;
    p8_image_draw(gfx, mem, 0, 0, 0);
}

static int case_build(int w, int h, p8_image_draw_t draw) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_image_build(&gfx, &mem, 0, w, h, draw, NULL);
    return 0;
}

static int case_never_built(void) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_image_draw(&gfx, &mem, 0, 0, 0);
    return 0;
}

static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    char *text = calloc(1, 4096);
    assert(text != NULL);
    size_t read = fread(text, 1, 4095, file);
    (void)read;
    fclose(file);
    return text;
}

static void expect_fatal(const char *self_path, const char *name, const char *message) {
    char cmd[512];
    remove(STDERR_LOG);
    snprintf(cmd, sizeof cmd, "%s %s > /dev/null 2> " STDERR_LOG, self_path, name);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    char *err = read_whole_file(STDERR_LOG);
    if (WEXITSTATUS(status) != 3 || strstr(err, message) == NULL) {
        fprintf(stderr, "%s: expected exit 3 and \"%s\", exit %d\n%s", name, message, WEXITSTATUS(status), err);
        assert(0 && "a broken image contract must call p8_fatal");
    }
    free(err);
    remove(STDERR_LOG);
}

int main(int argc, char **argv) {
    if (argc == 2) {
        if (strcmp(argv[1], "cls") == 0) return case_build(16, 16, clear_screen);
        if (strcmp(argv[1], "nested") == 0) return case_build(16, 16, build_again);
        if (strcmp(argv[1], "copy") == 0) return case_build(16, 16, copy_inside);
        if (strcmp(argv[1], "wide") == 0) return case_build(257, 16, fill_everything);
        if (strcmp(argv[1], "tall") == 0) return case_build(16, 257, fill_everything);
        if (strcmp(argv[1], "never-built") == 0) return case_never_built();
        fprintf(stderr, "test_image: unknown case %s\n", argv[1]);
        return 2;
    }
    assert(p8_image_size(0).width == 256 && p8_image_size(0).height == 256);
    test_a_built_map_copies_like_the_map_drawn_directly();
    test_a_full_build_runs_six_windows_row_major_and_restores_the_camera();
    test_a_partial_image_clips_at_its_edge_and_leaves_the_rest_transparent();
    test_empty_cells_and_transparent_colours_show_what_was_underneath();
    test_an_outlined_print_in_a_build_copies_like_the_print_drawn_directly();
    test_a_48_by_8_slot_of_overlapping_sprites_copies_like_the_sprites_drawn_directly();
    test_the_slots_after_the_48_by_8_ones_are_128_by_8();
    test_the_button_slots_follow_the_square_ones_eight_16_by_11_then_ten_112_by_11();
    test_the_16_by_16_slots_hold_circles_that_copy_like_the_circles_drawn_directly();
    test_every_map_write_on_any_vram_gives_a_map_version_never_seen();
    expect_fatal(argv[0], "cls", "image slot 0: cls during a build");
    expect_fatal(argv[0], "nested", "image slot 0: build inside a build");
    expect_fatal(argv[0], "copy", "image slot 0: draw during a build");
    expect_fatal(argv[0], "wide", "image slot 0: 257x16 over its 256x256");
    expect_fatal(argv[0], "tall", "image slot 0: 16x257 over its 256x256");
    expect_fatal(argv[0], "never-built", "image slot 0: drawn before any build");
    printf("test_image: ok\n");
    return 0;
}
