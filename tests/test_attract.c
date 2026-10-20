#include "game/attract.h"
#include "game/demo_data.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_an_empty_title_starts_the_demo_on_frame_n(void) {
    enum { N = 3 };
    attract_t attract;
    attract_init(&attract, N);
    int played = -1;
    for (int i = 0; i < N; i++) {
        assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_NONE);
        assert(played == 0);
        assert(attract_phase(&attract) == ATTRACT_PHASE_IDLE);
    }
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_START);
    assert(played == demo_data_pad_buttons[0]);
    assert(attract_phase(&attract) == ATTRACT_PHASE_PLAYING);
    assert(attract_demo_frame(&attract) == 1);
}

/* Empty title frames until the demo starts; returns how many frames ran, the start frame included. */
static int frames_until_start(attract_t *attract, int limit) {
    int played;
    for (int i = 0; i < limit; i++) {
        attract_action_t action = attract_frame(attract, 1, 0, &played);
        if (action == ATTRACT_ACTION_START) {
            return i + 1;
        }
        assert(action == ATTRACT_ACTION_NONE && played == 0);
    }
    return -1;
}

static void test_a_button_or_an_off_title_frame_plays_as_is_and_restarts_the_count(void) {
    enum { N = 5 };
    for (int k = 0; k < N; k++) {
        for (int off_title = 0; off_title < 2; off_title++) {
            attract_t attract;
            attract_init(&attract, N);
            int played;
            for (int i = 0; i < k; i++) {
                assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_NONE);
            }
            int raw = off_title ? 0 : 0x21;
            assert(attract_frame(&attract, !off_title, raw, &played) == ATTRACT_ACTION_NONE);
            assert(played == raw);
            assert(frames_until_start(&attract, 10 * N) == N + 1);
        }
    }
}

static void test_no_count_off_the_title(void) {
    attract_t attract;
    attract_init(&attract, 2);
    int played;
    for (int i = 0; i < 5000; i++) {
        assert(attract_frame(&attract, 0, 0, &played) == ATTRACT_ACTION_NONE);
        assert(played == 0 && attract_phase(&attract) == ATTRACT_PHASE_IDLE);
    }
}

static void start(attract_t *attract, int attract_after) {
    attract_init(attract, attract_after);
    assert(frames_until_start(attract, attract_after + 1) == attract_after + 1);
}

static void test_the_frames_after_the_start_play_the_demo_whatever_the_title_says(void) {
    attract_t attract;
    start(&attract, 1);
    int played;
    for (int i = 1; i < demo_data_frame_count; i++) {
        assert(attract_frame(&attract, i % 2, 0, &played) == ATTRACT_ACTION_NONE);
        assert(played == demo_data_pad_buttons[i]);
        assert(attract_phase(&attract) == ATTRACT_PHASE_PLAYING);
        assert(attract_demo_frame(&attract) == i + 1);
    }
}

static void play_to_the_last_frame(attract_t *attract) {
    int played;
    while (attract_demo_frame(attract) < demo_data_frame_count) {
        assert(attract_frame(attract, 0, 0, &played) == ATTRACT_ACTION_NONE);
    }
    assert(played == demo_data_pad_buttons[demo_data_frame_count - 1]);
    assert(attract_phase(attract) == ATTRACT_PHASE_PLAYING);
}

static void test_the_frame_after_the_last_returns_empty_and_the_demo_loops(void) {
    enum { N = 4 };
    attract_t attract;
    start(&attract, N);
    play_to_the_last_frame(&attract);
    int played = -1;
    assert(attract_frame(&attract, 0, 0, &played) == ATTRACT_ACTION_RETURN);
    assert(played == 0);
    assert(attract_phase(&attract) == ATTRACT_PHASE_IDLE);
    assert(frames_until_start(&attract, 10 * N) == N + 1);
}

static void play_until_demo_frame(attract_t *attract, int demo_frame) {
    int played;
    while (attract_demo_frame(attract) < demo_frame) {
        assert(attract_frame(attract, 0, 0, &played) == ATTRACT_ACTION_NONE);
    }
}

/* demo_frame == demo_data_frame_count presses on the frame a completed demo would return on. */
static void test_a_button_down_while_playing_returns_empty_and_swallows(void) {
    const int demo_frames[] = {1, 2, demo_data_frame_count / 2, demo_data_frame_count - 1, demo_data_frame_count};
    for (int i = 0; i < (int)(sizeof demo_frames / sizeof demo_frames[0]); i++) {
        attract_t attract;
        start(&attract, 3);
        play_until_demo_frame(&attract, demo_frames[i]);
        int played = -1;
        assert(attract_frame(&attract, 0, 0x10, &played) == ATTRACT_ACTION_RETURN);
        assert(played == 0);
        assert(attract_phase(&attract) == ATTRACT_PHASE_SWALLOWING);
    }
}

static void interrupt(attract_t *attract, int attract_after, int demo_frame) {
    start(attract, attract_after);
    play_until_demo_frame(attract, demo_frame);
    int played;
    assert(attract_frame(attract, 0, 0x10, &played) == ATTRACT_ACTION_RETURN);
}

static void test_held_buttons_play_empty_and_the_first_all_up_frame_is_idle_frame_1(void) {
    enum { N = 3 };
    attract_t attract;
    interrupt(&attract, N, 40);
    int played = -1;
    for (int i = 0; i < 10 * N; i++) {
        int held = (i % 3 == 0) ? 0x10 : 0x13;
        assert(attract_frame(&attract, 1, held, &played) == ATTRACT_ACTION_NONE);
        assert(played == 0);
        assert(attract_phase(&attract) == ATTRACT_PHASE_SWALLOWING);
    }
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_NONE);
    assert(played == 0 && attract_phase(&attract) == ATTRACT_PHASE_IDLE);
    assert(frames_until_start(&attract, 10 * N) == N);
}

static void test_a_release_off_the_title_ends_the_swallow_without_counting(void) {
    enum { N = 3 };
    attract_t attract;
    interrupt(&attract, N, 40);
    int played = -1;
    assert(attract_frame(&attract, 0, 0, &played) == ATTRACT_ACTION_NONE);
    assert(played == 0 && attract_phase(&attract) == ATTRACT_PHASE_IDLE);
    assert(attract_frame(&attract, 0, 0x08, &played) == ATTRACT_ACTION_NONE);
    assert(played == 0x08);
    assert(frames_until_start(&attract, 10 * N) == N + 1);
}

static void test_threshold_1_leaves_two_frames_between_a_return_and_the_next_start(void) {
    attract_t attract;
    start(&attract, 1);
    play_to_the_last_frame(&attract);
    int played;
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_RETURN);
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_NONE);
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_START);

    interrupt(&attract, 1, 7);
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_NONE);
    assert(attract_frame(&attract, 1, 0, &played) == ATTRACT_ACTION_START);
}

static void test_the_demo_record_is_the_compiled_record(void) {
    static uint8_t encoded[SAVE_STATE_MAX_BYTES];
    int count = save_state_encode(attract_demo_record(), encoded);
    assert(count == demo_data_record_count);
    assert(memcmp(encoded, demo_data_record, (size_t)count) == 0);
}

int main(void) {
    test_an_empty_title_starts_the_demo_on_frame_n();
    test_a_button_or_an_off_title_frame_plays_as_is_and_restarts_the_count();
    test_no_count_off_the_title();
    test_the_frames_after_the_start_play_the_demo_whatever_the_title_says();
    test_the_frame_after_the_last_returns_empty_and_the_demo_loops();
    test_a_button_down_while_playing_returns_empty_and_swallows();
    test_held_buttons_play_empty_and_the_first_all_up_frame_is_idle_frame_1();
    test_a_release_off_the_title_ends_the_swallow_without_counting();
    test_threshold_1_leaves_two_frames_between_a_return_and_the_next_start();
    test_the_demo_record_is_the_compiled_record();
    printf("test_attract: ok\n");
    return 0;
}
