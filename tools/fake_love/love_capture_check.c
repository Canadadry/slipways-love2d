#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "disk.h"
#include "host.h"
#include "png_file.h"

#define HARNESS_CANVAS 0

typedef struct {
    const char *program;
    const char *png;
} pair_t;

static void report(const pair_t *pair, const char *format, ...) {
    va_list args;
    va_start(args, format);
    fprintf(stderr, "love_capture_check: %s: ", pair->program);
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
    va_end(args);
}

static unsigned long rgba_at(const unsigned char *pixels, int index) {
    const unsigned char *p = pixels + 4 * index;
    return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
}

static bool compare(const pair_t *pair, const unsigned char *canvas, int width, int height) {
    unsigned char *expected;
    int png_width, png_height;
    const char *reason = png_file_load(pair->png, &expected, &png_width, &png_height);
    if (reason != NULL) {
        report(pair, "%s: %s", pair->png, reason);
        return false;
    }
    bool same_size = png_width == width && png_height == height;
    int first = -1, differing = 0;
    for (int i = 0; same_size && i < width * height; i++)
        if (memcmp(canvas + 4 * i, expected + 4 * i, 4) != 0) {
            if (first < 0)
                first = i;
            differing++;
        }
    if (!same_size)
        report(pair, "%s is %dx%d, the canvas is %dx%d", pair->png, png_width, png_height, width, height);
    else if (differing > 0)
        report(pair, "pixel (%d, %d) is %08lX in the fake, %08lX in %s (%d pixels differ)", first % width,
               first / width, rgba_at(canvas, first), rgba_at(expected, first), pair->png, differing);
    free(expected);
    return same_size && differing == 0;
}

static bool check(const char *harness, const pair_t *pair) {
    host_t *host = host_new();
    if (host == NULL) {
        report(pair, "out of memory");
        return false;
    }
    disk_t *disk = disk_create(harness);
    const char *const arguments[] = {pair->program};
    bool ok = host_open(host, harness, disk) && host_load(host, arguments, 1) && host_frame(host, 0, 0);
    if (!ok)
        report(pair, "%s", host_error(host));
    int width = 0, height = 0;
    const unsigned char *canvas = ok ? host_read_canvas(host, HARNESS_CANVAS, &width, &height) : NULL;
    if (ok && canvas == NULL) {
        report(pair, "no canvas was created");
        ok = false;
    }
    ok = ok && compare(pair, canvas, width, height);
    host_close(host);
    disk_free(disk);
    return ok;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: love_capture_check <harness dir> <program.lua> <program.png>\n");
        return 2;
    }
    const pair_t pair = {argv[2], argv[3]};
    return check(argv[1], &pair) ? 0 : 1;
}
