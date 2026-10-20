#include "engine/collect.h"

#include <assert.h>
#include <string.h>

#include "p8/str.h"

pointer_list_t collect_filter_pointer_list(const allocator_t *zone, const pointer_list_t *qk,
                           filter_pointer_list_predicate_fn_t ow, void *ctx) {
    pointer_list_t f;
    pointer_list_iter_t it;
    void *e;
    pointer_list_init(&f, zone);
    if (qk == NULL) {
        return f;
    }
    it = pointer_list_all(qk);
    while (pointer_list_iter_next(&it, &e)) {
        if (ow(e, ctx)) {
            pointer_list_add(&f, e);
        }
    }
    return f;
}

pointer_list_t collect_map_pointer_list(const allocator_t *zone, const pointer_list_t *qk,
                           map_pointer_list_fn_t kp, void *ctx) {
    pointer_list_t m;
    pointer_list_iter_t it;
    void *e;
    assert(qk != NULL && "collect_map_pointer_list: a NULL list is not tolerated here, unlike collect_contains_string and collect_filter_pointer_list");
    pointer_list_init(&m, zone);
    it = pointer_list_all(qk);
    while (pointer_list_iter_next(&it, &e)) {
        void *v;
        if (kp(e, ctx, &v)) {
            pointer_list_add(&m, v);
        }
    }
    return m;
}

static void append_pointer_list(void *e, void *ctx) {
    pointer_list_add(ctx, e);
}

pointer_list_t collect_concat_pointer_list(const allocator_t *zone, const pointer_list_t *a,
                           const pointer_list_t *b) {
    pointer_list_t t;
    assert(a != NULL && b != NULL && "collect_concat_pointer_list: a NULL list is not tolerated here, unlike collect_contains_string and collect_filter_pointer_list");
    pointer_list_init(&t, zone);
    pointer_list_foreach(a, append_pointer_list, &t);
    pointer_list_foreach(b, append_pointer_list, &t);
    return t;
}

/* Strings in this port are copied, never interned, so two equal strings are
   not the same pointer: compare by content (strcmp), never by pointer. */
int collect_contains_string(const pointer_list_t *qk, const char *kb) {
    pointer_list_iter_t it;
    void *e;
    if (qk == NULL) {
        return 0;
    }
    it = pointer_list_all(qk);
    while (pointer_list_iter_next(&it, &e)) {
        if (e != NULL && strcmp(e, kb) == 0) {
            return 1;
        }
    }
    return 0;
}

char *collect_format_signed(const allocator_t *zone, p8_fixed_t d, int c, const char *ku,
               int *out_color) {
    char *num_str = p8_tostr_fixed(zone, d);
    const char *suffix = ku != NULL ? ku : "";
    char *result;
    size_t prefix_len;

    if (d >= 0) {
        *out_color = c;
        prefix_len = 1 + strlen(num_str);
        result = (char *)allocator_alloc(zone, prefix_len + strlen(suffix) + 1);
        result[0] = '+';
        memcpy(result + 1, num_str, strlen(num_str) + 1);
    } else {
        *out_color = 8;
        prefix_len = strlen(num_str);
        result = (char *)allocator_alloc(zone, prefix_len + strlen(suffix) + 1);
        memcpy(result, num_str, strlen(num_str) + 1);
    }
    strcat(result, suffix);
    allocator_release(zone, (unsigned char *)num_str);
    return result;
}
