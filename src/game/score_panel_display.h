#ifndef GAME_SCORE_PANEL_DISPLAY_H
#define GAME_SCORE_PANEL_DISPLAY_H

#include "p8/fixed.h"
#include "game/score_panel.h"

void score_panel_update(score_panel_t *self);

void score_panel_new_year(p8_fixed_t year);

modal_backdrop_on_click_fn_t score_panel_toggle_on_click(void);

#endif
