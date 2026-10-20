#ifndef ENGINE_ALLOCATOR_H
#define ENGINE_ALLOCATOR_H

#include <stddef.h>

#include "engine/zone.h"

typedef struct allocator {
    unsigned char *(*alloc)(const struct allocator *z, size_t n);
    unsigned char *(*grow)(const struct allocator *z, unsigned char *p,
                           size_t old_n, size_t new_n);
    void (*release)(const struct allocator *z, unsigned char *p);
    bump_zone_t *bump;
    slot_zone_t *slot;
} allocator_t;

allocator_t allocator_from_bump_zone(bump_zone_t *bump);

allocator_t allocator_from_slot_zone(slot_zone_t *slot);

unsigned char *allocator_grow_refused(const allocator_t *z, unsigned char *p,
                                       size_t old_n, size_t new_n);

typedef struct {
    const char *zone_name;
    const char *slot_name;
    zone_cell_t *cells;
    unsigned char *taken;
    size_t size;
    size_t count;
} slot_kind_zone_t;

void allocator_init_slot_kinds(const slot_kind_zone_t *s, int n, slot_zone_t *zones,
                            allocator_t *handles);

typedef struct {
    allocator_t handle;
    flip_zone_t *zone;
} flip_zone_allocator_t;

const allocator_t *allocator_from_flip_zone(flip_zone_allocator_t *storage,
                                                       flip_zone_t *zone);

unsigned char *allocator_alloc(const allocator_t *z, size_t n);
unsigned char *allocator_grow(const allocator_t *z, unsigned char *p,
                               size_t old_n, size_t new_n);

void allocator_release(const allocator_t *z, unsigned char *p);

/* `handle` must stay first: arena_t is reached by casting the handle. */
typedef struct {
    allocator_t handle;
    bump_zone_t bump;
    const allocator_t *from;
} arena_t;

#define ARENA_HEAD_BYTES ZONE_BYTES(sizeof(arena_t))

#define ARENA_LIST_BYTES(list_head_bytes, capacity, element_bytes) \
    (ARENA_HEAD_BYTES + ZONE_BYTES(list_head_bytes) + ZONE_BYTES((capacity) * (element_bytes)))

const allocator_t *arena_take(const allocator_t *from,
                                                     const char *name, size_t size);
size_t arena_size_for(size_t n);

size_t arena_size(const allocator_t *arena);

size_t arena_used(const allocator_t *arena);
void arena_spill(const allocator_t *arena);
void arena_flush(const allocator_t *arena);

void arena_release(const allocator_t *arena);

#endif
