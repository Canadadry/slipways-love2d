#include "devtools/vram_digest.h"
#include "devtools/screencast.h"
#include "p8/sprite_sheet.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIGEST "tmp/test_vram_digest.txt"
#define CAPTURE "tmp/test_vram_digest.bin"

typedef struct {
    char lines[4][64];
    int count;
} captured_t;

static void capture_line(void *context, const char *line) {
    captured_t *captured = context;
    assert(captured->count < 4);
    assert(strlen(line) < sizeof captured->lines[0]);
    strcpy(captured->lines[captured->count++], line);
}

static vram_digest_sink_t sink_into(captured_t *captured) {
    captured->count = 0;
    vram_digest_sink_t sink = { capture_line, captured };
    return sink;
}

/* Published FNV-1a 32-bit test vectors. */
static void test_fnv1a_matches_the_reference_values(void) {
    assert(vram_digest_fnv1a((const unsigned char *)"", 0) == 0x811c9dc5u);
    assert(vram_digest_fnv1a((const unsigned char *)"a", 1) == 0xe40c292cu);
    assert(vram_digest_fnv1a((const unsigned char *)"foobar", 6) == 0xbf9cf968u);
}

static void test_samples_every_32nd_frame_and_the_last(void) {
    int sampled[100];
    int count = 0;
    for (int frame = 0; frame < 100; frame++) {
        if (vram_digest_samples_frame(frame, 100)) sampled[count++] = frame;
    }
    static const int expected[] = {0, 32, 64, 96, 99};
    assert(count == 5);
    assert(memcmp(sampled, expected, sizeof expected) == 0);

    assert(vram_digest_samples_frame(0, 1));
    assert(vram_digest_samples_frame(63, 64));
    assert(!vram_digest_samples_frame(62, 64));
}

static void test_sprite_sheet_line_holds_the_hash_of_the_sheet(void) {
    static p8_vram_t memory;
    p8_vram_init(&memory);
    p8_sprite_sheet_load(&memory);
    captured_t captured;
    vram_digest_sink_t sink = sink_into(&captured);

    vram_digest_write_sprite_sheet(&sink, &memory);

    char expected[64];
    snprintf(expected, sizeof expected, "sprite-sheet %08x\n",
             (unsigned)vram_digest_fnv1a(&memory.sprite_sheet[0][0], sizeof memory.sprite_sheet));
    assert(captured.count == 1);
    assert(strcmp(captured.lines[0], expected) == 0);
}

static void expect_screen_line(const p8_vram_t *memory, int frame) {
    captured_t captured;
    vram_digest_sink_t sink = sink_into(&captured);

    vram_digest_write_screen(&sink, memory, frame);

    char expected[64];
    snprintf(expected, sizeof expected, "screen %d %08x\n", frame,
             (unsigned)vram_digest_fnv1a(&memory->screen[0][0], sizeof memory->screen));
    assert(captured.count == 1);
    assert(strcmp(captured.lines[0], expected) == 0);
}

static void test_screen_line_holds_the_frame_and_the_hash_of_the_screen(void) {
    static p8_vram_t memory;
    p8_vram_init(&memory);
    expect_screen_line(&memory, 0);
    expect_screen_line(&memory, 4777);
    expect_screen_line(&memory, 2147483647);
}

/* Searches pixel values until the hash starts with a zero hex digit, then
   with a letter, so both the padding and the case are pinned. */
static void test_screen_hash_is_zero_padded_and_lowercase(void) {
    static p8_vram_t memory;
    p8_vram_init(&memory);
    int seen_leading_zero = 0;
    int seen_leading_letter = 0;
    for (int value = 0; value < 4096 && !(seen_leading_zero && seen_leading_letter); value++) {
        memory.screen[0][0] = (unsigned char)(value & 0xff);
        memory.screen[0][1] = (unsigned char)(value >> 8);
        uint32_t hash = vram_digest_fnv1a(&memory.screen[0][0], sizeof memory.screen);
        if (hash >> 28 == 0 && !seen_leading_zero) {
            seen_leading_zero = 1;
            expect_screen_line(&memory, 7);
        }
        if (hash >> 28 >= 0xa && !seen_leading_letter) {
            seen_leading_letter = 1;
            expect_screen_line(&memory, 7);
        }
    }
    assert(seen_leading_zero && seen_leading_letter);
}

static void test_one_pixel_changes_the_screen_line(void) {
    static p8_vram_t memory;
    p8_vram_init(&memory);
    captured_t before;
    vram_digest_sink_t sink = sink_into(&before);
    vram_digest_write_screen(&sink, &memory, 32);

    memory.screen[P8_SCREEN_HEIGHT - 1][P8_SCREEN_WIDTH - 1] = 7;
    captured_t after;
    sink = sink_into(&after);
    vram_digest_write_screen(&sink, &memory, 32);

    assert(strcmp(before.lines[0], after.lines[0]) != 0);
}

static void test_map_line_holds_the_hash_of_the_map_cells(void) {
    static p8_vram_t memory;
    p8_vram_init(&memory);
    memory.map_cells[P8_MAP_HEIGHT - 1][P8_MAP_WIDTH - 1] = 0x41;
    captured_t captured;
    vram_digest_sink_t sink = sink_into(&captured);

    vram_digest_write_map(&sink, &memory);

    char expected[64];
    snprintf(expected, sizeof expected, "map %08x\n",
             (unsigned)vram_digest_fnv1a(&memory.map_cells[0][0], sizeof memory.map_cells));
    assert(captured.count == 1);
    assert(strcmp(captured.lines[0], expected) == 0);
}

static void expect_next_line(FILE *digest, const char *expected) {
    char line[64];
    assert(fgets(line, sizeof line, digest) != NULL);
    assert(strcmp(line, expected) == 0);
}

/* Every line is checked against what the replay left elsewhere: the embedded
   sheet, and its capture of each frame after cart_draw. */
static void test_replay_writes_the_digest_file_when_digest_is_set(void) {
    remove(DIGEST);
    assert(system("DIGEST=" DIGEST " ./tmp/replay tests/e2e/pad_title.json " CAPTURE " tmp/test_vram_digest_dump.txt > /dev/null") == 0);
    FILE *digest = fopen(DIGEST, "r");
    assert(digest != NULL && "digest file missing");

    static p8_vram_t memory;
    p8_vram_init(&memory);
    p8_sprite_sheet_load(&memory);
    captured_t captured;
    vram_digest_sink_t sink = sink_into(&captured);
    vram_digest_write_sprite_sheet(&sink, &memory);
    expect_next_line(digest, captured.lines[0]);

    screencast_reader_t capture;
    assert(screencast_reader_open(&capture, CAPTURE) == SCREENCAST_OK);
    int frame_count = screencast_frame_count(&capture);
    assert(frame_count == 32);
    for (int frame = 0; frame < frame_count; frame++) {
        if (!vram_digest_samples_frame(frame, frame_count)) continue;
        assert(screencast_read_frame(&capture, frame, &memory.screen[0][0]));
        sink = sink_into(&captured);
        vram_digest_write_screen(&sink, &memory, frame);
        expect_next_line(digest, captured.lines[0]);
    }
    screencast_reader_close(&capture);

    char line[64];
    assert(fgets(line, sizeof line, digest) != NULL);
    assert(strncmp(line, "map ", 4) == 0 && strlen(line) == strlen("map 01234567\n"));
    assert(strcmp(line, "map 811c9dc5\n") != 0 && "a started game has generated its map");
    assert(fgets(line, sizeof line, digest) == NULL);
    fclose(digest);
}

int main(void) {
    test_fnv1a_matches_the_reference_values();
    test_samples_every_32nd_frame_and_the_last();
    test_sprite_sheet_line_holds_the_hash_of_the_sheet();
    test_screen_line_holds_the_frame_and_the_hash_of_the_screen();
    test_screen_hash_is_zero_padded_and_lowercase();
    test_one_pixel_changes_the_screen_line();
    test_map_line_holds_the_hash_of_the_map_cells();
    test_replay_writes_the_digest_file_when_digest_is_set();
    return 0;
}
