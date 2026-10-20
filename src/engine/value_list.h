#ifndef ENGINE_VALUE_LIST_H
#define ENGINE_VALUE_LIST_H

#include <stddef.h>

#include "engine/allocator.h"

typedef struct {
    unsigned char *items;
    size_t element_size;
    int count;
    int capacity;
    const allocator_t *zone;
} value_list_t;

void value_list_init(value_list_t *list, const allocator_t *zone, size_t element_size);
void value_list_free(value_list_t *list);
void value_list_clear(value_list_t *list);
/* A new list in zone holding a copy of every value of list. */
value_list_t value_list_copy(const allocator_t *zone, const value_list_t *list);
void value_list_add(value_list_t *list, const void *value);
int value_list_count(const value_list_t *list);
/* Valid until the next add or free: never store it. */
void *value_list_at(const value_list_t *list, int index);

#endif
