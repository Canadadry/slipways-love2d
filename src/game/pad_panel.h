#ifndef GAME_PAD_PANEL_H
#define GAME_PAD_PANEL_H

#include "engine/entity_class.h"
#include "game/pad_direction.h"

enum { PAD_PANEL_ANY_LAYER = -32768 };

int pad_panel_box_open(void);

int pad_panel_open(void);

/* Inside the pair, pad_panel_open reuses its answer while the ui, the economy, the box's ephemerals count and
 * the registered set are unchanged; a backdrop's is_dead is not watched, nothing writes it inside a pad step. */
void pad_panel_step_begin(void);
void pad_panel_step_end(void);

int pad_panel_is_candidate(const entity_t *e);

entity_t *pad_panel_entity_under(int x, int y, int above, int (*accept)(const entity_t *e));

int pad_panel_is_item(const entity_t *e);

void pad_panel_focus_point(const entity_t *e, int *x, int *y);

entity_t *pad_panel_first_item(void);

entity_t *pad_panel_neighbor(const entity_t *item, pad_direction_t direction);

entity_t *pad_panel_step(entity_t *focus, int choose_held, pad_direction_t direction, int *x, int *y,
                         int *mouse_buttons);

#endif
