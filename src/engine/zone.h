#ifndef ENGINE_ZONE_H
#define ENGINE_ZONE_H

#include <stddef.h>

typedef union {
    unsigned long long align_integer;
    char *align_pointer;
} zone_cell_t;

#define ZONE_CELLS(bytes) \
    (((bytes) + sizeof(zone_cell_t) - 1) / sizeof(zone_cell_t))
#define ZONE_BYTES(bytes) (ZONE_CELLS(bytes) * sizeof(zone_cell_t))

typedef char zone_pointer_width_is_4_or_8_t[(sizeof(void *) == 4 || sizeof(void *) == 8) ? 1 : -1];

/* bytes_32 when pointers are 4 bytes, else bytes_64. Fails to compile when
 * bytes_32 is not a whole number of cells or exceeds bytes_64. */
#define ZONE_BYTES_BY_WIDTH(bytes_64, bytes_32) \
    (sizeof(void *) == 4 \
         ? (bytes_32) + 0 * sizeof(char[((bytes_32) % sizeof(zone_cell_t) == 0 && (bytes_32) <= (bytes_64)) ? 1 : -1]) \
         : (bytes_64))

#if defined(__SANITIZE_ADDRESS__)
#define ZONE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define ZONE_ASAN 1
#endif
#endif

#ifdef ZONE_ASAN
#define BUMP_ZONE_HALVES 2
#else
#define BUMP_ZONE_HALVES 1
#endif

#define BUMP_ZONE_STORAGE(cells, capacity) \
    static zone_cell_t cells[BUMP_ZONE_HALVES * ZONE_CELLS(capacity)]

#define FLIP_ZONE_STORAGE(cells, capacity) \
    static zone_cell_t cells[ZONE_CELLS(capacity)]

#define SLOT_ZONE_STORAGE(cells, taken, slot_size, count) \
    static zone_cell_t cells[ZONE_CELLS(slot_size) * (count)]; \
    static unsigned char taken[count]

#define SLOT_ZONE_BYTES(size, count) \
    (ZONE_CELLS(size) * sizeof(zone_cell_t) * (count) + (count))

typedef struct {
    const char *name;
    zone_cell_t *storage;
    size_t capacity;
    size_t used;
    size_t half;
    int never_reset;
} bump_zone_t;

typedef struct {
    const char *name;
    zone_cell_t *storage;
    unsigned char *taken;
    size_t slot_size;
    size_t count;
    size_t cursor;
} slot_zone_t;

typedef struct {
    const char *name;
    zone_cell_t *storage;
    size_t capacity;
    size_t bottom;
    size_t top;
    int current_top;
    int pending;
    size_t max;
} flip_zone_t;

void bump_zone_init(bump_zone_t *zone, const char *name,
                          zone_cell_t *storage, size_t capacity);
void bump_zone_init_single(bump_zone_t *zone, const char *name,
                                 zone_cell_t *storage, size_t capacity);
unsigned char *bump_zone_alloc(bump_zone_t *zone, size_t size);
void bump_zone_reset(bump_zone_t *zone);
void bump_zone_rewind(bump_zone_t *zone, size_t used);

void slot_zone_init(slot_zone_t *zone, const char *name,
                          zone_cell_t *storage, unsigned char *taken,
                          size_t slot_size, size_t count);
unsigned char *slot_zone_alloc(slot_zone_t *zone);
void slot_zone_release(slot_zone_t *zone, unsigned char *slot);
size_t slot_zone_taken(const slot_zone_t *zone);

void flip_zone_init(flip_zone_t *zone, const char *name,
                             zone_cell_t *storage, size_t capacity);
unsigned char *flip_zone_alloc(flip_zone_t *zone, size_t size);
void flip_zone_flip(flip_zone_t *zone);
int flip_zone_pending(const flip_zone_t *zone);
void flip_zone_release(flip_zone_t *zone);
void flip_zone_reset(flip_zone_t *zone);
size_t flip_zone_used(const flip_zone_t *zone);
size_t flip_zone_max(const flip_zone_t *zone);

#endif
