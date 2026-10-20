#ifndef ENGINE_BOOTSTRAP_H
#define ENGINE_BOOTSTRAP_H

#include "engine/entity_registry.h"

void reset_world(const entity_registry_reserve_t *reserve);

void bootstrap_shutdown(void);

void update_entities(void);

#endif
