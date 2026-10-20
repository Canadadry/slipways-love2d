#ifndef GAME_PAD_SELECT_H
#define GAME_PAD_SELECT_H

#include "engine/entity_class.h"
#include "engine/entity_list.h"

#define PAD_SELECT_HOLD_FRAMES 30
#define PAD_SELECT_TAP_FRAMES 10
#define PAD_SELECT_CHAIN_FRAMES 15

typedef struct {
    int held_frames;
    int fired;
    int chain_frames;
} pad_select_t;

typedef enum {
    PAD_SELECT_NONE,
    PAD_SELECT_TAP,
    PAD_SELECT_YEAR
} pad_select_event_t;

void pad_select_init(pad_select_t *s);

pad_select_event_t pad_select_step(pad_select_t *s, int down, int allowed);

void pad_select_year_ended(pad_select_t *s);

int pad_select_year_just_ended(const pad_select_t *s);

int pad_select_needs_attention(const entity_t *e);

entity_t *pad_select_next(const entity_list_t *candidates, const entity_t *current);

#endif
