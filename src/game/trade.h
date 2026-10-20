#ifndef GAME_TRADE_H
#define GAME_TRADE_H

#include "engine/allocator.h"
#include "engine/entity_class.h"
#include "engine/entity_id_list.h"
#include "engine/value_list.h"
#include "p8/fixed.h"

/* The id lists are immutable once built: value copies of a route share their items. */
typedef struct {
    entity_id_list_t planets;
    entity_id_list_t slipways;
    p8_fixed_t resource;
    int has_resource;
    int resource_is_bool;
} trade_route_t;

void trade_init(void);
void trade_shutdown(void);

int trade_ready(void);

typedef struct {
    enum { RESOURCE_NIL, RESOURCE_FALSE, RESOURCE_FIXED } kind;
    p8_fixed_t number;
} resource_t;

int resource_equal(resource_t a, resource_t b);

typedef struct {
    entity_id_list_t planets;
    entity_id_list_t slipways;
    p8_fixed_t resource;
    int has_resource;
    int resource_is_bool;
} trade_keys_t;

/* NULL keys give a route with empty lists and no resource; the route shares the keys' list items. */
trade_route_t trade_new(const trade_keys_t *keys);

/* The copy's id lists are new lists in zone. */
trade_route_t trade_copy(const allocator_t *zone, const trade_route_t *self);

resource_t trade_resource(const trade_route_t *self);

int trade_last_planet(const trade_route_t *self, entity_t **out);

/* The new route's id lists are new lists in zone; a NULL node or slipway adds nothing. */
trade_route_t trade_extend(const allocator_t *zone, const trade_route_t *self,
                           entity_t *node, entity_t *slipway);

int trade_equal(const trade_route_t *l, const trade_route_t *r);

typedef struct {
    value_list_t values;
} trade_route_list_t;

void trade_route_list_init(trade_route_list_t *list, const allocator_t *zone);
void trade_route_list_free(trade_route_list_t *list);
void trade_route_list_clear(trade_route_list_t *list);
/* Stores a copy of the route; its id lists are shared, not copied. */
void trade_route_list_add(trade_route_list_t *list, const trade_route_t *route);
int trade_route_list_count(const trade_route_list_t *list);
/* Valid until the next add or free on this list: never store it. */
const trade_route_t *trade_route_list_at(const trade_route_list_t *list, int index);

#endif
