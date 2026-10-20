#ifndef ENGINE_SLOT_GROUP_H
#define ENGINE_SLOT_GROUP_H

#include <stddef.h>

#include "engine/allocator.h"
#include "engine/slot_tally.h"
#include "engine/zone.h"

struct slot_group;

typedef struct {
    allocator_t handle;
    struct slot_group *group;
    int kind;
} slot_group_handle_t;

typedef struct slot_group {
    const slot_kind_zone_t *kinds;
    int n;
    int ready;
    slot_zone_t zones[SLOT_TALLY_KINDS_MAX];
    allocator_t slots[SLOT_TALLY_KINDS_MAX];
    slot_group_handle_t handles[SLOT_TALLY_KINDS_MAX];
    slot_tally_t tally;
    int max_elements[SLOT_TALLY_KINDS_MAX];
} slot_group_t;

#define SLOT_GROUP(kind_table, kind_count) {.kinds = (kind_table), .n = (kind_count)}

const allocator_t *slot_group_handle(slot_group_t *g, int kind);
const allocator_t *slot_group_slot(slot_group_t *g, int kind);

int slot_group_live(slot_group_t *g, int kind);
int slot_group_max_live(slot_group_t *g, int kind);
size_t slot_group_max_bytes(slot_group_t *g, int kind);
size_t slot_group_used(slot_group_t *g);
size_t slot_group_max_used(slot_group_t *g);
size_t slot_group_reserved(slot_group_t *g);

int slot_group_count(const slot_group_t *g, int kind);
size_t slot_group_slot_size(const slot_group_t *g, int kind);
int slot_group_is_arena(const slot_group_t *g, int kind);

void slot_group_record_elements(slot_group_t *g, int kind, int count);
int slot_group_max_elements(const slot_group_t *g, int kind);

#endif
