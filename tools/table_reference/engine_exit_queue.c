#include "engine/exit_queue.h"
#include "table_reference.h"

void table_reference_engine_exit_queue(void) {
    table_reference_int("engine.exit_queue", EXIT_QUEUE_CAP, "EXIT_QUEUE_CAP");
}
