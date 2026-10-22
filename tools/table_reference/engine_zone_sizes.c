#include <stdint.h>

#include "engine/zone_sizes.h"
#include "table_reference.h"

void table_reference_engine_zone_sizes(void) {
    const char *module = "engine.zone_sizes";
    table_reference_int(module, ENTITY_TABLE_CAP, "ENTITY_TABLE_CAP");
    table_reference_int(module, (long long)ENTITY_TABLE_BYTES, "ENTITY_TABLE_BYTES");
    table_reference_int(module, FRAME_ZONE_BYTES, "FRAME_ZONE_BYTES");
    table_reference_int(module, UI_ZONE_BYTES, "UI_ZONE_BYTES");
    table_reference_int(module, PARTICLES_ZONE_BYTES, "PARTICLES_ZONE_BYTES");
    table_reference_int(module, (long long)GENERATION_ZONE_BYTES, "GENERATION_ZONE_BYTES");
    table_reference_int(module, LOAD_ZONE_BYTES, "LOAD_ZONE_BYTES");
    table_reference_int(module, (long long)GAME_ZONE_BYTES, "GAME_ZONE_BYTES");
    table_reference_int(module, HEADER_ID_BYTES, "HEADER_ID_BYTES");
    table_reference_int(module, (long long)ZONE_BUDGET_BYTES, "ZONE_BUDGET_BYTES");
    table_reference_int(module, (long long)ZONE_BUDGET_USED_BYTES, "ZONE_BUDGET_USED_BYTES");
    table_reference_int(module, (long long)sizeof(void *), "SIZEOF[\"void *\"]");
    table_reference_int(module, (long long)sizeof(uint16_t), "SIZEOF.uint16_t");
}
