#include "engine/entity_list.h"

#include <assert.h>
#include <stddef.h>

#define ENTITY_LIST_INITIAL_CAPACITY 4

void entity_list_init(entity_list_t *entity_list, const allocator_t *zone) {
    assert(zone != NULL && "entity_list_init: a list without a zone would grow nowhere");
    entity_list->items = NULL;
    entity_list->count = 0;
    entity_list->capacity = 0;
    entity_list->zone = zone;
}

void entity_list_init_reserve(entity_list_t *entity_list, const allocator_t *zone, int capacity) {
    assert(capacity >= 0);
    entity_list_init(entity_list, zone);
    if (capacity == 0) {
        return;
    }
    entity_list->items = (entity_t **)allocator_alloc(zone, (size_t)capacity * sizeof *entity_list->items);
    entity_list->capacity = capacity;
}

void entity_list_free(entity_list_t *entity_list) {
    assert(entity_list != NULL && "entity_list primitive called on a NULL container");
    if (entity_list->items != NULL) {
        allocator_release(entity_list->zone, (unsigned char *)entity_list->items);
    }
    entity_list->items = NULL;
    entity_list->count = 0;
    entity_list->capacity = 0;
}

void entity_list_clear(entity_list_t *entity_list) {
    assert(entity_list != NULL && "entity_list primitive called on a NULL container");
    entity_list->count = 0;
}

static void entity_list_grow(entity_list_t *entity_list) {
    int new_capacity = entity_list->capacity == 0 ? ENTITY_LIST_INITIAL_CAPACITY : entity_list->capacity * 2;
    assert(entity_list->zone != NULL && "entity_list_add on a list entity_list_init never gave a zone");
    entity_list->items = (entity_t **)allocator_grow(
        entity_list->zone, (unsigned char *)entity_list->items, (size_t)entity_list->capacity * sizeof *entity_list->items,
        (size_t)new_capacity * sizeof *entity_list->items);
    entity_list->capacity = new_capacity;
}

void entity_list_add(entity_list_t *entity_list, entity_t *e) {
    assert(entity_list != NULL && "entity_list primitive called on a NULL container");
    if (entity_list->count == entity_list->capacity) {
        entity_list_grow(entity_list);
    }
    entity_list->items[entity_list->count] = e;
    entity_list->count++;
}

int entity_list_del(entity_list_t *entity_list, const entity_t *e) {
    int i;
    assert(entity_list != NULL && "entity_list primitive called on a NULL container");
    for (i = 0; i < entity_list->count; i++) {
        if (entity_list->items[i] == e) {
            int j;
            for (j = i; j < entity_list->count - 1; j++) {
                entity_list->items[j] = entity_list->items[j + 1];
            }
            entity_list->count--;
            return 1;
        }
    }
    return 0;
}

int entity_list_has(const entity_list_t *entity_list, const entity_t *e) {
    int i;
    if (entity_list == NULL) {
        return 0;
    }
    for (i = 0; i < entity_list->count; i++) {
        if (entity_list->items[i] == e) {
            return 1;
        }
    }
    return 0;
}

entity_list_t entity_list_concat(const allocator_t *zone, const entity_list_t *a,
                            const entity_list_t *b) {
    entity_list_t out;
    int i;
    assert(a != NULL && b != NULL && "entity_list_concat: a NULL list is not tolerated, unlike entity_list_has");
    entity_list_init(&out, zone);
    for (i = 0; i < a->count; i++) {
        entity_list_add(&out, a->items[i]);
    }
    for (i = 0; i < b->count; i++) {
        entity_list_add(&out, b->items[i]);
    }
    return out;
}

entity_list_t entity_list_filter(const allocator_t *zone, const entity_list_t *qk,
                           filter_entity_list_predicate_fn_t ow, void *ctx) {
    entity_list_t f;
    entity_list_iter_t it;
    entity_t *e;
    entity_list_init(&f, zone);
    if (qk == NULL) {
        return f;
    }
    it = entity_list_all(qk);
    while (entity_list_iter_next(&it, &e)) {
        if (ow(e, ctx)) {
            entity_list_add(&f, e);
        }
    }
    return f;
}

entity_t *entity_list_find_first(const entity_list_t *entity_list, filter_entity_list_predicate_fn_t predicate,
                                 void *ctx) {
    entity_list_iter_t it;
    entity_t *e;
    if (entity_list == NULL) {
        return NULL;
    }
    it = entity_list_all(entity_list);
    while (entity_list_iter_next(&it, &e)) {
        if (predicate(e, ctx)) {
            return e;
        }
    }
    return NULL;
}

entity_list_iter_t entity_list_all(const entity_list_t *entity_list) {
    entity_list_iter_t it;
    assert(entity_list != NULL && "entity_list primitive called on a NULL container");
    it.entity_list = entity_list;
    it.index = -1;
    it.last_count = entity_list->count;
    return it;
}

int entity_list_iter_next(entity_list_iter_t *iter, entity_t **out) {
    int next_index;

    if (iter->index < 0) {
        next_index = 0;
    } else if (iter->entity_list->count < iter->last_count) {
        next_index = iter->index;
    } else {
        next_index = iter->index + 1;
    }

    if (next_index >= iter->entity_list->count) {
        return 0;
    }

    *out = iter->entity_list->items[next_index];
    iter->index = next_index;
    iter->last_count = iter->entity_list->count;
    return 1;
}
