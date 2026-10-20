#include "game/cheat.h"

#include "p8/fixed.h"
#include "game/main_economy.h"

#include <stddef.h>

enum { CHORD_FIRST = P8_PAD_BUTTON_5, CHORD_SECOND = P8_PAD_BUTTON_6 };

void cheat_init(cheat_t *cheat) {
    cheat->previous_first = 0;
    cheat->previous_second = 0;
    cheat->chords = 0;
}

static void apply(void) {
    entity_t *fm = main_economy();
    economy_t *hy;
    if (fm == NULL) {
        return;
    }
    if (((const entity_t *)fm)->class == main_economy_placeholder_class()) {
        main_economy_placeholder_t *provisional = (main_economy_placeholder_t *)fm;
        provisional->research_points = p8_fixed_from_int(99);
        provisional->has_research_points = 1;
        provisional->credits = p8_fixed_from_int(200);
        provisional->has_credits = 1;
        return;
    }
    hy = main_economy_typed();
    hy->research_points = p8_fixed_from_int(99);
    hy->credits = p8_fixed_from_int(200);
}

int cheat_chord_buttons(void) {
    return CHORD_FIRST | CHORD_SECOND;
}

int cheat_step(cheat_t *cheat, int pad_buttons) {
    int first = (pad_buttons & CHORD_FIRST) != 0;
    int second = (pad_buttons & CHORD_SECOND) != 0;
    int first_edge = first && !cheat->previous_first;
    int second_edge = second && !cheat->previous_second;

    cheat->previous_first = first;
    cheat->previous_second = second;

    if (!first_edge && !second_edge) {
        return 0;
    }
    if (!(first_edge && second_edge)) {
        cheat->chords = 0;
        return 0;
    }
    cheat->chords++;
    if (cheat->chords < CHEAT_CHORDS) {
        return 0;
    }
    cheat->chords = 0;
    apply();
    return 1;
}
