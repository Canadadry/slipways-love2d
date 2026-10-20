#include "engine/frame_zone.h"

#include <assert.h>

#include "engine/zone_sizes.h"

BUMP_ZONE_STORAGE(frame_cells, FRAME_ZONE_BYTES);

static bump_zone_t frame;
/* Static: an entity_list_t or pointer_list_t keeps a pointer to its zone. */
static allocator_t frame_handle;
static int frame_ready;
static unsigned long frame_generation;
static size_t frame_max;

/* Lazy: bump_zone_init poisons the storage under ASan, which a static
 * initialiser would not do. */
static void frame_init_once(void) {
    if (frame_ready) return;
    bump_zone_init(&frame, "frame", frame_cells, FRAME_ZONE_BYTES);
    frame_handle = allocator_from_bump_zone(&frame);
    frame_ready = 1;
}

const allocator_t *frame_zone(void) {
    frame_init_once();
    return &frame_handle;
}

static void sample_max(void) {
    if (frame.used > frame_max) frame_max = frame.used;
}

void frame_zone_reset(void) {
    frame_init_once();
    sample_max();
    bump_zone_reset(&frame);
    frame_generation++;
}

frame_zone_mark_t frame_zone_mark(void) {
    frame_init_once();
    frame_zone_mark_t mark = {frame.used, frame_generation};
    return mark;
}

void frame_zone_rewind(frame_zone_mark_t mark) {
    frame_init_once();
    assert(mark.generation == frame_generation &&
           "rewind to a mark from before a reset: the room taken since then is no longer the marker's to give back");
    sample_max();
    bump_zone_rewind(&frame, mark.used);
}

size_t frame_zone_used(void) {
    frame_init_once();
    return frame.used;
}

size_t frame_zone_max(void) {
    frame_init_once();
    sample_max();
    return frame_max;
}
