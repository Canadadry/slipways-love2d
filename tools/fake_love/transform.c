#include "transform.h"

#include <assert.h>

void transform_init(transform_t *transform) {
    transform->current = (transform_offset_t){0, 0};
    transform->depth = 0;
}

bool transform_push(transform_t *transform) {
    assert(transform->depth >= 0 && transform->depth <= TRANSFORM_STACK_LIMIT);
    if (transform->depth == TRANSFORM_STACK_LIMIT) {
        return false;
    }
    transform->saved[transform->depth] = transform->current;
    transform->depth++;
    return true;
}

bool transform_pop(transform_t *transform) {
    assert(transform->depth >= 0 && transform->depth <= TRANSFORM_STACK_LIMIT);
    if (transform->depth == 0) {
        return false;
    }
    transform->depth--;
    transform->current = transform->saved[transform->depth];
    return true;
}

static bool below_limit(int value) {
    return value > -TRANSFORM_OFFSET_LIMIT && value < TRANSFORM_OFFSET_LIMIT;
}

bool transform_translate(transform_t *transform, int dx, int dy) {
    assert(below_limit(dx) && below_limit(dy));
    int x = transform->current.x + dx;
    int y = transform->current.y + dy;
    if (!below_limit(x) || !below_limit(y)) {
        return false;
    }
    transform->current = (transform_offset_t){x, y};
    return true;
}

void transform_reset(transform_t *transform) {
    transform->current = (transform_offset_t){0, 0};
}

transform_offset_t transform_get_offset(const transform_t *transform) {
    return transform->current;
}

int transform_get_depth(const transform_t *transform) {
    return transform->depth;
}
