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

typedef struct host_frame_input {
    unsigned held;
    bool resized;
    int width;
    int height;
    const host_touch_t *touches;
    size_t touch_count;
    host_mouse_t mouse;
    bool record_screen;
} host_frame_input_t;

host_t *host_new(void);
void host_close(host_t *host);

void host_set_window(host_t *host, int width, int height);

const char *host_runtime_version(host_t *host);

bool host_open(host_t *host, const char *game_dir, disk_t *disk);
bool host_load(host_t *host, const char *const *arguments, size_t count);
bool host_frame(host_t *host, size_t frame, const host_frame_input_t *input);
bool host_debug_state(host_t *host, const char **text, size_t *length);

const char *host_error(const host_t *host);

const unsigned char *host_read_canvas(const host_t *host, int canvas, int *width, int *height);

const unsigned char *host_read_screen(const host_t *host, int *width, int *height);

#endif
