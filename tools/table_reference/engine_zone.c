#include "engine/zone.h"
#include "table_reference.h"

void table_reference_engine_zone(void) {
    table_reference_int("engine.zone", BUMP_ZONE_HALVES, "BUMP_ZONE_HALVES");
    table_reference_int("engine.zone", (long long)sizeof(zone_cell_t), "SIZEOF.zone_cell_t");
    table_reference_int("engine.zone", (long long)sizeof(void *), "SIZEOF[\"void *\"]");
}
