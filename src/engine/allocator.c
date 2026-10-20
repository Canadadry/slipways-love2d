#include "engine/allocator.h"

#include <assert.h>
#include <string.h>

static unsigned char *bump_alloc(const allocator_t *z, size_t n) {
    return bump_zone_alloc(z->bump, n);
}

static unsigned char *bump_grow(const allocator_t *z, unsigned char *p,
                                size_t old_n, size_t new_n) {
    unsigned char *q = bump_zone_alloc(z->bump, new_n);
    if (old_n > 0) memcpy(q, p, old_n);
    return q;
}

static void release_nothing(const allocator_t *z, unsigned char *p) {
    (void)z;
    (void)p;
}

allocator_t allocator_from_bump_zone(bump_zone_t *bump) {
    assert(bump != NULL && "handle on a missing zone");
    allocator_t z = {bump_alloc, bump_grow, release_nothing, bump, NULL};
    return z;
}

static unsigned char *slot_alloc(const allocator_t *z, size_t n) {
    assert(n <= z->slot->slot_size && "slot zone: block larger than a slot");
    return slot_zone_alloc(z->slot);
}

unsigned char *allocator_grow_refused(const allocator_t *z, unsigned char *p,
                                       size_t old_n, size_t new_n) {
    (void)z;
    (void)p;
    (void)old_n;
    (void)new_n;
    assert(0 && "slot zone: a slot does not grow");
    return NULL;
}

static void slot_release(const allocator_t *z, unsigned char *p) {
    if (p != NULL) slot_zone_release(z->slot, p);
}

allocator_t allocator_from_slot_zone(slot_zone_t *slot) {
    assert(slot != NULL && "handle on a missing zone");
    allocator_t z = {slot_alloc, allocator_grow_refused, slot_release, NULL, slot};
    return z;
}

void allocator_init_slot_kinds(const slot_kind_zone_t *s, int n, slot_zone_t *zones,
                            allocator_t *handles) {
    for (int i = 0; i < n; i++) {
        slot_zone_init(&zones[i], s[i].zone_name, s[i].cells, s[i].taken, s[i].size, s[i].count);
        handles[i] = allocator_from_slot_zone(&zones[i]);
    }
}

static flip_zone_t *flip_zone_of(const allocator_t *z) {
    return ((const flip_zone_allocator_t *)z)->zone;
}

static unsigned char *flip_zone_handle_alloc(const allocator_t *z, size_t n) {
    return flip_zone_alloc(flip_zone_of(z), n);
}

static unsigned char *flip_zone_handle_grow(const allocator_t *z, unsigned char *p,
                                   size_t old_n, size_t new_n) {
    unsigned char *q = flip_zone_alloc(flip_zone_of(z), new_n);
    if (old_n > 0) memcpy(q, p, old_n);
    return q;
}

const allocator_t *allocator_from_flip_zone(flip_zone_allocator_t *storage,
                                                       flip_zone_t *zone) {
    assert(storage != NULL && zone != NULL && "handle on a missing zone");
    allocator_t z = {flip_zone_handle_alloc, flip_zone_handle_grow, release_nothing, NULL, NULL};
    storage->handle = z;
    storage->zone = zone;
    return &storage->handle;
}

unsigned char *allocator_alloc(const allocator_t *z, size_t n) {
    assert(z != NULL && "alloc without an allocator");
    return z->alloc(z, n);
}

unsigned char *allocator_grow(const allocator_t *z, unsigned char *p,
                               size_t old_n, size_t new_n) {
    assert(z != NULL && "grow without an allocator");
    assert(old_n <= new_n && (p != NULL || old_n == 0));
    return z->grow(z, p, old_n, new_n);
}

void allocator_release(const allocator_t *z, unsigned char *p) {
    assert(z != NULL && "release without an allocator");
    z->release(z, p);
}

#define ARENA_HEAD_CELLS (ARENA_HEAD_BYTES / sizeof(zone_cell_t))

/* Not bump_zone_init: under ASan it poisons two halves, past the block. */
const allocator_t *arena_take(const allocator_t *from,
                                                     const char *name, size_t size) {
    size_t cells = size / sizeof(zone_cell_t);
    assert(cells > ARENA_HEAD_CELLS && "arena smaller than its header");
    zone_cell_t *block = (zone_cell_t *)allocator_alloc(from, size);
    arena_t *arena = (arena_t *)block;
    bump_zone_init_single(&arena->bump, name, block + ARENA_HEAD_CELLS,
                                (cells - ARENA_HEAD_CELLS) * sizeof(zone_cell_t));
    arena->handle = allocator_from_bump_zone(&arena->bump);
    arena->from = from;
    return &arena->handle;
}

size_t arena_size_for(size_t n) {
    return ARENA_HEAD_CELLS * sizeof(zone_cell_t) + ZONE_BYTES(n);
}

size_t arena_size(const allocator_t *arena) {
    const arena_t *a = (const arena_t *)arena;
    assert(arena != NULL && arena->bump == &a->bump && "size of a handle that is not an arena");
    return ARENA_HEAD_CELLS * sizeof(zone_cell_t) + a->bump.capacity;
}

static unsigned char *arena_spill_alloc(const allocator_t *z, size_t n) {
    arena_t *a = (arena_t *)z;
    if (ZONE_BYTES(n) <= a->bump.capacity - a->bump.used)
        return bump_zone_alloc(&a->bump, n);
    return allocator_alloc(a->from, n);
}

static unsigned char *arena_spill_grow(const allocator_t *z, unsigned char *p,
                                       size_t old_n, size_t new_n) {
    unsigned char *q = arena_spill_alloc(z, new_n);
    if (old_n > 0) memcpy(q, p, old_n);
    return q;
}

void arena_spill(const allocator_t *arena) {
    arena_t *a = (arena_t *)arena;
    assert(arena != NULL && arena->bump == &a->bump && "spill of a handle that is not an arena");
    a->handle.alloc = arena_spill_alloc;
    a->handle.grow = arena_spill_grow;
}

size_t arena_used(const allocator_t *arena) {
    const arena_t *a = (const arena_t *)arena;
    assert(arena != NULL && arena->bump == &a->bump && "usage of a handle that is not an arena");
    return ARENA_HEAD_CELLS * sizeof(zone_cell_t) + a->bump.used;
}

void arena_flush(const allocator_t *arena) {
    arena_t *a = (arena_t *)arena;
    assert(arena != NULL && arena->bump == &a->bump && "flush of a handle that is not an arena");
    bump_zone_init_single(&a->bump, a->bump.name, a->bump.storage, a->bump.capacity);
    a->handle = allocator_from_bump_zone(&a->bump);
}

void arena_release(const allocator_t *arena) {
    const arena_t *a = (const arena_t *)arena;
    assert(arena != NULL && arena->bump == &a->bump && "release of a handle that is not an arena");
    allocator_release(a->from, (unsigned char *)a);
}
