#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "p8/draw_backend.h"
#include "p8/gfx.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>


#define BACKGROUND_STAR_CELL_X 2
#define BACKGROUND_STAR_CELL_Y 1
#define BACKGROUND_STAR_SPRITE 72

static unsigned char screen_at(const p8_vram_t *mem, int x, int y) {
    return mem->screen[y][x];
}

static void setup_sky(p8_vram_t *mem, p8_graphics_t *gfx) {
    p8_vram_init(mem);
    p8_gfx_init(gfx);
    p8_sprite_sheet_load(mem);
    p8_mset(mem, BACKGROUND_STAR_CELL_X, BACKGROUND_STAR_CELL_Y, BACKGROUND_STAR_SPRITE);
}

static void test_background_draw_leaves_empty_map_cells_black(void) {
    static const int probe_cells[][2] = {{1, 1}, {6, 2}, {2, 5}, {13, 6}, {10, 10},
                                         {P8_SCREEN_WIDTH / P8_SPRITE_SIZE - 1, P8_SCREEN_HEIGHT / P8_SPRITE_SIZE - 1}};
    const int probe_count = (int)(sizeof(probe_cells) / sizeof(probe_cells[0]));
    p8_vram_t mem;
    p8_graphics_t gfx;

    setup_sky(&mem, &gfx);

    background_draw(&gfx, &mem, NULL);

    for (int i = 0; i < probe_count; i++) {
        int cx = probe_cells[i][0];
        int cy = probe_cells[i][1];
        assert(cx < P8_SCREEN_WIDTH / P8_SPRITE_SIZE && cy < P8_SCREEN_HEIGHT / P8_SPRITE_SIZE);
        for (int dy = 0; dy < P8_SPRITE_SIZE; dy++) {
            for (int dx = 0; dx < P8_SPRITE_SIZE; dx++) {
                assert(screen_at(&mem, cx * P8_SPRITE_SIZE + dx, cy * P8_SPRITE_SIZE + dy) == 0);
            }
        }
    }
}

static void test_background_draw_still_draws_non_empty_map_cells(void) {
    p8_vram_t mem;
    p8_graphics_t gfx;
    int star_x = BACKGROUND_STAR_CELL_X * P8_SPRITE_SIZE;
    int star_y = BACKGROUND_STAR_CELL_Y * P8_SPRITE_SIZE;
    int lit = 0;

    setup_sky(&mem, &gfx);

    background_draw(&gfx, &mem, NULL);

    for (int dy = 0; dy < P8_SPRITE_SIZE; dy++) {
        for (int dx = 0; dx < P8_SPRITE_SIZE; dx++) {
            unsigned char expected = p8_sget(&mem, (BACKGROUND_STAR_SPRITE % 16) * P8_SPRITE_SIZE + dx,
                                                (BACKGROUND_STAR_SPRITE / 16) * P8_SPRITE_SIZE + dy);
            assert(screen_at(&mem, star_x + dx, star_y + dy) == expected);
            if (expected != 0) {
                lit++;
            }
        }
    }
    assert(lit == 1);
}

static int builds;
static p8_draw_backend_t counting_backend;

static void count_image_begin(int slot, int w, int h) {
    builds++;
    p8_gfx_software_backend()->image_begin(slot, w, h);
}

static const p8_draw_backend_t *start_counting_builds(void) {
    counting_backend = *p8_gfx_software_backend();
    counting_backend.image_begin = count_image_begin;
    builds = 0;
    return p8_draw_backend_set(&counting_backend);
}

static void test_background_draw_builds_the_map_once_until_it_changes(void) {
    static p8_vram_t mem;
    p8_graphics_t gfx;
    const p8_draw_backend_t *previous;

    setup_sky(&mem, &gfx);
    previous = start_counting_builds();

    background_draw(&gfx, &mem, NULL);
    assert(builds == 1 && "the first draw builds the map");
    background_draw(&gfx, &mem, NULL);
    p8_camera(&gfx, 40, 30);
    background_draw(&gfx, &mem, NULL);
    assert(builds == 1 && "a camera move only copies the image");

    p8_mset(&mem, 5, 5, BACKGROUND_STAR_SPRITE);
    background_draw(&gfx, &mem, NULL);
    assert(builds == 2 && "a map write rebuilds");

    p8_palt(&mem, 3, 1);
    background_draw(&gfx, &mem, NULL);
    assert(builds == 3 && "a draw palette change rebuilds");
    background_draw(&gfx, &mem, NULL);
    assert(builds == 3);

    p8_draw_backend_set(previous);
}

static void draw_background_the_old_way(p8_graphics_t *gfx, p8_vram_t *mem) {
    int xy;
    p8_map(gfx, mem, 0, 0, 0, 0, 32, 32);
    p8_fillp(gfx, 0xA5A5u);
    for (xy = 3; xy <= BACKGROUND_GRID_EDGE; xy += 32) {
        p8_rectfill(gfx, mem, xy, 0, xy, BACKGROUND_GRID_EDGE, 1);
        p8_rectfill(gfx, mem, 0, xy, BACKGROUND_GRID_EDGE, xy, 1);
    }
    p8_fillp_reset(gfx);
    p8_rect(gfx, mem, 0, 0, BACKGROUND_GRID_EDGE, BACKGROUND_GRID_EDGE, 5);
}

static void test_background_draw_matches_the_map_drawn_directly(void) {
    static const int cameras[][2] = {{0, 0}, {-80, -60}, {37, 21}, {48, 68}, {96, 136}, {-7, 3}, {120, 200}};
    static p8_vram_t direct, cached;
    p8_graphics_t gfx;

    setup_sky(&direct, &gfx);
    for (int y = 0; y < P8_MAP_HEIGHT; y++) {
        for (int x = 0; x < P8_MAP_WIDTH; x++) {
            p8_mset(&direct, x, y, (x * 7 + y * 13) % 5 == 0 ? 72 + (x + y) % 6 : 0);
        }
    }
    cached = direct;
    cached.map_version = p8_vram_next_map_version();

    for (size_t i = 0; i < sizeof cameras / sizeof cameras[0]; i++) {
        p8_camera(&gfx, cameras[i][0], cameras[i][1]);
        memset(direct.screen, 0, sizeof direct.screen);
        memset(cached.screen, 0, sizeof cached.screen);
        draw_background_the_old_way(&gfx, &direct);
        background_draw(&gfx, &cached, NULL);
        assert(memcmp(direct.screen, cached.screen, sizeof direct.screen) == 0 && "same screen as the direct draw");
    }
}

int main(void) {
    test_background_draw_leaves_empty_map_cells_black();
    test_background_draw_still_draws_non_empty_map_cells();
    test_background_draw_builds_the_map_once_until_it_changes();
    test_background_draw_matches_the_map_drawn_directly();

    printf("test_background_draw: all tests passed\n");
    return 0;
}
