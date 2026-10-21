#include "game/input_step.h"
#include "table_reference.h"

void table_reference_game_input_step(void) {
    const char *module = "game.input_step";
    table_reference_int(module, INPUT_STEP_BINDINGS_SESSION, "INPUT_STEP_BINDINGS_SESSION");
    table_reference_int(module, INPUT_STEP_BINDINGS_DEFAULT, "INPUT_STEP_BINDINGS_DEFAULT");
}
