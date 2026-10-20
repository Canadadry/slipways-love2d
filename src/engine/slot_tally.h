#ifndef ENGINE_SLOT_TALLY_H
#define ENGINE_SLOT_TALLY_H

#include <stddef.h>

#include "engine/allocator.h"
#include "engine/zone.h"

#define SLOT_TALLY_KINDS_MAX 8

typedef struct {
    const slot_kind_zone_t *kinds;
    const slot_zone_t *zones;
    int n;
    int live[SLOT_TALLY_KINDS_MAX];
    int max_live[SLOT_TALLY_KINDS_MAX];
    size_t released_max_bytes[SLOT_TALLY_KINDS_MAX];
    size_t zone_bytes;
    size_t zone_max_bytes;
} slot_tally_t;

void slot_tally_init(slot_tally_t *t, const slot_kind_zone_t *kinds, const slot_zone_t *zones, int n);
void slot_tally_take(slot_tally_t *t, int kind);
void slot_tally_give_back(slot_tally_t *t, int kind, const unsigned char *slot);

int slot_tally_live(const slot_tally_t *t, int kind);
int slot_tally_max_live(const slot_tally_t *t, int kind);
size_t slot_tally_max_bytes(const slot_tally_t *t, int kind);
size_t slot_tally_zone_bytes(const slot_tally_t *t);
size_t slot_tally_zone_max_bytes(const slot_tally_t *t);
size_t slot_tally_zone_reserved(const slot_tally_t *t);

#endif
