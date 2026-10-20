#include "engine/slot_group.h"

#include <assert.h>

static void check_kind(const slot_group_t *g, int kind) {
    assert(kind >= 0 && kind < g->n && "slot group: kind outside the group");
}

static unsigned char *slot_alloc(const allocator_t *z, size_t n);
static void slot_release(const allocator_t *z, unsigned char *p);

static void init_once(slot_group_t *g) {
    if (g->ready) return;
    assert(g->n > 0 && g->n <= SLOT_TALLY_KINDS_MAX);
    allocator_init_slot_kinds(g->kinds, g->n, g->zones, g->slots);
    slot_tally_init(&g->tally, g->kinds, g->zones, g->n);
    for (int i = 0; i < g->n; i++) {
        g->handles[i] = (slot_group_handle_t){{slot_alloc, allocator_grow_refused, slot_release, NULL, NULL}, g, i};
    }
    g->ready = 1;
}

static unsigned char *slot_alloc(const allocator_t *z, size_t n) {
    const slot_group_handle_t *h = (const slot_group_handle_t *)z;
    unsigned char *p = allocator_alloc(&h->group->slots[h->kind], n);
    slot_tally_take(&h->group->tally, h->kind);
    return p;
}

static void slot_release(const allocator_t *z, unsigned char *p) {
    const slot_group_handle_t *h = (const slot_group_handle_t *)z;
    if (p == NULL) return;
    slot_tally_give_back(&h->group->tally, h->kind, p);
    allocator_release(&h->group->slots[h->kind], p);
}

const allocator_t *slot_group_handle(slot_group_t *g, int kind) {
    check_kind(g, kind);
    init_once(g);
    return &g->handles[kind].handle;
}

const allocator_t *slot_group_slot(slot_group_t *g, int kind) {
    assert(slot_group_is_arena(g, kind) && "slot_group_slot: a bare slot is not an arena");
    return arena_take(slot_group_handle(g, kind), g->kinds[kind].slot_name, g->kinds[kind].size);
}

int slot_group_live(slot_group_t *g, int kind) {
    check_kind(g, kind);
    init_once(g);
    assert((size_t)slot_tally_live(&g->tally, kind) == slot_zone_taken(&g->zones[kind]) &&
           "slot_group_live: a slot taken or given back without its kind's handle");
    return slot_tally_live(&g->tally, kind);
}

int slot_group_max_live(slot_group_t *g, int kind) {
    check_kind(g, kind);
    init_once(g);
    return slot_tally_max_live(&g->tally, kind);
}

size_t slot_group_max_bytes(slot_group_t *g, int kind) {
    check_kind(g, kind);
    init_once(g);
    return slot_tally_max_bytes(&g->tally, kind);
}

size_t slot_group_used(slot_group_t *g) {
    init_once(g);
    return slot_tally_zone_bytes(&g->tally);
}

size_t slot_group_max_used(slot_group_t *g) {
    init_once(g);
    return slot_tally_zone_max_bytes(&g->tally);
}

size_t slot_group_reserved(slot_group_t *g) {
    init_once(g);
    return slot_tally_zone_reserved(&g->tally);
}

int slot_group_count(const slot_group_t *g, int kind) {
    check_kind(g, kind);
    return (int)g->kinds[kind].count;
}

size_t slot_group_slot_size(const slot_group_t *g, int kind) {
    check_kind(g, kind);
    return g->kinds[kind].size;
}

int slot_group_is_arena(const slot_group_t *g, int kind) {
    check_kind(g, kind);
    return g->kinds[kind].slot_name != NULL;
}

void slot_group_record_elements(slot_group_t *g, int kind, int count) {
    check_kind(g, kind);
    if (count > g->max_elements[kind]) g->max_elements[kind] = count;
}

int slot_group_max_elements(const slot_group_t *g, int kind) {
    check_kind(g, kind);
    return g->max_elements[kind];
}
