#include "game/pad_cycle.h"

#include <assert.h>

void pad_cycle_init(pad_cycle_t *cycle) {
    *cycle = (pad_cycle_t){.panel = PAD_CYCLE_CLOSED, .landing_due = 0};
}

pad_cycle_panel_t pad_cycle_panel(const pad_cycle_t *cycle) {
    return cycle->panel;
}

int pad_cycle_landing_due(const pad_cycle_t *cycle) {
    return cycle->landing_due;
}

void pad_cycle_set(pad_cycle_t *cycle, pad_cycle_panel_t panel) {
    cycle->panel = panel;
    cycle->landing_due = panel != PAD_CYCLE_CLOSED;
}

void pad_cycle_landed(pad_cycle_t *cycle) {
    cycle->landing_due = 0;
}

pad_cycle_panel_t pad_cycle_next(pad_cycle_panel_t panel) {
    switch (panel) {
    case PAD_CYCLE_CLOSED:
        return PAD_CYCLE_TECH;
    case PAD_CYCLE_TECH:
        return PAD_CYCLE_SCORE;
    case PAD_CYCLE_SCORE:
        return PAD_CYCLE_CLOSED;
    }
    assert(0 && "pad_cycle_next: not a pad_cycle_panel_t");
    return PAD_CYCLE_CLOSED;
}

int pad_cycle_hud_button(pad_cycle_panel_t panel) {
    return panel == PAD_CYCLE_TECH ? 2 : panel == PAD_CYCLE_SCORE ? 3 : 0;
}
