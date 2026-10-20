#ifndef GAME_PAD_BUILD_H
#define GAME_PAD_BUILD_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"

typedef struct {
    int open;
    entity_id_t icon;
} pad_build_t;

void pad_build_init(pad_build_t *b);

void pad_build_open(pad_build_t *b);
void pad_build_close(pad_build_t *b);
int pad_build_is_open(const pad_build_t *b);

entity_t *pad_build_icon(const pad_build_t *b);

entity_t *pad_build_focus(pad_build_t *b);

void pad_build_pick(pad_build_t *b, int step);

#endif
