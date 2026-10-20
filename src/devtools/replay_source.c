#include "devtools/replay_source.h"

#include "game/save_storage.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_whole_file(const char *path, size_t *byte_count) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    long size = -1;
    if (fseek(file, 0, SEEK_END) == 0) {
        size = ftell(file);
    }
    if (size >= 0 && fseek(file, 0, SEEK_SET) == 0) {
        text = malloc((size_t)size + 1);
    }
    if (text) {
        size_t got = fread(text, 1, (size_t)size, file);
        text[got] = '\0';
        if (byte_count) {
            *byte_count = got;
        }
    }
    fclose(file);
    return text;
}

static int hex_digit_value(char digit) {
    if (digit >= '0' && digit <= '9') {
        return digit - '0';
    }
    if (digit >= 'a' && digit <= 'f') {
        return digit - 'a' + 10;
    }
    if (digit >= 'A' && digit <= 'F') {
        return digit - 'A' + 10;
    }
    return -1;
}

static const char *find_header_value(const char *json, const char *key) {
    const char *header_end = strstr(json, "\"frames\":");
    const char *p = strstr(json, key);
    if (!p || !header_end || p >= header_end) {
        return NULL;
    }
    p += strlen(key);
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static int is_repeated_in_header(const char *json, const char *value, const char *key) {
    const char *second = strstr(value, key);
    const char *header_end = strstr(json, "\"frames\":");
    return second && second < header_end;
}

static int parse_initial_save(replay_source_t *source, const char *json) {
    static const char key[] = "\"initial_save\":";
    source->initial_save_count = 0;

    const char *p = find_header_value(json, key);
    if (!p) {
        return 1;
    }
    if (*p != '"') {
        return 0;
    }
    p++;

    int digit_count = 0;
    while (p[digit_count] != '"') {
        if (hex_digit_value(p[digit_count]) < 0 || digit_count == 2 * SAVE_STATE_MAX_BYTES) {
            return 0;
        }
        digit_count++;
    }
    if (digit_count == 0 || digit_count % 2 != 0) {
        return 0;
    }

    for (int i = 0; i < digit_count / 2; i++) {
        source->initial_save[i] = (uint8_t)(hex_digit_value(p[2 * i]) * 16 + hex_digit_value(p[2 * i + 1]));
    }
    source->initial_save_count = digit_count / 2;
    return 1;
}

static int parse_attract_after(replay_source_t *source, const char *json) {
    static const char key[] = "\"attract_after\":";
    const char *p = find_header_value(json, key);
    if (!p) {
        return 1;
    }
    if (is_repeated_in_header(json, p, key)) {
        return 0;
    }
    if (*p < '1' || *p > '9') {
        return 0;
    }
    int value = 0;
    while (*p >= '0' && *p <= '9') {
        int digit = *p - '0';
        if (value > (INT_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        p++;
    }
    if (*p == '\0' || !strchr(",} \t\n\r", *p)) {
        return 0;
    }
    source->attract_after = value;
    return 1;
}

static int parse_start(replay_source_t *source, const char *json) {
    static const char key[] = "\"start\":";
    static const char resume[] = "\"resume\"";
    const char *p = find_header_value(json, key);
    if (!p) {
        return 1;
    }
    if (is_repeated_in_header(json, p, key) || strncmp(p, resume, strlen(resume)) != 0 ||
        source->initial_save_count == 0) {
        return 0;
    }
    source->start = REPLAY_SOURCE_START_RESUME;
    return 1;
}

static void reset_header(replay_source_t *source) {
    source->initial_save_count = 0;
    source->attract_after = REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT;
    source->start = REPLAY_SOURCE_START_TITLE;
}

replay_source_status_t replay_source_open(replay_source_t *source, const char *path) {
    assert(source);

    source->path = path;
    source->next_frame_index = 0;
    reset_header(source);
    p8_input_init(&source->recording);

    if (!path) {
        source->status = REPLAY_SOURCE_NONE;
        return source->status;
    }

    char *json = read_whole_file(path, NULL);
    int header_ok =
        json && parse_initial_save(source, json) && parse_attract_after(source, json) && parse_start(source, json);
    free(json);
    if (!header_ok || !p8_input_load(&source->recording, path)) {
        reset_header(source);
        source->status = REPLAY_SOURCE_LOAD_FAILED;
        return source->status;
    }

    source->status = REPLAY_SOURCE_LOADED;
    return source->status;
}

int replay_source_frame_count(const replay_source_t *source) {
    assert(source);

    return source->recording.frame_count;
}

int replay_source_frames_served(const replay_source_t *source) {
    assert(source);

    return source->next_frame_index;
}

int replay_source_next_frame(replay_source_t *source, p8_input_frame_t *out_frame) {
    assert(source);
    assert(out_frame);

    if (source->next_frame_index >= source->recording.frame_count) {
        return 0;
    }

    *out_frame = source->recording.frames[source->next_frame_index];
    source->next_frame_index++;
    return 1;
}

int replay_source_attract_after(const replay_source_t *source) {
    assert(source);

    return source->attract_after;
}

int replay_source_format_attract_after(char *out, size_t size, int frames) {
    assert(out && size > 0);
    int length = snprintf(out, size, "\"attract_after\": %d", frames);
    assert(length > 0 && (size_t)length < size && "replay_source_format_attract_after: the member outgrew its buffer");
    return length;
}

replay_source_start_t replay_source_start(const replay_source_t *source) {
    assert(source);

    return source->start;
}

int replay_source_seed_storage(const replay_source_t *source) {
    assert(source);

    if (source->status != REPLAY_SOURCE_LOADED) {
        return 1;
    }
    if (source->initial_save_count == 0 || source->start == REPLAY_SOURCE_START_RESUME) {
        return save_storage_erase();
    }
    return save_storage_write(source->initial_save, source->initial_save_count);
}

void replay_source_close(replay_source_t *source) {
    assert(source);

    p8_input_free(&source->recording);
    source->status = REPLAY_SOURCE_NONE;
    source->path = NULL;
    source->next_frame_index = 0;
    reset_header(source);
}

static const char *find_bindings_value(const char *json, int *value_length) {
    static const char key[] = "\"bindings\":";
    const char *p = find_header_value(json, key);
    if (!p) {
        return NULL;
    }
    const char *close = *p == '"' ? strchr(p + 1, '"') : NULL;
    *value_length = close && !is_repeated_in_header(json, p, key) ? (int)(close - p - 1) : -1;
    return p + 1;
}

#define FIXTURE_MAX_BYTES 65536

#define FIXTURE_NAME_MAX_LENGTH 64

static int is_fixture_name(const char *name, int length) {
    if (length < 5 || length > FIXTURE_NAME_MAX_LENGTH || name[0] == '.' || strncmp(name + length - 4, ".cfg", 4) != 0 ||
        (length == 12 && strncmp(name, "bindings.cfg", 12) == 0)) {
        return 0;
    }
    for (int i = 0; i < length; i++) {
        char c = name[i];
        int allowed = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' ||
                      c == '.' || c == '-';
        if (!allowed) {
            return 0;
        }
    }
    return 1;
}

static replay_source_bindings_status_t read_fixture(const char *recording, bindings_t *out,
                                                    replay_source_bindings_report_t *report) {
    const char *path = report->fixture_path;
    size_t byte_count = 0;
    char *text = read_whole_file(path, &byte_count);
    if (!text) {
        snprintf(report->message, sizeof(report->message), "%s: cannot read the bindings fixture named by %s", path,
                 recording);
        return REPLAY_SOURCE_BINDINGS_MISSING;
    }
    replay_source_bindings_status_t status = REPLAY_SOURCE_BINDINGS_FIXTURE;
    bindings_error_t error;
    if (byte_count > FIXTURE_MAX_BYTES) {
        snprintf(report->message, sizeof(report->message), "%s: larger than %d bytes", path, FIXTURE_MAX_BYTES);
        status = REPLAY_SOURCE_BINDINGS_INVALID;
    } else if (!bindings_parse(path, text, out, &error)) {
        snprintf(report->message, sizeof(report->message), "%s", error.text);
        status = REPLAY_SOURCE_BINDINGS_INVALID;
    }
    free(text);
    return status;
}

replay_source_bindings_status_t replay_source_read_bindings(const replay_source_t *source, const bindings_t *fallback,
                                                            bindings_t *out, replay_source_bindings_report_t *report) {
    assert(source && fallback && out && report);
    assert(source->status == REPLAY_SOURCE_LOADED && "read the bindings of a recording that replay_source_open loaded");

    report->fixture_path[0] = '\0';
    report->message[0] = '\0';

    char *json = read_whole_file(source->path, NULL);
    if (!json) {
        snprintf(report->message, sizeof(report->message), "%s: cannot re-read the recording", source->path);
        return REPLAY_SOURCE_BINDINGS_MISSING;
    }
    int value_length = 0;
    const char *value = find_bindings_value(json, &value_length);
    if (!value) {
        free(json);
        *out = *fallback;
        return REPLAY_SOURCE_BINDINGS_DEFAULT;
    }
    if (value_length == 8 && strncmp(value, "personal", 8) == 0) {
        free(json);
        snprintf(report->message, sizeof(report->message),
                 "%s: recorded with a personal bindings.cfg (\"bindings\": \"personal\"); replay runs only the default "
                 "bindings or a fixture next to the recording",
                 source->path);
        return REPLAY_SOURCE_BINDINGS_PERSONAL;
    }

    const char *slash = strrchr(source->path, '/');
    int directory_length = slash ? (int)(slash - source->path + 1) : 0;
    int fits = directory_length + value_length < (int)sizeof(report->fixture_path);
    if (!is_fixture_name(value, value_length) || !fits) {
        free(json);
        snprintf(report->message, sizeof(report->message),
                 "%s: bad \"bindings\" member: expected \"personal\" or a file name ending in .cfg in the recording's "
                 "directory",
                 source->path);
        return REPLAY_SOURCE_BINDINGS_BAD_MEMBER;
    }
    snprintf(report->fixture_path, sizeof(report->fixture_path), "%.*s%.*s", directory_length, source->path,
             value_length, value);
    free(json);
    return read_fixture(source->path, out, report);
}
