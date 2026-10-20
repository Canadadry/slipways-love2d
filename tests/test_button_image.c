#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "game/button_image.h"
#include "game/image_slot.h"
#include "game/visual_random.h"
#include "p8/draw_backend.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/vram.h"

static p8_vram_t memory;
static p8_graphics_t graphics;

static int g_builds;
static int g_copies;
static int g_rebuilt_after_copy;
static int g_copied[P8_IMAGE_SLOT_COUNT];
static const p8_draw_backend_t *g_software;

static void count_image_begin(int slot, int w, int h) {
    assert(slot >= IMAGE_SLOT_BUTTON_ICON_FIRST && slot < IMAGE_SLOT_BUTTON_WIDE_FIRST + IMAGE_SLOT_BUTTON_WIDE_COUNT
           && "a button slot");
    g_builds++;
    g_rebuilt_after_copy += g_copied[slot];
    g_software->image_begin(slot, w, h);
}

static void count_image_draw(const p8_graphics_t *gfx, p8_vram_t *mem, int slot, int x, int y) {
    g_copies++;
    g_copied[slot] = 1;
    g_software->image_draw(gfx, mem, slot, x, y);
}

static p8_draw_backend_t counting;

typedef struct {
    int w;
    int h;
    int seed;
} picture_t;

/* Leaves one pixel in three unwritten, so the copy's transparency shows. */
static void draw_picture(p8_graphics_t *gfx, p8_vram_t *mem, void *context) {
    const picture_t *picture = context;
    for (int y = 0; y < picture->h; y++) {
        for (int x = 0; x < picture->w; x++) {
            if ((x + y + picture->seed) % 3 != 0) {
                p8_pset(gfx, mem, x, y, (x * y + picture->seed) % 16);
            }
        }
    }
}

static void new_frame(void) {
    button_image_frame_begin();
    memset(g_copied, 0, sizeof g_copied);
}

static void start(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    g_software = p8_draw_backend_get();
    counting = *g_software;
    counting.image_begin = count_image_begin;
    counting.image_draw = count_image_draw;
    p8_draw_backend_set(&counting);
    g_builds = 0;
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

static button_image_key_t key_of(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    button_image_key_t key = {{a, b, c, d}};
    return key;
}

/* Draws key's image at (x, y) and, on a copy of the screen, the picture itself shifted there; returns
 * what button_image_draw returned, after checking the two screens (or, on 0, that nothing moved). */
static int assert_draws_like_direct(button_image_key_t key, picture_t picture, int x, int y, int camera_x,
                                    int camera_y) {
    static p8_vram_t direct;
    static p8_vram_t before;
    p8_graphics_t direct_gfx;
    p8_camera(&graphics, camera_x, camera_y);
    stripes(&memory);
    direct = memory;
    before = memory;
    direct_gfx = graphics;
    p8_camera(&direct_gfx, camera_x - x, camera_y - y);
    p8_draw_backend_set(g_software);
    draw_picture(&direct_gfx, &direct, &picture);
    p8_draw_backend_set(&counting);

    int drawn = button_image_draw(&graphics, &memory, &key, x, y, picture.w, picture.h, draw_picture, &picture);

    assert(graphics.camera_x == camera_x && graphics.camera_y == camera_y && "the camera is back");
    const p8_vram_t *want = drawn ? &direct : &before;
    if (memcmp(want->screen, memory.screen, sizeof memory.screen) != 0) {
        fprintf(stderr, "screens differ: %dx%d at (%d, %d) camera (%d, %d), drawn %d\n", picture.w, picture.h, x, y,
                camera_x, camera_y, drawn);
        assert(0);
    }
    return drawn;
}

static void test_a_key_is_built_once_then_copied_frame_after_frame(void) {
    picture_t wide = {100, 11, 1};
    start();
    assert(assert_draws_like_direct(key_of(1, 0, 13, 6), wide, 20, 30, 0, 0));
    assert(g_builds == 1 && g_copies == 1);
    for (int frame = 0; frame < 3; frame++) {
        new_frame();
        assert(assert_draws_like_direct(key_of(1, 0, 13, 6), wide, 20 + frame, 31, 0, 0));
    }
    assert(g_builds == 1 && g_copies == 4 && "every later frame copies the image");
}

static void test_images_draw_like_the_picture_at_every_edge_and_camera(void) {
    static const int xs[] = {-120, -111, -10, -1, 0, 1, 37, 100, 120, 127, 128, 140};
    static const int ys[] = {-11, -10, -1, 0, 1, 60, 117, 118, 127, 128};
    static const int cameras[][2] = {{0, 0}, {-13, 21}, {218, 276}};
    static const picture_t pictures[] = {{11, 11, 0}, {16, 11, 2}, {17, 11, 1}, {63, 11, 2}, {112, 11, 0}};
    start();
    for (size_t p = 0; p < sizeof pictures / sizeof pictures[0]; p++) {
        for (size_t c = 0; c < sizeof cameras / sizeof cameras[0]; c++) {
            for (size_t i = 0; i < sizeof xs / sizeof xs[0]; i++) {
                for (size_t j = 0; j < sizeof ys / sizeof ys[0]; j++) {
                    assert(assert_draws_like_direct(key_of((uint32_t)p + 1, 0, 0, 0), pictures[p],
                                                    xs[i] + cameras[c][0], ys[j] + cameras[c][1], cameras[c][0],
                                                    cameras[c][1]));
                }
            }
        }
    }
    assert(g_builds == 5);
}

static void test_equal_keys_share_a_slot_and_a_changed_word_rebuilds(void) {
    picture_t picture = {40, 11, 0};
    start();
    assert(assert_draws_like_direct(key_of(7, 8, 13, 6), picture, 10, 10, 0, 0));
    assert(assert_draws_like_direct(key_of(7, 8, 13, 6), picture, 50, 70, 0, 0));
    assert(g_builds == 1 && "keyed by the key, not the call");
    for (int word = 0; word < 4; word++) {
        button_image_key_t changed = key_of(7, 8, 13, 6);
        changed.words[word]++;
        picture.seed = word + 1;
        assert(assert_draws_like_direct(changed, picture, 10, 10, 0, 0));
        assert(g_builds == 2 + word && "every word is part of the key");
    }
}

static void test_the_icon_and_wide_pools_never_evict_each_other(void) {
    start();
    for (uint32_t k = 0; k < IMAGE_SLOT_BUTTON_ICON_COUNT; k++) {
        picture_t icon = {11, 11, (int)k};
        assert(assert_draws_like_direct(key_of(100 + k, 0, 13, 6), icon, 0, 0, 0, 0));
    }
    for (int frame = 0; frame < 3; frame++) {
        new_frame();
        for (uint32_t k = 0; k < IMAGE_SLOT_BUTTON_WIDE_COUNT; k++) {
            picture_t wide = {17 + (int)k, 11, (int)k};
            assert(assert_draws_like_direct(key_of(200 + k + (uint32_t)frame * 100, 0, 13, 6), wide, 5, 40, 0, 0));
        }
    }
    int builds = g_builds;
    new_frame();
    for (uint32_t k = 0; k < IMAGE_SLOT_BUTTON_ICON_COUNT; k++) {
        picture_t icon = {11, 11, (int)k};
        assert(assert_draws_like_direct(key_of(100 + k, 0, 13, 6), icon, 3, 4, 0, 0));
    }
    assert(g_builds == builds && "the wide images left every icon image in place");
}

static void test_more_keys_than_slots_draw_nothing_past_the_pool_in_one_frame(void) {
    enum { MORE = IMAGE_SLOT_BUTTON_WIDE_COUNT + 3 };
    int drawn = 0;
    start();
    for (uint32_t k = 0; k < MORE; k++) {
        picture_t wide = {30, 11, (int)k};
        drawn += assert_draws_like_direct(key_of(k + 1, 0, 0, 0), wide, (int)k * 7, (int)k * 9, 0, 0);
    }
    assert(drawn == IMAGE_SLOT_BUTTON_WIDE_COUNT && g_builds == IMAGE_SLOT_BUTTON_WIDE_COUNT);
    assert(g_rebuilt_after_copy == 0);

    new_frame();
    for (uint32_t k = MORE; k-- > 0;) {
        picture_t wide = {30, 11, (int)k};
        (void)assert_draws_like_direct(key_of(k + 1, 0, 0, 0), wide, (int)k * 5, (int)k * 3, 0, 0);
    }
    assert(g_rebuilt_after_copy == 0);
    assert(g_builds > IMAGE_SLOT_BUTTON_WIDE_COUNT && "the next frame evicts what it needs");
}

static void test_an_image_over_the_largest_slot_draws_nothing(void) {
    picture_t too_wide = {BUTTON_IMAGE_WIDE_WIDTH + 1, 11, 0};
    picture_t too_high = {40, BUTTON_IMAGE_HEIGHT + 1, 0};
    start();
    assert(!assert_draws_like_direct(key_of(1, 0, 0, 0), too_wide, 0, 0, 0, 0));
    assert(!assert_draws_like_direct(key_of(2, 0, 0, 0), too_high, 0, 0, 0, 0));
    assert(g_builds == 0 && g_copies == 0);
}

static void test_a_build_touches_neither_random_stream(void) {
    picture_t picture = {50, 11, 0};
    start();
    uint32_t game = p8_fixed_rnd_state();
    uint32_t visual = visual_random_state();
    assert(assert_draws_like_direct(key_of(1, 2, 3, 4), picture, 64, 60, 0, 0));
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
    in_fresh_child(test_a_key_is_built_once_then_copied_frame_after_frame);
    in_fresh_child(test_images_draw_like_the_picture_at_every_edge_and_camera);
    in_fresh_child(test_equal_keys_share_a_slot_and_a_changed_word_rebuilds);
    in_fresh_child(test_the_icon_and_wide_pools_never_evict_each_other);
    in_fresh_child(test_more_keys_than_slots_draw_nothing_past_the_pool_in_one_frame);
    in_fresh_child(test_an_image_over_the_largest_slot_draws_nothing);
    in_fresh_child(test_a_build_touches_neither_random_stream);
    printf("test_button_image: all tests passed\n");
    return 0;
}
