#include "engine/entity_geom.h"

#include <assert.h>

#include <stddef.h>

#include "engine/entity_registry.h"
#include "engine/geom.h"
#include "p8/vram.h"

static vec_t zero_vec(void) {
    return vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

static vec_t pos_of(const entity_t *e) {
    return e != NULL ? entity_pos(e) : zero_vec();
}

static vec_t map_pos_of(const entity_t *e) {
    return e != NULL ? entity_map_pos(e) : zero_vec();
}

int entity_geom_contains_cursor(const entity_t *e, mouse_pos_t mouse) {
    return entity_geom_contains_cursor_inline(e, mouse);
}

int entity_geom_is_on_screen(const entity_t *e) {
    vec_t e_pos = pos_of(e);
    vec_t main_camera_pos = pos_of(main_camera());
    p8_fixed_t half_w = p8_fixed_from_int(P8_SCREEN_WIDTH / 2);
    p8_fixed_t half_h = p8_fixed_from_int(P8_SCREEN_HEIGHT / 2);
    p8_fixed_t margin = p8_fixed_from_int(12);

    return p8_fixed_abs(p8_fixed_sub(p8_fixed_sub(e_pos.x, main_camera_pos.x), half_w)) <= p8_fixed_add(half_w, margin) &&
           p8_fixed_abs(p8_fixed_sub(p8_fixed_sub(e_pos.y, main_camera_pos.y), half_h)) <= p8_fixed_add(half_h, margin);
}

vec_t entity_geom_shake(vec_t p, entity_t *e) {
    p8_fixed_t decremented;
    p8_fixed_t clamped;
    p8_fixed_t t = p8_fixed_from_int(0);

    if (e != NULL) {
        assert(entity_has(e, ENTITY_HAS_SHAKE) && "entity_geom_shake: the entity is a planet (planet_draw) or the camera (camera_update)");
    }
    decremented = p8_fixed_sub(e != NULL ? e->shake : p8_fixed_from_int(0), P8_FIXED_LIT(0x00001999, 0.1));
    clamped = (decremented > 0) ? decremented : 0;
    if (e != NULL) {
        t = e->state_time;
        e->shake = clamped;
    }
    return vec_add(p, vec_from_polar(clamped, p8_fixed_mul(t, P8_FIXED_LIT(0x00003333, 0.2))));
}

static p8_fixed_t nearest_step(vec_t p, entity_t *o, p8_fixed_t best, entity_t **out_nearest) {
    vec_t o_p = map_pos_of(o);
    p8_fixed_t d = vec_length_squared(vec_sub(p, o_p));
    int same_position = (p.x == o_p.x && p.y == o_p.y);

    if (!same_position && d < best) {
        *out_nearest = o;
        return d;
    }
    return best;
}

p8_fixed_t entity_geom_nearest(vec_t p, const entity_list_t *j_, entity_t **out_nearest) {
    p8_fixed_t mk = p8_fixed_from_int(32767);
    entity_list_iter_t it;
    entity_t *o;

    *out_nearest = NULL;
    if (j_ == NULL) {
        return mk;
    }
    it = entity_list_all(j_);
    while (entity_list_iter_next(&it, &o)) {
        mk = nearest_step(p, o, mk, out_nearest);
    }
    return mk;
}

int entity_geom_any_within(vec_t p, const entity_list_t *list, p8_fixed_t threshold) {
    entity_list_iter_t it;
    entity_t *o;

    if (list == NULL) {
        return 0;
    }
    it = entity_list_all(list);
    while (entity_list_iter_next(&it, &o)) {
        vec_t o_p = map_pos_of(o);
        int same_position = (p.x == o_p.x && p.y == o_p.y);

        if (!same_position && vec_length_squared(vec_sub(p, o_p)) <= threshold) {
            return 1;
        }
    }
    return 0;
}

p8_fixed_t entity_geom_nearest_except(vec_t p, const entity_list_t *j_, const entity_t *excluded, entity_t **out_nearest) {
    p8_fixed_t best = p8_fixed_from_int(32767);
    entity_list_iter_t it;
    entity_t *o;

    *out_nearest = NULL;
    if (j_ == NULL) return best;
    it = entity_list_all(j_);
    while (entity_list_iter_next(&it, &o)) {
        p8_fixed_t d = vec_length_squared(vec_sub(p, map_pos_of(o)));
        if (o != excluded && d < best) {
            *out_nearest = o;
            best = d;
        }
    }
    return best;
}

p8_fixed_t entity_geom_nearest_in_ids(vec_t p, const entity_id_list_t *ids, entity_t **out_nearest) {
    p8_fixed_t best = p8_fixed_from_int(32767);
    entity_t *o;

    *out_nearest = NULL;
    if (ids == NULL) return best;
    entity_id_list_iter_t it = entity_id_list_all(ids);
    while (entity_id_list_iter_next(&it, &o)) best = nearest_step(p, o, best, out_nearest);
    return best;
}
