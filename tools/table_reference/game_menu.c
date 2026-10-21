#include "game/menu.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_menu(void) {
    const char *module = "game.menu";
    reference_class(module, "pause_menu_open_box_class_def", &pause_menu_open_box_class_def);
    reference_header(module, "g_pause_menu_open_box", &g_pause_menu_open_box.header, "pause_menu_open_box_class_def",
                     &pause_menu_open_box_class_def);
    table_reference_int(module, g_pause_menu_open_box.has, "g_pause_menu_open_box.has_");
    table_reference_int(module, g_pause_menu_open_box.fixed_pos.x, "g_pause_menu_open_box.fixed_pos.x");
    table_reference_int(module, g_pause_menu_open_box.fixed_pos.y, "g_pause_menu_open_box.fixed_pos.y");
    table_reference_int(module, g_pause_menu_open_box.row_step.x, "g_pause_menu_open_box.row_step.x");
    table_reference_int(module, g_pause_menu_open_box.row_step.y, "g_pause_menu_open_box.row_step.y");
    table_reference_int(module, g_pause_menu_open_box.back_color, "g_pause_menu_open_box.back_color");
    table_reference_int(module, g_pause_menu_open_box.has_back_color, "g_pause_menu_open_box.has_back_color");
    for (int i = 0; i < GALAXY_MENU_COUNT; i++) {
        table_reference_function(module, galaxy_menu_on_click_table[i] != NULL, "galaxy_menu_on_click_table[%d]", i);
        table_reference_function(module, galaxy_menu_refresh_table[i] != NULL, "galaxy_menu_refresh_table[%d]", i);
    }
    table_reference_int(module, (long long)sizeof(data_row_t), "SIZEOF.data_row_t");
    table_reference_int(module, (long long)sizeof(data_item_t), "SIZEOF.data_item_t");
    table_reference_int(module, (long long)sizeof(button_keys_t), "SIZEOF.button_keys_t");
}
