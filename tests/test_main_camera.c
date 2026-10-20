/* The placeholder main camera that entity_registry_init() leaves until
 * `reset_world()` replaces it with a `camera_t`, read through
 * main_camera_typed(). The frozen values were taken on the code where the
 * placeholder was still a class-less instance and must not be re-taken. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "engine/vec.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}


#define IS_ON_SCREEN_NEAR 1
#define IS_ON_SCREEN_FAR 0
#define UPDATE_MOUSE_WORLD_X 0
#define UPDATE_MOUSE_WORLD_Y 0
#define CAMERA_X 176
#define CAMERA_Y 196

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static int entity_list_listed(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return 1;
        }
    }
    return 0;
}

/* Not a `camera_t`; no method answers and the camera stays put; the live
 * entities and the messages are empty. */
static void assert_placeholder(void) {
    const entity_message_t buckets[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
    entity_t *mo = main_camera();
    int i;

    assert(mo != NULL && main_camera_typed() == NULL);
    p8_camera(&graphics, 5, 6);
    assert(mo->class != camera_class());
    assert(mo->class->states[ENTITY_STATE_NORMAL] == NULL && mo->class->draw == NULL);
    assert(graphics.camera_x == 5 && graphics.camera_y == 6);
    assert(live_entities()->count == 0);
    for (i = 0; i < 5; i++) {
        assert(entities_by_message(buckets[i])->count == 0);
    }
}

static void assert_a_camera(void) {
    const vec_t *ra;

    assert(main_camera_typed() != NULL && pos_ref(&main_camera_typed()->header) != NULL);
    ra = pos_ref(&main_camera_typed()->header);
    assert(ra->x == p8_fixed_from_int(CAMERA_X) && ra->y == p8_fixed_from_int(CAMERA_Y));
}

static void test_placeholder_after_init(void) {
    entity_registry_init(NULL);
    assert_placeholder();
    assert(!entity_list_listed(live_entities(), main_camera()));
}

static const entity_class_t pos_only_class_def = {0};

/* entity_geom_is_on_screen reads the camera position as (0,0): 16 off the
 * centre is inside the 92 margin, 93 is not. */
static void test_is_on_screen_on_placeholder(void) {
    vec_t near_pos = vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64));
    vec_t far_pos = vec_make(p8_fixed_from_int(80 + 93), p8_fixed_from_int(64));
    entity_t near_e = {.class = &pos_only_class_def};
    entity_t far_e = {.class = &pos_only_class_def};
    int near_hit;
    int far_hit;

    entity_registry_init(NULL);
    entity_set_pos(&near_e, near_pos);
    entity_set_pos(&far_e, far_pos);
    near_hit = entity_geom_is_on_screen(&near_e);
    far_hit = entity_geom_is_on_screen(&far_e);
    printf("[is_on_screen] near %d far %d\n", near_hit, far_hit);
    fflush(stdout);
    assert(near_hit == IS_ON_SCREEN_NEAR && far_hit == IS_ON_SCREEN_FAR);
}

/* The frame around the screen reaches 12 pixels past each edge: an entity
 * on the frame is on screen, one pixel past it is not, on each of the four
 * edges, under the placeholder camera at (0,0). */
static int on_screen_at(int x, int y) {
    vec_t at = vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
    entity_t e = {.class = &pos_only_class_def};

    entity_set_pos(&e, at);
    return entity_geom_is_on_screen(&e);
}

static void test_is_on_screen_frame_is_the_screen_plus_12_on_each_edge(void) {
    const int mid_x = P8_SCREEN_WIDTH / 2;
    const int mid_y = P8_SCREEN_HEIGHT / 2;

    entity_registry_init(NULL);
    assert(on_screen_at(-12, mid_y) && !on_screen_at(-13, mid_y));
    assert(on_screen_at(P8_SCREEN_WIDTH + 12, mid_y) && !on_screen_at(P8_SCREEN_WIDTH + 13, mid_y));
    assert(on_screen_at(mid_x, -12) && !on_screen_at(mid_x, -13));
    assert(on_screen_at(mid_x, P8_SCREEN_HEIGHT + 12) && !on_screen_at(mid_x, P8_SCREEN_HEIGHT + 13));
}

/* The same frame pinned at 160x120: columns 141 to 159, beyond the cart's
 * 128 columns, are on screen. */
static void test_is_on_screen_frame_at_160x120(void) {
    entity_registry_init(NULL);
    assert(on_screen_at(172, 60) && !on_screen_at(173, 60));
    assert(on_screen_at(-12, 60) && !on_screen_at(-13, 60));
    assert(on_screen_at(80, 132) && !on_screen_at(80, 133));
    assert(on_screen_at(80, -12) && !on_screen_at(80, -13));
    assert(on_screen_at(150, 60));
}

/* A ui_update on the placeholder, the `ui_t` built by hand since
 * `reset_world()` has not run: `mouse_world` is the mouse position plus the
 * camera position. */
static void test_update_on_placeholder(void) {
    ui_t *ok;
    const vec_t *qa;

    entity_registry_init(NULL);
    ok = ui_new();
    ui_update(&memory, ok);
    assert(ok->has & UI_HAS_MOUSE_WORLD);
    qa = &ok->mouse_world;
    printf("[update mouse_world] (%d,%d)\n", (int)qa->x, (int)qa->y);
    fflush(stdout);
    assert(qa->x == UPDATE_MOUSE_WORLD_X && qa->y == UPDATE_MOUSE_WORLD_Y);
}

static void test_shutdown_and_init_cycle(void) {
    entity_registry_shutdown();
    assert(main_camera() == NULL);
    entity_registry_init(NULL);
    assert_placeholder();
    entity_registry_init(NULL);
    entity_registry_init(NULL);
    assert_placeholder();
    entity_registry_shutdown();
    entity_registry_shutdown();
    assert(main_camera() == NULL);
}

/* `reset_world()` puts a `camera_t` in its place; entity_registry_init()
 * puts it back. */
static void test_replaced_by_reset_world(void) {
    reset_world(NULL);
    assert_a_camera();
    entity_registry_init(NULL);
    assert_placeholder();
    reset_world(NULL);
    bootstrap_shutdown();
    assert(main_camera() == NULL);
    reset_world(NULL);
    assert_a_camera();
}

static int is_a_struct_of(const void *e, const entity_class_t *cls) {
    return e != NULL &&
           ((const entity_t *)e)->class == cls;
}

/* A struct of its own class, one object for the whole process. A
 * `camera_t` once `reset_world()` has run. */
static void test_placeholder_is_a_struct(void) {
    const void *placeholder;

    entity_registry_init(NULL);
    placeholder = main_camera();
    assert(is_a_struct_of(placeholder, main_camera_placeholder_class()));
    reset_world(NULL);
    assert(is_a_struct_of(main_camera(), camera_class()));
    entity_registry_init(NULL);
    assert(main_camera() == placeholder);
    bootstrap_shutdown();
}

/* A shaken camera: its position is off its position before shake, and off
 * its own diagonal. entity_geom_is_on_screen reads the camera's position,
 * never its position before shake, and each axis on its own. */
static void test_is_on_screen_under_a_shaken_camera(void) {
    static const vec_t zero_pos_before_shake = {0, 0};
    static vec_t shaken_pos;
    vec_t in_pos = vec_make(p8_fixed_from_int(200 + 64), p8_fixed_from_int(100 + 64));
    vec_t below_pos = vec_make(p8_fixed_from_int(200 + 64), p8_fixed_from_int(100 + 64 + 77));
    entity_t in_e = {.class = &pos_only_class_def};
    entity_t below_e = {.class = &pos_only_class_def};
    camera_t *qb;
    entity_t saved;

    reset_world(NULL);
    qb = main_camera_typed();
    assert(qb != NULL);
    saved = qb->header;
    shaken_pos = vec_make(p8_fixed_from_int(200), p8_fixed_from_int(100));
    entity_set_map_pos(&qb->header, zero_pos_before_shake);
    entity_set_pos(&qb->header, shaken_pos);
    entity_set_pos(&in_e, in_pos);
    entity_set_pos(&below_e, below_pos);
    assert(entity_geom_is_on_screen(&in_e) == IS_ON_SCREEN_NEAR);
    assert(entity_geom_is_on_screen(&below_e) == IS_ON_SCREEN_FAR);
    qb->header = saved;
    bootstrap_shutdown();
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

    test_placeholder_after_init();
    test_is_on_screen_on_placeholder();
    test_is_on_screen_frame_is_the_screen_plus_12_on_each_edge();
    test_is_on_screen_frame_at_160x120();
    test_update_on_placeholder();
    test_shutdown_and_init_cycle();
    test_replaced_by_reset_world();
    test_placeholder_is_a_struct();
    test_is_on_screen_under_a_shaken_camera();

    printf("test_main_camera: all tests passed\n");
    return 0;
}
