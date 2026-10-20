#include "game/trade.h"

#include <assert.h>
#include <stddef.h>

#include "engine/entity_id.h"

static int g_trade_up = 0;

resource_t trade_resource(const trade_route_t *self) {
    if (self == NULL || !self->has_resource) {
        return (resource_t){.kind = RESOURCE_NIL};
    }
    assert(!(self->resource_is_bool && self->resource != 0) && "no route of src/ holds true: resource_t has no kind for it");
    return self->resource_is_bool ? (resource_t){.kind = RESOURCE_FALSE}
                            : (resource_t){.kind = RESOURCE_FIXED, .number = self->resource};
}

int resource_equal(resource_t a, resource_t b) {
    return a.kind == b.kind && (a.kind != RESOURCE_FIXED || a.number == b.number);
}

void trade_init(void) {
    trade_shutdown();
    g_trade_up = 1;
}

void trade_shutdown(void) {
    g_trade_up = 0;
}

int trade_ready(void) {
    return g_trade_up;
}

trade_route_t trade_new(const trade_keys_t *keys) {
    trade_route_t self = {0};

    if (keys == NULL) {
        return self;
    }
    assert((!keys->resource_is_bool || keys->resource == 0 || keys->resource == 1) &&
           "trade_new: a boolean key holds 0 or 1; trade_resource would read any other as a fixed");
    assert((keys->has_resource || (!keys->resource_is_bool && keys->resource == 0)) &&
           "trade_new: an absent key carries neither a kind nor a value; the member reads 0 when absent");
    self.planets = keys->planets;
    self.slipways = keys->slipways;
    self.resource = keys->resource;
    self.has_resource = keys->has_resource;
    self.resource_is_bool = keys->resource_is_bool;
    return self;
}

/* The ids of list, then extra unless it is NULL, in a new list of zone; an empty list with no zone when there is no id. */
static entity_id_list_t id_list_extended(const allocator_t *zone, const entity_id_list_t *list, const entity_t *extra) {
    const int count = list->count + (extra != NULL ? 1 : 0);
    entity_id_list_t out = {0};

    if (count == 0) {
        return out;
    }
    entity_id_list_init_reserve(&out, zone, count);
    for (int i = 0; i < list->count; i++) {
        entity_id_list_add(&out, list->items[i]);
    }
    if (extra != NULL) {
        entity_id_t id = entity_id_of(extra);
        assert(!entity_id_is_none(id) && "trade_extend: a route holds constructed planets and slipways only");
        entity_id_list_add(&out, id);
    }
    return out;
}

trade_route_t trade_copy(const allocator_t *zone, const trade_route_t *self) {
    trade_route_t copy;

    assert(self != NULL && "trade_copy: a route value is always there to copy");
    copy = *self;
    copy.planets = id_list_extended(zone, &self->planets, NULL);
    copy.slipways = id_list_extended(zone, &self->slipways, NULL);
    return copy;
}

int trade_last_planet(const trade_route_t *self, entity_t **out) {
    if (self == NULL || self->planets.count == 0) {
        return 0;
    }
    *out = entity_id_list_at(&self->planets, self->planets.count - 1);
    return 1;
}

trade_route_t trade_extend(const allocator_t *zone, const trade_route_t *self,
                           entity_t *node, entity_t *slipway) {
    trade_route_t extended = {0};

    assert(self != NULL && "trade_extend: a route value is always there to extend");
    extended.has_resource = 1;
    extended.resource_is_bool = self->has_resource ? self->resource_is_bool : 1;
    extended.resource = self->has_resource ? self->resource : 0;
    extended.planets = id_list_extended(zone, &self->planets, node);
    extended.slipways = id_list_extended(zone, &self->slipways, slipway);
    return extended;
}

int trade_equal(const trade_route_t *l, const trade_route_t *r) {
    entity_t *l_last, *r_last;

    if (l == NULL || r == NULL || l->planets.count == 0 || r->planets.count == 0) {
        return 0;
    }
    if (entity_id_list_at(&l->planets, 0) != entity_id_list_at(&r->planets, 0)) {
        return 0;
    }
    if (!trade_last_planet(l, &l_last) || !trade_last_planet(r, &r_last)) {
        return 0;
    }
    return l_last == r_last;
}

void trade_route_list_init(trade_route_list_t *list, const allocator_t *zone) {
    value_list_init(&list->values, zone, sizeof(trade_route_t));
}

void trade_route_list_free(trade_route_list_t *list) {
    value_list_free(&list->values);
}

void trade_route_list_clear(trade_route_list_t *list) {
    value_list_clear(&list->values);
}

void trade_route_list_add(trade_route_list_t *list, const trade_route_t *route) {
    /* route may point into this list: copy it before the add grows the list. */
    const trade_route_t copy = *route;
    value_list_add(&list->values, &copy);
}

int trade_route_list_count(const trade_route_list_t *list) {
    return value_list_count(&list->values);
}

const trade_route_t *trade_route_list_at(const trade_route_list_t *list, int index) {
    return (const trade_route_t *)value_list_at(&list->values, index);
}
