/* Every served frame is asserted value by value and in order: counting
 * them, or checking them for being non-zero, would pass on a source serving
 * them backwards.
 *
 * The three open outcomes are pinned separately: an unreadable recording
 * must stay an explicit launch error, never a silent new game that would
 * look to the player like a lost save.
 *
 * The failure fixtures are those of `tests/test_input.c`: this module
 * delegates "valid recording" to `p8_input_load`, so it is tested against
 * that function's idea of a bad file, not a second one. */

#include "devtools/replay_source.h"
#include "game/save_state.h"
#include "game/save_storage.h"

#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>

static void test_no_path_requests_no_recording(void) {
    replay_source_t source;

    assert(replay_source_open(&source, NULL) == REPLAY_SOURCE_NONE);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
}

static void test_valid_recording_loads_with_its_frame_count(void) {
    replay_source_t source;

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") ==
           REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == 3);

    replay_source_close(&source);
}

static void test_missing_path_reports_load_failure(void) {
    replay_source_t source;

    assert(replay_source_open(&source, "tests/fixtures/no_such_recording.json") ==
           REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
}

static void test_malformed_recording_reports_load_failure(void) {
    replay_source_t source;

    assert(replay_source_open(&source, "tests/fixtures/events_pad_missing_buttons.json") ==
           REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
}

/* The three frames of tests/fixtures/events_valid.json, transcribed: the
 * exact values are what catch an off-by-one, a reversed order or a frame
 * served twice. */
static const p8_input_frame_t events_valid_frames[] = {
    { 0, 0, 0, P8_PAD_BUTTON_4 },
    { 0, 0, 0, P8_PAD_BUTTON_1 | P8_PAD_BUTTON_3 },
    { 0, 0, 0, P8_PAD_BUTTON_2 }
};
static const int events_valid_count = 3;

static void assert_frame_equals(const p8_input_frame_t *got, const p8_input_frame_t *want) {
    assert(got->mouse_x == want->mouse_x);
    assert(got->mouse_y == want->mouse_y);
    assert(got->mouse_buttons == want->mouse_buttons);
    assert(got->pad_buttons == want->pad_buttons);
}

static void test_loaded_recording_serves_its_first_frame(void) {
    replay_source_t source;
    p8_input_frame_t frame;

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") ==
           REPLAY_SOURCE_LOADED);

    assert(replay_source_next_frame(&source, &frame) == 1);
    assert_frame_equals(&frame, &events_valid_frames[0]);

    replay_source_close(&source);
}

static void test_loaded_recording_serves_every_frame_in_order(void) {
    replay_source_t source;

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") ==
           REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == events_valid_count);

    for (int i = 0; i < events_valid_count; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&source, &frame) == 1);
        assert_frame_equals(&frame, &events_valid_frames[i]);
    }

    replay_source_close(&source);
}

/* 100 is far longer than the recording: exhaustion is a terminal state,
 * never a loop back to frame 0 or a re-served last frame. */
static void test_exhausted_recording_reports_exhaustion_forever(void) {
    replay_source_t source;
    p8_input_frame_t frame;

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") ==
           REPLAY_SOURCE_LOADED);

    for (int i = 0; i < events_valid_count; i++) {
        assert(replay_source_next_frame(&source, &frame) == 1);
    }

    for (int i = 0; i < 100; i++) {
        assert(replay_source_next_frame(&source, &frame) == 0);
    }

    replay_source_close(&source);
}

static void test_no_recording_is_exhausted_from_the_first_frame(void) {
    replay_source_t source;
    p8_input_frame_t frame;

    assert(replay_source_open(&source, NULL) == REPLAY_SOURCE_NONE);

    assert(replay_source_next_frame(&source, &frame) == 0);
    assert(replay_source_next_frame(&source, &frame) == 0);

    replay_source_close(&source);
}

/* Counted in frames served, so 1 on the first frame and N on the last --
 * one ahead of the 0-based index the marker file writes. */
static void test_frames_served_tracks_the_replay_position(void) {
    replay_source_t source;
    p8_input_frame_t frame;

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") ==
           REPLAY_SOURCE_LOADED);
    assert(replay_source_frames_served(&source) == 0);

    assert(replay_source_next_frame(&source, &frame) == 1);
    assert(replay_source_frames_served(&source) == 1);
    assert(replay_source_next_frame(&source, &frame) == 1);
    assert(replay_source_next_frame(&source, &frame) == 1);
    assert(replay_source_frames_served(&source) == 3);
    assert(replay_source_frames_served(&source) == replay_source_frame_count(&source));

    /* Exhaustion does not advance it: the replay stays at N of N instead
     * of counting the live frames the player now plays. */
    assert(replay_source_next_frame(&source, &frame) == 0);
    assert(replay_source_frames_served(&source) == 3);

    replay_source_close(&source);
}

#define PAD_FRAME_COUNT 4
static const p8_input_frame_t pad_frames[PAD_FRAME_COUNT] = {
    { 0, 0, 0, P8_PAD_BUTTON_5 },
    { 0, 0, 0, 0 },
    { 0, 0, 0, P8_PAD_BUTTON_1 | P8_PAD_BUTTON_6 | P8_PAD_BUTTON_11 },
    { 0, 0, 0, P8_PAD_BUTTON_12 | P8_PAD_BUTTON_3 },
};

static void test_pad_recording_serves_its_masks_in_order(void) {
    const char *path = "tmp/test_replay_source_pad.json";
    assert(p8_input_save(pad_frames, PAD_FRAME_COUNT, path, NULL));

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == PAD_FRAME_COUNT);
    for (int i = 0; i < PAD_FRAME_COUNT; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&source, &frame) == 1);
        assert_frame_equals(&frame, &pad_frames[i]);
    }

    replay_source_close(&source);
    remove(path);
}

static void test_old_mouse_recording_reports_load_failure(void) {
    replay_source_t source;

    assert(replay_source_open(&source, "tests/fixtures/events_mouse_recording_refused.json") ==
           REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
}

#define SAVE_FRAMES_TEXT "  \"frames\": [\n    { \"pad\": [1] },\n    { \"pad\": [] }\n  ]\n"

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert(file);
    assert(fputs(text, file) >= 0);
    assert(fclose(file) == 0);
}

static void write_recording_with_member(const char *path, const char *member) {
    char text[4 * SAVE_STATE_MAX_BYTES];
    int used = snprintf(text, sizeof(text), "{\n  \"input\": \"pad\",\n%s%s%s" SAVE_FRAMES_TEXT "}\n",
                        member ? "  " : "", member ? member : "", member ? ",\n" : "");
    assert(used > 0 && (size_t)used < sizeof(text));
    write_text(path, text);
}

static void test_hand_written_select_is_served_as_its_bit(void) {
    const char *path = "tmp/test_replay_source_select.json";
    write_text(path, "{\n  \"input\": \"pad\",\n  \"frames\": [\n"
                     "    { \"pad\": [12] },\n"
                     "    { \"pad\": [11, 12, 1] },\n"
                     "    { \"pad\": [] }\n  ]\n}\n");
    static const int expected[3] = { P8_PAD_BUTTON_12, P8_PAD_BUTTON_12 | P8_PAD_BUTTON_1 | P8_PAD_BUTTON_11, 0 };

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == 3);
    for (int i = 0; i < 3; i++) {
        p8_input_frame_t frame;
        assert(replay_source_next_frame(&source, &frame) == 1);
        assert(frame.pad_buttons == expected[i]);
    }

    replay_source_close(&source);
    remove(path);
}

static void store_a_record(void) {
    const uint8_t previous[3] = { 9, 9, 9 };
    assert(save_storage_write(previous, 3) == 1);
}

static void assert_storage_holds(const uint8_t *bytes, int count) {
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == count);
    assert(memcmp(got, bytes, (size_t)count) == 0);
}

static void test_initial_save_seeds_the_storage_with_its_bytes(void) {
    const char *path = "tmp/test_replay_source_initial_save.json";
    write_recording_with_member(path, "\"initial_save\": \"00a17fff\"");
    store_a_record();

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == 2);
    assert(replay_source_seed_storage(&source) == 1);
    const uint8_t expected[4] = { 0x00, 0xa1, 0x7f, 0xff };
    assert_storage_holds(expected, 4);

    replay_source_close(&source);
    remove(path);
}

static void test_recording_without_initial_save_empties_the_storage(void) {
    const char *path = "tmp/test_replay_source_no_initial_save.json";
    write_recording_with_member(path, NULL);
    store_a_record();

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source) == 1);
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == 0);

    replay_source_close(&source);
    remove(path);
}

static void test_live_play_leaves_the_storage_untouched(void) {
    store_a_record();

    replay_source_t source;
    assert(replay_source_open(&source, NULL) == REPLAY_SOURCE_NONE);
    assert(replay_source_seed_storage(&source) == 1);
    const uint8_t previous[3] = { 9, 9, 9 };
    assert_storage_holds(previous, 3);

    replay_source_close(&source);
}

static void test_initial_save_accepts_uppercase_hex(void) {
    const char *path = "tmp/test_replay_source_uppercase.json";
    write_recording_with_member(path, "\"initial_save\": \"ABcdEF\"");

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source) == 1);
    const uint8_t expected[3] = { 0xab, 0xcd, 0xef };
    assert_storage_holds(expected, 3);

    replay_source_close(&source);
    remove(path);
}

/* A session that started from a corrupt save must replay as it happened:
 * the bytes are never decoded on load. One byte is shorter than any
 * record's header. */
static void test_initial_save_that_is_not_a_valid_record_loads(void) {
    const char *path = "tmp/test_replay_source_invalid_record.json";
    write_recording_with_member(path, "\"initial_save\": \"00\"");
    const uint8_t record[1] = { 0 };

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source) == 1);
    assert_storage_holds(record, 1);

    replay_source_close(&source);
    remove(path);
}

static void assert_member_fails_to_load(const char *member) {
    const char *path = "tmp/test_replay_source_malformed_initial_save.json";
    write_recording_with_member(path, member);

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
    remove(path);
}

static void format_long_member(char *member, size_t size, int digit_count) {
    int used = snprintf(member, size, "\"initial_save\": \"");
    assert(used > 0 && (size_t)(used + digit_count + 2) < size);
    for (int i = 0; i < digit_count; i++) {
        member[used + i] = (i % 2) ? 'b' : 'a';
    }
    member[used + digit_count] = '"';
    member[used + digit_count + 1] = '\0';
}

static void test_malformed_initial_save_fails_to_load(void) {
    assert_member_fails_to_load("\"initial_save\": \"abc\"");
    assert_member_fails_to_load("\"initial_save\": \"0g\"");
    assert_member_fails_to_load("\"initial_save\": \"\"");
    assert_member_fails_to_load("\"initial_save\": 12");

    static char member[2 * SAVE_STATE_MAX_BYTES + 64];
    format_long_member(member, sizeof(member), 2 * SAVE_STATE_MAX_BYTES + 2);
    assert_member_fails_to_load(member);
}

static void test_initial_save_of_the_full_size_loads(void) {
    const char *path = "tmp/test_replay_source_full_size.json";
    static char member[2 * SAVE_STATE_MAX_BYTES + 64];
    format_long_member(member, sizeof(member), 2 * SAVE_STATE_MAX_BYTES);
    write_recording_with_member(path, member);

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source) == 1);
    static uint8_t expected[SAVE_STATE_MAX_BYTES];
    memset(expected, 0xab, sizeof(expected));
    assert_storage_holds(expected, SAVE_STATE_MAX_BYTES);

    replay_source_close(&source);
    remove(path);
}

/* The rule "input" follows: only the header, before "frames", counts. */
static void test_initial_save_after_the_frames_is_ignored(void) {
    const char *path = "tmp/test_replay_source_after_frames.json";
    write_text(path, "{\n  \"input\": \"pad\",\n" SAVE_FRAMES_TEXT "  ,\"initial_save\": \"zz\"\n}\n");
    store_a_record();

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_seed_storage(&source) == 1);
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == 0);

    replay_source_close(&source);
    remove(path);
}

static void test_attract_after_defaults_without_the_member(void) {
    replay_source_t source;
    assert(replay_source_open(&source, NULL) == REPLAY_SOURCE_NONE);
    assert(replay_source_attract_after(&source) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&source);

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") == REPLAY_SOURCE_LOADED);
    assert(replay_source_attract_after(&source) == 1200);
    replay_source_close(&source);
}

static int attract_after_of(const char *member) {
    const char *path = "tmp/test_replay_source_attract_after.json";
    write_recording_with_member(path, member);

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_frame_count(&source) == 2);
    int attract_after = replay_source_attract_after(&source);

    replay_source_close(&source);
    remove(path);
    return attract_after;
}

static void test_attract_after_in_the_header_is_read(void) {
    assert(attract_after_of("\"attract_after\": 3") == 3);
    assert(attract_after_of("\"attract_after\": 1200") == 1200);
    assert(attract_after_of("\"attract_after\":\t2147483647") == 2147483647);
    assert(attract_after_of("\"attract_after\":45") == 45);
}

static void test_invalid_attract_after_fails_to_load(void) {
    static const char *const values[] = { "0", "-3", "+3", "03", "3.5", "3e2", "\"3\"", "null", "true", "", "12abc",
                                          "2147483648", "99999999999999999999" };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        char member[64];
        int used = snprintf(member, sizeof(member), "\"attract_after\": %s", values[i]);
        assert(used > 0 && (size_t)used < sizeof(member));
        assert_member_fails_to_load(member);
    }
    assert_member_fails_to_load("\"attract_after\": 3,\n  \"attract_after\": 3");
}

#define BINDINGS_DIR "tmp/test_replay_source_bindings"

/* Distinct from every fixture these tests write, so a copied fallback is never mistaken for a parsed fixture. */
static bindings_t fallback_table(void) {
    bindings_t table;
    memset(&table, 0, sizeof(table));
    table.masks[BINDINGS_MODE_TITLE][BINDINGS_ACTION_CHOOSE_ROW] = P8_PAD_BUTTON_12;
    return table;
}

static void make_bindings_dir(void) {
    assert(mkdir(BINDINGS_DIR, 0777) == 0 || errno == EEXIST);
}

static replay_source_bindings_status_t read_bindings_of(const char *path, bindings_t *out,
                                                        replay_source_bindings_report_t *report) {
    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    bindings_t fallback = fallback_table();
    replay_source_bindings_status_t status = replay_source_read_bindings(&source, &fallback, out, report);
    replay_source_close(&source);
    return status;
}

static void test_attract_after_beside_the_other_members_is_read(void) {
    const char *path = "tmp/test_replay_source_attract_after_members.json";
    static const char *const headers[] = {
        "\"attract_after\": 3,\n  \"bindings\": \"personal\",\n  \"initial_save\": \"00a1\"",
        "\"initial_save\": \"00a1\",\n  \"attract_after\": 3,\n  \"bindings\": \"personal\"",
        "\"bindings\": \"personal\",\n  \"initial_save\": \"00a1\",\n  \"attract_after\": 3",
    };
    for (size_t i = 0; i < sizeof(headers) / sizeof(headers[0]); i++) {
        write_recording_with_member(path, headers[i]);
        replay_source_t source;
        assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
        assert(replay_source_attract_after(&source) == 3);
        assert(replay_source_frame_count(&source) == 2);
        assert(replay_source_seed_storage(&source) == 1);
        const uint8_t expected[2] = { 0x00, 0xa1 };
        assert_storage_holds(expected, 2);
        bindings_t fallback = fallback_table();
        bindings_t table;
        replay_source_bindings_report_t report;
        assert(replay_source_read_bindings(&source, &fallback, &table, &report) == REPLAY_SOURCE_BINDINGS_PERSONAL);
        replay_source_close(&source);
    }
    remove(path);
}

static void test_attract_after_after_the_frames_is_ignored(void) {
    const char *path = "tmp/test_replay_source_attract_after_late.json";
    write_text(path, "{\n  \"input\": \"pad\",\n" SAVE_FRAMES_TEXT "  ,\"attract_after\": 0\n}\n");

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_attract_after(&source) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);

    replay_source_close(&source);
    remove(path);
}

static void test_attract_after_returns_to_the_default(void) {
    const char *path = "tmp/test_replay_source_attract_after_reset.json";
    write_recording_with_member(path, "\"attract_after\": 3");
    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_attract_after(&source) == 3);
    replay_source_close(&source);
    assert(replay_source_attract_after(&source) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);

    write_text(path, "{\n  \"input\": \"pad\",\n  \"attract_after\": 3,\n  \"frames\": [ { \"pad\": [1] }\n}\n");
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_attract_after(&source) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&source);

    write_recording_with_member(path, "\"attract_after\": 0");
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_attract_after(&source) == REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);
    replay_source_close(&source);
    remove(path);
}

static void test_start_defaults_to_the_title_without_the_member(void) {
    replay_source_t source;
    assert(replay_source_open(&source, NULL) == REPLAY_SOURCE_NONE);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_TITLE);
    replay_source_close(&source);

    assert(replay_source_open(&source, "tests/fixtures/events_valid.json") == REPLAY_SOURCE_LOADED);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_TITLE);
    replay_source_close(&source);
}

#define RESUME_MEMBERS "\"start\": \"resume\",\n  \"initial_save\": \"00a1\""

/* The record is the demo's start, not a player's save: storage is erased, never seeded with it. */
static void test_resume_start_is_read_and_erases_the_storage(void) {
    const char *path = "tmp/test_replay_source_resume.json";
    write_recording_with_member(path, RESUME_MEMBERS);
    store_a_record();

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_RESUME);
    assert(replay_source_frame_count(&source) == 2);
    assert(replay_source_seed_storage(&source) == 1);
    static uint8_t got[SAVE_STATE_MAX_BYTES];
    assert(save_storage_read(got, SAVE_STATE_MAX_BYTES) == 0);
    const uint8_t expected[2] = { 0x00, 0xa1 };
    assert(source.initial_save_count == 2 && memcmp(source.initial_save, expected, 2) == 0);

    replay_source_close(&source);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_TITLE);
    remove(path);
}

static void test_resume_start_written_by_the_writer_is_read(void) {
    const char *path = "tmp/test_replay_source_resume_written.json";
    assert(p8_input_save(pad_frames, PAD_FRAME_COUNT, path, RESUME_MEMBERS));

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_RESUME);
    assert(replay_source_frame_count(&source) == PAD_FRAME_COUNT);

    replay_source_close(&source);
    remove(path);
}

static void assert_start_fails_to_load(const char *member) {
    const char *path = "tmp/test_replay_source_bad_start.json";
    write_recording_with_member(path, member);

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOAD_FAILED);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_TITLE);
    assert(replay_source_frame_count(&source) == 0);

    replay_source_close(&source);
    remove(path);
}

static void test_invalid_start_fails_to_load(void) {
    assert_start_fails_to_load("\"start\": \"title\",\n  \"initial_save\": \"00a1\"");
    assert_start_fails_to_load("\"start\": \"resumed\",\n  \"initial_save\": \"00a1\"");
    assert_start_fails_to_load("\"start\": \"\",\n  \"initial_save\": \"00a1\"");
    assert_start_fails_to_load("\"start\": 1,\n  \"initial_save\": \"00a1\"");
    assert_start_fails_to_load("\"start\": \"resume\"");
    assert_start_fails_to_load("\"start\": \"resume\",\n  \"start\": \"resume\",\n  \"initial_save\": \"00a1\"");
}

static void test_start_after_the_frames_is_ignored(void) {
    const char *path = "tmp/test_replay_source_start_late.json";
    write_text(path, "{\n  \"input\": \"pad\",\n" SAVE_FRAMES_TEXT "  ,\"start\": \"resume\"\n}\n");

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(replay_source_start(&source) == REPLAY_SOURCE_START_TITLE);

    replay_source_close(&source);
    remove(path);
}

#define HEADER_MEMBERS RESUME_MEMBERS ",\n  \"attract_after\": 3"
#define CAPTIONS_MEMBER                                                                                             \
    "\"captions\": [\n"                                                                                          \
    "    { \"from\": 0, \"to\": 150, \"text\": \"build a trading empire\\nacross the stars\" },\n"              \
    "    { \"from\": 150, \"to\": 330, \"text\": \"spend science on new tech\" }\n"                             \
    "  ]"

typedef struct {
    int status;
    int frame_count;
    p8_input_frame_t frames[2];
    replay_source_start_t start;
    int attract_after;
    int initial_save_count;
    uint8_t initial_save[SAVE_STATE_MAX_BYTES];
} loaded_header_t;

static loaded_header_t load_header(const char *path) {
    static loaded_header_t loaded;
    memset(&loaded, 0, sizeof(loaded));
    replay_source_t source;
    loaded.status = replay_source_open(&source, path);
    loaded.frame_count = replay_source_frame_count(&source);
    for (int i = 0; i < loaded.frame_count && i < 2; i++) {
        assert(replay_source_next_frame(&source, &loaded.frames[i]) == 1);
    }
    loaded.start = replay_source_start(&source);
    loaded.attract_after = replay_source_attract_after(&source);
    loaded.initial_save_count = source.initial_save_count;
    memcpy(loaded.initial_save, source.initial_save, (size_t)source.initial_save_count);
    replay_source_close(&source);
    return loaded;
}

static void assert_same_header(const loaded_header_t *got, const loaded_header_t *expected) {
    assert(got->status == REPLAY_SOURCE_LOADED && expected->status == REPLAY_SOURCE_LOADED);
    assert(got->frame_count == expected->frame_count && got->frame_count == 2);
    for (int i = 0; i < 2; i++) assert_frame_equals(&got->frames[i], &expected->frames[i]);
    assert(got->start == expected->start && got->start == REPLAY_SOURCE_START_RESUME);
    assert(got->attract_after == expected->attract_after && got->attract_after == 3);
    assert(got->initial_save_count == expected->initial_save_count && got->initial_save_count == 2);
    assert(memcmp(got->initial_save, expected->initial_save, 2) == 0);
}

static void test_captions_member_is_ignored_in_the_header(void) {
    const char *path = "tmp/test_replay_source_captions.json";
    write_recording_with_member(path, HEADER_MEMBERS);
    loaded_header_t without = load_header(path);
    write_recording_with_member(path, HEADER_MEMBERS ",\n  " CAPTIONS_MEMBER);
    loaded_header_t with = load_header(path);
    assert_same_header(&with, &without);
    remove(path);
}

static void test_captions_member_after_the_frames_is_ignored(void) {
    const char *path = "tmp/test_replay_source_captions_late.json";
    write_recording_with_member(path, HEADER_MEMBERS);
    loaded_header_t without = load_header(path);
    write_text(path, "{\n  \"input\": \"pad\",\n  " HEADER_MEMBERS ",\n" SAVE_FRAMES_TEXT "  ," CAPTIONS_MEMBER "\n}\n");
    loaded_header_t with = load_header(path);
    assert_same_header(&with, &without);
    remove(path);
}

static void test_recording_without_bindings_member_uses_the_fallback(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/no_member.json";
    write_recording_with_member(path, NULL);

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_DEFAULT);
    bindings_t fallback = fallback_table();
    assert(memcmp(&got, &fallback, sizeof(got)) == 0);
    assert(report.fixture_path[0] == '\0');
    assert(report.message[0] == '\0');

    remove(path);
}

#define SWAP_FIXTURE_TEXT "# swapped\ntitle.choose_row = 6\ntitle.move_up = 2\ntitle.move_down = 1\n"

/* The fixture sits only in the recording's directory, so finding it proves resolution against that directory and
 * not the working directory. */
static void test_fixture_next_to_the_recording_drives_the_table(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/swap.json";
    const char *fixture = BINDINGS_DIR "/swap.cfg";
    write_recording_with_member(path, "\"bindings\": \"swap.cfg\"");
    write_text(fixture, SWAP_FIXTURE_TEXT);

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_FIXTURE);
    bindings_t expected;
    bindings_error_t error;
    assert(bindings_parse(fixture, SWAP_FIXTURE_TEXT, &expected, &error));
    assert(memcmp(&got, &expected, sizeof(got)) == 0);
    bindings_t fallback = fallback_table();
    assert(memcmp(&got, &fallback, sizeof(got)) != 0);
    assert(strcmp(report.fixture_path, fixture) == 0);
    assert(report.message[0] == '\0');

    remove(path);
    remove(fixture);
}

static void test_missing_fixture_names_the_path_and_the_recording(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/missing.json";
    write_recording_with_member(path, "\"bindings\": \"absent.cfg\"");
    remove(BINDINGS_DIR "/absent.cfg");

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_MISSING);
    assert(strcmp(report.fixture_path, BINDINGS_DIR "/absent.cfg") == 0);
    assert(strcmp(report.message, BINDINGS_DIR "/absent.cfg: cannot read the bindings fixture named by " BINDINGS_DIR
                                  "/missing.json") == 0);

    remove(path);
}

/* The refusal belongs to the replayers: replay_source_open itself still loads the recording. */
static void test_personal_recording_is_refused_but_still_opens(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/personal.json";
    write_recording_with_member(path, "\"bindings\": \"personal\"");

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_PERSONAL);
    assert(report.fixture_path[0] == '\0');
    assert(strcmp(report.message, BINDINGS_DIR "/personal.json: recorded with a personal bindings.cfg (\"bindings\": "
                                  "\"personal\"); replay runs only the default bindings or a fixture next to the "
                                  "recording") == 0);

    remove(path);
}

static void test_invalid_fixture_reports_the_parser_line(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/bad.json";
    const char *fixture = BINDINGS_DIR "/bad.cfg";
    write_recording_with_member(path, "\"bindings\": \"bad.cfg\"");
    write_text(fixture, "jump.jump_up = 7\njump.jump_down = 7\n");

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_INVALID);
    assert(strcmp(report.fixture_path, fixture) == 0);
    assert(strcmp(report.message, BINDINGS_DIR "/bad.cfg:2: button 7 already bound in mode jump") == 0);

    remove(path);
    remove(fixture);
}

/* 65536 bytes is bindings_generate's limit, so a fixture the build would refuse is refused here too. */
static void test_oversized_fixture_is_invalid(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/big.json";
    const char *fixture = BINDINGS_DIR "/big.cfg";
    write_recording_with_member(path, "\"bindings\": \"big.cfg\"");
    FILE *file = fopen(fixture, "wb");
    assert(file);
    for (int i = 0; i < 65536 / 16; i++) {
        assert(fputs("# fifteen bytes\n", file) >= 0);
    }
    assert(fputc('#', file) == '#');
    assert(fclose(file) == 0);

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_INVALID);
    assert(strcmp(report.message, BINDINGS_DIR "/big.cfg: larger than 65536 bytes") == 0);

    remove(path);
    remove(fixture);
}

static replay_source_bindings_status_t read_bindings_with_member(const char *member,
                                                                 replay_source_bindings_report_t *report) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/member.json";
    write_recording_with_member(path, member);
    bindings_t got;
    replay_source_bindings_status_t status = read_bindings_of(path, &got, report);
    remove(path);
    return status;
}

static void format_long_name_member(char *member, size_t size, int name_length) {
    int used = snprintf(member, size, "\"bindings\": \"%.*s.cfg\"", name_length - 4,
                        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    assert(used > 0 && (size_t)used < size);
}

/* A fixture can only be a plain file next to its replay, never a player's bindings.cfg. */
static void test_bad_bindings_members_are_refused(void) {
    static char too_long[128];
    format_long_name_member(too_long, sizeof(too_long), 65);
    const char *members[] = {
        "\"bindings\": \"sub/x.cfg\"",
        "\"bindings\": \"../x.cfg\"",
        "\"bindings\": \"x.txt\"",
        "\"bindings\": \"\"",
        "\"bindings\": \".cfg\"",
        "\"bindings\": \"bindings.cfg\"",
        too_long,
        "\"bindings\": \"a\\\"b.cfg\"",
        "\"bindings\": 3",
        "\"bindings\": \"x.cfg\",\n  \"bindings\": \"y.cfg\"",
    };
    for (size_t i = 0; i < sizeof(members) / sizeof(members[0]); i++) {
        replay_source_bindings_report_t report;
        assert(read_bindings_with_member(members[i], &report) == REPLAY_SOURCE_BINDINGS_BAD_MEMBER);
        assert(report.fixture_path[0] == '\0');
        assert(strcmp(report.message, BINDINGS_DIR "/member.json: bad \"bindings\" member: expected \"personal\" or a "
                                      "file name ending in .cfg in the recording's directory") == 0);
    }

    static char longest[128];
    format_long_name_member(longest, sizeof(longest), 64);
    replay_source_bindings_report_t report;
    assert(read_bindings_with_member(longest, &report) == REPLAY_SOURCE_BINDINGS_MISSING);
}

/* A recording that vanished after its open must not fall back to the default table and replay a different game. */
static void test_recording_removed_after_open_is_not_read_as_default(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/removed.json";
    write_recording_with_member(path, "\"bindings\": \"personal\"");

    replay_source_t source;
    assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
    assert(remove(path) == 0);
    bindings_t fallback = fallback_table();
    bindings_t got;
    replay_source_bindings_report_t report;
    assert(replay_source_read_bindings(&source, &fallback, &got, &report) == REPLAY_SOURCE_BINDINGS_MISSING);
    assert(report.fixture_path[0] == '\0');
    assert(strcmp(report.message, BINDINGS_DIR "/removed.json: cannot re-read the recording") == 0);

    replay_source_close(&source);
}

/* The size is the bytes read, not the text up to a NUL: a NUL early in an oversized fixture must not hide its size. */
static void test_oversized_fixture_with_a_nul_is_invalid(void) {
    make_bindings_dir();
    const char *path = BINDINGS_DIR "/nul.json";
    const char *fixture = BINDINGS_DIR "/nul.cfg";
    write_recording_with_member(path, "\"bindings\": \"nul.cfg\"");
    FILE *file = fopen(fixture, "wb");
    assert(file);
    assert(fputc('\0', file) == 0);
    for (int i = 0; i < 65536; i++) {
        assert(fputc('#', file) == '#');
    }
    assert(fclose(file) == 0);

    bindings_t got;
    replay_source_bindings_report_t report;
    assert(read_bindings_of(path, &got, &report) == REPLAY_SOURCE_BINDINGS_INVALID);
    assert(strcmp(report.message, BINDINGS_DIR "/nul.cfg: larger than 65536 bytes") == 0);

    remove(path);
    remove(fixture);
}

int main(void) {
    test_no_path_requests_no_recording();
    test_valid_recording_loads_with_its_frame_count();
    test_missing_path_reports_load_failure();
    test_malformed_recording_reports_load_failure();
    test_loaded_recording_serves_its_first_frame();
    test_loaded_recording_serves_every_frame_in_order();
    test_exhausted_recording_reports_exhaustion_forever();
    test_no_recording_is_exhausted_from_the_first_frame();
    test_frames_served_tracks_the_replay_position();
    test_pad_recording_serves_its_masks_in_order();
    test_old_mouse_recording_reports_load_failure();
    test_hand_written_select_is_served_as_its_bit();
    test_initial_save_seeds_the_storage_with_its_bytes();
    test_recording_without_initial_save_empties_the_storage();
    test_live_play_leaves_the_storage_untouched();
    test_initial_save_accepts_uppercase_hex();
    test_initial_save_that_is_not_a_valid_record_loads();
    test_malformed_initial_save_fails_to_load();
    test_initial_save_of_the_full_size_loads();
    test_initial_save_after_the_frames_is_ignored();
    test_attract_after_defaults_without_the_member();
    test_attract_after_in_the_header_is_read();
    test_invalid_attract_after_fails_to_load();
    test_attract_after_beside_the_other_members_is_read();
    test_attract_after_after_the_frames_is_ignored();
    test_attract_after_returns_to_the_default();
    test_start_defaults_to_the_title_without_the_member();
    test_resume_start_is_read_and_erases_the_storage();
    test_resume_start_written_by_the_writer_is_read();
    test_invalid_start_fails_to_load();
    test_start_after_the_frames_is_ignored();
    test_captions_member_is_ignored_in_the_header();
    test_captions_member_after_the_frames_is_ignored();
    test_recording_without_bindings_member_uses_the_fallback();
    test_fixture_next_to_the_recording_drives_the_table();
    test_missing_fixture_names_the_path_and_the_recording();
    test_personal_recording_is_refused_but_still_opens();
    test_invalid_fixture_reports_the_parser_line();
    test_oversized_fixture_is_invalid();
    test_bad_bindings_members_are_refused();
    test_recording_removed_after_open_is_not_read_as_default();
    test_oversized_fixture_with_a_nul_is_invalid();

    printf("test_replay_source: all tests passed\n");
    return 0;
}
