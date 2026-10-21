#include "p8/sprite_sheet_packed.h"
#include "table_reference.h"

void table_reference_p8_sprite_sheet_packed(void) {
    for (int i = 0; i < (int)sizeof p8_sprite_sheet_packed_bytes; i++) {
        table_reference_int("p8.sprite_sheet_packed", p8_sprite_sheet_packed_bytes[i],
                            "p8_sprite_sheet_packed_bytes[%d]", i);
    }
}
