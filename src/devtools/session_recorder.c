#include "devtools/session_recorder.h"

#include "devtools/replay_source.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void session_recorder_open(session_recorder_t *recorder, const char *path) {
    assert(recorder);
    assert(path);

    int written = snprintf(recorder->path, sizeof(recorder->path), "%s", path);
    assert(written > 0 && (size_t)written < sizeof(recorder->path) &&
           "a truncated path records to a file the caller did not name, and one that no longer pairs with the marker file");

    recorder->frame_count = 0;
    recorder->tail_offset = 0;
    recorder->initial_save_count = 0;
    recorder->personal_bindings = 0;
}

void session_recorder_set_initial_save(session_recorder_t *recorder, const uint8_t *bytes, int count) {
    assert(recorder);
    assert(recorder->frame_count == 0 && "initial_save is written into the header with the first frame");
    assert(count >= 0 && count <= SAVE_STATE_MAX_BYTES);
    assert(count == 0 || bytes);

    if (count > 0) {
        memcpy(recorder->initial_save, bytes, (size_t)count);
    }
    recorder->initial_save_count = count;
}

void session_recorder_set_personal_bindings(session_recorder_t *recorder, int personal) {
    assert(recorder);
    assert(recorder->frame_count == 0 && "the bindings flag is written into the header with the first frame");
    assert(personal == 0 || personal == 1);

    recorder->personal_bindings = personal;
}

static char *append_text(char *end, const char *text) {
    size_t length = strlen(text);
    memcpy(end, text, length);
    return end + length;
}

static char *format_header_members(const session_recorder_t *recorder) {
    static const char bindings_member[] = ",\n  \"bindings\": \"personal\"";
    static const char save_key[] = ",\n  \"initial_save\": \"";
    static const char hex_digits[] = "0123456789abcdef";
    char attract_member[64];
    int attract_length =
        replay_source_format_attract_after(attract_member, sizeof(attract_member), REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT);

    size_t length = (size_t)attract_length;
    if (recorder->personal_bindings) {
        length += strlen(bindings_member);
    }
    if (recorder->initial_save_count > 0) {
        length += strlen(save_key) + 2 * (size_t)recorder->initial_save_count + 1;
    }
    char *members = malloc(length + 1);
    if (!members) {
        return NULL;
    }
    char *end = append_text(members, attract_member);
    if (recorder->personal_bindings) {
        end = append_text(end, bindings_member);
    }
    if (recorder->initial_save_count > 0) {
        end = append_text(end, save_key);
        for (int i = 0; i < recorder->initial_save_count; i++) {
            *end++ = hex_digits[recorder->initial_save[i] >> 4];
            *end++ = hex_digits[recorder->initial_save[i] & 0x0f];
        }
        *end++ = '"';
    }
    *end = '\0';
    assert((size_t)(end - members) == length);
    return members;
}

int session_recorder_record_frame(session_recorder_t *recorder, const p8_input_frame_t *frame) {
    assert(recorder);
    assert(frame);

    char *member = NULL;
    if (recorder->frame_count == 0) {
        member = format_header_members(recorder);
        if (!member) {
            return 0;
        }
    }
    int appended = p8_input_append(frame, recorder->path, &recorder->tail_offset, member);
    free(member);
    if (!appended) {
        /* Dropped, not fatal: the count must stay unchanged, since the marker file indexes frames by it. */
        return 0;
    }

    recorder->frame_count++;
    return 1;
}

int session_recorder_frame_count(const session_recorder_t *recorder) {
    assert(recorder);

    return recorder->frame_count;
}

void session_recorder_close(session_recorder_t *recorder) {
    assert(recorder);

    recorder->path[0] = '\0';
    recorder->frame_count = 0;
    recorder->tail_offset = 0;
    recorder->initial_save_count = 0;
    recorder->personal_bindings = 0;
}
