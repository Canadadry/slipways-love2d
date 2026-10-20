#include "engine/slot_tally.h"

#include <assert.h>
#include <string.h>

static void check_kind(const slot_tally_t *t, int kind) {
    assert(kind >= 0 && kind < t->n && "slot tally: kind outside the group");
}

static int is_arena(const slot_tally_t *t, int kind) {
    return t->kinds[kind].slot_name != NULL;
}

static size_t arena_bytes(const unsigned char *arena) {
    return arena_used((const allocator_t *)arena);
}

void slot_tally_init(slot_tally_t *t, const slot_kind_zone_t *kinds, const slot_zone_t *zones, int n) {
    assert(n > 0 && n <= SLOT_TALLY_KINDS_MAX);
    memset(t, 0, sizeof *t);
    t->kinds = kinds;
    t->zones = zones;
    t->n = n;
}

void slot_tally_take(slot_tally_t *t, int kind) {
    check_kind(t, kind);
    t->live[kind]++;
    if (t->live[kind] > t->max_live[kind]) t->max_live[kind] = t->live[kind];
    t->zone_bytes += SLOT_ZONE_BYTES(t->kinds[kind].size, 1);
    if (t->zone_bytes > t->zone_max_bytes) t->zone_max_bytes = t->zone_bytes;
}

void slot_tally_give_back(slot_tally_t *t, int kind, const unsigned char *slot) {
    check_kind(t, kind);
    assert(t->live[kind] > 0 && "slot tally: give back of a slot never taken");
    if (is_arena(t, kind)) {
        size_t used = arena_bytes(slot);
        if (used > t->released_max_bytes[kind]) t->released_max_bytes[kind] = used;
    }
    t->live[kind]--;
    t->zone_bytes -= SLOT_ZONE_BYTES(t->kinds[kind].size, 1);
}

int slot_tally_live(const slot_tally_t *t, int kind) {
    check_kind(t, kind);
    return t->live[kind];
}

int slot_tally_max_live(const slot_tally_t *t, int kind) {
    check_kind(t, kind);
    return t->max_live[kind];
}

size_t slot_tally_max_bytes(const slot_tally_t *t, int kind) {
    check_kind(t, kind);
    size_t max = t->released_max_bytes[kind];
    if (is_arena(t, kind)) {
        const slot_zone_t *zone = &t->zones[kind];
        for (size_t i = 0; i < zone->count; i++) {
            if (!zone->taken[i]) continue;
            size_t used = arena_bytes((const unsigned char *)(zone->storage + i * ZONE_CELLS(zone->slot_size)));
            if (used > max) max = used;
        }
    }
    return max;
}

size_t slot_tally_zone_bytes(const slot_tally_t *t) {
    return t->zone_bytes;
}

size_t slot_tally_zone_max_bytes(const slot_tally_t *t) {
    return t->zone_max_bytes;
}

size_t slot_tally_zone_reserved(const slot_tally_t *t) {
    size_t bytes = 0;
    for (int i = 0; i < t->n; i++) bytes += SLOT_ZONE_BYTES(t->kinds[i].size, t->kinds[i].count);
    return bytes;
}
