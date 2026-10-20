#include "game/pad_cycle.h"

#include <assert.h>
#include <stdio.h>

static void test_init_is_closed_with_no_landing_due(void) {
    pad_cycle_t cycle;
    pad_cycle_init(&cycle);
    assert(pad_cycle_panel(&cycle) == PAD_CYCLE_CLOSED);
    assert(!pad_cycle_landing_due(&cycle));
}

static void test_next_goes_closed_tech_score_closed(void) {
    assert(pad_cycle_next(PAD_CYCLE_CLOSED) == PAD_CYCLE_TECH);
    assert(pad_cycle_next(PAD_CYCLE_TECH) == PAD_CYCLE_SCORE);
    assert(pad_cycle_next(PAD_CYCLE_SCORE) == PAD_CYCLE_CLOSED);
}

static void test_setting_a_panel_makes_its_landing_due_and_closing_clears_it(void) {
    static const pad_cycle_panel_t panels[2] = { PAD_CYCLE_TECH, PAD_CYCLE_SCORE };
    for (int i = 0; i < 2; i++) {
        pad_cycle_t cycle;
        pad_cycle_init(&cycle);
        pad_cycle_set(&cycle, panels[i]);
        assert(pad_cycle_panel(&cycle) == panels[i] && pad_cycle_landing_due(&cycle));
        pad_cycle_landed(&cycle);
        assert(pad_cycle_panel(&cycle) == panels[i] && !pad_cycle_landing_due(&cycle));
        pad_cycle_set(&cycle, panels[i]);
        pad_cycle_set(&cycle, PAD_CYCLE_CLOSED);
        assert(pad_cycle_panel(&cycle) == PAD_CYCLE_CLOSED && !pad_cycle_landing_due(&cycle));
    }
}

static void test_the_hud_button_is_tech_2_score_3_and_none_when_closed(void) {
    assert(pad_cycle_hud_button(PAD_CYCLE_CLOSED) == 0);
    assert(pad_cycle_hud_button(PAD_CYCLE_TECH) == 2);
    assert(pad_cycle_hud_button(PAD_CYCLE_SCORE) == 3);
}

int main(void) {
    test_init_is_closed_with_no_landing_due();
    test_next_goes_closed_tech_score_closed();
    test_setting_a_panel_makes_its_landing_due_and_closing_clears_it();
    test_the_hud_button_is_tech_2_score_3_and_none_when_closed();
    printf("test_pad_cycle: ok\n");
    return 0;
}
