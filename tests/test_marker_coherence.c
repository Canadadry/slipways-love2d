/* Every iteration is checked, not one chosen marker: an off-by-N can cancel
 * out at a single frame. */

#include "devtools/session_recorder.h"
#include "devtools/replay_source.h"

#include <assert.h>
#include <stdio.h>

/* The session being resumed. Every mask stays below P8_PAD_BUTTON_9, and the live
 * frames below hold it, so a marker pointing into the wrong half names a
 * visibly different frame rather than just an unequal number. */
#define REPLAYED_FRAME_COUNT 6
static const p8_input_frame_t replayed_frames[REPLAYED_FRAME_COUNT] = {
    { 0, 0, 0, P8_PAD_BUTTON_1 },
    { 0, 0, 0, P8_PAD_BUTTON_2 },
    { 0, 0, 0, P8_PAD_BUTTON_3 },
    { 0, 0, 0, P8_PAD_BUTTON_4 },
    { 0, 0, 0, P8_PAD_BUTTON_5 },
    { 0, 0, 0, P8_PAD_BUTTON_6 | P8_PAD_BUTTON_7 },
};

#define LIVE_FRAME_COUNT 5
static const p8_input_frame_t live_frames[LIVE_FRAME_COUNT] = {
    { 0, 0, 0, P8_PAD_BUTTON_9 },
    { 0, 0, 0, P8_PAD_BUTTON_9 | P8_PAD_BUTTON_1 },
    { 0, 0, 0, P8_PAD_BUTTON_9 | P8_PAD_BUTTON_10 },
    { 0, 0, 0, P8_PAD_BUTTON_9 | P8_PAD_BUTTON_11 },
    { 0, 0, 0, P8_PAD_BUTTON_9 | P8_PAD_BUTTON_8 },
};

#define TOTAL_FRAME_COUNT (REPLAYED_FRAME_COUNT + LIVE_FRAME_COUNT)

static void assert_frame_equals(const p8_input_frame_t *got, const p8_input_frame_t *want) {
    assert(got->pad_buttons == want->pad_buttons);
}

/* A recording session's loop, with the live input scripted and the game
 * step dropped. `marker_for_iteration` holds the count read where a live
 * pad would be polled: just before the frame is recorded.
 * One line later it would name the following frame. */
static void run_resumed_session(const char *source_path, const char *resumed_path, int *marker_for_iteration) {
    replay_source_t replay;
    assert(replay_source_open(&replay, source_path) == REPLAY_SOURCE_LOADED);

    session_recorder_t recorder;
    session_recorder_open(&recorder, resumed_path);

    int live_index = 0;
    for (int i = 0; i < TOTAL_FRAME_COUNT; i++) {
        p8_input_frame_t frame;
        if (!replay_source_next_frame(&replay, &frame)) {
            frame = live_frames[live_index];
            live_index++;
        }
        marker_for_iteration[i] = session_recorder_frame_count(&recorder);
        assert(session_recorder_record_frame(&recorder, &frame) == 1);
    }

    assert(live_index == LIVE_FRAME_COUNT);
    session_recorder_close(&recorder);
    replay_source_close(&replay);
}

/* A marker on a live frame of a resumed session must be N plus that
 * frame's rank among the live frames, not the rank alone: the recording it
 * indexes into opens with the N replayed frames. */
static void test_marker_index_counts_the_replayed_frames_too(void) {
    const char *source_path = "tmp/test_marker_coherence_source.json";
    const char *resumed_path = "tmp/test_marker_coherence_resumed.json";
    int marker_for_iteration[TOTAL_FRAME_COUNT];

    /* Written by `p8_input_save`, not by the recorder under test. */
    assert(p8_input_save(replayed_frames, REPLAYED_FRAME_COUNT, source_path, NULL));

    run_resumed_session(source_path, resumed_path, marker_for_iteration);

    for (int i = 0; i < TOTAL_FRAME_COUNT; i++) {
        assert(marker_for_iteration[i] == i);
    }
    /* The same thing in the user's terms: the first frame the player
     * actually plays is numbered N, not 0. */
    assert(marker_for_iteration[REPLAYED_FRAME_COUNT] == REPLAYED_FRAME_COUNT);
    assert(marker_for_iteration[TOTAL_FRAME_COUNT - 1] == TOTAL_FRAME_COUNT - 1);

    remove(source_path);
    remove(resumed_path);
}

/* The half arithmetic cannot reach: N plus the rank is only right if the
 * file on disk really does hold the replayed frames first. */
static void test_marker_index_names_the_frame_that_was_on_screen(void) {
    const char *source_path = "tmp/test_marker_coherence_resolve_source.json";
    const char *resumed_path = "tmp/test_marker_coherence_resolve_resumed.json";
    int marker_for_iteration[TOTAL_FRAME_COUNT];

    assert(p8_input_save(replayed_frames, REPLAYED_FRAME_COUNT, source_path, NULL));
    run_resumed_session(source_path, resumed_path, marker_for_iteration);

    p8_input_t resumed;
    p8_input_init(&resumed);
    assert(p8_input_load(&resumed, resumed_path));
    assert(resumed.frame_count == TOTAL_FRAME_COUNT);

    for (int i = 0; i < TOTAL_FRAME_COUNT; i++) {
        int marked = marker_for_iteration[i];
        assert(marked >= 0 && marked < resumed.frame_count);
        const p8_input_frame_t *want = (i < REPLAYED_FRAME_COUNT)
                                              ? &replayed_frames[i]
                                              : &live_frames[i - REPLAYED_FRAME_COUNT];
        assert_frame_equals(&resumed.frames[marked], want);
    }

    p8_input_free(&resumed);
    remove(source_path);
    remove(resumed_path);
}

static void test_markers_are_unchanged_when_no_recording_is_replayed(void) {
    const char *path = "tmp/test_marker_coherence_fresh.json";

    replay_source_t replay;
    assert(replay_source_open(&replay, NULL) == REPLAY_SOURCE_NONE);

    session_recorder_t recorder;
    session_recorder_open(&recorder, path);

    for (int i = 0; i < LIVE_FRAME_COUNT; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&replay, &frame) == 0);
        frame = live_frames[i];
        assert(session_recorder_frame_count(&recorder) == i);
        assert(session_recorder_record_frame(&recorder, &frame) == 1);
    }

    session_recorder_close(&recorder);
    replay_source_close(&replay);
    remove(path);
}

int main(void) {
    test_marker_index_counts_the_replayed_frames_too();
    test_marker_index_names_the_frame_that_was_on_screen();
    test_markers_are_unchanged_when_no_recording_is_replayed();

    printf("test_marker_coherence: all tests passed\n");
    return 0;
}
