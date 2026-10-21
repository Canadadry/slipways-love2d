#ifndef TABLE_REFERENCE_REFERENCE_CLASS_H
#define TABLE_REFERENCE_REFERENCE_CLASS_H

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

#endif
