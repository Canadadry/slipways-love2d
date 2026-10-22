#include "game/mouse.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_mouse(void) {
    const char *module = "game.mouse";
    reference_class(module, "mouse_class_def", &mouse_class_def);
    table_reference_int(module, (long long)sizeof(mouse_t), "SIZEOF.mouse_t");
}
