#ifndef GAME_ECONOMY_SCORE_H
#define GAME_ECONOMY_SCORE_H

#include "engine/allocator.h"
#include "engine/entity_class.h"
#include "p8/fixed.h"
#include "game/ui_button.h"

p8_fixed_t economy_score_planet_upkeep(entity_t *pt);

p8_fixed_t economy_score_colony_upkeep(void);

p8_fixed_t economy_score_trade_income(const entity_t *entity);

typedef struct {
    p8_fixed_t development;
    p8_fixed_t population;
    p8_fixed_t technology;
    p8_fixed_t happiness;
    p8_fixed_t total;
    p8_fixed_t tier;
} final_score_t;

final_score_t economy_score_final(void);

/* Returns `value` printed with a trailing '%', allocated in `zone`. */
char *economy_score_percent_text(const allocator_t *zone, p8_fixed_t value);

p8_fixed_t economy_score_happiness(void);

const button_t *economy_score_difficulty(void);
void economy_score_set_difficulty(const button_t *dk);

p8_fixed_t economy_score_net_income(entity_t *p);

#endif
