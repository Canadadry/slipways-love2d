#ifndef DEVTOOLS_REPLAY_SOURCE_H
#define DEVTOOLS_REPLAY_SOURCE_H

#include "game/attract.h"
#include "game/bindings.h"
#include "game/save_state.h"
#include "p8/input.h"

#include <stddef.h>
#include <stdint.h>

#define REPLAY_SOURCE_ATTRACT_AFTER_DEFAULT ATTRACT_AFTER_DEFAULT

typedef enum {
    REPLAY_SOURCE_NONE = 0,
    REPLAY_SOURCE_LOADED = 1,
    REPLAY_SOURCE_LOAD_FAILED = 2
} replay_source_status_t;

typedef enum {
    REPLAY_SOURCE_START_TITLE = 0,
    REPLAY_SOURCE_START_RESUME = 1
} replay_source_start_t;

typedef struct {
    replay_source_status_t status;
    const char *path;
    p8_input_t recording;
    int next_frame_index;
    uint8_t initial_save[SAVE_STATE_MAX_BYTES];
    int initial_save_count;
    int attract_after;
    replay_source_start_t start;
} replay_source_t;

replay_source_status_t replay_source_open(replay_source_t *source, const char *path);

int replay_source_frame_count(const replay_source_t *source);

int replay_source_frames_served(const replay_source_t *source);

int replay_source_next_frame(replay_source_t *source, p8_input_frame_t *out_frame);

int replay_source_attract_after(const replay_source_t *source);

int replay_source_format_attract_after(char *out, size_t size, int frames);

replay_source_start_t replay_source_start(const replay_source_t *source);

int replay_source_seed_storage(const replay_source_t *source);

void replay_source_close(replay_source_t *source);

typedef enum {
    REPLAY_SOURCE_BINDINGS_DEFAULT,
    REPLAY_SOURCE_BINDINGS_FIXTURE,
    REPLAY_SOURCE_BINDINGS_PERSONAL,
    REPLAY_SOURCE_BINDINGS_BAD_MEMBER,
    REPLAY_SOURCE_BINDINGS_MISSING,
    REPLAY_SOURCE_BINDINGS_INVALID
} replay_source_bindings_status_t;

#define REPLAY_SOURCE_PATH_SIZE 1024

typedef struct {
    char fixture_path[REPLAY_SOURCE_PATH_SIZE];
    char message[2 * REPLAY_SOURCE_PATH_SIZE + 256];
} replay_source_bindings_report_t;

replay_source_bindings_status_t replay_source_read_bindings(const replay_source_t *source, const bindings_t *fallback,
                                                            bindings_t *out, replay_source_bindings_report_t *report);

#endif
