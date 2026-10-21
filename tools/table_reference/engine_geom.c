#include "engine/geom.h"
#include "table_reference.h"

void table_reference_engine_geom(void) {
    const char *module = "engine.geom";
    table_reference_int(module, SIDE_ON_LINE, "SIDE_ON_LINE");
    table_reference_int(module, SIDE_POSITIVE, "SIDE_POSITIVE");
    table_reference_int(module, SIDE_NEGATIVE, "SIDE_NEGATIVE");
    table_reference_int(module, (long long)sizeof(box_t), "SIZEOF.box_t");
}
