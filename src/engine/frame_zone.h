#ifndef ENGINE_FRAME_ZONE_H
#define ENGINE_FRAME_ZONE_H

#include "engine/allocator.h"

const allocator_t *frame_zone(void);
void frame_zone_reset(void);

size_t frame_zone_used(void);
size_t frame_zone_max(void);

typedef struct {
    size_t used;
    unsigned long generation;
} frame_zone_mark_t;
frame_zone_mark_t frame_zone_mark(void);
void frame_zone_rewind(frame_zone_mark_t mark);

#endif
