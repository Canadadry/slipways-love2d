/* probe_spawn_particle draws rnd(angle_slices) before rnd(0.5) on the
 * visual stream: the angle sums flr(rnd(angle_slices)) and rnd(0.5), drawn
 * left to right. The particle a probe emits through emitter_spawn must land
 * where that order puts it, whatever the compiler.
 *
 * The same visual state is reached twice by fork(): the child emits the
 * particle, the parent redraws the expected sequence from the identical
 * state with visual_random_draw. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "engine/entity.h"
#include "game/particle.h"
#include "game/probe.h"
#include "game/visual_random.h"
#include "engine/vec.h"

/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
}

static probe_t *new_probe(p8_fixed_t pn, p8_fixed_t t, p8_fixed_t du, p8_fixed_t py) {
    probe_keys_t keys = {0};

    keys.has_angle_slices = 1;
    keys.angle_slices = pn;
    keys.state_time = t;
    keys.has_base_radius = 1;
    keys.base_radius = du;
    keys.has_speed_factor = 1;
    keys.speed_factor = py;
    return probe_new(&keys);
}

static vec_t emitted_particle_pos(probe_t *probe) {
    void *particle;
    const particle_t *q;
    pointer_list_iter_t it;

    emitter_spawn((entity_t *)probe, p8_fixed_from_int(1));

    assert(particles_of(&probe->emitter) != NULL);
    it = pointer_list_all(particles_of(&probe->emitter));
    assert(pointer_list_iter_next(&it, &particle) && particle != NULL);
    q = particle;
    return q->pos;
}

static void test_probe_particle_draws_rnd_angle_slices_before_rnd_half(void) {
    p8_fixed_t angle_slices = p8_fixed_from_int(7);
    p8_fixed_t state_time = p8_fixed_from_int(3);
    p8_fixed_t base_radius = p8_fixed_from_int(10);
    p8_fixed_t speed_factor = p8_fixed_from_int(1);
    probe_t *probe;
    int pipe_ends[2];
    pid_t child;
    int status;
    vec_t got;
    vec_t want;
    p8_fixed_t first;
    p8_fixed_t second;
    p8_fixed_t a;

    entity_registry_init(NULL);
    entity_init();
    probe_init();
    probe = new_probe(angle_slices, state_time, base_radius, speed_factor);
    assert(probe != NULL);

    assert(pipe(pipe_ends) == 0);
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        vec_t p = emitted_particle_pos(probe);
        close(pipe_ends[0]);
        _exit(write(pipe_ends[1], &p, sizeof p) == (ssize_t)sizeof p ? 0 : 1);
    }
    close(pipe_ends[1]);
    assert(read(pipe_ends[0], &got, sizeof got) == (ssize_t)sizeof got);
    close(pipe_ends[0]);
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);

    /* emitter_spawn's loop test, then the angle's two draws, left to right. */
    (void)visual_random_draw_one();
    first = visual_random_draw(angle_slices);
    second = visual_random_draw(P8_FIXED_LIT(0x00008000, 0.5));
    a = p8_fixed_add(p8_fixed_div(p8_fixed_add(p8_fixed_flr(first), second), angle_slices),
                        p8_fixed_mul(state_time, P8_FIXED_LIT(0x00000041, 0.001)));
    want = vec_from_polar(p8_fixed_add(base_radius, visual_random_draw_one()), a);

    if (got.x != want.x || got.y != want.y) {
        fprintf(stderr, "probe_spawn_particle particle pos = (%08x,%08x), left-to-right order gives (%08x,%08x)\n", (unsigned)got.x,
                (unsigned)got.y, (unsigned)want.x, (unsigned)want.y);
    }
    assert(got.x == want.x && got.y == want.y);
}

int main(void) {
    test_probe_particle_draws_rnd_angle_slices_before_rnd_half();
    printf("test_probe_spawn_particle_order: ok\n");
    return 0;
}
