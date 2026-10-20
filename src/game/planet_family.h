#ifndef GAME_PLANET_FAMILY_H
#define GAME_PLANET_FAMILY_H

#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/particle.h"
#include "game/planet.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

void planet_family_init(void);

void planet_family_shutdown(void);

int structure_ready(void);
const entity_kind_t *structure_kind(void);
const entity_class_t *structure_class(void);

void structure_init_instance(planet_t *self, const entity_class_t *cls, const planet_archetype_t *archetype);

planet_t *structure_new(const planet_keys_t *keys);

int trading_hub_ready(void);
const entity_kind_t *trading_hub_kind(void);
const entity_class_t *trading_hub_class(void);

planet_t *trading_hub_new(const planet_keys_t *keys);

int processor_ready(void);
const entity_kind_t *processor_kind(void);
const entity_class_t *processor_class(void);

planet_t *processor_new(const planet_keys_t *keys);

int ascension_ready(void);
const entity_kind_t *ascension_kind(void);
const entity_class_t *ascension_class(void);

planet_t *ascension_new(const planet_keys_t *keys);

void ascension_init_instance(planet_t *self, const entity_class_t *cls, const planet_archetype_t *archetype);

void structure_update_settled_method(entity_t *self, p8_fixed_t t);
/* 1 when e is settled and its settled update calls planet_bob. */
int planet_family_bobs(const entity_t *e);
void ascension_draw_method(entity_t *self, vec_t screen_pos);
void ascension_release_method(entity_t *self);

int protostar_ready(void);
const entity_kind_t *protostar_kind(void);
const entity_class_t *protostar_class(void);

planet_t *protostar_new(const planet_keys_t *keys);

int burst_ready(void);
const entity_kind_t *burst_kind(void);
const entity_class_t *burst_class(void);

typedef struct {
    planet_keys_t planet;
    const burst_construct_keys_t *construct;
} burst_keys_t;

planet_t *burst_new(const burst_keys_t *keys);

void trading_hub_update_settled(planet_t *self, p8_fixed_t t);

void ascension_draw(p8_graphics_t *gfx, p8_vram_t *mem, planet_t *self, vec_t p);

void protostar_construct(planet_t *self);

void burst_construct(planet_t *self);

particle_t *burst_spawn_particle(entity_t *self);

void burst_draw(p8_graphics_t *graphics, p8_vram_t *memory, planet_t *self, vec_t screen_pos);

#endif
