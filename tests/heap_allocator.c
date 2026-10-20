#include "heap_allocator.h"

#include <stdlib.h>

static unsigned char *heap_alloc(const allocator_t *z, size_t n) {
    (void)z;
    unsigned char *p = malloc(n);
    if (p == NULL) abort();
    return p;
}

static unsigned char *heap_grow(const allocator_t *z, unsigned char *p, size_t old_n,
                               size_t new_n) {
    (void)z;
    (void)old_n;
    unsigned char *q = realloc(p, new_n);
    if (q == NULL) abort();
    return q;
}

static void heap_release(const allocator_t *z, unsigned char *p) {
    (void)z;
    free(p);
}

const allocator_t heap_allocator = {heap_alloc, heap_grow, heap_release, NULL, NULL};
