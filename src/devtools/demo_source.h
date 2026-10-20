#ifndef DEVTOOLS_DEMO_SOURCE_H
#define DEVTOOLS_DEMO_SOURCE_H

#include "devtools/pad_caption.h"
#include "devtools/replay_source.h"
#include "game/demo_data.h"

#include <stddef.h>

#define DEMO_SOURCE_CAPTIONS_MAX (DEMO_DATA_FRAME_LIMIT / PAD_CAPTION_SHOWN_MIN + 1)

typedef struct {
    replay_source_t replay;
    pad_caption_t captions[DEMO_SOURCE_CAPTIONS_MAX];
    int caption_count;
} demo_source_t;

int demo_source_load(demo_source_t *source, const char *path, char *error, size_t error_size);

void demo_source_close(demo_source_t *source);

#endif
