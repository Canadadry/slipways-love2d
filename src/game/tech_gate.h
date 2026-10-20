#ifndef GAME_TECH_GATE_H
#define GAME_TECH_GATE_H

#include "p8/fixed.h"
#include "game/map_tables.h"

int tech_gate_is_unlocked(const char *px);

p8_fixed_t tech_gate_if_unlocked(const char *px, p8_fixed_t lk, p8_fixed_t mc);

p8_fixed_t tech_gate_research_cost(const technologies_entry_t *t);

typedef struct {
    const technologies_entry_t *items[TECHNOLOGIES_COUNT];
    int count;
} available_technologies_kept_t;

available_technologies_kept_t tech_gate_available_technologies(int bi);

int tech_gate_affordable_flags(const technologies_entry_t *t);

void tech_gate_reset_affordable_flags(void);

#endif
