#ifndef GAME_PAD_JUMP_H
#define GAME_PAD_JUMP_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"
#include "engine/entity_list.h"
#include "game/bindings.h"
#include "game/pad_direction.h"

#define PAD_JUMP_REPEAT_DELAY 18
#define PAD_JUMP_REPEAT_PERIOD 8
#define PAD_JUMP_SCROLL_MARGIN 20
#define PAD_JUMP_SCROLL_MARGIN_BOTTOM 31
#define PAD_JUMP_SCROLL_SPEED 8

enum { PAD_JUMP_DIRECTIONS = 4 };

typedef struct {
    entity_id_t hovered;
    int seeded;
    pad_direction_t repeat_direction;
    int repeat_frames;
} pad_jump_t;

void pad_jump_init(pad_jump_t *jump);

void pad_jump_hover(pad_jump_t *jump, const entity_t *e);
entity_t *pad_jump_hovered(const pad_jump_t *jump);
void pad_jump_hover_seeded(pad_jump_t *jump, const entity_t *e);
entity_t *pad_jump_reseed(pad_jump_t *jump, const entity_list_t *candidates, int centre_x, int centre_y);

pad_direction_t pad_jump_direction(int index);

pad_direction_t pad_jump_repeat_step(pad_jump_t *jump, const bindings_t *bindings, bindings_mode_t mode,
                                     int pad_buttons, int pressed);
void pad_jump_repeat_clear(pad_jump_t *jump);

int pad_jump_inside(int x, int y);
void pad_jump_scroll(int x, int y, int *dx, int *dy);

void pad_jump_world_point(const entity_t *e, int *x, int *y);
entity_t *pad_jump_nearest(int x, int y, const entity_list_t *candidates);
entity_t *pad_jump_seed(const entity_list_t *candidates, int centre_x, int centre_y);
entity_list_t pad_jump_candidates(const allocator_t *zone, const entity_list_t *nodes);
int pad_jump_any_candidate(const entity_list_t *nodes);

#endif
