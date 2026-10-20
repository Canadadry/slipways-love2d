#ifndef ENGINE_POINTER_LIST_H
#define ENGINE_POINTER_LIST_H

#include "engine/allocator.h"

typedef struct {
    void **items;
    int count;
    int capacity;
    const allocator_t *zone;
} pointer_list_t;

void pointer_list_init(pointer_list_t *pointer_list, const allocator_t *zone);
void pointer_list_init_reserve(pointer_list_t *pointer_list, const allocator_t *zone, int capacity);
void pointer_list_free(pointer_list_t *pointer_list);
void pointer_list_add(pointer_list_t *pointer_list, void *e);
int pointer_list_del(pointer_list_t *pointer_list, const void *e);

typedef struct {
    const pointer_list_t *pointer_list;
    int index;
    int last_count;
} pointer_list_iter_t;
pointer_list_iter_t pointer_list_all(const pointer_list_t *pointer_list);
int pointer_list_iter_next(pointer_list_iter_t *iter, void **out);

typedef void (*pointer_list_foreach_fn_t)(void *e, void *ctx);
void pointer_list_foreach(const pointer_list_t *pointer_list, pointer_list_foreach_fn_t fn, void *ctx);

#endif
