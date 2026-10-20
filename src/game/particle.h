#ifndef GAME_PARTICLE_H
#define GAME_PARTICLE_H

#include "engine/entity_class.h"
#include "game/game_data.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "engine/pointer_list.h"
#include "engine/vec.h"
#include "game/particle_zone.h"

typedef struct particle {
    vec_t pos;
    vec_t velocity;
    int has_acceleration;
    vec_t acceleration;
    p8_fixed_t life;
} particle_t;

typedef struct emitter {
    const emitter_archetype_t *archetype;
    uint16_t has;
    p8_fixed_t angle_slices;
    p8_fixed_t base_radius;
    p8_fixed_t speed_factor;
    const int_seq_t *trail_colors;
    p8_fixed_t matrix_xx;
    p8_fixed_t matrix_xy;
    p8_fixed_t matrix_yx;
    p8_fixed_t matrix_yy;
    pointer_list_t particles;
    particle_slot_kind_t particles_slot_kind;
} emitter_t;

#define EMITTER_HAS_PARTICLES (1u << 0)

/* Points into the host: a probe, a help screen or a planet that carries one;
 * NULL for any other class. Valid while the host lives: never store it. */
emitter_t *emitter_of(entity_t *e);

void emitter_init(entity_t *self);
void emitter_init_in(entity_t *self, particle_slot_kind_t slot_kind);

particle_t *particle_take(void);

const allocator_t *particle_burst_slot_take(void);

void emitter_release_particles(emitter_t *emitter);

void emitter_spawn(entity_t *self, p8_fixed_t n);

void emitter_update(const p8_graphics_t *graphics, p8_vram_t *memory, entity_t *self);

void emitter_update_method(entity_t *self, vec_t screen_pos);

/* Off by default; only devtools turns it on. Hides the pixels of emitter
   particles, burst rings and transit dots, and nothing else: their state and
   every random draw are unchanged. */
void particle_hide_pixels(int hidden);
int particle_pixels_hidden(void);

#endif
