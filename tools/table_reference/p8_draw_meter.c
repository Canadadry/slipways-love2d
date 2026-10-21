#include "p8/draw_meter.c"
#include "reference_backend.h"

void table_reference_p8_draw_meter(void) {
    table_reference_draw_backend("p8.draw_meter", "METER", &METER);
}
