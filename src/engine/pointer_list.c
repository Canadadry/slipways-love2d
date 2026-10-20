#include "engine/pointer_list.h"

#include <assert.h>
#include <stddef.h>

#define POINTER_LIST_INITIAL_CAPACITY 4

void pointer_list_init(pointer_list_t *pointer_list, const allocator_t *zone) {
    assert(zone != NULL && "pointer_list_init: a list without a zone would grow nowhere");
    pointer_list->items = NULL;
    pointer_list->count = 0;
    pointer_list->capacity = 0;
    pointer_list->zone = zone;
}

void pointer_list_init_reserve(pointer_list_t *pointer_list, const allocator_t *zone, int capacity) {
    assert(capacity > 0);
    pointer_list_init(pointer_list, zone);
    pointer_list->items = (void **)allocator_alloc(zone, (size_t)capacity * sizeof(void *));
    pointer_list->capacity = capacity;
}

void pointer_list_free(pointer_list_t *pointer_list) {
    assert(pointer_list != NULL && "pointer_list primitive called on a NULL container");
    if (pointer_list->items != NULL) {
        allocator_release(pointer_list->zone, (unsigned char *)pointer_list->items);
    }
    pointer_list->items = NULL;
    pointer_list->count = 0;
    pointer_list->capacity = 0;
}

static void pointer_list_grow(pointer_list_t *pointer_list) {
    int new_capacity = pointer_list->capacity == 0 ? POINTER_LIST_INITIAL_CAPACITY : pointer_list->capacity * 2;
    assert(pointer_list->zone != NULL && "pointer_list_add on a list pointer_list_init never gave a zone");
    pointer_list->items = (void **)allocator_grow(pointer_list->zone, (unsigned char *)pointer_list->items,
                                           (size_t)pointer_list->capacity * sizeof(void *),
                                           (size_t)new_capacity * sizeof(void *));
    pointer_list->capacity = new_capacity;
}

void pointer_list_add(pointer_list_t *pointer_list, void *e) {
    assert(pointer_list != NULL && "pointer_list primitive called on a NULL container");
    if (pointer_list->count == pointer_list->capacity) {
        pointer_list_grow(pointer_list);
    }
    pointer_list->items[pointer_list->count] = e;
    pointer_list->count++;
}

int pointer_list_del(pointer_list_t *pointer_list, const void *e) {
    int i;
    assert(pointer_list != NULL && "pointer_list primitive called on a NULL container");
    for (i = 0; i < pointer_list->count; i++) {
        if (pointer_list->items[i] == e) {
            int j;
            for (j = i; j < pointer_list->count - 1; j++) {
                pointer_list->items[j] = pointer_list->items[j + 1];
            }
            pointer_list->count--;
            return 1;
        }
    }
    return 0;
}

pointer_list_iter_t pointer_list_all(const pointer_list_t *pointer_list) {
    pointer_list_iter_t it;
    assert(pointer_list != NULL && "pointer_list primitive called on a NULL container");
    it.pointer_list = pointer_list;
    it.index = -1;
    it.last_count = pointer_list->count;
    return it;
}

int pointer_list_iter_next(pointer_list_iter_t *iter, void **out) {
    int next_index;

    if (iter->index < 0) {
        next_index = 0;
    } else if (iter->pointer_list->count < iter->last_count) {
        next_index = iter->index;
    } else {
        next_index = iter->index + 1;
    }

    if (next_index >= iter->pointer_list->count) {
        return 0;
    }

    *out = iter->pointer_list->items[next_index];
    iter->index = next_index;
    iter->last_count = iter->pointer_list->count;
    return 1;
}

void pointer_list_foreach(const pointer_list_t *pointer_list, pointer_list_foreach_fn_t fn, void *ctx) {
    int i;
    assert(pointer_list != NULL && "pointer_list primitive called on a NULL container");
    for (i = 0; i < pointer_list->count; i++) {
        fn(pointer_list->items[i], ctx);
    }
}
