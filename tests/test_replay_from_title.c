/* tmp/replay replays every file from the title screen: the X the file
 * presses on a title row picks the difficulty, and the driver picks none
 * before the first frame.
 *
 * The two fixtures differ only by the Down press at frame 10, which moves
 * the title focus from forgiving to reasonable before X picks the row at
 * frame 12, held through the loading screen to frame 14; R1 at frames 22
 * and 27 then opens the tech panel, then the
 * score panel, whose score row
 * shows the name of the chosen difficulty button (economy_score_difficulty,
 * read by score_panel_display.c). */

#include "devtools/replay_source.h"
#include "p8/input.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRIVER "./tmp/replay"
#define FORGIVING "tests/fixtures/title_pick_forgiving.json"
#define REASONABLE "tests/fixtures/title_pick_reasonable.json"
#define CAPTURE "tmp/test_replay_from_title.bin"
#define DUMP_A "tmp/test_replay_from_title_a.txt"
#define DUMP_B "tmp/test_replay_from_title_b.txt"
#define INTERRUPTED "tmp/test_replay_from_title_interrupted.json"
#define ATTRACT_STATE "tmp/test_replay_from_title_attract.txt"

enum { ATTRACT_AFTER = 3, DEMO_FRAMES_BEFORE_THE_PRESS = 20, TITLE_FRAMES_AFTER_THE_PRESS = 2 };

/* Allocated buffer, freed by the caller. */
static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size > 0);
    rewind(file);
    char *buf = malloc((size_t)size);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    *out_len = (size_t)size;
    return buf;
}

static void replay(const char *events, const char *dump) {
    char cmd[512];
    remove(dump);
    snprintf(cmd, sizeof cmd, DRIVER " %s " CAPTURE " %s > /dev/null", events, dump);
    assert(system(cmd) == 0);
}

static int same_final_screen(void) {
    replay(FORGIVING, DUMP_A);
    replay(REASONABLE, DUMP_B);
    size_t a_len, b_len;
    char *a = read_whole_file(DUMP_A, &a_len);
    char *b = read_whole_file(DUMP_B, &b_len);
    int same = a_len == b_len && memcmp(a, b, a_len) == 0;
    free(a);
    free(b);
    return same;
}

static void test_title_pick_chooses_the_difficulty(void) {
    assert(!same_final_screen() &&
           "the title-screen pick must choose the difficulty shown in the score panel");
}

static void write_interrupted_demo(const char *path) {
    enum { FRAMES = ATTRACT_AFTER + DEMO_FRAMES_BEFORE_THE_PRESS + 1 + TITLE_FRAMES_AFTER_THE_PRESS };
    static p8_input_frame_t frames[FRAMES];
    char member[64];
    frames[ATTRACT_AFTER + DEMO_FRAMES_BEFORE_THE_PRESS].pad_buttons = 1;
    assert(replay_source_format_attract_after(member, sizeof member, ATTRACT_AFTER) > 0);
    assert(p8_input_save(frames, FRAMES, path, member));
}

static void test_an_interrupted_demo_writes_no_attract_state(void) {
    write_interrupted_demo(INTERRUPTED);
    remove(ATTRACT_STATE ".1");
    remove(ATTRACT_STATE ".1.return");
    remove(ATTRACT_STATE ".1.return.hex");
    assert(system("ATTRACT_STATE=" ATTRACT_STATE " " DRIVER " " INTERRUPTED " /dev/null " DUMP_A " > /dev/null") == 0);
    assert(fopen(ATTRACT_STATE ".1", "r") == NULL && "only a playback that reached the demo's last frame writes its state");
    assert(fopen(ATTRACT_STATE ".1.return", "r") == NULL && "an interrupted playback writes no return state");
    assert(fopen(ATTRACT_STATE ".1.return.hex", "r") == NULL && "an interrupted playback writes no return record");
}

int main(void) {
    test_title_pick_chooses_the_difficulty();
    test_an_interrupted_demo_writes_no_attract_state();
    remove(INTERRUPTED);
    remove(CAPTURE);
    remove(DUMP_A);
    remove(DUMP_B);
    printf("test_replay_from_title: all tests passed\n");
    return 0;
}
