#include "engine/slot_tally.h"
#include "table_reference.h"

void table_reference_engine_slot_tally(void) {
    table_reference_int("engine.slot_tally", SLOT_TALLY_KINDS_MAX, "SLOT_TALLY_KINDS_MAX");
}
