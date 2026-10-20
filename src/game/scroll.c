#include "game/scroll.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/vec.h"
#include "engine/game_zone.h"
#include "game/galaxy.h"

static int g_camera_up = 0;

static void camera_update_method(entity_t *self, p8_fixed_t t) {
    (void)t;
    camera_update((camera_t *)self);
}

static void camera_apply_method(entity_t *self, p8_fixed_t parallax) {
    camera_apply(dispatch_context_gfx(), (camera_t *)self, parallax);
}

static const entity_class_t camera_class_def = {
    .states = {[ENTITY_STATE_NORMAL] = camera_update_method},
    .apply_camera = camera_apply_method,
};

static camera_t *camera_create(void) {
    camera_t *self = (camera_t *)allocator_alloc(game_zone(), sizeof *self);

    entity_init_instance(&self->header, &camera_class_def, camera_archetype.state, camera_archetype.state_time);
    self->archetype = &camera_archetype;
    self->header.has |= ENTITY_HAS_SHAKE;
    self->header.shake = self->archetype->shake;
    entity_set_map_pos(&self->header, self->archetype->pos_before_shake);
    entity_set_pos(&self->header, self->archetype->pos);
    entity_construct(&self->header);
    return self;
}

void camera_init(void) {
    camera_shutdown();
    assert(entity_ready() && "camera_init: entity_init() runs first");
    g_camera_up = 1;
}

void camera_shutdown(void) {
    g_camera_up = 0;
}

int camera_ready(void) {
    return g_camera_up;
}

const entity_class_t *camera_class(void) {
    return &camera_class_def;
}

camera_t *camera_new(void) {
    return camera_create();
}

/* The pad has no mouse to reach the screen's edges: the bound keeps the screen centre over the galaxy's planet area. */
static p8_fixed_t camera_pad_bound(p8_fixed_t p, int frame_size) {
    p8_fixed_t scale = p8_fixed_from_int(GALAXY_MAP_SCALE);
    p8_fixed_t half = p8_fixed_from_int(frame_size / 2);
    p8_fixed_t near = p8_fixed_sub(p8_fixed_mul(GALAXY_MAP_MARGIN, scale), half);
    p8_fixed_t far = p8_fixed_sub(p8_fixed_mul(p8_fixed_add(GALAXY_MAP_MARGIN, GALAXY_MAP_SPAN), scale), half);

    return p8_fixed_mid(near, far, p);
}

static vec_t camera_bounded(vec_t p) {
    return vec_make(camera_pad_bound(p.x, P8_SCREEN_WIDTH), camera_pad_bound(p.y, P8_SCREEN_HEIGHT));
}

void camera_update(camera_t *self) {
    vec_t p;

    if (self == NULL) {
        return;
    }

    p = camera_bounded(camera_pos_before_shake(self));
    entity_set_map_pos(&self->header, p);
    entity_set_pos(&self->header, camera_bounded(entity_geom_shake(p, (entity_t *)self)));
}

void camera_pan(camera_t *self, int dx, int dy) {
    vec_t step = vec_make(p8_fixed_from_int(dx), p8_fixed_from_int(dy));

    assert(self != NULL && "camera_pan: the caller checks for the placeholder camera");
    entity_set_map_pos(&self->header, camera_bounded(vec_add(camera_pos_before_shake(self), step)));
    entity_set_pos(&self->header, camera_bounded(vec_add(entity_pos(&self->header), step)));
}

void camera_apply(p8_graphics_t *gfx, camera_t *self, p8_fixed_t parallax) {
    vec_t cp;

    if (self == NULL || gfx == NULL) {
        return;
    }

    cp = vec_mul(entity_pos(&self->header), parallax);
    p8_camera(gfx, p8_fixed_flr_int(cp.x), p8_fixed_flr_int(cp.y));
}

camera_t *main_camera_typed(void) {
    entity_t *mo = main_camera();

    if (mo == NULL || mo->class == main_camera_placeholder_class()) {
        return NULL;
    }
    assert(mo->class == camera_class() && "main_camera_typed: only the placeholder or a camera is ever stored");
    return (camera_t *)mo;
}
