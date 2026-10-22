#include "p8/image.c"
#include "reference_backend.h"

void table_reference_p8_image(void) {
    for (int i = 0; i < P8_IMAGE_SLOT_COUNT; i++) {
        table_reference_int("p8.image", SLOT_SIZES[i].width, "SLOT_SIZES[%d].width", i);
        table_reference_int("p8.image", SLOT_SIZES[i].height, "SLOT_SIZES[%d].height", i);
    }
    table_reference_draw_backend("p8.image", "GUARD", &GUARD);
}
