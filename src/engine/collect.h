#ifndef ENGINE_COLLECT_H
#define ENGINE_COLLECT_H

#include "engine/geom.h"
#include "engine/pointer_list.h"

typedef int (*filter_pointer_list_predicate_fn_t)(void *e, void *ctx);
pointer_list_t collect_filter_pointer_list(const allocator_t *zone, const pointer_list_t *qk,
                           filter_pointer_list_predicate_fn_t ow, void *ctx);

pointer_list_t collect_concat_pointer_list(const allocator_t *zone, const pointer_list_t *a,
                           const pointer_list_t *b);

typedef int (*map_pointer_list_fn_t)(void *e, void *ctx, void **out);
pointer_list_t collect_map_pointer_list(const allocator_t *zone, const pointer_list_t *qk,
                           map_pointer_list_fn_t kp, void *ctx);

int collect_contains_string(const pointer_list_t *qk, const char *kb);

char *collect_format_signed(const allocator_t *zone, p8_fixed_t d, int c, const char *ku,
               int *out_color);

#endif
