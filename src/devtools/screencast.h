/* The capture format, public: an outside script must be able to read it
 * without the project's code.
 * Raw frames concatenated in capture order, with no header, no index and
 * no version number. A frame takes SCREENCAST_FRAME_BYTES = 9600 bytes:
 * the 19200 pixels of the 160x120 screen, two pixels per byte, in the
 * scan order of the screen memory. Each byte holds two horizontally
 * adjacent pixels: the even-indexed one in its LOW nibble, the
 * odd-indexed one in its high nibble. Frame N starts at offset N x 9600,
 * and the frame count is the file size divided by 9600, never a stored
 * counter. */

#ifndef DEVTOOLS_SCREENCAST_H
#define DEVTOOLS_SCREENCAST_H

#include "p8/gfx.h"
#include "p8/vram.h"

#include <stdio.h>

#define SCREENCAST_FRAME_PIXELS (P8_SCREEN_WIDTH * P8_SCREEN_HEIGHT)

#define SCREENCAST_FRAME_BYTES (SCREENCAST_FRAME_PIXELS / 2)

typedef struct {
    FILE *file;
} screencast_writer_t;

int screencast_writer_open(screencast_writer_t *writer, const char *path);

int screencast_write_frame(screencast_writer_t *writer, const p8_vram_t *mem);

int screencast_writer_close(screencast_writer_t *writer);

typedef enum {
    SCREENCAST_OK = 0,
    SCREENCAST_UNREADABLE = 1,
    SCREENCAST_EMPTY = 2,
    SCREENCAST_TRUNCATED = 3
} screencast_status_t;

typedef struct {
    FILE *file;
    int frame_count;
} screencast_reader_t;

screencast_status_t screencast_reader_open(screencast_reader_t *reader, const char *path);

int screencast_frame_count(const screencast_reader_t *reader);

int screencast_read_frame(screencast_reader_t *reader, int index, unsigned char *out_pixels);

void screencast_reader_close(screencast_reader_t *reader);

#endif
