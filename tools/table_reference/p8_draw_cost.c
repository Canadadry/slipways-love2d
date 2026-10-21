#include "p8/draw_cost.c"
#include "table_reference.h"

void table_reference_p8_draw_cost(void) {
    table_reference_int("p8.draw_cost", (long long)FREE.bytes, "FREE.bytes");
    table_reference_int("p8.draw_cost", FREE.primitives, "FREE.primitives");
}
