#include "devtools/demo_source.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define WORK_DIR "tmp/test_demo_source_work"
#define DEMO_PATH WORK_DIR "/demo.json"
#define FRAMES_MAX (DEMO_DATA_FRAME_LIMIT + 1)

static const unsigned char RECORD[] = {0x53, 0x4c, 0x57, 0x59, 0x01, 0xab};

typedef struct {
    p8_input_frame_t frames[FRAMES_MAX];
    int frame_count;
    pad_caption_t captions[DEMO_SOURCE_CAPTIONS_MAX];
    int caption_count;
} fixture_t;

static fixture_t fixture;

static void fixture_init(int frame_count) {
    assert(frame_count <= FRAMES_MAX);
    memset(fixture.frames, 0, sizeof fixture.frames);
    fixture.frame_count = frame_count;
    fixture.caption_count = 0;
}

/* Unchecked, so a refused fixture is still written by pad_caption_write_member. */
static void fixture_caption(int from, int to, const char *text) {
    assert(fixture.caption_count < DEMO_SOURCE_CAPTIONS_MAX);
    pad_caption_t *caption = &fixture.captions[fixture.caption_count++];
    caption->from = from;
    caption->to = to;
    snprintf(caption->text, sizeof caption->text, "%s", text);
}

static void fixture_write(void) {
    pad_caption_track_t track = {
        .captions = fixture.captions, .capacity = DEMO_SOURCE_CAPTIONS_MAX, .count = fixture.caption_count};
    static char header[8192];
    int used = snprintf(header, sizeof header, "\"start\": \"resume\",\n  \"initial_save\": \"");
    for (size_t i = 0; i < sizeof RECORD; i++) {
        used += snprintf(header + used, sizeof header - (size_t)used, "%02x", RECORD[i]);
    }
    used += snprintf(header + used, sizeof header - (size_t)used, "\"");
    if (track.count > 0) {
        used += snprintf(header + used, sizeof header - (size_t)used, ",\n  ");
        pad_caption_write_member(&track, header + used, sizeof header - (size_t)used);
    }
    assert(mkdir(WORK_DIR, 0777) == 0 || errno == EEXIST);
    assert(p8_input_save(fixture.frames, fixture.frame_count, DEMO_PATH, header));
}

static void assert_loads(demo_source_t *source) {
    char error[512] = "";
    int ok = demo_source_load(source, DEMO_PATH, error, sizeof error);
    if (!ok) {
        fprintf(stderr, "refused: %s\n", error);
    }
    assert(ok);
}

static void test_tiling_captions_a_gap_and_a_press_at_frame_90_load(void) {
    fixture_init(500);
    fixture_caption(0, 150, "build a trading empire\nacross the stars");
    fixture.frames[90].pad_buttons = 0x01;
    fixture.frames[170].pad_buttons = 0x3f;
    fixture_caption(200, 350, "probe the dark");
    fixture_caption(350, 500, "slipways\npress any button");
    fixture.frames[499].pad_buttons = 0x800;
    fixture_write();

    demo_source_t source;
    assert_loads(&source);
    assert(source.caption_count == 3);
    assert(source.captions[0].from == 0 && source.captions[0].to == 150);
    assert(source.captions[1].from == 200 && source.captions[1].to == 350);
    assert(source.captions[2].from == 350 && source.captions[2].to == 500);
    demo_source_close(&source);
}

static void edit_file(const char *old_text, const char *new_text) {
    FILE *file = fopen(DEMO_PATH, "rb");
    assert(file);
    static char text[1 << 18];
    size_t length = fread(text, 1, sizeof text - 1, file);
    fclose(file);
    assert(length < sizeof text - 1);
    text[length] = '\0';
    char *at = strstr(text, old_text);
    assert(at && "the fixture holds the text to edit");
    file = fopen(DEMO_PATH, "wb");
    assert(file);
    fwrite(text, 1, (size_t)(at - text), file);
    fputs(new_text, file);
    fputs(at + strlen(old_text), file);
    assert(fclose(file) == 0);
}

static void assert_refused_at(const char *path, const char *expected_reason) {
    demo_source_t source;
    char error[512] = "";
    char expected[512];
    snprintf(expected, sizeof expected, "%s: %s", path, expected_reason);
    int ok = demo_source_load(&source, path, error, sizeof error);
    if (ok || strcmp(error, expected) != 0) {
        fprintf(stderr, "loaded %d, error '%s', expected '%s'\n", ok, error, expected);
    }
    assert(!ok);
    assert(strcmp(error, expected) == 0);
}

static void assert_refused(const char *expected_reason) {
    assert_refused_at(DEMO_PATH, expected_reason);
}

static void write_one_caption_demo(void) {
    fixture_init(200);
    fixture_caption(0, 200, "probe the dark");
    fixture_write();
}

static void test_one_caption_over_150_frames_loads_with_its_frames_record_and_caption(void) {
    fixture_init(150);
    fixture.frames[120].pad_buttons = 0x10;
    fixture_caption(0, 150, "probe the dark\nto find new planets");
    fixture_write();

    demo_source_t source;
    assert_loads(&source);

    assert(replay_source_frame_count(&source.replay) == 150);
    for (int i = 0; i < 150; i++) {
        assert(source.replay.recording.frames[i].pad_buttons == (i == 120 ? 0x10 : 0));
    }
    assert(source.replay.initial_save_count == (int)sizeof RECORD);
    assert(memcmp(source.replay.initial_save, RECORD, sizeof RECORD) == 0);
    assert(source.caption_count == 1);
    assert(source.captions[0].from == 0);
    assert(source.captions[0].to == 150);
    assert(strcmp(source.captions[0].text, "probe the dark\nto find new planets") == 0);
    demo_source_close(&source);
}

static void test_a_file_the_replay_reader_refuses_is_refused(void) {
    assert_refused_at(WORK_DIR "/no_such_demo.json", "cannot load as a replay");
    write_one_caption_demo();
    edit_file("{ \"pad\": [] }", "{ \"mouse_x\": 64, \"pad\": [] }");
    assert_refused("cannot load as a replay");
}

static void test_a_demo_without_a_resume_start_is_refused(void) {
    write_one_caption_demo();
    edit_file("\"start\": \"resume\",\n  ", "");
    assert_refused("not a from-record demo");
}

static void test_a_bindings_member_is_refused(void) {
    write_one_caption_demo();
    edit_file("\"start\"", "\"bindings\": \"personal\",\n  \"start\"");
    assert_refused("has a \"bindings\" member: the demo plays on the default bindings");
}

static void test_3601_frames_are_refused(void) {
    fixture_init(DEMO_DATA_FRAME_LIMIT + 1);
    fixture_caption(0, DEMO_DATA_FRAME_LIMIT + 1, "probe the dark");
    fixture_write();
    assert_refused("3601 frames, at most 3600");
}

static void test_a_demo_without_a_caption_is_refused(void) {
    fixture_init(200);
    fixture_write();
    assert_refused("no caption");
}

static void test_an_unreadable_captions_member_is_refused_with_the_reader_reason(void) {
    write_one_caption_demo();
    edit_file("probe the dark", "probe\\tthe dark");
    assert_refused("\"captions\": caption 1: escape \\t, only \\n is read");
}

static void test_a_captions_member_not_as_pad_author_writes_it_is_refused(void) {
    write_one_caption_demo();
    edit_file("\"from\": 0,", "\"from\":  0,");
    assert_refused("\"captions\" is not as pad_author writes it");
    write_one_caption_demo();
    edit_file("\" }\n  ]", "\"}\n  ]");
    assert_refused("\"captions\" is not as pad_author writes it");
}

static void write_two_caption_demo(void) {
    fixture_init(300);
    fixture_caption(0, 150, "probe the dark");
    fixture_caption(150, 300, "colonize planets");
    fixture_write();
}

static void test_overlapping_captions_are_refused(void) {
    write_two_caption_demo();
    edit_file("\"from\": 150", "\"from\": 140");
    assert_refused("caption 2: frames 140-300 start before caption 1 ends, at frame 150");
}

static void test_a_caption_past_the_last_frame_is_refused(void) {
    write_one_caption_demo();
    edit_file("\"to\": 200", "\"to\": 201");
    assert_refused("caption 1: frames 0-201 end past the demo's 200 frames");
}

static void test_an_empty_caption_range_is_refused(void) {
    write_one_caption_demo();
    edit_file("\"to\": 200", "\"to\": 0");
    assert_refused("caption 1: frames 0-0 are empty");
    write_two_caption_demo();
    edit_file("\"to\": 300", "\"to\": 100");
    assert_refused("caption 2: frames 150-100 are empty");
}

static void assert_text_refused(const char *text, const char *expected_reason) {
    fixture_init(300);
    fixture_caption(0, 150, "probe the dark");
    fixture_caption(150, 300, text);
    fixture_write();
    assert_refused(expected_reason);
}

static void test_a_caption_text_is_refused_with_the_caption_rules_message(void) {
    assert_text_refused("abcdefghijklmnopqrstuwxyz01234567", "caption 2: caption: line 1 has 33 characters, at most 32");
    assert_text_refused("a\nb\nc", "caption 2: caption: 3 lines, at most 2");
    assert_text_refused("discover new worlds", "caption 2: caption: the letter v draws as u");
    assert_text_refused("trade & grow", "caption 2: caption: no font glyph for '&'");
}

static void test_a_caption_text_the_rules_would_change_is_refused(void) {
    assert_text_refused("a/b", "caption 2: text is not as pad_author writes it");
    assert_text_refused("a \nb", "caption 2: text is not as pad_author writes it");
}

static void test_a_caption_shown_149_frames_is_refused_at_its_first_frame(void) {
    fixture_init(300);
    fixture_caption(0, 149, "probe the dark");
    fixture_caption(149, 300, "colonize planets");
    fixture_write();
    assert_refused("frame 0: caption: shown 149 frames, at least 150");

    fixture_init(400);
    fixture_caption(10, 159, "probe the dark");
    fixture_caption(200, 400, "colonize planets");
    fixture_write();
    assert_refused("frame 10: caption: shown 149 frames, at least 150");
}

static void test_a_last_caption_cut_short_is_refused(void) {
    fixture_init(299);
    fixture_caption(0, 150, "probe the dark");
    fixture_caption(150, 299, "colonize planets");
    fixture_write();
    assert_refused("frame 150: caption: shown 149 frames, at least 150");
}

static void test_a_press_at_frame_89_of_a_caption_is_refused(void) {
    fixture_init(300);
    fixture_caption(0, 150, "probe the dark");
    fixture_caption(150, 300, "colonize planets");
    fixture.frames[239].pad_buttons = 0x01;
    fixture_write();
    assert_refused("frame 239: caption: a button down 89 frames after the caption appeared, at least 90");
}

int main(void) {
    test_one_caption_over_150_frames_loads_with_its_frames_record_and_caption();
    test_tiling_captions_a_gap_and_a_press_at_frame_90_load();
    test_a_file_the_replay_reader_refuses_is_refused();
    test_a_demo_without_a_resume_start_is_refused();
    test_a_bindings_member_is_refused();
    test_3601_frames_are_refused();
    test_a_demo_without_a_caption_is_refused();
    test_an_unreadable_captions_member_is_refused_with_the_reader_reason();
    test_a_captions_member_not_as_pad_author_writes_it_is_refused();
    test_overlapping_captions_are_refused();
    test_a_caption_past_the_last_frame_is_refused();
    test_an_empty_caption_range_is_refused();
    test_a_caption_text_is_refused_with_the_caption_rules_message();
    test_a_caption_text_the_rules_would_change_is_refused();
    test_a_caption_shown_149_frames_is_refused_at_its_first_frame();
    test_a_last_caption_cut_short_is_refused();
    test_a_press_at_frame_89_of_a_caption_is_refused();

    printf("test_demo_source: all tests passed\n");
    return 0;
}
