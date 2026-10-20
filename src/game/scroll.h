#ifndef GAME_SCROLL_H
#define GAME_SCROLL_H

#include "p8/fixed.h"
#include "game/game_data.h"
#include "p8/gfx.h"
#include "engine/entity.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

typedef struct {
    entity_t header;
    const camera_archetype_t *archetype;
} camera_t;

void camera_init(void);

void camera_shutdown(void);

int camera_ready(void);
const entity_class_t *camera_class(void);

camera_t *camera_new(void);

void camera_update(camera_t *self);

/* Moves the map position and the position by whole pixels, within the camera bound. */
void camera_pan(camera_t *self, int dx, int dy);

void camera_apply(p8_graphics_t *gfx, camera_t *self, p8_fixed_t parallax);

camera_t *main_camera_typed(void);

/* The camera's map position is its position before shake; its position is the shaken one. */
static inline vec_t camera_pos_before_shake(const camera_t *self) {
    return entity_map_pos(&self->header);
}

#endif
