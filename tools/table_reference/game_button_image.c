#include "game/button_image.c"
#include "table_reference.h"

void table_reference_game_button_image(void) {
    const char *module = "game.button_image";
    table_reference_int(module, BUTTON_IMAGE_HEIGHT, "BUTTON_IMAGE_HEIGHT");
    table_reference_int(module, BUTTON_IMAGE_ICON_WIDTH, "BUTTON_IMAGE_ICON_WIDTH");
    table_reference_int(module, BUTTON_IMAGE_WIDE_WIDTH, "BUTTON_IMAGE_WIDE_WIDTH");
    table_reference_int(module, g_icon_pool.first, "g_icon_pool.first");
    table_reference_int(module, g_icon_pool.count, "g_icon_pool.count");
    table_reference_int(module, g_wide_pool.first, "g_wide_pool.first");
    table_reference_int(module, g_wide_pool.count, "g_wide_pool.count");
}
