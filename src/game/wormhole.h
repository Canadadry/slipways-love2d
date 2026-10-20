#ifndef GAME_WORMHOLE_H
#define GAME_WORMHOLE_H

#include "engine/entity_id_list.h"
#include "p8/fixed.h"
#include "game/game_data.h"
#include "game/targeting.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

typedef struct {
    entity_t header;
    const char *status_text;
} wormhole_too_far_target_t;

typedef struct {
    entity_t header;
    const wormhole_archetype_t *archetype;
    uint16_t has;
    p8_fixed_t radius;
    entity_id_list_t anchors;
    entity_id_t anchor;
    targeting_t targeting;
} wormhole_t;

#define WORMHOLE_HAS_TOO_FAR_TARGET (1u << 0)

void wormhole_init(void);

void wormhole_shutdown(void);

int wormhole_ready(void);
const entity_class_t *wormhole_class(void);

wormhole_t *wormhole_new(void);

/* The entities in the wormhole tag list, a dying one included until the exit queue flushes; 0 when the list does not exist. */
int wormhole_live_count(void);

wormhole_too_far_target_t *wormhole_too_far_target_wrapper(void);
const entity_class_t *wormhole_too_far_target_class(void);

void wormhole_construct(wormhole_t *self);

void wormhole_drag(wormhole_t *self, mouse_pos_t mouse);

void wormhole_update_placing(wormhole_t *self, p8_fixed_t t);

void wormhole_drop(wormhole_t *self);

void wormhole_update_unfolding(wormhole_t *self, p8_fixed_t t);

p8_fixed_t wormhole_reveal_radius(wormhole_t *self, p8_fixed_t t);

void wormhole_draw_method(entity_t *self, vec_t screen_pos);

#endif
