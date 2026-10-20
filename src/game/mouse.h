#ifndef GAME_MOUSE_H
#define GAME_MOUSE_H

#include "game/game_data.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "game/mouse_button.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

typedef struct {
    entity_t header;
    const mouse_archetype_t *archetype;
} mouse_t;

void mouse_init(void);

void mouse_shutdown(void);

const mouse_button_t *mouse_left_button(void);

int mouse_ready(void);
const entity_class_t *mouse_class(void);

mouse_t *mouse_new(void);

void mouse_update(const p8_input_t *input, mouse_t *self);

#endif
