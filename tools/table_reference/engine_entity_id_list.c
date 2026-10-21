#include "engine/entity_id_list.h"
#include "table_reference.h"

void table_reference_engine_entity_id_list(void) {
    table_reference_int("engine.entity_id_list", (long long)sizeof(entity_id_t), "SIZEOF.entity_id_t");
}
