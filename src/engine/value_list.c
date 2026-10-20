#include "engine/value_list.h"

#include <assert.h>
#include <string.h>

#define VALUE_LIST_INITIAL_CAPACITY 4

void value_list_init(value_list_t *list, const allocator_t *zone, size_t element_size) {
    assert(zone != NULL && "value_list_init: a list without a zone would grow nowhere");
    assert(element_size > 0);
    list->items = NULL;
    list->element_size = element_size;
    list->count = 0;
    list->capacity = 0;
    list->zone = zone;
}

void value_list_free(value_list_t *list) {
    if (list->items != NULL) allocator_release(list->zone, list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void value_list_clear(value_list_t *list) {
    list->count = 0;
}

value_list_t value_list_copy(const allocator_t *zone, const value_list_t *list) {
    value_list_t out;
    value_list_init(&out, zone, list->element_size);
    if (list->count == 0) return out;
    size_t bytes = (size_t)list->count * list->element_size;
    out.items = allocator_alloc(zone, bytes);
    memcpy(out.items, list->items, bytes);
    out.count = list->count;
    out.capacity = list->count;
    return out;
}

static void grow(value_list_t *list) {
    int new_capacity = list->capacity == 0 ? VALUE_LIST_INITIAL_CAPACITY : list->capacity * 2;
    list->items = allocator_grow(list->zone, list->items, (size_t)list->capacity * list->element_size,
                                 (size_t)new_capacity * list->element_size);
    list->capacity = new_capacity;
}

void value_list_add(value_list_t *list, const void *value) {
    if (list->count == list->capacity) grow(list);
    memcpy(list->items + (size_t)list->count * list->element_size, value, list->element_size);
    list->count++;
}

int value_list_count(const value_list_t *list) {
    return list->count;
}

void *value_list_at(const value_list_t *list, int index) {
    assert(index >= 0 && index < list->count && "value_list_at: index out of range");
    return list->items + (size_t)index * list->element_size;
}
