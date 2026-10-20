#include "p8/input.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int current_pad_buttons(const p8_input_t *input) {
    return input->frames[input->current_frame].pad_buttons;
}

static void test_load_and_step_multi_frame_matches_fields(void) {
    p8_input_t input;
    p8_input_init(&input);

    int ok = p8_input_load(&input, "tests/fixtures/events_valid.json");
    assert(ok);
    assert(input.frame_count == 3);

    assert(current_pad_buttons(&input) == P8_PAD_BUTTON_4);
    assert(p8_input_read_mouse(&input).x == 0);
    assert(p8_input_read_mouse(&input).y == 0);
    assert(p8_input_read_mouse(&input).buttons == 0);

    p8_input_step(&input);
    assert(current_pad_buttons(&input) == (P8_PAD_BUTTON_1 | P8_PAD_BUTTON_3));

    p8_input_step(&input);
    assert(current_pad_buttons(&input) == P8_PAD_BUTTON_2);
    assert(p8_input_read_mouse(&input).buttons == 0);

    p8_input_free(&input);
}

static void test_frames_without_cross_or_circle_load_them_released(void) {
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/fixtures/events_valid.json"));

    for (int i = 0; i < input.frame_count; i++) {
        assert((input.frames[i].pad_buttons & (P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6)) == 0);
    }

    p8_input_free(&input);
}

static void test_cross_and_circle_load(void) {
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/fixtures/events_valid_ox.json"));

    assert(current_pad_buttons(&input) == (P8_PAD_BUTTON_4 | P8_PAD_BUTTON_5));
    p8_input_step(&input);
    assert(current_pad_buttons(&input) == (P8_PAD_BUTTON_1 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_6));

    p8_input_free(&input);
}

static void test_read_mouse_without_a_current_frame_is_all_zero(void) {
    p8_input_t input;
    p8_input_init(&input);

    p8_mouse_t mouse = p8_input_read_mouse(&input);
    assert(mouse.x == 0 && mouse.y == 0 && mouse.buttons == 0);

    assert(p8_input_load(&input, "tests/fixtures/events_valid.json"));
    input.current_frame = input.frame_count;
    mouse = p8_input_read_mouse(&input);
    assert(mouse.x == 0 && mouse.y == 0 && mouse.buttons == 0);

    p8_input_free(&input);
}

static void test_load_missing_file_is_hard_error(void) {
    p8_input_t input;
    p8_input_init(&input);

    int ok = p8_input_load(&input, "tests/fixtures/does_not_exist.json");
    assert(!ok);

    p8_input_free(&input);
}

static void test_load_malformed_no_frames_key_is_hard_error(void) {
    p8_input_t input;
    p8_input_init(&input);

    int ok = p8_input_load(&input, "tests/fixtures/events_malformed_no_frames_key.json");
    assert(!ok);

    p8_input_free(&input);
}

static char *read_file_raw(const char *path) {
    FILE *f = fopen(path, "rb");
    assert(f);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size >= 0);
    assert(fseek(f, 0, SEEK_SET) == 0);
    char *buf = malloc((size_t)size + 1);
    assert(buf);
    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';
    return buf;
}

/* Offset of the last frame object's line in a recording -- i.e. the length
 * of the "history" region holding every frame recorded before the last
 * one. Used to assert that recording one more frame leaves that region
 * alone; deliberately derived from the file's own content rather than from
 * any constant of the writer's layout. */
static long history_region_length(const char *json) {
    const char *last = NULL;
    const char *p = json;
    while ((p = strstr(p, "    {")) != NULL) {
        last = p;
        p += 5;
    }
    assert(last != NULL);
    return (long)(last - json);
}

/* Recording frame N must
 * not re-emit the N-1 frames already on disk. Proven by marking the
 * already-recorded region with a byte no correct re-serialisation of those
 * frames could produce, then recording one more frame and checking the
 * mark survives -- a writer that rewrites history necessarily overwrites
 * it with the correct value. */
static void test_append_does_not_rewrite_already_recorded_frames(void) {
    const char *path = "tmp/test_input_append_history.json";
    long tail_offset = 0;

    for (int i = 0; i < 40; i++) {
        p8_input_frame_t fr = {0};
        fr.pad_buttons = i % 2 ? P8_PAD_BUTTON_1 : P8_PAD_BUTTON_2 | P8_PAD_BUTTON_5;
        assert(p8_input_append(&fr, path, &tail_offset, NULL));
    }

    char *before = read_file_raw(path);
    long history_len = history_region_length(before);
    char *victim = strstr(before, "[1]");
    assert(victim != NULL);
    long victim_offset = (long)(victim - before) + 1;
    assert(victim_offset < history_len);
    free(before);

    FILE *f = fopen(path, "r+b");
    assert(f);
    assert(fseek(f, victim_offset, SEEK_SET) == 0);
    assert(fputc('9', f) == '9');
    assert(fclose(f) == 0);

    char *marked = read_file_raw(path);

    p8_input_frame_t extra = {0};
    extra.pad_buttons = P8_PAD_BUTTON_11;
    assert(p8_input_append(&extra, path, &tail_offset, NULL));

    char *after = read_file_raw(path);
    assert(after[victim_offset] == '9');
    assert(memcmp(marked, after, (size_t)history_len) == 0);

    free(marked);
    free(after);
}

static void test_append_with_zero_tail_offset_starts_a_fresh_recording(void) {
    const char *path = "tmp/test_input_append_fresh.json";
    p8_input_frame_t first[3] = {{0}, {0}, {0}};
    first[0].pad_buttons = P8_PAD_BUTTON_1;
    first[1].pad_buttons = P8_PAD_BUTTON_2;
    first[2].pad_buttons = P8_PAD_BUTTON_3;
    long tail_offset = 0;
    for (int i = 0; i < 3; i++) {
        assert(p8_input_append(&first[i], path, &tail_offset, NULL));
    }

    p8_input_frame_t only = {0};
    only.pad_buttons = P8_PAD_BUTTON_11;
    tail_offset = 0;
    assert(p8_input_append(&only, path, &tail_offset, NULL));

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 1);
    assert(input.frames[0].pad_buttons == P8_PAD_BUTTON_11);
    p8_input_free(&input);
}

#define PAD_BUTTON_TOTAL 12
#define PAD_FRAME_COUNT (PAD_BUTTON_TOTAL + 2)

static const int every_pad_button[PAD_BUTTON_TOTAL] = {
    P8_PAD_BUTTON_1, P8_PAD_BUTTON_2, P8_PAD_BUTTON_3, P8_PAD_BUTTON_4, P8_PAD_BUTTON_5, P8_PAD_BUTTON_6,
    P8_PAD_BUTTON_7, P8_PAD_BUTTON_8, P8_PAD_BUTTON_9, P8_PAD_BUTTON_10, P8_PAD_BUTTON_11, P8_PAD_BUTTON_12,
};

/* Every button alone, then all twelve together, then none. */
static void fill_pad_frames(p8_input_frame_t frames[PAD_FRAME_COUNT]) {
    memset(frames, 0, sizeof(p8_input_frame_t) * PAD_FRAME_COUNT);
    for (int i = 0; i < PAD_BUTTON_TOTAL; i++) {
        frames[i].pad_buttons = every_pad_button[i];
        frames[PAD_BUTTON_TOTAL].pad_buttons |= every_pad_button[i];
    }
}

static void test_pad_save_then_load_round_trips_every_button(void) {
    p8_input_frame_t written[PAD_FRAME_COUNT];
    fill_pad_frames(written);
    const char *path = "tmp/test_input_pad_roundtrip.json";

    assert(p8_input_save(written, PAD_FRAME_COUNT, path, NULL));

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == PAD_FRAME_COUNT);
    for (int i = 0; i < PAD_FRAME_COUNT; i++) {
        const p8_input_frame_t *got = &input.frames[i];
        assert(got->pad_buttons == written[i].pad_buttons);
        assert(got->mouse_x == 0 && got->mouse_y == 0 && got->mouse_buttons == 0);
    }
    assert(written[PAD_BUTTON_TOTAL].pad_buttons == (1 << PAD_BUTTON_TOTAL) - 1);
    assert(written[PAD_BUTTON_TOTAL + 1].pad_buttons == 0);
    p8_input_free(&input);
}

static void test_pad_append_is_byte_identical_to_pad_save_every_frame(void) {
    p8_input_frame_t frames[PAD_FRAME_COUNT];
    fill_pad_frames(frames);
    const char *appended_path = "tmp/test_input_pad_append.json";
    const char *reference_path = "tmp/test_input_pad_append_reference.json";
    long tail_offset = 0;

    for (int i = 0; i < PAD_FRAME_COUNT; i++) {
        assert(p8_input_append(&frames[i], appended_path, &tail_offset, NULL));
        assert(p8_input_save(frames, i + 1, reference_path, NULL));

        char *appended = read_file_raw(appended_path);
        char *reference = read_file_raw(reference_path);
        assert(strcmp(appended, reference) == 0);
        free(appended);
        free(reference);
    }

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, appended_path));
    assert(input.frame_count == PAD_FRAME_COUNT);
    assert(input.frames[PAD_BUTTON_TOTAL].pad_buttons == frames[PAD_BUTTON_TOTAL].pad_buttons);
    p8_input_free(&input);
}

static void test_pad_save_writes_the_declaration_and_buttons_in_canonical_order(void) {
    p8_input_frame_t written[3] = {
        {0, 0, 0, P8_PAD_BUTTON_11 | P8_PAD_BUTTON_5 | P8_PAD_BUTTON_1},
        {0, 0, 0, P8_PAD_BUTTON_12 | P8_PAD_BUTTON_11 | P8_PAD_BUTTON_10},
        {0, 0, 0, 0},
    };
    const char *path = "tmp/test_input_pad_shape.json";

    assert(p8_input_save(written, 3, path, NULL));

    char *saved = read_file_raw(path);
    assert(strcmp(saved,
                  "{\n"
                  "  \"input\": \"pad\",\n"
                  "  \"frames\": [\n"
                  "    { \"pad\": [1, 5, 11] },\n"
                  "    { \"pad\": [10, 11, 12] },\n"
                  "    { \"pad\": [] }\n"
                  "  ]\n"
                  "}\n") == 0);
    free(saved);

    /* Cross-check against a committed recording this test did not write --
     * same key names in the same shape, not a self-consistent-but-different
     * schema invented by writer and reader agreeing with each other alone. */
    char *committed = read_file_raw("tests/fixtures/demo_events.json");
    assert(strncmp(committed, "{\n  \"input\": \"pad\",\n  \"frames\": [\n",
                   strlen("{\n  \"input\": \"pad\",\n  \"frames\": [\n")) == 0);
    assert(strstr(committed, "    { \"pad\": [4, 5] },\n") != NULL);
    assert(strstr(committed, "    { \"pad\": [] }\n  ]\n}\n") != NULL);
    free(committed);

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/fixtures/demo_events.json"));
    p8_input_free(&input);
}

static void test_pad_buttons_load_in_any_order(void) {
    p8_input_t input;
    p8_input_init(&input);

    assert(p8_input_load(&input, "tests/fixtures/events_pad_unordered.json"));
    assert(input.frame_count == 3);
    assert(input.frames[0].pad_buttons == (P8_PAD_BUTTON_1 | P8_PAD_BUTTON_5));
    assert(input.frames[1].pad_buttons == (P8_PAD_BUTTON_9 | P8_PAD_BUTTON_8 | P8_PAD_BUTTON_11));
    assert(input.frames[2].pad_buttons == 0);

    p8_input_free(&input);
}

static void assert_load_fails_and_leaves_input_untouched(const char *path) {
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/fixtures/events_pad_unordered.json"));
    p8_input_frame_t *frames_before = input.frames;

    assert(!p8_input_load(&input, path));
    assert(input.frames == frames_before);
    assert(input.frame_count == 3);

    p8_input_free(&input);
}

static void test_malformed_pad_frames_fail_and_leave_input_untouched(void) {
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_pad_unknown_button.json");
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_pad_duplicate_button.json");
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_pad_missing_buttons.json");
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_pad_mouse_field.json");
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_pad_dpad_field.json");
}

static void write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    assert(f);
    assert(fputs(text, f) >= 0);
    assert(fclose(f) == 0);
}

#define PAD_FRAMES_TEXT "  \"frames\": [\n    { \"pad\": [1] },\n    { \"pad\": [] }\n  ]\n"

static void test_select_named_twice_in_a_frame_fails(void) {
    const char *path = "tmp/test_input_select_twice.json";
    write_text(path, "{\n  \"input\": \"pad\",\n  \"frames\": [\n"
                     "    { \"pad\": [12, 1, 12] }\n  ]\n}\n");
    assert_load_fails_and_leaves_input_untouched(path);
    remove(path);
}

#define ONE_PAD_FRAME_TEXT(buttons) "{\n  \"input\": \"pad\",\n  \"frames\": [\n    { \"pad\": " buttons " }\n  ]\n}\n"

static int load_one_pad_frame(const char *buttons_text) {
    const char *path = "tmp/test_input_one_frame.json";
    char text[256];
    snprintf(text, sizeof(text), ONE_PAD_FRAME_TEXT("%s"), buttons_text);
    write_text(path, text);
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 1);
    int buttons = input.frames[0].pad_buttons;
    p8_input_free(&input);
    remove(path);
    return buttons;
}

static void test_pad_button_numbers_load(void) {
    assert(load_one_pad_frame("[1, 5, 11]") == (P8_PAD_BUTTON_1 | P8_PAD_BUTTON_5 | P8_PAD_BUTTON_11));
    assert(load_one_pad_frame("[4, 5]") == (P8_PAD_BUTTON_4 | P8_PAD_BUTTON_5));
    assert(load_one_pad_frame("[12]") == P8_PAD_BUTTON_12);
}

static void test_pad_button_numbers_load_in_any_order(void) {
    assert(load_one_pad_frame("[11, 1, 5]") == (P8_PAD_BUTTON_1 | P8_PAD_BUTTON_5 | P8_PAD_BUTTON_11));
    assert(load_one_pad_frame("[10,9]") == (P8_PAD_BUTTON_9 | P8_PAD_BUTTON_10));
}

static void assert_pad_frame_fails_and_leaves_input_untouched(const char *buttons_text) {
    const char *path = "tmp/test_input_refused_frame.json";
    char text[256];
    snprintf(text, sizeof(text), ONE_PAD_FRAME_TEXT("%s"), buttons_text);
    write_text(path, text);
    assert_load_fails_and_leaves_input_untouched(path);
    remove(path);
}

static void test_pad_button_number_forms_outside_the_grammar_fail(void) {
    assert_pad_frame_fails_and_leaves_input_untouched("[0]");
    assert_pad_frame_fails_and_leaves_input_untouched("[13]");
    assert_pad_frame_fails_and_leaves_input_untouched("[120]");
    assert_pad_frame_fails_and_leaves_input_untouched("[99999999999]");
    assert_pad_frame_fails_and_leaves_input_untouched("[05]");
    assert_pad_frame_fails_and_leaves_input_untouched("[-1]");
    assert_pad_frame_fails_and_leaves_input_untouched("[5.0]");
    assert_pad_frame_fails_and_leaves_input_untouched("[5e0]");
    assert_pad_frame_fails_and_leaves_input_untouched("[5x]");
    assert_pad_frame_fails_and_leaves_input_untouched("[1, 5x]");
    assert_pad_frame_fails_and_leaves_input_untouched("[5\"up\"]");
}

static void test_pad_button_number_repeated_in_a_frame_fails(void) {
    assert_pad_frame_fails_and_leaves_input_untouched("[12, 1, 12]");
    assert_pad_frame_fails_and_leaves_input_untouched("[5, 5]");
}

static void test_pad_button_names_are_refused(void) {
    assert_pad_frame_fails_and_leaves_input_untouched("[\"cross\"]");
    assert_pad_frame_fails_and_leaves_input_untouched("[4, \"cross\"]");
    assert_pad_frame_fails_and_leaves_input_untouched("[13]");
}

static void test_recording_without_the_pad_header_fails(void) {
    const char *path = "tmp/test_input_no_header.json";
    write_text(path, "{\n" PAD_FRAMES_TEXT "}\n");
    assert_load_fails_and_leaves_input_untouched(path);
}

static void test_input_other_than_pad_fails(void) {
    const char *path = "tmp/test_input_other_input.json";
    write_text(path, "{\n  \"input\": \"mouse\",\n" PAD_FRAMES_TEXT "}\n");
    assert_load_fails_and_leaves_input_untouched(path);
    write_text(path, "{\n  \"input\": \"gamepad\",\n" PAD_FRAMES_TEXT "}\n");
    assert_load_fails_and_leaves_input_untouched(path);
}

static void test_input_after_the_frames_is_not_a_header(void) {
    const char *path = "tmp/test_input_after_frames.json";
    write_text(path, "{\n" PAD_FRAMES_TEXT "  ,\"input\": \"pad\"\n}\n");
    assert_load_fails_and_leaves_input_untouched(path);
}

/* Byte for byte what the recorder wrote before the pad was the only input. */
static void test_old_mouse_recording_fails(void) {
    const char *path = "tmp/test_input_old_mouse.json";
    write_text(path,
               "{\n  \"frames\": [\n"
               "    { \"mouse_x\": 64, \"mouse_y\": 64, \"mouse_buttons\": 1, "
               "\"dpad\": [false, false, false, false, false, false] }\n"
               "  ]\n}\n");
    assert_load_fails_and_leaves_input_untouched(path);
    assert_load_fails_and_leaves_input_untouched("tests/fixtures/events_mouse_recording_refused.json");
}

static void test_unknown_header_members_are_ignored(void) {
    const char *path = "tmp/test_input_unknown_member.json";
    write_text(path, "{\n  \"version\": 2,\n  \"input\": \"pad\",\n  \"note\": \"x\",\n" PAD_FRAMES_TEXT "}\n");
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 2);
    assert(input.frames[0].pad_buttons == P8_PAD_BUTTON_1 && input.frames[1].pad_buttons == 0);
    p8_input_free(&input);
}

static void test_captions_member_is_ignored(void) {
    const char *path = "tmp/test_input_captions.json";
    write_text(path, "{\n  \"input\": \"pad\",\n  \"start\": \"resume\",\n  \"initial_save\": \"00a1\",\n"
                     "  \"captions\": [\n"
                     "    { \"from\": 0, \"to\": 150, \"text\": \"build a trading empire\\nacross the stars\" },\n"
                     "    { \"from\": 150, \"to\": 330, \"text\": \"spend science on new tech\" }\n"
                     "  ],\n" PAD_FRAMES_TEXT "}\n");
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 2);
    assert(input.frames[0].pad_buttons == P8_PAD_BUTTON_1 && input.frames[1].pad_buttons == 0);
    p8_input_free(&input);
    remove(path);
}

static void test_append_writes_a_header_member_into_a_fresh_file_only(void) {
    const char *path = "tmp/test_input_append_header_member.json";
    const char *member = "\"initial_save\": \"00ff\"";
    p8_input_frame_t frames[3] = {{0}, {0}, {0}};
    frames[0].pad_buttons = P8_PAD_BUTTON_1;
    frames[2].pad_buttons = P8_PAD_BUTTON_5;
    long tail_offset = 0;
    for (int i = 0; i < 3; i++) {
        assert(p8_input_append(&frames[i], path, &tail_offset, member));
    }

    char *text = read_file_raw(path);
    const char *expected_head = "{\n  \"input\": \"pad\",\n  \"initial_save\": \"00ff\",\n  \"frames\": [\n";
    assert(strncmp(text, expected_head, strlen(expected_head)) == 0);
    const char *first = strstr(text, "initial_save");
    assert(first && !strstr(first + 1, "initial_save"));
    free(text);

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 3);
    for (int i = 0; i < 3; i++) {
        assert(input.frames[i].pad_buttons == frames[i].pad_buttons);
    }
    p8_input_free(&input);
    remove(path);
}

static void test_save_writes_header_members_after_the_input_declaration(void) {
    const char *path = "tmp/test_input_save_header_members.json";
    p8_input_frame_t frames[2] = {{0, 0, 0, P8_PAD_BUTTON_5}, {0, 0, 0, 0}};

    assert(p8_input_save(frames, 2, path, "\"start\": \"resume\",\n  \"initial_save\": \"00ff\""));

    char *text = read_file_raw(path);
    assert(strcmp(text, "{\n"
                        "  \"input\": \"pad\",\n"
                        "  \"start\": \"resume\",\n"
                        "  \"initial_save\": \"00ff\",\n"
                        "  \"frames\": [\n"
                        "    { \"pad\": [5] },\n"
                        "    { \"pad\": [] }\n"
                        "  ]\n"
                        "}\n") == 0);
    free(text);

    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, path));
    assert(input.frame_count == 2);
    assert(input.frames[0].pad_buttons == P8_PAD_BUTTON_5 && input.frames[1].pad_buttons == 0);
    p8_input_free(&input);
    remove(path);
}

static void test_save_rejects_empty_frame_list(void) {
    const char *path = "tmp/test_input_save_empty.json";
    int saved = p8_input_save(NULL, 0, path, NULL);
    assert(!saved);
}

int main(void) {
    test_load_and_step_multi_frame_matches_fields();
    test_frames_without_cross_or_circle_load_them_released();
    test_cross_and_circle_load();
    test_read_mouse_without_a_current_frame_is_all_zero();
    test_load_missing_file_is_hard_error();
    test_load_malformed_no_frames_key_is_hard_error();
    test_save_rejects_empty_frame_list();
    test_pad_save_then_load_round_trips_every_button();
    test_pad_append_is_byte_identical_to_pad_save_every_frame();
    test_pad_save_writes_the_declaration_and_buttons_in_canonical_order();
    test_pad_buttons_load_in_any_order();
    test_malformed_pad_frames_fail_and_leave_input_untouched();
    test_select_named_twice_in_a_frame_fails();
    test_pad_button_numbers_load();
    test_pad_button_numbers_load_in_any_order();
    test_pad_button_number_forms_outside_the_grammar_fail();
    test_pad_button_number_repeated_in_a_frame_fails();
    test_pad_button_names_are_refused();
    test_recording_without_the_pad_header_fails();
    test_input_other_than_pad_fails();
    test_input_after_the_frames_is_not_a_header();
    test_old_mouse_recording_fails();
    test_unknown_header_members_are_ignored();
    test_captions_member_is_ignored();
    test_append_does_not_rewrite_already_recorded_frames();
    test_append_with_zero_tail_offset_starts_a_fresh_recording();
    test_append_writes_a_header_member_into_a_fresh_file_only();
    test_save_writes_header_members_after_the_input_declaration();

    printf("test_input: all tests passed\n");
    return 0;
}
