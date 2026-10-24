#ifndef FAKE_LOVE_REPLAY_H
#define FAKE_LOVE_REPLAY_H

#include <stdbool.h>
#include <stddef.h>

#define REPLAY_SCREENS_MAX 16

typedef struct replay replay_t;

replay_t *replay_read(const char *path, char *error, size_t error_size);
replay_t *replay_parse(const char *name, const char *text, size_t length, char *error, size_t error_size);

size_t replay_frame_count(const replay_t *replay);
unsigned replay_frame_held(const replay_t *replay, size_t frame);
int replay_frame_resize(const replay_t *replay, size_t frame, int *width, int *height);
typedef struct {
    int id;
    int x;
    int y;
} replay_touch_t;

typedef struct {
    int x;
    int y;
    int left;
} replay_mouse_t;

const replay_touch_t *replay_frame_touches(const replay_t *replay, size_t frame, size_t *count);
replay_mouse_t replay_frame_mouse(const replay_t *replay, size_t frame);
const unsigned char *replay_initial_save(const replay_t *replay, size_t *count);
const char *replay_bindings(const replay_t *replay);
const char *replay_pad_sheet(const replay_t *replay);
int replay_attract_after(const replay_t *replay);
void replay_window(const replay_t *replay, int *width, int *height);
bool replay_frame_screen_listed(const replay_t *replay, size_t frame);
void replay_free(replay_t *replay);

#endif
