#include "engine/allocator.h"
#include "table_reference.h"

void table_reference_engine_allocator(void) {
    table_reference_int("engine.allocator", (long long)ARENA_HEAD_BYTES, "ARENA_HEAD_BYTES");
    table_reference_int("engine.allocator", (long long)sizeof(arena_t), "SIZEOF.arena_t");
    table_reference_int("engine.allocator", (long long)sizeof(zone_cell_t), "SIZEOF.zone_cell_t");
}
