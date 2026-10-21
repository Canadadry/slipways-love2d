#include "game/attract.h"
#include "table_reference.h"

#define ATTRACT_CONSTANTS(X) \
    X(ATTRACT_AFTER_DEFAULT) \
    X(ATTRACT_PHASE_IDLE) \
    X(ATTRACT_PHASE_PLAYING) \
    X(ATTRACT_PHASE_SWALLOWING) \
    X(ATTRACT_ACTION_NONE) \
    X(ATTRACT_ACTION_START) \
    X(ATTRACT_ACTION_RETURN)

#define RENDER_CONSTANT(name) table_reference_int(module, name, #name);

void table_reference_game_attract(void) {
    const char *module = "game.attract";
    ATTRACT_CONSTANTS(RENDER_CONSTANT)
}
