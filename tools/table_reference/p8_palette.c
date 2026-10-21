#include "p8/palette.c"
#include "table_reference.h"

void table_reference_p8_palette(void) {
    for (int i = 0; i < P8_PALETTE_COLORS; i++) {
        table_reference_int("p8.palette", P8_PALETTE_RGB[i].red, "P8_PALETTE_RGB[%d].red", i);
        table_reference_int("p8.palette", P8_PALETTE_RGB[i].green, "P8_PALETTE_RGB[%d].green", i);
        table_reference_int("p8.palette", P8_PALETTE_RGB[i].blue, "P8_PALETTE_RGB[%d].blue", i);
    }
}
