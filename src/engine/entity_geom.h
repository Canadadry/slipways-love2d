#ifndef ENGINE_ENTITY_GEOM_H
#define ENGINE_ENTITY_GEOM_H

#include "engine/entity_id_list.h"
#include "engine/entity_list.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

/* The body of entity_geom_contains_cursor, inline so the hover loop makes no call. */
static inline int entity_geom_contains_cursor_inline(const entity_t *e, mouse_pos_t mouse) {
    vec_t chosen;
    vec_t point;

    if (e == NULL || !entity_has(e, ENTITY_HAS_HIT_BOX)) {
        return 0;
    }
    chosen = (entity_has(e, ENTITY_HAS_LAYER) && e->layer >= P8_FIXED_INT(10)) ? mouse_pos_screen(mouse) : mouse_pos_world(mouse);
    point = chosen;
    if (entity_has(e, ENTITY_HAS_POS)) {
        point.x = (p8_fixed_t)((uint32_t)chosen.x - (uint32_t)e->pos.x);
        point.y = (p8_fixed_t)((uint32_t)chosen.y - (uint32_t)e->pos.y);
    }
    return box_contains(e->hit_box, point);
}

int entity_geom_contains_cursor(const entity_t *e, mouse_pos_t mouse);

int entity_geom_is_on_screen(const entity_t *e);

vec_t entity_geom_shake(vec_t p, entity_t *e);

p8_fixed_t entity_geom_nearest(vec_t p, const entity_list_t *j_, entity_t **out_nearest);
/* Answers entity_geom_nearest(p, list, &n) <= threshold for any threshold under its 32767 bound; 0 for a NULL list. */
int entity_geom_any_within(vec_t p, const entity_list_t *list, p8_fixed_t threshold);
/* Skips only `excluded`, by identity: another entity on the very point counts. */
p8_fixed_t entity_geom_nearest_except(vec_t p, const entity_list_t *j_, const entity_t *excluded, entity_t **out_nearest);
p8_fixed_t entity_geom_nearest_in_ids(vec_t p, const entity_id_list_t *ids, entity_t **out_nearest);

#endif
