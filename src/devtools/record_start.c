#include "devtools/record_start.h"

#include "game/save_state.h"

#include <assert.h>
#include <stdio.h>

static save_state_t g_state;

static int hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int refuse(const char *path, const char *reason, char *error, size_t error_size) {
    snprintf(error, error_size, "%s: %s", path, reason);
    return 0;
}

static int read_line(FILE *file, const char *path, uint8_t *bytes, int capacity, char *error, size_t error_size) {
    int count = 0;
    int high;

    while ((high = fgetc(file)) != '\n') {
        int low;
        if (high == EOF) return refuse(path, "no newline after the hex digits", error, error_size);
        if (hex_digit(high) < 0) return refuse(path, "not a hex digit", error, error_size);
        low = fgetc(file);
        if (low == '\n' || low == EOF) return refuse(path, "an odd number of hex digits", error, error_size);
        if (hex_digit(low) < 0) return refuse(path, "not a hex digit", error, error_size);
        if (count == capacity || count == SAVE_STATE_MAX_BYTES) return refuse(path, "more than SAVE_STATE_MAX_BYTES bytes", error, error_size);
        bytes[count++] = (uint8_t)(hex_digit(high) << 4 | hex_digit(low));
    }
    if (count == 0) return refuse(path, "no hex digits", error, error_size);
    if (fgetc(file) != EOF) return refuse(path, "something follows the newline", error, error_size);
    if (!save_state_decode(bytes, count, &g_state)) return refuse(path, "the record does not decode", error, error_size);
    return count;
}

int record_start_read_hex(const char *path, uint8_t *bytes, int capacity, char *error, size_t error_size) {
    FILE *file;
    int count;

    assert(path != NULL && bytes != NULL && error != NULL && error_size > 0);
    file = fopen(path, "r");
    if (file == NULL) return refuse(path, "cannot open the file", error, error_size);
    count = read_line(file, path, bytes, capacity, error, error_size);
    fclose(file);
    return count;
}

int record_start_resume(input_step_t *step, const uint8_t *bytes, int count) {
    assert(step != NULL && bytes != NULL);
    if (!save_state_decode(bytes, count, &g_state)) return 0;
    input_step_start_record(step, &g_state);
    return 1;
}

void record_start_return(input_step_t *step) {
    assert(step != NULL);
    input_step_return_to_title(step);
}
