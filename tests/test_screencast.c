/* The capture format seen from outside: pixels at given positions, a
 * frame count, refusals of invalid files -- never the presence of an
 * internal field, function or structure. The working files are written
 * under tmp/ and removed. */

#include "devtools/screencast.h"

#include "p8/gfx.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* A pixel's value is its index plus the seed, on four bits: two frames
 * with distinct seeds differ almost everywhere, which gives the
 * assertions below the power to tell frame N from frame N+1, and so to
 * fail on a shifted offset or on two swapped frames. A constant pattern
 * would make them all blind. */
static void paint_frame(p8_vram_t *mem, int seed) {
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            mem->screen[y][x] = (unsigned char)((y * P8_SCREEN_WIDTH + x + seed) & 0x0f);
        }
    }
}

static unsigned char expected_pixel(int seed, int x, int y) {
    return (unsigned char)(((y * P8_SCREEN_WIDTH + x) + seed) & 0x0f);
}

static unsigned char pixel_at(const unsigned char *frame, int x, int y) {
    return frame[y * P8_SCREEN_WIDTH + x];
}

/* Writes `count` frames with seeds 0..count-1 to `path`. */
static void write_seeded_frames(const char *path, int count) {
    screencast_writer_t writer;
    p8_vram_t mem;

    p8_vram_init(&mem);
    assert(screencast_writer_open(&writer, path) == 1);
    for (int seed = 0; seed < count; seed++) {
        paint_frame(&mem, seed);
        assert(screencast_write_frame(&writer, &mem) == 1);
    }
    assert(screencast_writer_close(&writer) == 1);
}

/* The round trip: what was written from the game memory reads back pixel
 * for pixel. The asserted positions cover the four corners and the
 * centre, and both nibbles of one byte (an even and an odd column), the
 * only place where swapped nibbles would show. */
static void test_written_frame_reads_back_pixel_for_pixel(void) {
    const char *path = "tmp/test_screencast_roundtrip.bin";
    screencast_reader_t reader;
    unsigned char frame[SCREENCAST_FRAME_PIXELS];

    write_seeded_frames(path, 3);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_read_frame(&reader, 0, frame) == 1);

    assert(pixel_at(frame, 0, 0) == expected_pixel(0, 0, 0));
    assert(pixel_at(frame, 1, 0) == expected_pixel(0, 1, 0));
    assert(pixel_at(frame, 159, 0) == expected_pixel(0, 159, 0));
    assert(pixel_at(frame, 64, 64) == expected_pixel(0, 64, 64));
    assert(pixel_at(frame, 0, 119) == expected_pixel(0, 0, 119));
    assert(pixel_at(frame, 159, 119) == expected_pixel(0, 159, 119));

    screencast_reader_close(&reader);
    remove(path);
}

/* The frame count is the file size divided by the size of a frame, and
 * nothing else: the format has no header nor index where a counter could
 * hide. The assertion on the file size is the other half of the same
 * claim, and it is the one that pins 9 600 bytes per frame. */
static void test_frame_count_is_file_size_divided_by_frame_size(void) {
    const char *path = "tmp/test_screencast_count.bin";
    screencast_reader_t reader;

    write_seeded_frames(path, 5);

    FILE *raw = fopen(path, "rb");
    assert(raw != NULL);
    assert(fseek(raw, 0, SEEK_END) == 0);
    assert(ftell(raw) == 5L * 9600L);
    fclose(raw);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_frame_count(&reader) == 5);

    screencast_reader_close(&reader);
    remove(path);
}

/* Each frame of the file is its own, in the order it was written. The
 * assertions bear on pixel values and not on the frame count, because a
 * source that served the frames backwards would pass a test that counted
 * them. */
static void test_each_frame_holds_its_own_pixels_in_order(void) {
    const char *path = "tmp/test_screencast_in_order.bin";
    screencast_reader_t reader;
    unsigned char frame[SCREENCAST_FRAME_PIXELS];

    write_seeded_frames(path, 6);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_frame_count(&reader) == 6);

    for (int seed = 0; seed < 6; seed++) {
        assert(screencast_read_frame(&reader, seed, frame) == 1);
        assert(pixel_at(frame, 0, 0) == expected_pixel(seed, 0, 0));
        assert(pixel_at(frame, 3, 0) == expected_pixel(seed, 3, 0));
        assert(pixel_at(frame, 64, 64) == expected_pixel(seed, 64, 64));
        assert(pixel_at(frame, 159, 119) == expected_pixel(seed, 159, 119));
    }

    screencast_reader_close(&reader);
    remove(path);
}

/* Nothing in the format forces frames to be read in order, and a read
 * that depended on the order would belie "frame N is at offset
 * N x 9600". The tested order reads backwards, skips, and reads a frame
 * again; the pixels must be the same as in a sequential read. */
static void test_frames_can_be_read_out_of_sequence(void) {
    const char *path = "tmp/test_screencast_out_of_order.bin";
    screencast_reader_t reader;
    unsigned char frame[SCREENCAST_FRAME_PIXELS];
    static const int order[] = { 4, 0, 3, 1, 4, 2 };

    write_seeded_frames(path, 5);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);

    for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        int seed = order[i];
        assert(screencast_read_frame(&reader, seed, frame) == 1);
        assert(pixel_at(frame, 0, 0) == expected_pixel(seed, 0, 0));
        assert(pixel_at(frame, 1, 0) == expected_pixel(seed, 1, 0));
        assert(pixel_at(frame, 70, 12) == expected_pixel(seed, 70, 12));
        assert(pixel_at(frame, 159, 119) == expected_pixel(seed, 159, 119));
    }

    screencast_reader_close(&reader);
    remove(path);
}

/* A missing file is refused at opening, with a result of its own: a
 * window opened on black cannot be told from a session that starts on a
 * black screen. */
static void test_missing_file_is_refused(void) {
    screencast_reader_t reader;

    assert(screencast_reader_open(&reader, "tmp/test_screencast_no_such_file.bin") ==
           SCREENCAST_UNREADABLE);
    assert(screencast_frame_count(&reader) == 0);

    screencast_reader_close(&reader);
}

/* Writes `size` arbitrary bytes to `path`, to build the files that
 * reading must refuse. */
static void write_raw_bytes(const char *path, long size) {
    FILE *raw = fopen(path, "wb");
    assert(raw != NULL);
    for (long i = 0; i < size; i++) {
        assert(fputc((int)(i & 0xff), raw) != EOF);
    }
    assert(fclose(raw) == 0);
}

/* An empty file is refused and is not a capture of zero frames: zero is a
 * multiple of 9 600, so without this explicit refusal it would pass the
 * size validation, and a reader would be handed a capture with no frame. */
static void test_empty_file_is_refused(void) {
    const char *path = "tmp/test_screencast_empty.bin";
    screencast_reader_t reader;

    write_raw_bytes(path, 0);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_EMPTY);
    assert(screencast_frame_count(&reader) == 0);

    screencast_reader_close(&reader);
    remove(path);
}

/* A size that is not a multiple of the frame size describes an incomplete
 * frame: a truncated file, or an interrupted write. Refusing it at opening
 * is what keeps an incomplete file from being taken for a short session.
 * The two tested sizes are the two shapes of the case: 9599, less than
 * one frame; 2 x 9600 + 1, whole frames followed by a remainder. */
static void test_size_that_is_not_a_whole_number_of_frames_is_refused(void) {
    const char *short_path = "tmp/test_screencast_short.bin";
    const char *ragged_path = "tmp/test_screencast_ragged.bin";
    screencast_reader_t reader;

    write_raw_bytes(short_path, 9599);
    assert(screencast_reader_open(&reader, short_path) == SCREENCAST_TRUNCATED);
    assert(screencast_frame_count(&reader) == 0);
    screencast_reader_close(&reader);
    remove(short_path);

    write_raw_bytes(ragged_path, 2L * 9600L + 1L);
    assert(screencast_reader_open(&reader, ragged_path) == SCREENCAST_TRUNCATED);
    assert(screencast_frame_count(&reader) == 0);
    screencast_reader_close(&reader);
    remove(ragged_path);
}

/* A write failure is returned to the caller rather than silently
 * suffered, and no file is left behind: a partial file would yield a
 * wrong sha256, indistinguishable from a regression. */
static void test_writer_reports_a_path_it_cannot_open(void) {
    const char *path = "tmp/test_screencast_no_such_dir/capture.bin";
    screencast_writer_t writer;
    screencast_reader_t reader;

    assert(screencast_writer_open(&writer, path) == 0);
    assert(screencast_reader_open(&reader, path) == SCREENCAST_UNREADABLE);

    screencast_reader_close(&reader);
}

/* Reads the whole file and returns its size. */
static long read_whole_file(const char *path, unsigned char *out, long capacity) {
    FILE *raw = fopen(path, "rb");
    assert(raw != NULL);
    size_t got = fread(out, 1, (size_t)capacity, raw);
    assert(fclose(raw) == 0);
    return (long)got;
}

/* A round trip through reading does not change one byte of the file.
 * The sha256 of `make e2e` depends on this property: the day a read
 * modified the file it reads, looking at a capture would be enough to
 * make it diverge. */
static void test_reading_leaves_the_file_byte_for_byte_unchanged(void) {
    const char *path = "tmp/test_screencast_readonly.bin";
    screencast_reader_t reader;
    unsigned char frame[SCREENCAST_FRAME_PIXELS];
    static unsigned char before[3 * 9600];
    static unsigned char after[sizeof(before)];

    write_seeded_frames(path, 3);
    assert(read_whole_file(path, before, (long)sizeof(before)) == (long)sizeof(before));

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_read_frame(&reader, 2, frame) == 1);
    assert(screencast_read_frame(&reader, 0, frame) == 1);
    assert(screencast_read_frame(&reader, 1, frame) == 1);
    screencast_reader_close(&reader);

    assert(read_whole_file(path, after, (long)sizeof(after)) == (long)sizeof(after));
    assert(memcmp(before, after, sizeof(before)) == 0);

    remove(path);
}

/* Asking for a frame the file does not have is refused, and the caller's
 * buffer is left untouched: without the refusal, a caller that read past
 * the end would take the last frame it really read, still in its buffer,
 * for a new one. */
static void test_frame_index_past_the_end_is_refused(void) {
    const char *path = "tmp/test_screencast_past_end.bin";
    screencast_reader_t reader;
    unsigned char frame[SCREENCAST_FRAME_PIXELS];

    write_seeded_frames(path, 2);

    assert(screencast_reader_open(&reader, path) == SCREENCAST_OK);
    assert(screencast_read_frame(&reader, 1, frame) == 1);

    assert(screencast_read_frame(&reader, 2, frame) == 0);
    assert(screencast_read_frame(&reader, 4242, frame) == 0);

    /* The buffer still holds the last frame actually read. */
    assert(pixel_at(frame, 0, 0) == expected_pixel(1, 0, 0));
    assert(pixel_at(frame, 159, 119) == expected_pixel(1, 159, 119));

    screencast_reader_close(&reader);
    remove(path);
}

/* Writing a frame leaves the game memory alone, the screen and the rest:
 * a capture that changed what it observes would void the whole
 * instrument. The `const` of the signature already tells the compiler;
 * this test checks it on the whole memory, including what a future way
 * around the `const` would reach. The byte set off screen (a map cell)
 * is what gives the assertion a hold on the rest of the memory, and not
 * only on an area left at zero. */
static void test_writing_a_frame_leaves_the_game_memory_untouched(void) {
    const char *path = "tmp/test_screencast_readonly_memory.bin";
    screencast_writer_t writer;
    static p8_vram_t mem;
    static p8_vram_t before;

    p8_vram_init(&mem);
    paint_frame(&mem, 7);
    mem.map_cells[0][0] = 0x42;
    before = mem;

    assert(screencast_writer_open(&writer, path) == 1);
    assert(screencast_write_frame(&writer, &mem) == 1);
    assert(screencast_writer_close(&writer) == 1);

    assert(memcmp(&before, &mem, sizeof(mem)) == 0);

    remove(path);
}

/* The format as seen from outside the project: each byte holds the
 * even-indexed pixel of its pair in its low nibble and the odd-indexed
 * one in its high nibble. Hence the asserted values: pixels 0x03 and 0x0c
 * give the byte 0xc3, pixels 0x0f and 0x00 give 0x0f. A round trip cannot
 * prove this order -- writing and reading with the nibbles swapped on
 * both sides would give back the same pixels -- yet it is what is
 * promised to a throwaway script that reads the file without depending
 * on the project's code. */
static void test_on_disk_byte_holds_the_even_pixel_in_its_low_nibble(void) {
    const char *path = "tmp/test_screencast_nibbles.bin";
    screencast_writer_t writer;
    p8_vram_t mem;
    static unsigned char raw[9600];

    p8_vram_init(&mem);
    paint_frame(&mem, 0);
    mem.screen[0][0] = 0x03;
    mem.screen[0][1] = 0x0c;
    mem.screen[0][2] = 0x0f;
    mem.screen[0][3] = 0x00;

    assert(screencast_writer_open(&writer, path) == 1);
    assert(screencast_write_frame(&writer, &mem) == 1);
    assert(screencast_writer_close(&writer) == 1);

    assert(read_whole_file(path, raw, (long)sizeof(raw)) == (long)sizeof(raw));
    assert(raw[0] == 0xc3);
    assert(raw[1] == 0x0f);
    assert(raw[79] >> 4 == mem.screen[0][159]);
    assert((raw[119 * 80] & 0x0f) == mem.screen[119][0]);
    assert(raw[9599] >> 4 == mem.screen[119][159]);

    remove(path);
}

int main(void) {
    test_written_frame_reads_back_pixel_for_pixel();
    test_frame_count_is_file_size_divided_by_frame_size();
    test_each_frame_holds_its_own_pixels_in_order();
    test_frames_can_be_read_out_of_sequence();
    test_missing_file_is_refused();
    test_empty_file_is_refused();
    test_size_that_is_not_a_whole_number_of_frames_is_refused();
    test_writer_reports_a_path_it_cannot_open();
    test_reading_leaves_the_file_byte_for_byte_unchanged();
    test_frame_index_past_the_end_is_refused();
    test_writing_a_frame_leaves_the_game_memory_untouched();
    test_on_disk_byte_holds_the_even_pixel_in_its_low_nibble();
    printf("test_screencast: all tests passed\n");
    return 0;
}
