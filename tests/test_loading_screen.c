#include "game/loading_screen.h"

#include "engine/dispatch_context.h"
#include "game/bindings.h"
#include "game/cart.h"
#include "game/economy_score.h"
#include "game/input_step.h"
#include "game/main_economy.h"
#include "p8/sprite_sheet.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { TITLE_FRAMES_BEFORE_THE_PRESS = 12 };

#define CROSS P8_PAD_BUTTON_5

static p8_vram_t memory;
static p8_graphics_t graphics;
static input_step_t step;

typedef unsigned char screen_t[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];

static void play_update(int pad) {
    p8_input_frame_t raw = { 0, 0, 0, pad };
    input_step_run(&step, &raw);
    cart_update60();
}

static void play_frame(int pad) {
    play_update(pad);
    cart_draw();
}

/* Black, then "loading" in colour 7 with its last lit pixel one pixel off the right and bottom edges. */
static void build_expected_picture(screen_t expected) {
    static p8_vram_t blank;
    p8_graphics_t plain;

    p8_vram_init(&blank);
    p8_gfx_init(&plain);
    p8_cls(&blank);
    p8_print(&plain, &blank, "loading", 132, 114, 7);
    memcpy(expected, blank.screen, sizeof blank.screen);
}

static int screen_equals(const screen_t a, const screen_t b) {
    return memcmp(a, b, sizeof(screen_t)) == 0;
}

static void test_the_two_frames_after_a_press_show_the_loading_picture_then_the_map(void) {
    static screen_t expected;
    static screen_t press_frame;
    static screen_t second_frame;
    uint32_t rnd_after_the_press;
    int lit = 0;

    build_expected_picture(expected);
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            lit += expected[y][x] != 0;
        }
    }
    assert(lit > 0 && "the expected picture must hold the word");

    for (int i = 0; i < TITLE_FRAMES_BEFORE_THE_PRESS; i++) play_frame(0);
    assert(cart_title_shown());

    play_update(CROSS);
    rnd_after_the_press = p8_fixed_rnd_state();
    assert(main_economy_typed() == NULL && "the press frame must not generate the galaxy");
    assert(cart_loading());
    assert(!cart_title_shown() && "the attract counter must not take the loading screen for an idle title");
    cart_draw();
    memcpy(press_frame, memory.screen, sizeof press_frame);
    assert(screen_equals(press_frame, expected) && "the press frame draws the loading picture");

    play_update(CROSS | P8_PAD_BUTTON_1 | P8_PAD_BUTTON_6);
    assert(main_economy_typed() == NULL && "the second loading frame must not generate the galaxy");
    assert(cart_loading());
    assert(!cart_title_shown());
    assert(p8_fixed_rnd_state() == rnd_after_the_press && "a loading frame runs no game code");
    cart_draw();
    memcpy(second_frame, memory.screen, sizeof second_frame);
    assert(screen_equals(second_frame, expected) && "the second loading frame draws the same picture");
    assert(p8_fixed_rnd_state() == rnd_after_the_press);

    /* Two loading frames, not one: every e2e hash depends on the count. */
    assert(LOADING_SCREEN_FRAMES == 2);
    play_update(CROSS);
    assert(!cart_loading());
    assert(main_economy_typed() != NULL && "the third frame's update generates the galaxy");
    assert(economy_score_difficulty() != NULL && strcmp(economy_score_difficulty()->name, "forgiving") == 0);
    cart_draw();
    assert(!screen_equals((const unsigned char (*)[P8_SCREEN_WIDTH])memory.screen, expected) &&
           "the generation frame draws the new map");
}

static void test_the_loading_picture_leaves_the_draw_state_as_it_found_it(void) {
    static p8_vram_t drawn;
    static screen_t expected;
    p8_graphics_t state;
    unsigned char palette[16];

    p8_vram_init(&drawn);
    p8_gfx_init(&state);
    p8_camera(&state, 17, -5);
    p8_fillp(&state, 0x5a5a);
    state.pen_color = 9;
    p8_palt(&drawn, 7, 1);
    p8_palt(&drawn, 0, 0);
    memcpy(palette, drawn.draw_palette, sizeof palette);
    p8_graphics_t before = state;

    loading_screen_draw(&state, &drawn);
    build_expected_picture(expected);
    assert(screen_equals((const unsigned char (*)[P8_SCREEN_WIDTH])drawn.screen, expected) &&
           "the camera, the fill pattern and the palette must not reach the loading picture");
    assert(memcmp(&state, &before, sizeof state) == 0);
    assert(memcmp(drawn.draw_palette, palette, sizeof palette) == 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    cart_load();
    cart_init();
    input_step_init(&step, &bindings_default);

    test_the_loading_picture_leaves_the_draw_state_as_it_found_it();
    test_the_two_frames_after_a_press_show_the_loading_picture_then_the_map();
    printf("test_loading_screen: ok\n");
    return 0;
}
