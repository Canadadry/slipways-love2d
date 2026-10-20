#include "devtools/pad_caption.h"
#include "devtools/replay_source.h"
#include "game/demo_data.h"
#include "game/save_state.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char DEMO_PATH[] = "demo/demo.json";

static replay_source_t g_source;

static void test_compiled_frames_are_the_json_frames(void) {
    assert(demo_data_frame_count == replay_source_frame_count(&g_source));
    for (int frame = 0; frame < demo_data_frame_count; frame++) {
        assert(demo_data_pad_buttons[frame] == g_source.recording.frames[frame].pad_buttons);
    }
}

static void test_compiled_record_is_the_json_initial_save_and_decodes(void) {
    assert(demo_data_record_count == g_source.initial_save_count);
    for (int i = 0; i < demo_data_record_count; i++) {
        assert(demo_data_record[i] == g_source.initial_save[i]);
    }
    static save_state_t decoded;
    assert(save_state_decode(demo_data_record, demo_data_record_count, &decoded) && "the compiled record decodes");
}

static char g_json[1 << 20];

static void read_json(void) {
    FILE *file = fopen(DEMO_PATH, "rb");
    assert(file != NULL);
    size_t length = fread(g_json, 1, sizeof g_json - 1, file);
    assert(feof(file) && "demo/demo.json fits the test's buffer");
    fclose(file);
    g_json[length] = '\0';
}

/* The replay reader ignores captions: pad_caption_write_member is the independent witness. */
static void test_compiled_captions_write_back_the_json_member(void) {
    assert(demo_data_caption_count > 0);
    static pad_caption_t captions[DEMO_DATA_FRAME_LIMIT / PAD_CAPTION_SHOWN_MIN + 1];
    assert(demo_data_caption_count <= (int)(sizeof captions / sizeof captions[0]));
    for (int i = 0; i < demo_data_caption_count; i++) {
        const demo_data_caption_t *compiled = &demo_data_captions[i];
        assert(memchr(compiled->text, '\0', sizeof compiled->text) != NULL && "the text fits its array");
        captions[i].from = compiled->from;
        captions[i].to = compiled->to;
        snprintf(captions[i].text, sizeof captions[i].text, "%s", compiled->text);
    }
    pad_caption_track_t track = {
        .captions = captions, .capacity = demo_data_caption_count, .count = demo_data_caption_count};
    static char member[1 << 16];
    int length = pad_caption_write_member(&track, member, sizeof member);
    assert(length > 0 && length < (int)sizeof member - 1);
    read_json();
    assert(strstr(g_json, member) != NULL && "the compiled captions are the file's member, byte for byte");
}

int main(void) {
    assert(replay_source_open(&g_source, DEMO_PATH) == REPLAY_SOURCE_LOADED);
    test_compiled_frames_are_the_json_frames();
    test_compiled_record_is_the_json_initial_save_and_decodes();
    test_compiled_captions_write_back_the_json_member();
    replay_source_close(&g_source);
    printf("test_demo_data: ok\n");
    return 0;
}
