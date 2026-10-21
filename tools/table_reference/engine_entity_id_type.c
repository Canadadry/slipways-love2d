#include "engine/entity_id_type.h"
#include "table_reference.h"

void table_reference_engine_entity_id_type(void) {
    const char *module = "engine.entity_id_type";
    table_reference_int(module, ENTITY_ID_NONE.index, "ENTITY_ID_NONE.index");
    table_reference_int(module, ENTITY_ID_NONE.generation, "ENTITY_ID_NONE.generation");
}
