#ifndef ENGINE_ENTITY_LIST_H
#define ENGINE_ENTITY_LIST_H

#include "engine/entity_class.h"
#include "engine/allocator.h"

typedef struct {
    entity_t **items;
    int count;
    int capacity;
    const allocator_t *zone;
} entity_list_t;

void entity_list_init(entity_list_t *entity_list, const allocator_t *zone);
void entity_list_init_reserve(entity_list_t *entity_list, const allocator_t *zone, int capacity);
void entity_list_free(entity_list_t *entity_list);
void entity_list_clear(entity_list_t *entity_list);
void entity_list_add(entity_list_t *entity_list, entity_t *e);
int entity_list_del(entity_list_t *entity_list, const entity_t *e);
int entity_list_has(const entity_list_t *entity_list, const entity_t *e);
entity_list_t entity_list_concat(const allocator_t *zone, const entity_list_t *a,
                            const entity_list_t *b);

typedef int (*filter_entity_list_predicate_fn_t)(entity_t *e, void *ctx);
entity_list_t entity_list_filter(const allocator_t *zone, const entity_list_t *qk,
                           filter_entity_list_predicate_fn_t ow, void *ctx);
/* NULL when the list is NULL or nothing matches; asks the predicate about nothing after the match. */
entity_t *entity_list_find_first(const entity_list_t *entity_list, filter_entity_list_predicate_fn_t predicate,
                                 void *ctx);

typedef struct {
    const entity_list_t *entity_list;
    int index;
    int last_count;
} entity_list_iter_t;
entity_list_iter_t entity_list_all(const entity_list_t *entity_list);
int entity_list_iter_next(entity_list_iter_t *iter, entity_t **out);

#endif
