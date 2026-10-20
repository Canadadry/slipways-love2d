#ifndef DEVTOOLS_VRAM_DIGEST_H
#define DEVTOOLS_VRAM_DIGEST_H

#include <stddef.h>
#include <stdint.h>

#include "p8/vram.h"

/* Each call carries one whole line, its '\n' included, in a buffer the
   digest reuses once the call returns. */
typedef void (*vram_digest_line_fn)(void *context, const char *line);

typedef struct {
    vram_digest_line_fn write_line;
    void *context;
} vram_digest_sink_t;

uint32_t vram_digest_fnv1a(const unsigned char *bytes, size_t count);

/* 1 for every 32nd frame from 0 and for the last one, the frames both
   replay drivers write a "screen" line for. */
int vram_digest_samples_frame(int frame, int frame_count);

void vram_digest_write_sprite_sheet(const vram_digest_sink_t *sink, const p8_vram_t *memory);

void vram_digest_write_screen(const vram_digest_sink_t *sink, const p8_vram_t *memory, int frame);

void vram_digest_write_map(const vram_digest_sink_t *sink, const p8_vram_t *memory);

#endif
