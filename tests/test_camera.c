/* The camera `camera_t` (`main_camera()` once `reset_world()` has run),
 * read and written on its members through main_camera_typed() and driven
 * through the real update_entities and draw_layers, in games built directly:
 * no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/background.h"
#include "game/planet_family.h"
#include "game/galaxy.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


#define SHAKE_TICKS 31
#define SHAKE_HASH 0xF1E7A593u
#define GEOHARVEST_WINDOW 60

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return 1;
        }
    }
    return 0;
}

static int in_any_entities_by_message(const void *e) {
    const entity_message_t names[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
    int i;

    for (i = 0; i < 5; i++) {
        if (entity_list_listed(entities_by_message(names[i]), e)) {
            return 1;
        }
    }
    return 0;
}

static camera_t *checked_main_camera(void) {
    assert(main_camera_typed() != NULL);
    return main_camera_typed();
}

static vec_t must_vec(const vec_t *v) {
    assert(v != NULL);
    return *v;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void unscript(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
}

/* `reset_world()` builds the main camera: among the live entities only, the
 * archetype's defaults, the entity's state and state time, and the position
 * before shake and the position two distinct vectors. */
static void check_fresh_main_camera(const camera_t *mo) {
    vec_t p = must_vec(map_pos_ref(&mo->header));
    vec_t ra = must_vec(pos_ref(&mo->header));

    assert(entity_list_listed(live_entities(), mo) && !in_any_entities_by_message(mo));
    assert(p.x == p8_fixed_from_int(176) && p.y == p8_fixed_from_int(196));
    assert(ra.x == p8_fixed_from_int(176) && ra.y == p8_fixed_from_int(196));
    assert(mo->header.shake == 0);
    assert(mo->header.state_time == 0);
    assert(mo->header.state == ENTITY_STATE_NORMAL);
    assert(map_pos_ref(&mo->header) != pos_ref(&mo->header));
}

static void test_reset_world_builds_a_camera(void) {
    reset_world(NULL);
    check_fresh_main_camera(checked_main_camera());
}

static void apply_camera_to(p8_fixed_t fy, int want_x, int want_y) {
    p8_camera(&graphics, 7, 11);
    assert(main_camera()->class == camera_class());
    camera_apply(dispatch_context_gfx(), (camera_t *)main_camera(), fy);
    assert(graphics.camera_x == want_x && graphics.camera_y == want_y);
}

/* camera_apply puts the camera on the position times the factor. The later
 * tests of this process start from the camera the last call leaves: 176,196. */
static void test_apply_camera_sets_the_camera(void) {
    reset_world(NULL);
    apply_camera_to(p8_fixed_from_int(1), 176, 196);
    apply_camera_to(P8_FIXED_LIT(0x00008000, 0.5), 88, 98);
    apply_camera_to(0, 0, 0);
    apply_camera_to(p8_fixed_from_int(1), 176, 196);
}

/* Pushed far past each corner, camera_update brings the screen centre back
 * to the edge of the planet area, (0.125, 2.875) * 128 on both axes. */
static void test_update_keeps_the_screen_centre_over_the_planet_area(void) {
    static const int pushed[2] = { -1000, 1000 };
    p8_fixed_t scale = p8_fixed_from_int(GALAXY_MAP_SCALE);
    p8_fixed_t edges[2] = { p8_fixed_mul(GALAXY_MAP_MARGIN, scale),
                            p8_fixed_mul(p8_fixed_add(GALAXY_MAP_MARGIN, GALAXY_MAP_SPAN), scale) };

    reset_world(NULL);
    unscript();
    for (int i = 0; i < 2; i++) {
        camera_t *mo = checked_main_camera();
        entity_set_map_pos(&mo->header, vec_make(p8_fixed_from_int(pushed[i]), p8_fixed_from_int(pushed[i])));
        update_entities();
        vec_t p = must_vec(map_pos_ref(&mo->header));
        assert(p8_fixed_add(p.x, p8_fixed_from_int(P8_SCREEN_WIDTH / 2)) == edges[i]);
        assert(p8_fixed_add(p.y, p8_fixed_from_int(P8_SCREEN_HEIGHT / 2)) == edges[i]);
    }
}

/* A shake of 3, as a burst's construction writes it, decays by 0.1 per
 * camera_update, shaking the position off the position before shake by
 * entity_geom_shake. */
static void test_shake_decays(void) {
    uint32_t h = 2166136261u;
    int ticks = 0;
    const camera_t *mo;

    reset_world(NULL);
    mo = checked_main_camera();
    checked_main_camera()->header.shake = p8_fixed_from_int(3);
    assert(mo->header.shake == p8_fixed_from_int(3));
    unscript();
    while (mo->header.shake != 0) {
        vec_t off;
        update_entities();
        ticks++;
        assert(ticks < 100);
        off = vec_sub(must_vec(pos_ref(&mo->header)), must_vec(map_pos_ref(&mo->header)));
        h = fnv(h, (uint32_t)mo->header.shake);
        h = fnv(h, (uint32_t)off.x);
        h = fnv(h, (uint32_t)off.y);
    }
    printf("[shake] shake back to 0 after %d ticks\n", ticks);
    fflush(stdout);
    assert(ticks == SHAKE_TICKS);
    check("shake", h, SHAKE_HASH);
}

/* `cart_start_game()` runs `reset_world()` again over a live main camera; a
 * shutdown and a fresh `reset_world()` after it must not crash either. */
static void test_second_reset_world(void) {
    const void *first;

    reset_world(NULL);
    first = main_camera();
    reset_world(NULL);
    assert(main_camera() != first);
    assert(!entity_list_listed(live_entities(), first));
    check_fresh_main_camera(checked_main_camera());

    bootstrap_shutdown();
    reset_world(NULL);
    check_fresh_main_camera(checked_main_camera());
}

/* Before `reset_world()`, the main camera is the placeholder, not a
 * `camera_t`: nothing is applied and the camera stays put. */
static void test_placeholder_main_camera(void) {
    entity_registry_init(NULL);
    p8_camera(&graphics, 5, 6);
    assert(main_camera()->class != camera_class());
    assert(graphics.camera_x == 5 && graphics.camera_y == 6);
    assert(main_camera_typed() == NULL);
}

static int burst_alive(void) {
    const entity_list_t *hv = live_entities();
    int i;

    for (i = 0; i < hv->count; i++) {
        const void *e = hv->items[i];
        if (e != NULL &&
            ((const entity_t *)e)->class == burst_class()) {
            return 1;
        }
    }
    return 0;
}

/* A burst's construction shakes the camera: a shake of 3 as soon as it is
 * built, then the decay back to 0 within GEOHARVEST_WINDOW ticks. */
static void test_burst_shakes(void) {
    int i;

    cart_init();
    cart_start_game(NULL);
    assert(!burst_alive());
    (void)burst_new(NULL);
    assert(burst_alive());
    assert(checked_main_camera()->header.shake == p8_fixed_from_int(3));
    for (i = 0; i < GEOHARVEST_WINDOW; i++) {
        update_entities();
    }
    assert(checked_main_camera()->header.shake == 0);
}

/* The galaxy depends on the rnd state, which the tests before it consume: run
 * it in a child forked right after cart_load(), as a fresh process
 * would start it. */
static void in_fresh_child(void (*test)(void)) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static int is_a_camera_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == camera_class();
}

/* The main camera of `reset_world()` and one built by camera_new are camera
 * structs among the live entities only, with an update and an apply as
 * methods and no draw; inside the bound, unshaken, the update keeps it in place. */
static void test_camera_is_a_struct(void) {
    camera_t *generic;

    reset_world(NULL);
    generic = camera_new();
    assert(is_a_camera_struct(main_camera()) && is_a_camera_struct(generic));
    assert(entity_list_listed(live_entities(), generic) && !in_any_entities_by_message(generic));
    check_fresh_main_camera(generic);
    assert(entity_has_pos(&generic->header) && entity_has_map_pos(&generic->header));
    camera_class()->states[ENTITY_STATE_NORMAL](&generic->header, p8_fixed_from_int(0));
    camera_apply(dispatch_context_gfx(), generic, p8_fixed_from_int(1));
    assert(camera_class()->draw == NULL);
    {
        vec_t p_before = must_vec(map_pos_ref(&generic->header));

        camera_class()->states[ENTITY_STATE_NORMAL](&generic->header, p8_fixed_from_int(0));
        assert(map_pos_ref(&generic->header)->x == p_before.x && map_pos_ref(&generic->header)->y == p_before.y);
    }
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    in_fresh_child(test_burst_shakes);

    test_reset_world_builds_a_camera();
    test_apply_camera_sets_the_camera();
    test_update_keeps_the_screen_centre_over_the_planet_area();
    test_shake_decays();
    test_second_reset_world();
    test_placeholder_main_camera();
    test_camera_is_a_struct();

    printf("test_camera: all tests passed\n");
    return 0;
}
