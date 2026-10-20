#include "game/visual_random.h"
#include "p8/fixed.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

enum { GAME_DRAWS = 64 };

static void draw_game_values(p8_fixed_t out[GAME_DRAWS], int visual_draws_between) {
    for (int i = 0; i < GAME_DRAWS; i++) {
        out[i] = p8_fixed_rnd(p8_fixed_from_int(100));
        for (int v = 0; v < visual_draws_between + i % 3; v++) {
            visual_random_draw(p8_fixed_from_int(7));
            visual_random_draw_one();
        }
    }
}

static void test_visual_draws_between_game_draws_leave_them_unchanged(void) {
    uint32_t saved = p8_fixed_rnd_state();
    p8_fixed_t without[GAME_DRAWS];
    p8_fixed_t with[GAME_DRAWS];
    p8_fixed_set_rnd_state(0x12345678u);
    for (int i = 0; i < GAME_DRAWS; i++) {
        without[i] = p8_fixed_rnd(p8_fixed_from_int(100));
    }
    for (int between = 1; between <= 5; between++) {
        p8_fixed_set_rnd_state(0x12345678u);
        draw_game_values(with, between);
        for (int i = 0; i < GAME_DRAWS; i++) {
            assert(with[i] == without[i]);
        }
    }
    p8_fixed_set_rnd_state(saved);
}

static void test_visual_draws_leave_the_game_state_unchanged(void) {
    uint32_t game = p8_fixed_rnd_state();
    uint32_t visual = visual_random_state();
    for (int i = 0; i < 100; i++) {
        visual_random_draw(p8_fixed_from_int(3));
        visual_random_draw_one();
    }
    assert(p8_fixed_rnd_state() == game);
    assert(visual_random_state() != visual);
}

static void test_game_draws_leave_the_visual_state_unchanged(void) {
    uint32_t game = p8_fixed_rnd_state();
    uint32_t visual = visual_random_state();
    for (int i = 0; i < 100; i++) {
        p8_fixed_rnd(p8_fixed_from_int(3));
        p8_fixed_rnd_default();
    }
    assert(visual_random_state() == visual);
    assert(p8_fixed_rnd_state() != game);
}

static void test_explicit_state_draw_matches_the_global_rnd(void) {
    static const p8_fixed_t ranges[] = {
        P8_FIXED_INT(1), P8_FIXED_INT(100), P8_FIXED_INT(-5), P8_FIXED_LIT(0x00008000, 0.5), 0, (p8_fixed_t)0x7FFFFFFF,
    };
    uint32_t saved = p8_fixed_rnd_state();
    uint32_t copy = saved;
    for (int round = 0; round < 50; round++) {
        for (unsigned i = 0; i < sizeof ranges / sizeof ranges[0]; i++) {
            assert(p8_fixed_rnd_from_state(&copy, ranges[i]) == p8_fixed_rnd(ranges[i]));
            assert(copy == p8_fixed_rnd_state());
        }
    }
    p8_fixed_set_rnd_state(saved);
}

static void test_visual_stream_draws_as_the_explicit_state_draw(void) {
    uint32_t saved = visual_random_state();
    uint32_t copy = saved;
    for (int i = 0; i < 50; i++) {
        assert(visual_random_draw(P8_FIXED_INT(9)) == p8_fixed_rnd_from_state(&copy, P8_FIXED_INT(9)));
        assert(visual_random_draw_one() == p8_fixed_rnd_from_state(&copy, P8_FIXED_INT(1)));
        assert(visual_random_state() == copy);
    }
    visual_random_set_state(saved);
    assert(visual_random_state() == saved);
}

static void test_visual_stream_starts_apart_from_the_game_stream(void) {
    assert(visual_random_state() != p8_fixed_rnd_state());
}

int main(void) {
    test_visual_stream_starts_apart_from_the_game_stream();
    test_explicit_state_draw_matches_the_global_rnd();
    test_visual_stream_draws_as_the_explicit_state_draw();
    test_visual_draws_between_game_draws_leave_them_unchanged();
    test_visual_draws_leave_the_game_state_unchanged();
    test_game_draws_leave_the_visual_state_unchanged();
    printf("test_visual_random: all tests passed\n");
    return 0;
}
