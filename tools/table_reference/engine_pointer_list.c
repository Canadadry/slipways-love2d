#include "engine/pointer_list.h"
#include "table_reference.h"

void table_reference_engine_pointer_list(void) {
    table_reference_int("engine.pointer_list", (long long)sizeof(void *), "SIZEOF[\"void *\"]");
}
