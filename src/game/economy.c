#include "game/economy.h"

#include <assert.h>
#include <string.h>

#include "engine/collect.h"
#include "game/economy_formulas.h"
#include "game/economy_score.h"
#include "game/economy_tiers.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/entity_list.h"
#include "game/main_economy.h"
#include "game/game_data.h"
#include "game/map_tables.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "engine/pointer_list.h"
#include "game/research.h"
#include "game/tech_gate.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

static int g_economy_up = 0;

static const entity_class_t economy_class_def = {0};

const economy_archetype_t economy_archetype = {
    .credits = 0x640000,
    .era = 0x10000,
    .year_ticks = 0x10000,
    .year = 0xd490000,
};

static economy_t *economy_create(void) {
    economy_t *self = (economy_t *)allocator_alloc(game_zone(), sizeof *self);

    entity_init_header(&self->header, &economy_class_def);
    self->archetype = &economy_archetype;
    self->credits = p8_fixed_from_int(0);
    self->income = p8_fixed_from_int(0);
    self->research_points = p8_fixed_from_int(0);
    self->era = p8_fixed_from_int(0);
    self->year_ticks = p8_fixed_from_int(0);
    self->year = p8_fixed_from_int(0);
    self->production.count = 0;
    self->demand.count = 0;
    self->has = 0;
    self->happiness = p8_fixed_from_int(0);
    self->has_happiness = 0;
    self->end_status = NULL;
    self->has_end_status = 0;
    self->save_pending = 0;

    self->credits = self->archetype->credits;
    self->income = p8_fixed_from_int(0);
    self->research_points = p8_fixed_from_int(0);
    self->era = self->archetype->era;
    pointer_list_init(&self->invented, game_zone());
    self->year_ticks = self->archetype->year_ticks;
    self->year = self->archetype->year;

    /* A slot, never entity_register: in the live list the economy would join the update order. */
    entity_id_take(&self->header);
    return self;
}

static void assert_economy(const economy_t *self) {
    assert((self == NULL ||
            self->header.class == &economy_class_def) &&
           "assert_economy: every measured call passes the main economy; anything else would be read through the wrong layout");
}

void economy_init(void) {
    economy_shutdown();
    g_economy_up = 1;
}

void economy_shutdown(void) {
    g_economy_up = 0;
}

int economy_ready(void) {
    return g_economy_up;
}

const entity_class_t *economy_class(void) {
    return &economy_class_def;
}

economy_t *economy_new(void) {
    return economy_create();
}

static void economy_refresh_after_figures(economy_t *self) {
    const entity_list_t *refresh_list;
    entity_list_iter_t iter;
    entity_t *qc;

    if (self->year >= p8_fixed_from_int(3426)) {
        self->end_status = "final score";
    } else if (self->income <= p8_fixed_from_int(0) && self->credits < p8_fixed_from_int(3)) {
        self->end_status = "bankrupt";
    } else {
        self->end_status = NULL;
    }
    self->has_end_status = 1;

    refresh_list = entities_by_message(ENTITY_MESSAGE_REFRESH);
    iter = entity_list_all(refresh_list);
    while (entity_list_iter_next(&iter, &qc)) {
        if (qc != NULL) {
            if (qc->class->refresh != NULL) {
                qc->class->refresh(qc);
            }
        }
    }
}

static void economy_add_ticks(economy_t *self, p8_fixed_t hx, void (*bring_up_to_date)(economy_t *)) {
    p8_fixed_t kx;
    p8_fixed_t lo;

    assert_economy(self);
    if (self == NULL) {
        return;
    }
    self->save_pending = 1;

    kx = tech_gate_if_unlocked("time compression", p8_fixed_from_int(12), p8_fixed_from_int(15));

    lo = p8_fixed_add(self->year_ticks, hx);
    self->year_ticks = lo;

    bring_up_to_date(self);

    if (lo > kx) {
        self->year_ticks = p8_fixed_sub(lo, kx);
        economy_new_year(self);
    }
}

void economy_advance_clock(economy_t *self, p8_fixed_t hx) {
    economy_add_ticks(self, hx, economy_recompute);
}

void economy_tick_clock(economy_t *self, p8_fixed_t hx) {
    economy_add_ticks(self, hx, economy_refresh_after_figures);
}

void economy_new_year(economy_t *self) {
    p8_fixed_t credits;
    p8_fixed_t income;
    p8_fixed_t research_points;
    p8_fixed_t new_year;
    p8_fixed_t science_production = p8_fixed_from_int(0);
    const entity_list_t *new_year_list;
    entity_list_iter_t it;
    entity_t *qc;

    assert_economy(self);
    if (self == NULL) {
        return;
    }
    self->save_pending = 1;

    credits = self->credits;
    income = self->income;
    research_points = self->research_points;

    if (self->has & ECONOMY_HAS_PRODUCTION) {
        int k;
        if (resource_code_lookup("k", &k)) {
            resource_map_get(&self->production, p8_fixed_from_int(k), &science_production);
        }
    }

    self->credits = p8_fixed_add(credits, income);
    self->research_points = p8_fixed_add(p8_fixed_add(research_points, science_production), p8_fixed_from_int(1));
    new_year = p8_fixed_add(self->year, p8_fixed_from_int(1));
    self->year = new_year;

    economy_recompute(self);

    new_year_list = entities_by_message(ENTITY_MESSAGE_NEW_YEAR);
    it = entity_list_all(new_year_list);
    while (entity_list_iter_next(&it, &qc)) {
        if (qc != NULL) {
            if (qc->class->new_year != NULL) {
                qc->class->new_year(qc, new_year);
            }
        }
    }
}

int economy_spend(economy_t *self, p8_fixed_t jt) {
    p8_fixed_t nz;
    toast_keys_t keys = {0};

    assert_economy(self);
    nz = self != NULL ? self->credits : p8_fixed_from_int(0);

    if (nz < jt) {
        toast_keys_set_text(&keys, "-no money-");
        keys.color = p8_fixed_from_int(8);
        toast_new(&keys);
        return 0;
    }

    if (self != NULL) {
        self->credits = p8_fixed_sub(nz, jt);
        self->save_pending = 1;
    }

    if (jt != p8_fixed_from_int(0)) {
        int color;
        char *text = collect_format_signed(frame_zone(), p8_fixed_neg(jt), 10, "$", &color);
        toast_keys_set_text(&keys, text);
        keys.color = p8_fixed_from_int(color);
        toast_new(&keys);
    }

    return 1;
}

void economy_recompute(economy_t *self) {
    p8_fixed_t trade_total = p8_fixed_from_int(0);
    p8_fixed_t income;
    p8_fixed_t happiness;
    p8_fixed_t skill_multiplier;
    const entity_list_t *planet_list;
    entity_list_iter_t iter;
    entity_t *planet;

    assert_economy(self);
    if (self == NULL) {
        return;
    }

    /* Terminates only because update_tiers reports tier increases, capped at 4; a "changed" test spins forever. */
    do {
        economy_tiers_rebuild_incoming_routes();
    } while (economy_tiers_update_tiers());

    economy_total_production(&self->production);
    economy_crowding_penalty(&self->demand);
    self->has |= ECONOMY_HAS_PRODUCTION | ECONOMY_HAS_DEMAND;

    planet_list = entities_by_tag(TAG_PLANET);
    if (planet_list != NULL) {
        iter = entity_list_all(planet_list);
        while (entity_list_iter_next(&iter, &planet)) {
            if (planet != NULL) {
                p8_fixed_t kr = economy_score_net_income(planet);

                trade_total = p8_fixed_add(trade_total, kr);
            }
        }
    }

    skill_multiplier = tech_gate_if_unlocked("skill implants", p8_fixed_from_int(1), P8_FIXED_LIT(0x00012666, 1.15));
    income = p8_fixed_sub(p8_fixed_flr(p8_fixed_mul(trade_total, skill_multiplier)), economy_score_colony_upkeep());
    self->income = income;

    happiness = economy_score_happiness();
    self->happiness = happiness;
    self->has_happiness = 1;

    economy_refresh_after_figures(self);
}

void economy_invent(economy_t *self, const technologies_entry_t *t) {
    p8_fixed_t jk;
    p8_fixed_t ew;
    int count = 0;

    assert_economy(self);
    if (self == NULL) {
        return;
    }
    self->save_pending = 1;

    jk = self->research_points;
    self->research_points = p8_fixed_sub(jk, tech_gate_research_cost(t));

    if (t != NULL && t->name != NULL) {
        pointer_list_add(&self->invented, (void *)t->name);
    }
    count = self->invented.count;

    ew = self->era;
    if (ew < p8_fixed_from_int(4)) {
        int threshold;
        if (int_seq_at(tier_thresholds_table, TIER_THRESHOLDS_COUNT, p8_fixed_flr_int(ew), &threshold) &&
            p8_fixed_from_int(count) >= p8_fixed_from_int(threshold)) {
            self->era = p8_fixed_add(ew, p8_fixed_from_int(1));
        }
    }

    economy_recompute(self);
}

typedef void (*entities_by_tag_visit_fn_t)(entity_t *e, void *ctx);

static void for_each_tagged(tag_t tag, entities_by_tag_visit_fn_t fn, void *ctx) {
    const entity_list_t *list = entities_by_tag(tag);
    entity_list_iter_t it;
    entity_t *e;
    if (list == NULL) {
        return;
    }
    it = entity_list_all(list);
    while (entity_list_iter_next(&it, &e)) {
        if (e != NULL) {
            fn(e, ctx);
        }
    }
}

static void total_production_visit(entity_t *pt, void *ctx) {
    resource_map_t *p = (resource_map_t *)ctx;
    const planet_t *jo;
    const production_t *kw;
    p8_fixed_t current;

    assert(planet_is_family(pt) && "total_production_visit: the planet tag holds planets only");
    jo = (const planet_t *)pt;
    if (!(jo->has & PLANET_HAS_NEEDS)) {
        return;
    }
    kw = planet_production_read(jo);
    if (!kw->has_resource) {
        return;
    }
    if (!resource_map_get(p, kw->resource, &current)) {
        current = p8_fixed_from_int(0);
    }
    resource_map_set(p, kw->resource, p8_fixed_add(current, kw->amount));
}

void economy_total_production(resource_map_t *out) {
    economy_formulas_fill_resource_map(out, p8_fixed_from_int(0));
    for_each_tagged(TAG_PLANET, total_production_visit, out);
}

/* A planet with absent or empty needs is skipped where Lua would crash, like the malformed cases of entity_register. Making one site fatal alone breaks consistency. */
static void crowding_penalty_visit(entity_t *l, void *ctx) {
    resource_map_t *d = (resource_map_t *)ctx;
    const pointer_list_t *jy;
    const need_t *last;
    void *last_val;
    p8_fixed_t current;

    assert(planet_is_family(l) && "crowding_penalty_visit: the structure tag holds planets only");
    if (!(((const planet_t *)l)->has & PLANET_HAS_NEEDS)) {
        return;
    }
    jy = &((const planet_t *)l)->needs;
    if (jy->count == 0) {
        return;
    }
    last_val = jy->items[jy->count - 1];
    if (last_val == NULL) {
        return;
    }
    last = last_val;
    if (!last->row.has_resource) {
        return;
    }
    /* Fallback 1, not 0: the map was seeded with 1 for every resource code. */
    if (!resource_map_get(d, last->row.resource, &current)) {
        current = p8_fixed_from_int(1);
    }
    resource_map_set(d, last->row.resource, p8_fixed_sub(current, P8_FIXED_LIT(0x00002666, 0.15)));
}

void economy_crowding_penalty(resource_map_t *out) {
    economy_formulas_fill_resource_map(out, p8_fixed_from_int(1));
    for_each_tagged(TAG_STRUCTURE, crowding_penalty_visit, out);
}
