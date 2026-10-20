/* The frame tool, run the way the messages of make e2e-ref suggest, on a
 * capture made by the real replay driver. Like test_headless_capture,
 * this test rewrites the driver's dump. */

#include "devtools/screencast.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIXTURE "tests/fixtures/screencast_capture_events.json"
#define DRIVER "./tmp/replay"
#define TOOL "./tmp/frames_tool"
#define DRIVER_DUMP "tmp/replay_dump.txt"
#define CAPTURE "tmp/test_frames_tool.bin"
#define FRAME_OUT "tmp/test_frames_tool_frame.txt"
#define STDERR_LOG "tmp/test_frames_tool_stderr.txt"
#define BAD_CAPTURE "tmp/test_frames_tool_bad.bin"
#define MISSING_CAPTURE "tmp/no_such_capture_for_frames_tool.bin"

/* The whole content of `path`, followed by one NUL byte beyond the
 * `*out_len` bytes. Allocated buffer: the caller frees it. Returns NULL if
 * the file does not exist. */
static char *read_whole_file(const char *path, size_t *out_len) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    assert(fseek(file, 0, SEEK_END) == 0);
    long size = ftell(file);
    assert(size >= 0);
    rewind(file);

    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, file) == (size_t)size);
    fclose(file);
    buf[size] = '\0';

    *out_len = (size_t)size;
    return buf;
}

static void capture_fixture(void) {
    remove(CAPTURE);
    assert(system(DRIVER " " FIXTURE " " CAPTURE " > /dev/null") == 0);
}

static void assert_stderr_names(const char *needle) {
    size_t len;
    char *log = read_whole_file(STDERR_LOG, &len);
    assert(log != NULL);
    assert(strstr(log, needle) != NULL);
    free(log);
}

static void test_emitted_last_frame_is_the_driver_dump(void) {
    capture_fixture();

    screencast_reader_t reader;
    assert(screencast_reader_open(&reader, CAPTURE) == SCREENCAST_OK);
    int last = screencast_frame_count(&reader) - 1;
    screencast_reader_close(&reader);

    char command[256];
    snprintf(command, sizeof(command), TOOL " frame " CAPTURE " %d > " FRAME_OUT, last);
    assert(system(command) == 0);

    size_t dump_len;
    char *dump = read_whole_file(DRIVER_DUMP, &dump_len);
    size_t frame_len;
    char *frame = read_whole_file(FRAME_OUT, &frame_len);
    assert(dump != NULL && frame != NULL);
    assert(frame_len == dump_len);
    assert(memcmp(frame, dump, dump_len) == 0);

    /* The frame before differs from the dump: an index off by one would
     * not have passed the comparison above by accident. */
    snprintf(command, sizeof(command), TOOL " frame " CAPTURE " %d > " FRAME_OUT, last - 1);
    assert(system(command) == 0);
    free(frame);
    frame = read_whole_file(FRAME_OUT, &frame_len);
    assert(frame_len != dump_len || memcmp(frame, dump, dump_len) != 0);

    free(frame);
    free(dump);
    remove(FRAME_OUT);
    remove(CAPTURE);
}

static void test_out_of_bounds_frame_fails_and_names_the_index(void) {
    capture_fixture();

    screencast_reader_t reader;
    assert(screencast_reader_open(&reader, CAPTURE) == SCREENCAST_OK);
    int count = screencast_frame_count(&reader);
    screencast_reader_close(&reader);

    char command[256];
    char index_text[32];
    snprintf(index_text, sizeof(index_text), "%d", count);
    snprintf(command, sizeof(command), TOOL " frame " CAPTURE " %d > " FRAME_OUT " 2> " STDERR_LOG, count);
    assert(system(command) != 0);
    assert_stderr_names(index_text);

    assert(system(TOOL " frame " CAPTURE " -1 > " FRAME_OUT " 2> " STDERR_LOG) != 0);
    assert(system(TOOL " frame " CAPTURE " 2x > " FRAME_OUT " 2> " STDERR_LOG) != 0);
    assert_stderr_names("2x");

    remove(FRAME_OUT);
    remove(STDERR_LOG);
    remove(CAPTURE);
}

static void assert_frame_refuses(const char *capture_path) {
    char command[256];
    snprintf(command, sizeof(command), TOOL " frame %s 0 > " FRAME_OUT " 2> " STDERR_LOG, capture_path);
    assert(system(command) != 0);
    assert_stderr_names(capture_path);
}

static void test_truncated_empty_or_missing_capture_is_refused(void) {
    capture_fixture();
    size_t len;
    char *bytes = read_whole_file(CAPTURE, &len);
    assert(bytes != NULL);

    FILE *bad = fopen(BAD_CAPTURE, "wb");
    assert(bad != NULL);
    assert(fwrite(bytes, 1, len - 1, bad) == len - 1);
    fclose(bad);
    assert_frame_refuses(BAD_CAPTURE);

    bad = fopen(BAD_CAPTURE, "wb");
    assert(bad != NULL);
    fclose(bad);
    assert_frame_refuses(BAD_CAPTURE);

    remove(MISSING_CAPTURE);
    assert_frame_refuses(MISSING_CAPTURE);

    free(bytes);
    remove(BAD_CAPTURE);
    remove(FRAME_OUT);
    remove(STDERR_LOG);
    remove(CAPTURE);
}

int main(void) {
    test_emitted_last_frame_is_the_driver_dump();
    test_out_of_bounds_frame_fails_and_names_the_index();
    test_truncated_empty_or_missing_capture_is_refused();
    printf("test_frames_tool: all tests passed\n");
    return 0;
}
