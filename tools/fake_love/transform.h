#ifndef FAKE_LOVE_TRANSFORM_H
#define FAKE_LOVE_TRANSFORM_H

#include <stdbool.h>

#define TRANSFORM_STACK_LIMIT 128
#define TRANSFORM_OFFSET_LIMIT 1048576

typedef struct transform_offset {
    int x;
    int y;
} transform_offset_t;

typedef struct transform {
    transform_offset_t current;
    int depth;
    transform_offset_t saved[TRANSFORM_STACK_LIMIT];
} transform_t;

void transform_init(transform_t *transform);

/* False at depth TRANSFORM_STACK_LIMIT, as LÖVE 11.5; a refusal changes
 * nothing. */
bool transform_push(transform_t *transform);

/* False at depth 0; a refusal changes nothing. */
bool transform_pop(transform_t *transform);

/* False when either total would reach TRANSFORM_OFFSET_LIMIT (2^20) in
 * magnitude; a refusal changes nothing. Asserts |dx| and |dy| below it. */
bool transform_translate(transform_t *transform, int dx, int dy);

/* Zeroes the current offset; the depth and the saved offsets stay. */
void transform_reset(transform_t *transform);

transform_offset_t transform_get_offset(const transform_t *transform);

int transform_get_depth(const transform_t *transform);

#endif
