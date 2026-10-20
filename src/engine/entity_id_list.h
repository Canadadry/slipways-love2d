#ifndef ENGINE_ENTITY_ID_LIST_H
#define ENGINE_ENTITY_ID_LIST_H

#include "engine/allocator.h"
#include "engine/entity_class.h"
#include "engine/entity_id_type.h"

typedef struct {
    entity_id_t *items;
    int count;
    int capacity;
    const allocator_t *zone;
} entity_id_list_t;

void entity_id_list_init(entity_id_list_t *list, const allocator_t *zone);
void entity_id_list_init_reserve(entity_id_list_t *list, const allocator_t *zone, int capacity);
void entity_id_list_free(entity_id_list_t *list);
void entity_id_list_clear(entity_id_list_t *list);
void entity_id_list_add(entity_id_list_t *list, entity_id_t id);
int entity_id_list_count(const entity_id_list_t *list);
/* Removes the first id equal to id; 1 when one was removed. */
int entity_id_list_del(entity_id_list_t *list, entity_id_t id);
/* The id at index, resolved; asserts on an id that resolves to NULL. */
entity_t *entity_id_list_at(const entity_id_list_t *list, int index);
/* 0 for a NULL list. */
int entity_id_list_has(const entity_id_list_t *list, entity_id_t id);
entity_id_list_t entity_id_list_concat(const allocator_t *zone, const entity_id_list_t *a,
                                       const entity_id_list_t *b);

typedef int (*entity_id_list_predicate_fn_t)(entity_t *e, void *ctx);
/* The predicate sees each id resolved; a NULL list gives an empty list. */
entity_id_list_t entity_id_list_filter(const allocator_t *zone, const entity_id_list_t *list,
                                       entity_id_list_predicate_fn_t predicate, void *ctx);

typedef struct {
    const entity_id_list_t *list;
    int index;
    int last_count;
} entity_id_list_iter_t;

entity_id_list_iter_t entity_id_list_all(const entity_id_list_t *list);
/* Hands out each id resolved; asserts on an id that resolves to NULL. */
int entity_id_list_iter_next(entity_id_list_iter_t *iter, entity_t **out);

#endif
