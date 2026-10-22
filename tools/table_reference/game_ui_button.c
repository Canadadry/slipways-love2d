#include "game/ui_button.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_ui_button(void) {
    const char *module = "game.ui_button";
    reference_class(module, "button_class_def", &button_class_def);
    reference_class(module, "modal_backdrop_class_def", &modal_backdrop_class_def);
    table_reference_int(module, BUTTON_HAS_SLIDE_TARGET, "BUTTON_HAS_SLIDE_TARGET");
    table_reference_int(module, (long long)sizeof(button_t), "SIZEOF.button_t");
    table_reference_int(module, (long long)sizeof(modal_backdrop_t), "SIZEOF.modal_backdrop_t");
}
