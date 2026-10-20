#include "game/data_init.h"

#include "game/research.h"
#include "game/tech_gate.h"

void data_init(void) {
    /* research_init() forgets the projects, so it runs before the build. */
    research_init();
    research_build_projects();

    tech_gate_reset_affordable_flags();
}

void data_init_shutdown(void) {
    research_shutdown();
}
