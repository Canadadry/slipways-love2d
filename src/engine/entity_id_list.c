#include "engine/entity_id_list.h"

#include <assert.h>
#include <stddef.h>

#include "engine/entity_id.h"

#define ENTITY_ID_LIST_INITIAL_CAPACITY 4

void entity_id_list_init(entity_id_list_t *list, const allocator_t *zone) {
    assert(zone != NULL && "entity_id_list_init: a list without a zone would grow nowhere");
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
    list->zone = zone;
}

void entity_id_list_init_reserve(entity_id_list_t *list, const allocator_t *zone, int capacity) {
    assert(capacity >= 0);
    entity_id_list_init(list, zone);
    if (capacity == 0) return;
    list->items = (entity_id_t *)allocator_alloc(zone, (size_t)capacity * sizeof *list->items);
    list->capacity = capacity;
}

void entity_id_list_clear(entity_id_list_t *list) {
    list->count = 0;
}

void entity_id_list_free(entity_id_list_t *list) {
    if (list->items != NULL) allocator_release(list->zone, (unsigned char *)list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void grow(entity_id_list_t *list) {
    int new_capacity = list->capacity == 0 ? ENTITY_ID_LIST_INITIAL_CAPACITY : list->capacity * 2;
    assert(list->zone != NULL && "entity_id_list_add on a list entity_id_list_init never gave a zone");
    list->items = (entity_id_t *)allocator_grow(list->zone, (unsigned char *)list->items,
                                                (size_t)list->capacity * sizeof *list->items,
                                                (size_t)new_capacity * sizeof *list->items);
    list->capacity = new_capacity;
}

void entity_id_list_add(entity_id_list_t *list, entity_id_t id) {
    if (list->count == list->capacity) grow(list);
    list->items[list->count++] = id;
}

int entity_id_list_del(entity_id_list_t *list, entity_id_t id) {
    for (int i = 0; i < list->count; i++) {
        if (!entity_id_equal(list->items[i], id)) continue;
        for (int j = i; j < list->count - 1; j++) list->items[j] = list->items[j + 1];
        list->count--;
        return 1;
    }
    return 0;
}

entity_t *entity_id_list_at(const entity_id_list_t *list, int index) {
    assert(index >= 0 && index < list->count && "entity_id_list_at: index out of range");
    entity_t *e = entity_get(list->items[index]);
    assert(e != NULL && "a game id list holds no dead id: remove it where the entity dies");
    return e;
}

int entity_id_list_has(const entity_id_list_t *list, entity_id_t id) {
    if (list == NULL) return 0;
    for (int i = 0; i < list->count; i++)
        if (entity_id_equal(list->items[i], id)) return 1;
    return 0;
}

int entity_id_list_count(const entity_id_list_t *list) {
    return list->count;
}

entity_id_list_t entity_id_list_concat(const allocator_t *zone, const entity_id_list_t *a,
                                       const entity_id_list_t *b) {
    assert(a != NULL && b != NULL && "entity_id_list_concat: a NULL list is not tolerated, unlike entity_id_list_has");
    entity_id_list_t out;
    entity_id_list_init_reserve(&out, zone, a->count + b->count);
    for (int i = 0; i < a->count; i++) entity_id_list_add(&out, a->items[i]);
    for (int i = 0; i < b->count; i++) entity_id_list_add(&out, b->items[i]);
    return out;
}

entity_id_list_t entity_id_list_filter(const allocator_t *zone, const entity_id_list_t *list,
                                       entity_id_list_predicate_fn_t predicate, void *ctx) {
    entity_id_list_t out;
    entity_id_list_init(&out, zone);
    if (list == NULL) return out;
    entity_id_list_iter_t it = entity_id_list_all(list);
    entity_t *e;
    while (entity_id_list_iter_next(&it, &e))
        if (predicate(e, ctx)) entity_id_list_add(&out, it.list->items[it.index]);
    return out;
}

entity_id_list_iter_t entity_id_list_all(const entity_id_list_t *list) {
    assert(list != NULL && "entity_id_list primitive called on a NULL container");
    entity_id_list_iter_t it = {list, -1, list->count};
    return it;
}

int entity_id_list_iter_next(entity_id_list_iter_t *iter, entity_t **out) {
    int next_index;
    if (iter->index < 0) {
        next_index = 0;
    } else if (iter->list->count < iter->last_count) {
        next_index = iter->index;
    } else {
        next_index = iter->index + 1;
    }
    if (next_index >= iter->list->count) return 0;
    *out = entity_id_list_at(iter->list, next_index);
    iter->index = next_index;
    iter->last_count = iter->list->count;
    return 1;
}
