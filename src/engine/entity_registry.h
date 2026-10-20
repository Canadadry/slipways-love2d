#ifndef ENGINE_ENTITY_REGISTRY_H
#define ENGINE_ENTITY_REGISTRY_H

#include "engine/entity_list.h"
#include "engine/entity_class.h"

typedef enum {
    ENTITY_MESSAGE_DRAW,
    ENTITY_MESSAGE_DRAW_OVERLAY,
    ENTITY_MESSAGE_HIT_TEST,
    ENTITY_MESSAGE_NEW_YEAR,
    ENTITY_MESSAGE_REFRESH,
    ENTITY_MESSAGE_COUNT
} entity_message_t;

typedef struct {
    int live;
    int by_message[ENTITY_MESSAGE_COUNT];
    int by_tag[TAG_COUNT];
} entity_registry_reserve_t;

void entity_registry_init(const entity_registry_reserve_t *reserve);

void entity_registry_shutdown(void);

void entity_registry_abandon_live_entities(void);

void entity_registry_forget_registries(void);

const entity_list_t *live_entities(void);

const entity_list_t *entities_by_message(entity_message_t message);

const entity_list_t *entities_by_tag(tag_t tag);

/* 0 when no entity of the tag was ever registered. */
int entity_registry_count_tagged(tag_t tag);

entity_t *entity_register(entity_t *e);
void entity_unregister(entity_t *e);

/* Changes on every register, unregister and registry reset (an unsigned counter that wraps). */
unsigned entity_registry_generation(void);

#define ENTITY_REGISTRY_LAYERS 15

typedef struct {
    entity_t *const *items;
    int count;
} entity_layer_view_t;

/* Also sets ENTITY_HAS_LAYER. Re-sorts the draw order only for a constructed entity whose floored
 * layer changed, else costs one compare. An entity registered bare (entity_register alone) must
 * not change its layer afterwards. */
void entity_registry_set_layer(entity_t *e, p8_fixed_t layer);

/* The draw or draw_overlay members whose floored layer is layer (1..15), in registration order;
 * the view is invalidated by the next register, unregister or layer change. */
entity_layer_view_t entity_registry_layer(entity_message_t message, int layer);

void draw_layers(entity_message_t message);

int entity_registry_hit_test_always(entity_t *self, mouse_pos_t mouse);

entity_t *main_camera(void);

const entity_class_t *main_camera_placeholder_class(void);

void entity_registry_set_main_camera(entity_t *instance);

#endif
