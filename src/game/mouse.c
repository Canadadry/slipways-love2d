#include "game/mouse.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "engine/dispatch_context.h"
#include "game/game_data.h"
#include "game/mouse_button.h"
#include "engine/vec.h"
#include "engine/game_zone.h"

static int g_mouse_up = 0;
static mouse_button_t g_left_button;

static void mouse_update_method(entity_t *self, p8_fixed_t t) {
    (void)t;
    mouse_update(dispatch_context_input(), (mouse_t *)self);
}

static const entity_class_t mouse_class_def = {
    .states = {[ENTITY_STATE_NORMAL] = mouse_update_method},
};

static mouse_t *mouse_create(void) {
    mouse_t *self = (mouse_t *)allocator_alloc(game_zone(), sizeof *self);

    entity_init_instance(&self->header, &mouse_class_def, mouse_archetype.state, mouse_archetype.state_time);
    self->archetype = &mouse_archetype;
    entity_set_pos(&self->header, vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0)));
    entity_construct(&self->header);
    return self;
}

void mouse_init(void) {
    mouse_shutdown();
    assert(entity_ready() && "mouse_init: entity_init() runs first");
    g_mouse_up = 1;
    memset(&g_left_button, 0, sizeof g_left_button);
}

void mouse_shutdown(void) {
    g_mouse_up = 0;
}

const mouse_button_t *mouse_left_button(void) {
    return g_mouse_up ? &g_left_button : NULL;
}

int mouse_ready(void) {
    return g_mouse_up;
}

const entity_class_t *mouse_class(void) {
    return &mouse_class_def;
}

mouse_t *mouse_new(void) {
    return mouse_create();
}

void mouse_update(const p8_input_t *input, mouse_t *self) {
    if (self == NULL || input == NULL) {
        return;
    }

    p8_mouse_t mouse = p8_input_read_mouse(input);
    entity_set_pos(&self->header, vec_make(p8_fixed_from_int(mouse.x), p8_fixed_from_int(mouse.y)));

    if (g_mouse_up) {
        mouse_button_update(&g_left_button, (mouse.buttons & P8_MOUSE_LEFT) != 0);
    }
}
