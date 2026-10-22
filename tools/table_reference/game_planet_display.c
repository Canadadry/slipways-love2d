#include "game/planet_display.h"
#include "game/ui_button.h"
#include "table_reference.h"

void table_reference_game_planet_display(void) {
    const char *module = "game.planet_display";
    table_reference_int(module, (long long)sizeof(button_keys_t), "SIZEOF.button_keys_t");
}
