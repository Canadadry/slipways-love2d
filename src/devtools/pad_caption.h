#ifndef DEVTOOLS_PAD_CAPTION_H
#define DEVTOOLS_PAD_CAPTION_H

#include <stddef.h>

#define PAD_CAPTION_LINE_CHARS 32
#define PAD_CAPTION_LINES_MAX 2
#define PAD_CAPTION_SHOWN_MIN 150
#define PAD_CAPTION_QUIET_MIN 90
#define PAD_CAPTION_TEXT_SIZE (PAD_CAPTION_LINES_MAX * (PAD_CAPTION_LINE_CHARS + 1))

int pad_caption_parse(const char *script_text, char *out, size_t out_size, char *error, size_t error_size);

typedef struct {
    int from;
    int to;
    char text[PAD_CAPTION_TEXT_SIZE];
} pad_caption_t;

typedef struct {
    pad_caption_t *captions;
    int capacity;
    int count;
    int shown;
} pad_caption_track_t;

void pad_caption_track_init(pad_caption_track_t *track, pad_caption_t *storage, int capacity);
int pad_caption_track_show(pad_caption_track_t *track, int frame, const char *text, char *error, size_t error_size);
int pad_caption_track_clear(pad_caption_track_t *track, int frame, char *error, size_t error_size);
int pad_caption_track_frame(const pad_caption_track_t *track, int frame, int buttons, char *error, size_t error_size);
int pad_caption_track_end(pad_caption_track_t *track, int frame, int checked, char *error, size_t error_size);
int pad_caption_write_member(const pad_caption_track_t *track, char *out, size_t out_size);

int pad_caption_read_member(const char *json, pad_caption_t *captions, int capacity, char *error, size_t error_size);

#endif
