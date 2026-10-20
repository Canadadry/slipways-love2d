#ifndef GAME_PAD_SLIPWAY_H
#define GAME_PAD_SLIPWAY_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"
#include "engine/entity_list.h"

typedef enum { PAD_SLIPWAY_NONE, PAD_SLIPWAY_PRESSED, PAD_SLIPWAY_HELD, PAD_SLIPWAY_RELEASED } pad_slipway_phase_t;

typedef enum { PAD_SLIPWAY_REFUSED, PAD_SLIPWAY_BUILT } pad_slipway_outcome_t;

typedef struct {
    entity_id_t start;
    entity_id_t target;
    pad_slipway_phase_t phase;
    int linked_before;
} pad_slipway_t;

void pad_slipway_init(pad_slipway_t *slipway);

int pad_slipway_can_start(const entity_t *e);

entity_list_t pad_slipway_candidates(const allocator_t *zone, const entity_list_t *candidates, const entity_t *start);

entity_t *pad_slipway_between(const entity_t *start, const entity_t *target);
pad_slipway_outcome_t pad_slipway_outcome(const entity_t *start, const entity_t *target, int linked_before);

void pad_slipway_release(pad_slipway_t *slipway, int linked_before);
void pad_slipway_rebase(pad_slipway_t *slipway, pad_slipway_outcome_t outcome);

#endif
