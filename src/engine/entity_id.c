#include "engine/entity_id.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>

#include "engine/zone_sizes.h"
#include "p8/fatal.h"

typedef struct {
    entity_t **entities;
    uint16_t *generations;
    uint16_t *free_indexes;
    int free_count;
} table_t;

static table_t g_table;
static int g_max_live;
/* The next table starts every slot above any generation handed out so far,
 * so an old id never matches a new entity on a reused index. */
static uint16_t g_highest_generation;

static uint16_t next_generation(uint16_t generation) {
    return generation == UINT16_MAX ? 1 : (uint16_t)(generation + 1);
}

void entity_id_table_init(const allocator_t *zone) {
    unsigned char *block = allocator_alloc(zone, ENTITY_TABLE_BYTES);
    g_table.entities = (entity_t **)block;
    g_table.generations = (uint16_t *)(g_table.entities + ENTITY_TABLE_CAP);
    g_table.free_indexes = g_table.generations + ENTITY_TABLE_CAP;
    uint16_t epoch = next_generation(g_highest_generation);
    g_highest_generation = epoch;
    for (int i = 0; i < ENTITY_TABLE_CAP; i++) {
        g_table.entities[i] = NULL;
        g_table.generations[i] = epoch;
        g_table.free_indexes[i] = (uint16_t)(ENTITY_TABLE_CAP - 1 - i);
    }
    g_table.free_count = ENTITY_TABLE_CAP;
}

void entity_id_table_forget(void) {
    table_t none = {0};
    g_table = none;
}

entity_id_t entity_id_take(entity_t *e) {
    assert(g_table.entities != NULL && "entity_registry_init() must run before entity_construct");
    assert(e != NULL);
    if (g_table.free_count == 0) {
        char message[64];
        snprintf(message, sizeof message, "entity table full, limit %d", ENTITY_TABLE_CAP);
        p8_fatal(message);
    }
    uint16_t index = g_table.free_indexes[--g_table.free_count];
    g_table.entities[index] = e;
    entity_id_t id = {index, g_table.generations[index]};
    assert(id.generation != 0 && "generation 0 is reserved for ENTITY_ID_NONE");
    e->id = id;
    if (entity_id_live() > g_max_live) g_max_live = entity_id_live();
    return id;
}

static int slot_of(const entity_t *e) {
    assert(e != NULL && "a free slot holds NULL: no entity is NULL");
    return entity_get(e->id) == e ? e->id.index : -1;
}

void entity_id_release(const entity_t *e) {
    int index = slot_of(e);
    if (index < 0) return;
    g_table.entities[index] = NULL;
    uint16_t generation = next_generation(g_table.generations[index]);
    g_table.generations[index] = generation;
    if (generation > g_highest_generation) g_highest_generation = generation;
    g_table.free_indexes[g_table.free_count++] = (uint16_t)index;
}

entity_t *entity_get(entity_id_t id) {
    if (g_table.entities == NULL || id.index >= ENTITY_TABLE_CAP) return NULL;
    if (g_table.generations[id.index] != id.generation) return NULL;
    return g_table.entities[id.index];
}

entity_id_t entity_id_of(const entity_t *e) {
    return slot_of(e) < 0 ? ENTITY_ID_NONE : e->id;
}

entity_id_t entity_id_link(const entity_t *e) {
    if (e == NULL) return ENTITY_ID_NONE;
    entity_id_t id = entity_id_of(e);
    assert(!entity_id_is_none(id) && "a link names a constructed entity: one without a slot would read NULL at once");
    return id;
}

int entity_id_live(void) {
    return g_table.entities == NULL ? 0 : ENTITY_TABLE_CAP - g_table.free_count;
}

int entity_id_max(void) {
    return g_max_live;
}
