#include "engine/entity_list.h"
#include "table_reference.h"

void table_reference_engine_entity_list(void) {
    table_reference_int("engine.entity_list", (long long)sizeof(entity_t *), "SIZEOF[\"entity_t *\"]");
}
