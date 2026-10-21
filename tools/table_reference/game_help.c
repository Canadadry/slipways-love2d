#include "game/help.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_help(void) {
    const char *module = "game.help";
    reference_class(module, "help_screen_class_def", &help_screen_class_def);
    table_reference_int(module, (long long)sizeof(probe_t), "SIZEOF.probe_t");
}
