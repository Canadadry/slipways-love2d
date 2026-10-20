#include "game/tech_gate.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/collect.h"
#include "game/economy_formulas.h"
#include "game/main_economy.h"

static int g_affordable_flags[TECHNOLOGIES_COUNT];

int tech_gate_is_unlocked(const char *px) {
    const economy_t *fm = main_economy_typed();
    const pointer_list_t *invented_list = NULL;

    if (px == NULL) {
        return 1;
    }
    if (fm != NULL) {
        invented_list = &fm->invented;
    }
    return collect_contains_string(invented_list, px);
}

p8_fixed_t tech_gate_if_unlocked(const char *px, p8_fixed_t lk, p8_fixed_t mc) {
    if (tech_gate_is_unlocked(px)) {
        return mc;
    }
    return lk;
}

static p8_fixed_t research_cost_with_discount(p8_fixed_t od, int has_planet_type, p8_fixed_t nk) {
    const economy_t *fm = main_economy_typed();
    p8_fixed_t fu = p8_fixed_from_int(0);
    p8_fixed_t threshold = P8_FIXED_LIT(0x00008ccc, 0.55);
    p8_fixed_t factor;

    if (has_planet_type && fm != NULL && (fm->has & ECONOMY_HAS_DEMAND)) {
        resource_map_get(&fm->demand, nk, &fu);
    }

    factor = (fu > threshold) ? fu : threshold;
    return p8_fixed_flr(p8_fixed_mul(od, factor));
}

p8_fixed_t tech_gate_research_cost(const technologies_entry_t *t) {
    if (t == NULL) {
        return research_cost_with_discount(p8_fixed_from_int(0), 0, 0);
    }
    return research_cost_with_discount(p8_fixed_from_int(t->cost), 1, p8_fixed_from_int(t->icon));
}

static int available_technologies_predicate(const technologies_entry_t *t, int bi) {
    const economy_t *fm = main_economy_typed();
    const main_economy_placeholder_t *provisional = main_economy_placeholder();
    p8_fixed_t jk = p8_fixed_from_int(0);
    p8_fixed_t ew = p8_fixed_from_int(0);
    int affordable;

    if (fm != NULL) {
        jk = fm->research_points;
    } else if (provisional != NULL && provisional->has_research_points) {
        jk = provisional->research_points;
    }
    affordable = tech_gate_research_cost(t) <= jk;
    /* Written for every entry, before any early return: tech_gate_affordable_flags() reads it back. */
    g_affordable_flags[t - technologies_table] = affordable;

    if (fm != NULL) {
        ew = fm->era;
    }
    if (p8_fixed_from_int(t->era) != ew) {
        return 0;
    }

    if (tech_gate_is_unlocked(t->name)) {
        return 0;
    }

    return affordable || bi;
}

available_technologies_kept_t tech_gate_available_technologies(int bi) {
    available_technologies_kept_t result;
    int i;

    result.count = 0;
    for (i = 0; i < TECHNOLOGIES_COUNT; i++) {
        if (available_technologies_predicate(&technologies_table[i], bi)) {
            assert(result.count < TECHNOLOGIES_COUNT && "tech_gate_available_technologies: it keeps technologies_table entries only, so their count bounds the result");
            result.items[result.count++] = &technologies_table[i];
        }
    }
    return result;
}

int tech_gate_affordable_flags(const technologies_entry_t *t) {
    assert(t >= technologies_table && t < technologies_table + TECHNOLOGIES_COUNT && "tech_gate_affordable_flags: the flag is kept for technologies_table entries only");
    return g_affordable_flags[t - technologies_table];
}

void tech_gate_reset_affordable_flags(void) {
    memset(g_affordable_flags, 0, sizeof g_affordable_flags);
}
