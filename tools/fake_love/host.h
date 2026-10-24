#ifndef FAKE_LOVE_HOST_H
#define FAKE_LOVE_HOST_H

#include <stdbool.h>
#include <stddef.h>

#include "disk.h"

typedef struct host host_t;

#define HOST_TOUCH_MAX 10

typedef struct {
    int id;
    int x;
    int y;
} host_touch_t;

typedef struct {
    int x;
    int y;
    bool left;
} host_mouse_t;

/* What one replay frame gives the host: `width` and `height` are read only
 * when `resized`; `touches` is not owned. A zero struct holds nothing, the
 * mouse at (0, 0), up. */
typedef struct host_frame_input {
    unsigned held;
    bool resized;
    int width;
    int height;
    const host_touch_t *touches;
    size_t touch_count;
    host_mouse_t mouse;
} host_frame_input_t;

/* NULL only when memory runs out. The caller releases it with host_close. */
host_t *host_new(void);
void host_close(host_t *host);

/* The window size before the first resize, 640x480 when never set. Only
 * before host_open. */
void host_set_window(host_t *host, int width, int height);

/* `jit.version` of the running LuaJIT. */
const char *host_runtime_version(host_t *host);

/* Each call below returns false on failure and leaves the reason in
 * host_error, valid until the next call. */
/* The disk is not owned and must outlive the host. */
bool host_open(host_t *host, const char *game_dir, disk_t *disk);
/* love.load gets a new sequence table holding the `count` strings. */
bool host_load(host_t *host, const char *const *arguments, size_t count);
/* On a resize, makes the new size current and calls love.resize(w, h); then
 * makes the touches and the mouse current, calls the pad callbacks for the
 * changes from the previous frame's `held` (nothing held before frame 0),
 * then love.update, zeroes the translation
 * as LÖVE's origin() does, keeping the push stack, then love.draw; fails when
 * a canvas is still selected after the draw step, as LÖVE's present does. */
bool host_frame(host_t *host, size_t frame, const host_frame_input_t *input);
/* `*text` is owned by the host, valid until the next call. */
bool host_debug_state(host_t *host, const char **text, size_t *length);

const char *host_error(const host_t *host);

/* The RGBA8 pixels of the canvas created `canvas`-th, owned by the host and
 * valid until host_close; NULL, leaving the size unset, when fewer exist. */
const unsigned char *host_read_canvas(const host_t *host, int canvas, int *width, int *height);

#endif
