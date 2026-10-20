/* Frames are compared field by field and in order: a count of N+M would
 * pass on a recording that replayed the wrong frames, replayed them
 * backwards, or wrote the live ones first, and each of those reloads fine
 * and replays to a different game. */

#include "devtools/session_recorder.h"
#include "devtools/replay_source.h"
#include "game/save_storage.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The session that is resumed: every field distinct, all coordinates
 * below 50, so a live frame can never be mistaken for a replayed one.
 * Every frame carries mouse and pad bits at once: the recording
 * must keep the pad mask alone. */
#define SOURCE_FRAME_COUNT 5
static const p8_input_frame_t source_frames[SOURCE_FRAME_COUNT] = {
    { 10, 11, 0, P8_PAD_BUTTON_5 },
    { 12, 13, 1, P8_PAD_BUTTON_6 },
    { 14, 15, 2, P8_PAD_BUTTON_1 | P8_PAD_BUTTON_9 },
    { 16, 17, 4, P8_PAD_BUTTON_7 },
    { 18, 19, 3, P8_PAD_BUTTON_8 },
};

/* What the player does after getting the controls back, in a value range
 * the replayed frames never reach. */
#define LIVE_FRAME_COUNT 4
static const p8_input_frame_t live_frames[LIVE_FRAME_COUNT] = {
    { 90, 91, 1, P8_PAD_BUTTON_11 | P8_PAD_BUTTON_2 },
    { 92, 93, 5, P8_PAD_BUTTON_10 | P8_PAD_BUTTON_4 },
    { 94, 95, 7, P8_PAD_BUTTON_3 },
    { 96, 97, 6, P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6 },
};

static void assert_recorded_as(const p8_input_frame_t *got, const p8_input_frame_t *want) {
    assert(got->mouse_x == 0 && got->mouse_y == 0 && got->mouse_buttons == 0);
    assert(got->pad_buttons == want->pad_buttons);
}

/* Stands in for polling a live pad, which a headless test has none of. */
static void poll_simulated_live_frame(int live_index, p8_input_frame_t *out_frame) {
    assert(live_index >= 0 && live_index < LIVE_FRAME_COUNT);
    *out_frame = live_frames[live_index];
}

/* A recorded session's loop, with that call replaced and the game step
 * dropped. The frame is recorded before the game steps on it, and
 * unconditionally: this test must not reverse that. */
static void run_session(const char *source_path, const char *resumed_path, int iterations, int personal) {
    replay_source_t replay;
    replay_source_status_t expected = source_path ? REPLAY_SOURCE_LOADED : REPLAY_SOURCE_NONE;
    assert(replay_source_open(&replay, source_path) == expected);

    session_recorder_t recorder;
    session_recorder_open(&recorder, resumed_path);
    session_recorder_set_personal_bindings(&recorder, personal);

    int live_index = 0;
    for (int i = 0; i < iterations; i++) {
        p8_input_frame_t frame;
        if (!replay_source_next_frame(&replay, &frame)) {
            poll_simulated_live_frame(live_index, &frame);
            live_index++;
        }
        assert(session_recorder_record_frame(&recorder, &frame) == 1);
    }

    assert(session_recorder_frame_count(&recorder) == iterations);
    session_recorder_close(&recorder);
    replay_source_close(&replay);
}

static void test_recorder_writes_a_loadable_recording(void) {
    const char *path = "tmp/test_recording_continuity_basic.json";
    p8_input_frame_t frames[2] = {
        { 11, 22, 1, P8_PAD_BUTTON_3 },
        { 33, 44, 2, P8_PAD_BUTTON_10 | P8_PAD_BUTTON_7 },
    };

    session_recorder_t recorder;
    session_recorder_open(&recorder, path);
    assert(session_recorder_frame_count(&recorder) == 0);

    for (int i = 0; i < 2; i++) {
        assert(session_recorder_record_frame(&recorder, &frames[i]) == 1);
        assert(session_recorder_frame_count(&recorder) == i + 1);
    }
    session_recorder_close(&recorder);

    p8_input_t loaded;
    p8_input_init(&loaded);
    assert(p8_input_load(&loaded, path));
    assert(loaded.frame_count == 2);
    for (int i = 0; i < 2; i++) {
        assert_recorded_as(&loaded.frames[i], &frames[i]);
    }
    p8_input_free(&loaded);

    remove(path);
}

static void test_resumed_session_records_replayed_frames_then_new_ones(void) {
    const char *source_path = "tmp/test_recording_continuity_source.json";
    const char *resumed_path = "tmp/test_recording_continuity_resumed.json";

    assert(p8_input_save(source_frames, SOURCE_FRAME_COUNT, source_path, NULL));

    run_session(source_path, resumed_path, SOURCE_FRAME_COUNT + LIVE_FRAME_COUNT, 0);

    p8_input_t resumed;
    p8_input_init(&resumed);
    assert(p8_input_load(&resumed, resumed_path));
    assert(resumed.frame_count == SOURCE_FRAME_COUNT + LIVE_FRAME_COUNT);

    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert_recorded_as(&resumed.frames[i], &source_frames[i]);
    }
    for (int i = 0; i < LIVE_FRAME_COUNT; i++) {
        assert_recorded_as(&resumed.frames[SOURCE_FRAME_COUNT + i], &live_frames[i]);
    }

    p8_input_free(&resumed);
    remove(source_path);
    remove(resumed_path);
}

/* Served back as a replay source rather than merely reloaded: parsing
 * says nothing about what the next session does with the file. Serving
 * fewer than N+M would hand the controls back mid-game on the second
 * resume. */
static void test_resumed_recording_is_itself_resumable(void) {
    const char *source_path = "tmp/test_recording_continuity_chain_source.json";
    const char *resumed_path = "tmp/test_recording_continuity_chain_resumed.json";
    const int total = SOURCE_FRAME_COUNT + LIVE_FRAME_COUNT;

    assert(p8_input_save(source_frames, SOURCE_FRAME_COUNT, source_path, NULL));
    run_session(source_path, resumed_path, total, 0);

    replay_source_t second;
    assert(replay_source_open(&second, resumed_path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&second) == total);

    for (int i = 0; i < total; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&second, &frame) == 1);
        if (i < SOURCE_FRAME_COUNT) {
            assert_recorded_as(&frame, &source_frames[i]);
        } else {
            assert_recorded_as(&frame, &live_frames[i - SOURCE_FRAME_COUNT]);
        }
    }

    p8_input_frame_t after;
    assert(replay_source_next_frame(&second, &after) == 0);

    replay_source_close(&second);
    remove(source_path);
    remove(resumed_path);
}

/* Caller frees. */
static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);
    char *bytes = malloc((size_t)size + 1);
    assert(bytes);
    assert(fread(bytes, 1, (size_t)size, file) == (size_t)size);
    bytes[size] = '\0';
    fclose(file);
    return bytes;
}

/* The live frames carry mouse noise: a session records the pad
 * masks alone, so its replay can never carry mouse input. */
static void test_fresh_session_records_a_pad_replay(void) {
    const char *path = "tmp/test_recording_continuity_fresh_pad.json";

    run_session(NULL, path, LIVE_FRAME_COUNT, 0);

    char *bytes = read_whole_file(path);
    assert(strncmp(bytes, "{\n  \"input\": \"pad\",\n", strlen("{\n  \"input\": \"pad\",\n")) == 0);
    free(bytes);

    p8_input_t recorded;
    p8_input_init(&recorded);
    assert(p8_input_load(&recorded, path));
    assert(recorded.frame_count == LIVE_FRAME_COUNT);
    for (int i = 0; i < LIVE_FRAME_COUNT; i++) {
        assert_recorded_as(&recorded.frames[i], &live_frames[i]);
    }

    p8_input_free(&recorded);
    remove(path);
}

static void record_source_frames(const char *path, int personal, const uint8_t *save, int save_count) {
    session_recorder_t recorder;
    session_recorder_open(&recorder, path);
    session_recorder_set_initial_save(&recorder, save, save_count);
    session_recorder_set_personal_bindings(&recorder, personal);
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert(session_recorder_record_frame(&recorder, &source_frames[i]) == 1);
    }
    session_recorder_close(&recorder);
}

static int count_occurrences(const char *text, const char *needle) {
    int count = 0;
    for (const char *found = strstr(text, needle); found; found = strstr(found + 1, needle)) {
        count++;
    }
    return count;
}

/* The recorder writes the hex, the replay source reads it back and seeds
 * the storage: the two halves of the encoding must agree on every byte. */
static void assert_initial_save_round_trips(const uint8_t *save, int save_count, int personal) {
    const char *path = "tmp/test_recording_continuity_initial_save.json";
    record_source_frames(path, personal, save, save_count);

    const uint8_t previous[2] = { 7, 7 };
    assert(save_storage_write(previous, 2) == 1);

    replay_source_t replay;
    assert(replay_source_open(&replay, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&replay) == SOURCE_FRAME_COUNT);
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&replay, &frame) == 1);
        assert_recorded_as(&frame, &source_frames[i]);
    }
    assert(replay_source_seed_storage(&replay) == 1);
    assert(replay_source_attract_after(&replay) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&replay);

    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == save_count);
    assert(memcmp(got, save, (size_t)save_count) == 0);

    char *bytes = read_whole_file(path);
    const char *member = strstr(bytes, "\"initial_save\"");
    assert(member && member < strstr(bytes, "\"frames\""));
    assert(count_occurrences(bytes, "\"bindings\"") == personal);
    assert(count_occurrences(bytes, "\"attract_after\"") == 1);
    free(bytes);
    remove(path);
}

static void test_initial_save_of_every_byte_value_round_trips(void) {
    uint8_t save[256];
    for (int i = 0; i < 256; i++) {
        save[i] = (uint8_t)i;
    }
    assert_initial_save_round_trips(save, 256, 0);
}

static void test_initial_save_of_the_full_size_round_trips(void) {
    static uint8_t save[SAVE_STATE_MAX_BYTES];
    for (int i = 0; i < SAVE_STATE_MAX_BYTES; i++) {
        save[i] = (uint8_t)(i * 37 + 11);
    }
    assert_initial_save_round_trips(save, SAVE_STATE_MAX_BYTES, 0);
}

static void assert_recording_without_save_or_flag_writes_only_attract_after(int set_flag_explicitly) {
    const char *path = "tmp/test_recording_continuity_no_initial_save.json";
    const char *reference_path = "tmp/test_recording_continuity_no_initial_save_reference.json";
    session_recorder_t recorder;
    session_recorder_open(&recorder, path);
    session_recorder_set_initial_save(&recorder, NULL, 0);
    if (set_flag_explicitly) {
        session_recorder_set_personal_bindings(&recorder, 0);
    }
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert(session_recorder_record_frame(&recorder, &source_frames[i]) == 1);
    }
    session_recorder_close(&recorder);
    assert(p8_input_save(source_frames, SOURCE_FRAME_COUNT, reference_path, NULL));

    char *bytes = read_whole_file(path);
    char *reference = read_whole_file(reference_path);
    static const char input_line[] = "{\n  \"input\": \"pad\",\n";
    static const char attract_line[] = "  \"attract_after\": 1200,\n";
    const char *expected_prefix = "{\n  \"input\": \"pad\",\n  \"attract_after\": 1200,\n  \"frames\": [\n";
    assert(strncmp(bytes, expected_prefix, strlen(expected_prefix)) == 0);
    assert(strncmp(reference, input_line, strlen(input_line)) == 0);
    assert(strncmp(bytes + strlen(input_line), attract_line, strlen(attract_line)) == 0);
    assert(strcmp(bytes + strlen(input_line) + strlen(attract_line), reference + strlen(input_line)) == 0);
    free(bytes);
    free(reference);

    const uint8_t previous[2] = { 7, 7 };
    assert(save_storage_write(previous, 2) == 1);
    replay_source_t replay;
    assert(replay_source_open(&replay, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&replay) == 1);
    assert(replay_source_attract_after(&replay) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&replay);
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == 0);

    remove(path);
    remove(reference_path);
}

static void test_recorder_without_save_or_flag_writes_only_attract_after(void) {
    assert_recording_without_save_or_flag_writes_only_attract_after(0);
    assert_recording_without_save_or_flag_writes_only_attract_after(1);
}

static void test_flagged_recording_without_save_names_personal_bindings(void) {
    const char *path = "tmp/test_recording_continuity_personal_no_save.json";
    record_source_frames(path, 1, NULL, 0);

    char *bytes = read_whole_file(path);
    const char *expected =
        "{\n  \"input\": \"pad\",\n  \"attract_after\": 1200,\n  \"bindings\": \"personal\",\n  \"frames\": [\n";
    assert(strncmp(bytes, expected, strlen(expected)) == 0);
    assert(count_occurrences(bytes, "\"bindings\"") == 1);
    free(bytes);

    p8_input_t loaded;
    p8_input_init(&loaded);
    assert(p8_input_load(&loaded, path));
    assert(loaded.frame_count == SOURCE_FRAME_COUNT);
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert_recorded_as(&loaded.frames[i], &source_frames[i]);
    }
    p8_input_free(&loaded);

    const uint8_t previous[2] = { 7, 7 };
    assert(save_storage_write(previous, 2) == 1);
    replay_source_t replay;
    assert(replay_source_open(&replay, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&replay) == 1);
    assert(replay_source_attract_after(&replay) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&replay);
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == 0);

    remove(path);
}

static void test_flagged_recording_with_save_carries_both_members(void) {
    const uint8_t save[3] = { 0x00, 0xa5, 0xff };
    assert_initial_save_round_trips(save, 3, 1);

    const char *path = "tmp/test_recording_continuity_personal_save.json";
    record_source_frames(path, 1, save, 3);
    char *bytes = read_whole_file(path);
    const char *expected = "{\n  \"input\": \"pad\",\n  \"attract_after\": 1200,\n  \"bindings\": \"personal\",\n"
                           "  \"initial_save\": \"00a5ff\",\n  \"frames\": [\n";
    assert(strncmp(bytes, expected, strlen(expected)) == 0);
    free(bytes);
    remove(path);
}

static void test_open_and_close_clear_the_flag(void) {
    const char *first_path = "tmp/test_recording_continuity_flag_cleared_first.json";
    const char *second_path = "tmp/test_recording_continuity_flag_cleared_second.json";
    session_recorder_t recorder;
    session_recorder_open(&recorder, first_path);
    session_recorder_set_personal_bindings(&recorder, 1);
    session_recorder_close(&recorder);

    session_recorder_open(&recorder, second_path);
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert(session_recorder_record_frame(&recorder, &source_frames[i]) == 1);
    }
    session_recorder_close(&recorder);

    char *bytes = read_whole_file(second_path);
    assert(count_occurrences(bytes, "\"bindings\"") == 0);
    free(bytes);
    remove(second_path);
}

static void test_flagged_resume_writes_the_header_once(void) {
    const char *source_path = "tmp/test_recording_continuity_personal_source.json";
    const char *resumed_path = "tmp/test_recording_continuity_personal_resumed.json";
    const int total = SOURCE_FRAME_COUNT + LIVE_FRAME_COUNT;
    record_source_frames(source_path, 1, NULL, 0);

    run_session(source_path, resumed_path, total, 1);

    char *bytes = read_whole_file(resumed_path);
    assert(count_occurrences(bytes, "\"bindings\"") == 1);
    assert(count_occurrences(bytes, "\"attract_after\"") == 1);
    free(bytes);

    replay_source_t replay;
    assert(replay_source_open(&replay, resumed_path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_attract_after(&replay) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&replay);

    p8_input_t resumed;
    p8_input_init(&resumed);
    assert(p8_input_load(&resumed, resumed_path));
    assert(resumed.frame_count == total);
    for (int i = 0; i < SOURCE_FRAME_COUNT; i++) {
        assert_recorded_as(&resumed.frames[i], &source_frames[i]);
    }
    for (int i = 0; i < LIVE_FRAME_COUNT; i++) {
        assert_recorded_as(&resumed.frames[SOURCE_FRAME_COUNT + i], &live_frames[i]);
    }
    p8_input_free(&resumed);
    remove(source_path);
    remove(resumed_path);
}

static replay_source_bindings_status_t read_recorded_bindings(int personal) {
    const char *path = "tmp/test_recording_continuity_read_bindings.json";
    record_source_frames(path, personal, NULL, 0);

    replay_source_t replay;
    assert(replay_source_open(&replay, path) == REPLAY_SOURCE_LOADED);
    static bindings_t fallback;
    static bindings_t out;
    static replay_source_bindings_report_t report;
    replay_source_bindings_status_t status = replay_source_read_bindings(&replay, &fallback, &out, &report);
    replay_source_close(&replay);
    remove(path);
    return status;
}

static void test_flagged_recording_is_read_back_as_personal(void) {
    assert(read_recorded_bindings(1) == REPLAY_SOURCE_BINDINGS_PERSONAL);
    assert(read_recorded_bindings(0) == REPLAY_SOURCE_BINDINGS_DEFAULT);
}

int main(void) {
    test_recorder_writes_a_loadable_recording();
    test_resumed_session_records_replayed_frames_then_new_ones();
    test_resumed_recording_is_itself_resumable();
    test_fresh_session_records_a_pad_replay();
    test_initial_save_of_every_byte_value_round_trips();
    test_initial_save_of_the_full_size_round_trips();
    test_recorder_without_save_or_flag_writes_only_attract_after();
    test_flagged_recording_without_save_names_personal_bindings();
    test_flagged_recording_with_save_carries_both_members();
    test_open_and_close_clear_the_flag();
    test_flagged_resume_writes_the_header_once();
    test_flagged_recording_is_read_back_as_personal();

    printf("test_recording_continuity: all tests passed\n");
    return 0;
}
