#include "game/economy_tiers.h"

#include <assert.h>

#include "engine/entity_registry.h"
#include "game/game_data.h"
#include "game/planet.h"
#include "game/planet_trade.h"
#include "game/research.h"
#include "game/slipway.h"
#include "game/trade.h"
#include "engine/game_zone.h"

static p8_fixed_t fixed_min(p8_fixed_t a, p8_fixed_t b) {
    return (a < b) ? a : b;
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

static void rebuild_incoming_routes_reset_visit(entity_t *pt, void *ctx) {
    planet_t *jo;
    (void)ctx;
    if (pt == NULL) {
        return;
    }
    assert(planet_is_family(pt) && "rebuild_incoming_routes_reset_visit: the planet tag holds planets only");
    jo = (planet_t *)pt;
    if (jo->has & PLANET_HAS_INCOMING_ROUTES) {
        trade_route_list_clear(&jo->incoming_routes);
        return;
    }
    trade_route_list_init(&jo->incoming_routes, game_zone());
    jo->has |= PLANET_HAS_INCOMING_ROUTES;
}

static void rebuild_incoming_routes_route_visit(entity_t *entity, void *context) {
    planet_t *from_planet;
    trade_route_list_t *outgoing;
    (void)context;
    if (entity == NULL) {
        return;
    }

    assert(planet_is_family(entity) && "rebuild_incoming_routes_route_visit: the planet tag holds planets only");
    from_planet = (planet_t *)entity;
    outgoing = planet_rebuild_trades(from_planet);

    for (int route_index = 0; route_index < trade_route_list_count(outgoing); route_index++) {
        const trade_route_t *route = trade_route_list_at(outgoing, route_index);
        const entity_id_list_t *slipways = &route->slipways;
        const entity_id_list_t *planets = &route->planets;
        entity_t *to_val;
        int index;

        for (index = 0; index < slipways->count; index++) {
            entity_t *w_val = entity_id_list_at(slipways, index);
            slipway_t *w;
            const entity_t *planet_val;
            const planet_t *planet;

            if (index + 1 >= planets->count) {
                continue;
            }
            planet_val = entity_id_list_at(planets, index + 1);
            assert(planet_is_family(planet_val) && "rebuild_incoming_routes_route_visit: a trade's planets list holds planets only");
            planet = (const planet_t *)planet_val;
            if (!(planet->has & PLANET_HAS_ID)) {
                continue;
            }
            assert(w_val->class == slipway_class() && "rebuild_incoming_routes_route_visit: a trade's slipways list holds slipways only");
            w = (slipway_t *)w_val;
            slipway_served_planets_add(&w->served_planets, planet->planet_id);
        }

        if (trade_last_planet(route, &to_val)) {
            planet_t *dest;

            assert(planet_is_family(to_val) && "rebuild_incoming_routes_route_visit: a trade leads to a planet");
            dest = (planet_t *)to_val;
            if (dest->has & PLANET_HAS_INCOMING_ROUTES) {
                trade_route_list_add(&dest->incoming_routes, route);
            }
        }
    }
}

void economy_tiers_rebuild_incoming_routes(void) {
    for_each_tagged(TAG_PLANET, rebuild_incoming_routes_reset_visit, NULL);
    for_each_tagged(TAG_PLANET, rebuild_incoming_routes_route_visit, NULL);
}

static void update_tiers_visit(entity_t *entity, void *context) {
    int *changed = (int *)context;
    planet_t *planet;
    p8_fixed_t tier;
    int has_development_tier;
    p8_fixed_t development_tier = p8_fixed_from_int(0);

    if (entity == NULL) {
        return;
    }
    tier = economy_tiers_compute_tier(entity);
    assert(planet_is_family(entity) && "update_tiers_visit: the planet tag holds planets only");
    planet = (planet_t *)entity;
    has_development_tier = planet->has_development_tier;
    if (has_development_tier) {
        development_tier = planet->development_tier;
    }

    if (!has_development_tier || tier > development_tier) {
        if (has_development_tier && development_tier == p8_fixed_from_int(1)) {
            research_project_t *hn = research_project_get(planet->project);
            if (hn != NULL && hn->has_byproduct) {
                if (planet->has & PLANET_HAS_NEEDS) {
                    need_t *entry = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = hn->byproduct});
                    pointer_list_add(&planet->needs, entry);
                }
            }
        }
        planet->development_tier = tier;
        planet->has_development_tier = 1;
        development_tier = tier;
        *changed = 1;
    }

    economy_tiers_compute_production_amount(planet, &planet_production(planet)->amount);
}

int economy_tiers_compute_production_amount(const planet_t *planet, p8_fixed_t *out) {
    research_project_t *project;
    const production_tables_entry_t *table;
    int idx;
    int clamped_idx;
    p8_fixed_t base = p8_fixed_from_int(0);
    p8_fixed_t bonus = p8_fixed_from_int(0);

    assert(planet != NULL && out != NULL);
    if (!planet->has_development_tier || planet->development_tier == p8_fixed_from_int(0)) {
        return 0;
    }
    project = research_project_get(planet->project);
    table = project != NULL ? research_project_production(project) : NULL;
    if (table == NULL) {
        return 0;
    }
    idx = p8_fixed_flr_int(planet->development_tier);
    clamped_idx = (idx < table->count) ? idx : table->count;
    if (clamped_idx >= 1) {
        base = p8_fixed_from_int(table->values[clamped_idx - 1]);
    }
    if (planet->has_production_bonus) {
        bonus = planet->production_bonus;
    }
    *out = p8_fixed_add(base, bonus);
    return 1;
}

int economy_tiers_update_tiers(void) {
    int jg = 0;
    for_each_tagged(TAG_PLANET, update_tiers_visit, &jg);
    return jg;
}

p8_fixed_t economy_tiers_compute_tier(const entity_t *entity) {
    const planet_t *planet;
    const pointer_list_t *needs;
    const trade_route_list_t *incoming_routes;
    p8_fixed_t tier = p8_fixed_from_int(1);
    int index;

    if (entity == NULL) {
        return p8_fixed_from_int(0);
    }
    assert(planet_is_family(entity) && "economy_tiers_compute_tier: the entity is a planet");
    planet = (const planet_t *)entity;
    if (planet->header.state != ENTITY_STATE_SETTLED) {
        return p8_fixed_from_int(0);
    }

    needs = (planet->has & PLANET_HAS_NEEDS) ? &planet->needs : NULL;
    incoming_routes = (planet->has & PLANET_HAS_INCOMING_ROUTES) ? &planet->incoming_routes : NULL;
    if (needs != NULL) {
        for (index = 0; index < needs->count; index++) {
            void *i_val = needs->items[index];
            need_t *want;
            resource_t resource_val;
            int gc = 0;
            int j;

            if (i_val == NULL) {
                continue;
            }
            want = i_val;
            resource_val = need_resource(want);
            if (incoming_routes != NULL) {
                for (j = 0; j < trade_route_list_count(incoming_routes); j++) {
                    const trade_route_t *im = trade_route_list_at(incoming_routes, j);
                    if (im->has_resource && resource_equal(trade_resource(im), resource_val)) {
                        gc++;
                    }
                }
            }
            want->has_satisfied = 1;
            want->satisfied = p8_fixed_from_int(gc);
            tier = p8_fixed_mul(tier, p8_fixed_from_int(gc));
        }
    }

    if (tier == p8_fixed_from_int(0)) {
        return p8_fixed_from_int(1);
    }

    if (entity->class->lineage & LINEAGE(LINEAGE_TRADING_HUB)) {
        int count = 0;
        if (needs != NULL) {
            for (index = 0; index < needs->count; index++) {
                void *i_val = needs->items[index];
                const need_t *want;
                if (i_val == NULL) {
                    continue;
                }
                want = i_val;
                if (want->has_satisfied && want->satisfied > p8_fixed_from_int(0)) {
                    count++;
                }
            }
        }
        return fixed_min(p8_fixed_from_int(count), p8_fixed_from_int(3));
    }

    if (!planet->archetype->is_planet) {
        if (needs != NULL && needs->count > 0) {
            void *last_val = needs->items[needs->count - 1];
            if (last_val != NULL) {
                const need_t *last = last_val;
                if (last->has_satisfied) {
                    return fixed_min(p8_fixed_add(last->satisfied, p8_fixed_from_int(1)), p8_fixed_from_int(4));
                }
            }
        }
        return p8_fixed_from_int(2);
    }

    {
        const trade_route_list_t *by = (planet->has & PLANET_HAS_OUTGOING_ROUTES) ? &planet->outgoing_routes : NULL;
        int by_count = (by != NULL) ? trade_route_list_count(by) : 0;
        int incoming_routes_count = (incoming_routes != NULL) ? trade_route_list_count(incoming_routes) : 0;
        p8_fixed_t mid_val =
            p8_fixed_mid(p8_fixed_from_int(by_count), p8_fixed_from_int(incoming_routes_count), p8_fixed_from_int(1));
        return fixed_min(p8_fixed_from_int(4), p8_fixed_add(p8_fixed_from_int(1), mid_val));
    }
}
