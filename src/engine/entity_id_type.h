#ifndef ENGINE_ENTITY_ID_TYPE_H
#define ENGINE_ENTITY_ID_TYPE_H

#include <stdint.h>

typedef struct {
    uint16_t index;
    uint16_t generation;
} entity_id_t;

#define ENTITY_ID_NONE ((entity_id_t){0, 0})

static inline int entity_id_equal(entity_id_t a, entity_id_t b) {
    return a.index == b.index && a.generation == b.generation;
}

static inline int entity_id_is_none(entity_id_t id) {
    return entity_id_equal(id, ENTITY_ID_NONE);
}

#endif
