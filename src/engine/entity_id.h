#ifndef ENGINE_ENTITY_ID_H
#define ENGINE_ENTITY_ID_H

#include "engine/allocator.h"
#include "engine/entity_class.h"
#include "engine/entity_id_type.h"

/* Takes the table's ENTITY_TABLE_BYTES block from zone; every id of the
 * previous table resolves to NULL afterwards. */
void entity_id_table_init(const allocator_t *zone);

/* Drops the table without touching its block, for after the game zone
 * was reset. */
void entity_id_table_forget(void);

/* Also writes the id into e->id. */
entity_id_t entity_id_take(entity_t *e);

/* Frees the slot that holds exactly e, if any: an entity of a previous
 * table frees nothing in this one. */
void entity_id_release(const entity_t *e);

/* NULL for the "no link" id, a free slot, a stale generation or no table. */
entity_t *entity_get(entity_id_t id);

/* ENTITY_ID_NONE when e holds no slot of the current table. */
entity_id_t entity_id_of(const entity_t *e);

/* ENTITY_ID_NONE for NULL; asserts a non-NULL e holds a slot. */
entity_id_t entity_id_link(const entity_t *e);

int entity_id_live(void);
int entity_id_max(void);

#endif
