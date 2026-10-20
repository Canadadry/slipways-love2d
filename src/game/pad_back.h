#ifndef GAME_PAD_BACK_H
#define GAME_PAD_BACK_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"

typedef struct {
    entity_id_t last_acted;
} pad_back_t;

void pad_back_init(pad_back_t *back);

void pad_back_record(pad_back_t *back, const entity_t *e);

entity_t *pad_back_target(const pad_back_t *back, const entity_t *hovered);

#endif
