#ifndef TABLE_REFERENCE_REFERENCE_CLASS_H
#define TABLE_REFERENCE_REFERENCE_CLASS_H

#include <stdio.h>
#include <stdlib.h>

#include "engine/entity_class.h"
#include "table_reference.h"

/* static inline: a unit that never calls it would otherwise fail
 * -Wunused-function. */
static inline void reference_class(const char *module, const char *name, const entity_class_t *cls) {
    table_reference_function(module, cls->draw != NULL, "%s.draw", name);
    table_reference_function(module, cls->draw_overlay != NULL, "%s.draw_overlay", name);
    table_reference_function(module, cls->hit_test != NULL, "%s.hit_test", name);
    table_reference_function(module, cls->new_year != NULL, "%s.new_year", name);
    table_reference_function(module, cls->refresh != NULL, "%s.refresh", name);
    table_reference_function(module, cls->has_refresh != NULL, "%s.has_refresh", name);
    table_reference_function(module, cls->left_press != NULL, "%s.left_press", name);
    table_reference_function(module, cls->accept_drop != NULL, "%s.accept_drop", name);
    table_reference_function(module, cls->drag != NULL, "%s.drag", name);
    table_reference_function(module, cls->drop != NULL, "%s.drop", name);
    table_reference_function(module, cls->attach_slipway != NULL, "%s.attach_slipway", name);
    table_reference_function(module, cls->offer_trade != NULL, "%s.offer_trade", name);
    table_reference_function(module, cls->draw_at != NULL, "%s.draw_at", name);
    table_reference_function(module, cls->spawn_particle != NULL, "%s.spawn_particle", name);
    for (int i = 0; i < STATE_COUNT; i++) {
        table_reference_function(module, cls->states[i] != NULL, "%s.states[%d]", name, i);
    }
    table_reference_int(module, cls->tags, "%s.tags", name);
    table_reference_int(module, cls->lineage, "%s.lineage", name);
    table_reference_function(module, cls->apply_camera != NULL, "%s.apply_camera", name);
    table_reference_function(module, cls->release != NULL, "%s.release", name);
    table_reference_int(module, cls->skip_draw_off_screen, "%s.skip_draw_off_screen", name);
}

/* The header only: the Lua static is one table, the cast to entity_t * the identity.
 * Stops the build when e->class is neither NULL nor cls. */
static inline void reference_header(const char *module, const char *name, const entity_t *e, const char *class_name,
                                    const entity_class_t *cls) {
    if (e->class == cls) {
        table_reference_pointer(module, class_name, "%s.class", name);
    } else if (e->class != NULL) {
        fprintf(stderr, "table_reference: %s %s.class points at no listed table\n", module, name);
        exit(1);
    }
    table_reference_int(module, e->state, "%s.state", name);
    table_reference_int(module, e->state_time, "%s.state_time", name);
    table_reference_int(module, e->has, "%s.has", name);
    table_reference_int(module, e->layer, "%s.layer", name);
    table_reference_int(module, e->pos.x, "%s.pos.x", name);
    table_reference_int(module, e->pos.y, "%s.pos.y", name);
    table_reference_int(module, e->map_pos.x, "%s.map_pos.x", name);
    table_reference_int(module, e->map_pos.y, "%s.map_pos.y", name);
    table_reference_int(module, e->hit_box.left, "%s.hit_box.left", name);
    table_reference_int(module, e->hit_box.top, "%s.hit_box.top", name);
    table_reference_int(module, e->hit_box.right, "%s.hit_box.right", name);
    table_reference_int(module, e->hit_box.bottom, "%s.hit_box.bottom", name);
    table_reference_int(module, e->infraspace, "%s.infraspace", name);
    table_reference_int(module, e->shake, "%s.shake", name);
    table_reference_int(module, e->fold_anchor, "%s.fold_anchor", name);
    table_reference_int(module, e->is_dead, "%s.is_dead", name);
    table_reference_int(module, e->is_hovered, "%s.is_hovered", name);
    table_reference_int(module, e->id.index, "%s.id.index", name);
    table_reference_int(module, e->id.generation, "%s.id.generation", name);
}

#endif
