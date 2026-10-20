#include "game/particle.h"

#include <assert.h>
#include <stddef.h>

#include "engine/dispatch_context.h"
#include "engine/entity_geom.h"
#include "p8/fixed.h"
#include "game/game_data.h"
#include "engine/vec.h"
#include "game/particle_zone.h"
#include "game/planet.h"
#include "game/probe.h"
#include "game/visual_random.h"

typedef char particle_fills_its_slot_t[ZONE_BYTES(sizeof(particle_t)) == PARTICLE_SLOT ? 1 : -1];

enum { EMITTER_POINT_BATCH = 32 };

static int g_pixels_hidden;

void particle_hide_pixels(int hidden) {
    g_pixels_hidden = hidden != 0;
}

int particle_pixels_hidden(void) {
    return g_pixels_hidden;
}

/* A class property, not the position bit: a probe dragged without a mouse position carries a position that is absent. */
static int emitter_host_carries_pos(const entity_t *self) {
    return (self->class->lineage & (LINEAGE(LINEAGE_PROBE) | LINEAGE(LINEAGE_PLANET))) != 0;
}

static p8_fixed_t emitter_host_layer_pos(const entity_t *self, vec_t *ra) {
    assert(entity_has(self, ENTITY_HAS_LAYER) && emitter_host_carries_pos(self) &&
           "emitter_update: a host that carries no layer or no pos, none of whose draw runs it -- stop");
    *ra = entity_pos(self);
    return self->layer;
}

static int particle_to_int(p8_fixed_t x) {
    return p8_fixed_flr_int(x);
}

particle_t *particle_take(void) {
    return (particle_t *)allocator_alloc(particle_zone(PARTICLE_SLOT_PARTICLE), sizeof(particle_t));
}

const allocator_t *particle_burst_slot_take(void) {
    return particle_zone_slot(PARTICLE_SLOT_BURST);
}

emitter_t *emitter_of(entity_t *e) {
    if (e->class->lineage & LINEAGE(LINEAGE_PROBE)) {
        return &((probe_t *)e)->emitter;
    }
    if (planet_is_family(e)) {
        planet_t *planet = (planet_t *)e;
        return (planet->has & PLANET_HAS_EMITTER) ? &planet->emitter : NULL;
    }
    return NULL;
}

void emitter_init(entity_t *self) {
    emitter_init_in(self, PARTICLE_SLOT_LIST);
}

void emitter_init_in(entity_t *self, particle_slot_kind_t slot_kind) {
    emitter_t *emitter;

    if (self == NULL) {
        return;
    }
    emitter = emitter_of(self);
    assert(emitter != NULL && "emitter_init_in: a class that carries no particle emitter has no particle list -- stop");
    assert((slot_kind == PARTICLE_SLOT_LIST || slot_kind == PARTICLE_SLOT_HELP_LIST) && "emitter_init_in: a particle list is a list or a help screen list");

    emitter_release_particles(emitter);
    pointer_list_init_reserve(&emitter->particles, particle_zone_slot(slot_kind),
                              slot_kind == PARTICLE_SLOT_HELP_LIST ? PARTICLES_HELP_SCREEN_CAPACITY : PARTICLES_CAPACITY);
    emitter->has |= EMITTER_HAS_PARTICLES;
    emitter->particles_slot_kind = slot_kind;
}

void emitter_release_particles(emitter_t *emitter) {
    pointer_list_t *qy = &emitter->particles;
    const allocator_t *particle = particle_zone(PARTICLE_SLOT_PARTICLE);
    int i;

    if (!(emitter->has & EMITTER_HAS_PARTICLES)) {
        return;
    }
    for (i = 0; i < qy->count; i++) {
        allocator_release(particle, qy->items[i]);
    }
    arena_release(qy->zone);
    emitter->particles = (pointer_list_t){0};
    emitter->has &= (uint16_t)~EMITTER_HAS_PARTICLES;
}

void emitter_spawn(entity_t *self, p8_fixed_t n) {
    emitter_t *emitter;
    pointer_list_t *qy;

    if (self == NULL) {
        return;
    }
    emitter = emitter_of(self);
    qy = emitter != NULL && (emitter->has & EMITTER_HAS_PARTICLES) ? &emitter->particles : NULL;

    while (visual_random_draw_one() < n) {
        if (qy != NULL && self->class->spawn_particle != NULL) {
            pointer_list_add(qy, self->class->spawn_particle(self));
            particle_zone_record_elements(emitter->particles_slot_kind, qy->count);
        }
        n = p8_fixed_sub(n, p8_fixed_from_int(1));
    }
}

void emitter_update(const p8_graphics_t *graphics, p8_vram_t *memory, entity_t *self) {
    emitter_t *emitter;
    p8_fixed_t layer;
    vec_t screen_pos;
    p8_fixed_t center_x;
    p8_fixed_t center_y;
    p8_fixed_t matrix_xx;
    p8_fixed_t matrix_yx;
    p8_fixed_t matrix_xy;
    p8_fixed_t matrix_yy;
    const int_seq_t *colors;
    pointer_list_t *particles;
    int index;

    if (self == NULL) {
        return;
    }
    emitter = emitter_of(self);
    assert(emitter != NULL && "emitter_update: a class that carries no particle emitter is no receiver -- stop");

    layer = emitter_host_layer_pos(self, &screen_pos);
    if (layer < p8_fixed_from_int(10) && !entity_geom_is_on_screen(self)) {
        return;
    }

    center_x = screen_pos.x;
    center_y = screen_pos.y;
    matrix_xx = emitter->matrix_xx;
    matrix_yx = emitter->matrix_yx;
    matrix_xy = emitter->matrix_xy;
    matrix_yy = emitter->matrix_yy;
    colors = emitter->trail_colors;

    if (emitter->has & EMITTER_HAS_PARTICLES) {
        p8_point_t points[EMITTER_POINT_BATCH];
        int point_count = 0;
        particles = &emitter->particles;
        for (index = 0; index < particles->count; index++) {
            particle_t *particle = particles->items[index];
            vec_t pv;

            if (particle == NULL) {
                continue;
            }
            pv = particle->velocity;

            particle->pos = vec_add(particle->pos, pv);

            particle->velocity.x = p8_fixed_add(p8_fixed_mul(matrix_xx, pv.x), p8_fixed_mul(matrix_xy, pv.y));
            /* Reads the x just written, not pv.x: "fixing" it moves every particle whose matrix_yx is not 0. */
            particle->velocity.y = p8_fixed_add(p8_fixed_mul(matrix_yx, particle->velocity.x), p8_fixed_mul(matrix_yy, pv.y));

            if (particle->has_acceleration) {
                particle->velocity = vec_add(particle->velocity, particle->acceleration);
            }

            particle->life = p8_fixed_sub(particle->life, P8_FIXED_LIT(0x000023d7, 0.14));

            if (particle->life <= p8_fixed_from_int(1)) {
                /* Removing an element during this walk skips the next one; it is visited next frame. Compensating the index would change update timing and every replay. */
                pointer_list_del(particles, particle);
                allocator_release(particle_zone(PARTICLE_SLOT_PARTICLE), (unsigned char *)particle);
            } else {
                int px = particle_to_int(p8_fixed_add(center_x, particle->pos.x));
                int py = particle_to_int(p8_fixed_add(center_y, particle->pos.y));
                int col = 0;
                int idx = particle_to_int(p8_fixed_flr(particle->life));

                if (colors != NULL) {
                    int_seq_at(colors->items, colors->count, idx, &col);
                }

                if (!g_pixels_hidden) {
                    points[point_count].x = px;
                    points[point_count].y = py;
                    points[point_count].col = col;
                    if (++point_count == EMITTER_POINT_BATCH) {
                        p8_psets(graphics, memory, points, point_count);
                        point_count = 0;
                    }
                }
            }
        }
        if (point_count > 0) {
            p8_psets(graphics, memory, points, point_count);
        }
    }

    emitter_spawn(self, emitter->archetype->spawn_rate);
}

void emitter_update_method(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    emitter_update(dispatch_context_gfx(), dispatch_context_mem(), self);
}
