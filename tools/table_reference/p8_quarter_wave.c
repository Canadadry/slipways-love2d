#include "p8/quarter_wave.h"
#include "table_reference.h"

void table_reference_p8_quarter_wave(void) {
    for (int i = 0; i < (int)(sizeof p8_quarter_wave / sizeof p8_quarter_wave[0]); i++) {
        table_reference_int("p8.quarter_wave", p8_quarter_wave[i], "p8_quarter_wave[%d]", i);
    }
}
