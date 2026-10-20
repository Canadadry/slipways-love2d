#ifndef ENGINE_ENTITY_H
#define ENGINE_ENTITY_H

#include "engine/entity_class.h"

void entity_init(void);

void entity_shutdown(void);

int entity_ready(void);

void entity_set_state(entity_t *self, state_t state);

void entity_init_instance(entity_t *self, const entity_class_t *cls, state_t state, p8_fixed_t t);

const char *entity_state_name(state_t state);

void entity_construct(entity_t *self);

#endif
