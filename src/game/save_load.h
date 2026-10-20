#ifndef GAME_SAVE_LOAD_H
#define GAME_SAVE_LOAD_H

#include "game/save_state.h"
#include "game/ui_button.h"

/* The world rebuild of cart_resume_game, in the order it calls them. Every
   state came out of a save_state_decode that returned 1: nothing is
   validated again. None spends credits, advances the clock or rolls a type. */

/* Fills *out the way the title fills a difficulty button. */
void save_load_difficulty(const save_state_t *state, button_t *out);

/* Before the prologue builds the background: the background then draws
   the saved tile map. */
void save_load_background_seed(const save_state_t *state);

/* Where cart_start_game calls galaxy_populate: the saved nodes, then the
   type picker and vanish walk. */
void save_load_world(const save_state_t *state);

/* After the economy and the interface are built; restores rnd_state last. */
void save_load_globals(const save_state_t *state);

#endif
