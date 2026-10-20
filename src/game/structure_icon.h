#ifndef GAME_STRUCTURE_ICON_H
#define GAME_STRUCTURE_ICON_H

#include "game/game_data.h"
#include "p8/gfx.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

typedef struct {
    entity_t header;
    const structure_icon_archetype_t *archetype;
    const entity_kind_t *kind;
    tooltip_t *tooltip;
    int is_revealed;
    int has_is_revealed;
} structure_icon_t;

typedef struct {
    entity_t header;
    const char *required_tech;
    p8_fixed_t sprite;
    int has_sprite;
    state_t state;
    p8_fixed_t cost;
    int has_cost;
} structure_icon_kind_t;

void structure_icon_init(void);

void structure_icon_shutdown(void);

int structure_icon_ready(void);
const entity_class_t *structure_icon_class(void);

typedef struct {
    const entity_kind_t *kind;
    tooltip_t *tooltip;
} structure_icon_keys_t;

structure_icon_t *structure_icon_new(const structure_icon_keys_t *keys);

const vec_t *structure_icon_rest_cursor(void);

int structure_icon_has_cursor(void);

/* Zero while the cursor is absent. */
vec_t structure_icon_cursor(void);

int structure_icon_id_counter(p8_fixed_t *out);
void structure_icon_set_id_counter(p8_fixed_t id);

void structure_icon_set_cursor(vec_t cursor);

void structure_icon_refresh(structure_icon_t *self);

/* A live structure icon its refresh has revealed: drawn and hit-tested on screen. 0 for NULL or any other entity. */
int structure_icon_is_revealed(const entity_t *e);

down_t structure_icon_left_press(structure_icon_t *self);

structure_icon_kind_t structure_icon_kind_view(const structure_icon_t *self);
const entity_class_t *structure_icon_kind_view_class(void);

void structure_icon_draw(p8_graphics_t *gfx, p8_vram_t *mem, structure_icon_t *self, vec_t p);

#endif
