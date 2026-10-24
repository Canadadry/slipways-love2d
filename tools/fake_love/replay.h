#ifndef FAKE_LOVE_REPLAY_H
#define FAKE_LOVE_REPLAY_H

#include <stdbool.h>
#include <stddef.h>

#define REPLAY_SCREENS_MAX 16

typedef struct replay replay_t;

/* On failure: NULL, and `error` names the file, the byte offset and the
 * problem. On success the caller releases the result with replay_free. */
replay_t *replay_read(const char *path, char *error, size_t error_size);
replay_t *replay_parse(const char *name, const char *text, size_t length, char *error, size_t error_size);

size_t replay_frame_count(const replay_t *replay);
/* The buttons held on `frame`, bit n-1 for button n. */
unsigned replay_frame_held(const replay_t *replay, size_t frame);
/* 1 and the new window size when `frame` carries a "resize"; 0, `width` and
 * `height` untouched, otherwise. */
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

/* The touches held on `frame`, in the frame's order, owned by the replay;
 * a count of 0 when the frame has none. */
const replay_touch_t *replay_frame_touches(const replay_t *replay, size_t frame, size_t *count);
/* The mouse in force on `frame`: the last "mouse" given, or (0, 0) up. */
replay_mouse_t replay_frame_mouse(const replay_t *replay, size_t frame);
/* The decoded "initial_save" bytes, owned by the replay; NULL and a count of
 * 0 when the replay has none. */
const unsigned char *replay_initial_save(const replay_t *replay, size_t *count);
/* The "bindings" fixture name, owned by the replay; NULL when absent. */
const char *replay_bindings(const replay_t *replay);
/* 0 when the replay has no "attract_after". */
int replay_attract_after(const replay_t *replay);
/* The "window" size; 640 and 480 when the replay has none. */
void replay_window(const replay_t *replay, int *width, int *height);
/* Whether "screens" lists `frame`: false for every frame of a replay
 * without it. */
bool replay_frame_screen_listed(const replay_t *replay, size_t frame);
void replay_free(replay_t *replay);

#endif
