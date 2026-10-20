#include "game/planet.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "engine/bootstrap.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/vram.h"
#include "game/planet_display.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/vec.h"

/* The main camera's apply_camera call inside draw_layers(ENTITY_MESSAGE_DRAW), which
 * needs both a real camera and a real entity registered under "draw" to
 * run at all: reset_world(), a real planet and real graphics and memory. */

static int g_draw_calls = 0;
static int g_camera_x_at_draw;
static int g_camera_y_at_draw;
static vec_t g_pos_at_draw;

/* Records the camera apply_camera has left by the time the planet's draw
 * runs, then forwards to the real planet_draw, so the order is observed
 * without stubbing the behaviour out. Installed on the planet alone, through
 * a copy of the planet class whose draw is this spy, so no other planet sees
 * it. */
static void spy_draw(entity_t *self, vec_t ra) {
    g_draw_calls++;
    g_pos_at_draw = ra;
    g_camera_x_at_draw = dispatch_context_gfx()->camera_x;
    g_camera_y_at_draw = dispatch_context_gfx()->camera_y;
    planet_draw(dispatch_context_gfx(), dispatch_context_mem(), (planet_t *)self, ra);
}

static entity_class_t g_spied_planet;

static int vec_is(vec_t v, int x, int y) {
    return v.x == p8_fixed_from_int(x) && v.y == p8_fixed_from_int(y);
}

static void test_draw_layers_draw_dispatches_main_camera_apply_camera_then_real_planet_draw(void) {
    p8_vram_t memory;
    p8_graphics_t graphics;
    planet_t *planet;
    entity_t *camera;
    int screen_x;
    int screen_y;
    unsigned char pixel_after;

    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);

    g_draw_calls = 0;

    /* reset_world() builds the real registries and the real main camera. */
    reset_world(NULL);
    camera = main_camera();
    assert(camera != NULL);

    planet_init();
    planet = planet_new(NULL);
    assert(planet != NULL);

    /* The planet class has a draw, so the instance must land in the real
     * entities_by_message(ENTITY_MESSAGE_DRAW) bucket. */
    {
        const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_DRAW);
        int found = 0;
        int i;
        assert(bucket != NULL);
        for (i = 0; i < bucket->count; i++) {
            if (bucket->items[i] == (entity_t *)planet) {
                found = 1;
            }
        }
        assert(found);
    }

    /* A position inside the camera's default (176,196) viewport, so the
     * off-screen early return of planet_draw is not taken and the real draw
     * body runs. */
    entity_set_pos(&planet->header, vec_make(p8_fixed_from_int(256), p8_fixed_from_int(256)));

    assert(camera->class == camera_class());
    g_spied_planet = *planet_class();
    g_spied_planet.draw = spy_draw;
    planet->header.class = &g_spied_planet;

    /* planet_draw paints colour 0, which is also the framebuffer's cleared
     * value, so reading 0 back proves nothing unless the byte held something
     * else first -- hence the sentinel 9. The pixel is world (256,256)
     * minus the camera (176,196) that apply_camera sets inside this same
     * draw_layers(ENTITY_MESSAGE_DRAW): the centre of the screen, hardcoded
     * here because the camera is not set yet, and cross-checked against the
     * real camera below. */
    screen_x = 80;
    screen_y = 60;
    memory.screen[screen_y][screen_x] = 9;

    assert(graphics.camera_x == 0 && graphics.camera_y == 0);
    draw_layers(ENTITY_MESSAGE_DRAW);

    /* Order, never the reverse: within a layer, the camera call runs before
     * the per-entity loop, so the real planet_draw already sees its camera. */
    assert(g_draw_calls == 1);
    assert(g_camera_x_at_draw == 176 && g_camera_y_at_draw == 196);
    assert(vec_is(g_pos_at_draw, 256, 256));

    /* apply_camera really ran: the camera lands on its spec position
     * (176,196), its shake having decayed to its 0 default so it adds nothing. */
    assert(graphics.camera_x == 176);
    assert(graphics.camera_y == 196);

    /* The sentinel pixel against the camera actually set, so the two
     * cannot drift apart. */
    assert(screen_x == 256 - graphics.camera_x && screen_y == 256 - graphics.camera_y);

    /* planet_draw's filled circle of radius 7 in colour 0 paints a black disc on
     * world (256,256), i.e. screen (80,60): the sentinel is gone. */
    pixel_after = memory.screen[screen_y][screen_x];
    assert(pixel_after == 0);

    assert(main_camera() == camera);

    planet->header.class = planet_class();
    planet_shutdown();
    bootstrap_shutdown();
}

static void test_draw_layers_passes_the_zero_vector_to_an_entity_without_a_position(void) {
    p8_vram_t memory;
    p8_graphics_t graphics;
    planet_t *planet;

    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    g_draw_calls = 0;

    reset_world(NULL);
    planet_init();
    planet = planet_new(NULL);
    assert(planet != NULL);
    entity_set_pos(&planet->header, vec_make(p8_fixed_from_int(256), p8_fixed_from_int(256)));
    entity_clear_pos(&planet->header);

    g_spied_planet = *planet_class();
    g_spied_planet.draw = spy_draw;
    g_spied_planet.skip_draw_off_screen = 0;
    planet->header.class = &g_spied_planet;
    g_pos_at_draw = vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1));

    draw_layers(ENTITY_MESSAGE_DRAW);

    assert(g_draw_calls == 1);
    assert(vec_is(g_pos_at_draw, 0, 0));

    planet->header.class = planet_class();
    planet_shutdown();
    bootstrap_shutdown();
}

int main(void) {
    test_draw_layers_draw_dispatches_main_camera_apply_camera_then_real_planet_draw();
    test_draw_layers_passes_the_zero_vector_to_an_entity_without_a_position();

    printf("test_draw_layers_camera: all tests passed\n");
    return 0;
}
