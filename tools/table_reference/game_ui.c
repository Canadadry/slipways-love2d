#include "game/ui.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_ui(void) {
    const char *module = "game.ui";
    reference_class(module, "main_mouse_placeholder_class_def", &main_mouse_placeholder_class_def);
    reference_header(module, "g_main_mouse_placeholder", &g_main_mouse_placeholder.header,
                     "main_mouse_placeholder_class_def", &main_mouse_placeholder_class_def);
    reference_class(module, "ui_class_def", &ui_class_def);
    table_reference_int(module, UI_HOVER_LAYERS, "UI_HOVER_LAYERS");
    table_reference_int(module, MOUSE_SIDE_LEFT, "MOUSE_SIDE_LEFT");
    table_reference_int(module, UI_HAS_MOUSE_WORLD, "UI_HAS_MOUSE_WORLD");
    table_reference_int(module, UI_HAS_HOVERED_BY_LAYER, "UI_HAS_HOVERED_BY_LAYER");
    table_reference_int(module, UI_HAS_OWN_EPHEMERALS, "UI_HAS_OWN_EPHEMERALS");
    table_reference_int(module, UI_PAD_BLINK_PERIOD, "UI_PAD_BLINK_PERIOD");
    table_reference_int(module, UI_PAD_CROSSHAIR_ARM, "UI_PAD_CROSSHAIR_ARM");
    table_reference_int(module, UI_PAD_CROSSHAIR_COLOUR, "UI_PAD_CROSSHAIR_COLOUR");
    table_reference_int(module, SEND_TO_HOVERED_LEFT_PRESS, "SEND_TO_HOVERED_LEFT_PRESS");
    table_reference_int(module, SEND_TO_HOVERED_ACCEPT_DROP, "SEND_TO_HOVERED_ACCEPT_DROP");
    table_reference_int(module, (long long)sizeof(ui_t), "SIZEOF.ui_t");
}
