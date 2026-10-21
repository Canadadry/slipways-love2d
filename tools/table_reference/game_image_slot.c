#include "game/image_slot.h"
#include "table_reference.h"

void table_reference_game_image_slot(void) {
    const char *module = "game.image_slot";
    table_reference_int(module, IMAGE_SLOT_BACKGROUND, "IMAGE_SLOT_BACKGROUND");
    table_reference_int(module, IMAGE_SLOT_ROW_FIRST, "IMAGE_SLOT_ROW_FIRST");
    table_reference_int(module, IMAGE_SLOT_ROW_COUNT, "IMAGE_SLOT_ROW_COUNT");
    table_reference_int(module, IMAGE_SLOT_TEXT_FIRST, "IMAGE_SLOT_TEXT_FIRST");
    table_reference_int(module, IMAGE_SLOT_TEXT_COUNT, "IMAGE_SLOT_TEXT_COUNT");
    table_reference_int(module, IMAGE_SLOT_PLANET_DISC, "IMAGE_SLOT_PLANET_DISC");
    table_reference_int(module, IMAGE_SLOT_PLANET_RING_FIRST, "IMAGE_SLOT_PLANET_RING_FIRST");
    table_reference_int(module, IMAGE_SLOT_BUTTON_ICON_FIRST, "IMAGE_SLOT_BUTTON_ICON_FIRST");
    table_reference_int(module, IMAGE_SLOT_BUTTON_ICON_COUNT, "IMAGE_SLOT_BUTTON_ICON_COUNT");
    table_reference_int(module, IMAGE_SLOT_BUTTON_WIDE_FIRST, "IMAGE_SLOT_BUTTON_WIDE_FIRST");
    table_reference_int(module, IMAGE_SLOT_BUTTON_WIDE_COUNT, "IMAGE_SLOT_BUTTON_WIDE_COUNT");
    table_reference_int(module, IMAGE_SLOT_COUNT, "IMAGE_SLOT_COUNT");
}
