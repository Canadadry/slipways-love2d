#ifndef GAME_CHEAT_H
#define GAME_CHEAT_H

#include "p8/input.h"

#define CHEAT_CHORDS 4

typedef struct {
    int previous_first;
    int previous_second;
    int chords;
} cheat_t;

void cheat_init(cheat_t *cheat);

int cheat_chord_buttons(void);

int cheat_step(cheat_t *cheat, int pad_buttons);

#endif
