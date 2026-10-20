#ifndef TESTS_HEAP_ALLOCATOR_H
#define TESTS_HEAP_ALLOCATOR_H

#include "engine/allocator.h"

/* The unit tests' allocator, backed by malloc, realloc and free: a block it
 * allocates may also be freed with free. */
extern const allocator_t heap_allocator;

#endif
