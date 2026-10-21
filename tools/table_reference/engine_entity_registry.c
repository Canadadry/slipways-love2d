#include "engine/entity_registry.c"
#include <stdlib.h>

#include "table_reference.h"

#define MODULE "engine.entity_registry"

static void render_class(const char *name, const entity_class_t *cls) {
    table_reference_function(MODULE, cls->draw != NULL, "%s.draw", name);
    table_reference_function(MODULE, cls->draw_overlay != NULL, "%s.draw_overlay", name);
    table_reference_function(MODULE, cls->hit_test != NULL, "%s.hit_test", name);
    table_reference_function(MODULE, cls->new_year != NULL, "%s.new_year", name);
    table_reference_function(MODULE, cls->refresh != NULL, "%s.refresh", name);
    table_reference_function(MODULE, cls->has_refresh != NULL, "%s.has_refresh", name);
    table_reference_function(MODULE, cls->left_press != NULL, "%s.left_press", name);
    table_reference_function(MODULE, cls->accept_drop != NULL, "%s.accept_drop", name);
    table_reference_function(MODULE, cls->drag != NULL, "%s.drag", name);
    table_reference_function(MODULE, cls->drop != NULL, "%s.drop", name);
    table_reference_function(MODULE, cls->attach_slipway != NULL, "%s.attach_slipway", name);
    table_reference_function(MODULE, cls->offer_trade != NULL, "%s.offer_trade", name);
    table_reference_function(MODULE, cls->draw_at != NULL, "%s.draw_at", name);
    table_reference_function(MODULE, cls->spawn_particle != NULL, "%s.spawn_particle", name);
    for (int i = 0; i < STATE_COUNT; i++) {
        table_reference_function(MODULE, cls->states[i] != NULL, "%s.states[%d]", name, i);
    }
    table_reference_int(MODULE, cls->tags, "%s.tags", name);
    table_reference_int(MODULE, cls->lineage, "%s.lineage", name);
    table_reference_function(MODULE, cls->apply_camera != NULL, "%s.apply_camera", name);
    table_reference_function(MODULE, cls->release != NULL, "%s.release", name);
    table_reference_int(MODULE, cls->skip_draw_off_screen, "%s.skip_draw_off_screen", name);
}

/* The header only: the Lua placeholder is one table, the cast to entity_t * the identity. */
static void render_header(const char *name, const entity_t *e) {
    if (e->class == &main_camera_placeholder_class_def) {
        table_reference_pointer(MODULE, "main_camera_placeholder_class_def", "%s.class", name);
    } else if (e->class != NULL) {
        fprintf(stderr, "table_reference: %s %s.class points at no listed table\n", MODULE, name);
        exit(1);
    }
    table_reference_int(MODULE, e->state, "%s.state", name);
    table_reference_int(MODULE, e->state_time, "%s.state_time", name);
    table_reference_int(MODULE, e->has, "%s.has", name);
    table_reference_int(MODULE, e->layer, "%s.layer", name);
    table_reference_int(MODULE, e->pos.x, "%s.pos.x", name);
    table_reference_int(MODULE, e->pos.y, "%s.pos.y", name);
    table_reference_int(MODULE, e->map_pos.x, "%s.map_pos.x", name);
    table_reference_int(MODULE, e->map_pos.y, "%s.map_pos.y", name);
    table_reference_int(MODULE, e->hit_box.left, "%s.hit_box.left", name);
    table_reference_int(MODULE, e->hit_box.top, "%s.hit_box.top", name);
    table_reference_int(MODULE, e->hit_box.right, "%s.hit_box.right", name);
    table_reference_int(MODULE, e->hit_box.bottom, "%s.hit_box.bottom", name);
    table_reference_int(MODULE, e->infraspace, "%s.infraspace", name);
    table_reference_int(MODULE, e->shake, "%s.shake", name);
    table_reference_int(MODULE, e->fold_anchor, "%s.fold_anchor", name);
    table_reference_int(MODULE, e->is_dead, "%s.is_dead", name);
    table_reference_int(MODULE, e->is_hovered, "%s.is_hovered", name);
    table_reference_int(MODULE, e->id.index, "%s.id.index", name);
    table_reference_int(MODULE, e->id.generation, "%s.id.generation", name);
}

void table_reference_engine_entity_registry(void) {
    table_reference_int(MODULE, ENTITY_MESSAGE_DRAW, "ENTITY_MESSAGE_DRAW");
    table_reference_int(MODULE, ENTITY_MESSAGE_DRAW_OVERLAY, "ENTITY_MESSAGE_DRAW_OVERLAY");
    table_reference_int(MODULE, ENTITY_MESSAGE_HIT_TEST, "ENTITY_MESSAGE_HIT_TEST");
    table_reference_int(MODULE, ENTITY_MESSAGE_NEW_YEAR, "ENTITY_MESSAGE_NEW_YEAR");
    table_reference_int(MODULE, ENTITY_MESSAGE_REFRESH, "ENTITY_MESSAGE_REFRESH");
    table_reference_int(MODULE, ENTITY_MESSAGE_COUNT, "ENTITY_MESSAGE_COUNT");
    table_reference_int(MODULE, ENTITY_REGISTRY_LAYERS, "ENTITY_REGISTRY_LAYERS");
    render_class("main_camera_placeholder_class_def", &main_camera_placeholder_class_def);
    render_header("g_main_camera_placeholder", &g_main_camera_placeholder.header);
}
