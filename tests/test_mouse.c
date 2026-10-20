/* The mouse reader `mouse_t` (`main_mouse()` once `reset_world()` has run),
 * read through main_mouse_typed() and the shared button states and driven
 * through the real update_entities, in games built directly: no events
 * file. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/mouse_button.h"
#include "game/mouse.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/ui.h"
#include "engine/vec.h"

#define SCRIPTED_TICKS 12
#define SCRIPTED_HASH 0x06672AC5u

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

/* The shared left button state while the main mouse is a `mouse_t`, NULL on
 * the placeholder. */
static const mouse_button_t *main_mouse_left(void) {
    return main_mouse_typed() != NULL ? mouse_left_button() : NULL;
}

/* -1 absent, else the boolean. */
static int held_of(const mouse_button_t *b) {
    return b == NULL || !b->has_held ? -1 : b->held;
}

static int pressed_of(const mouse_button_t *b) {
    return b == NULL || !b->has_held ? -1 : b->pressed;
}

static int released_of(const mouse_button_t *b) {
    return b == NULL || !b->has_held ? -1 : b->released;
}

static int pos_of_main_mouse(vec_t *out) {
    const mouse_t *nf = main_mouse_typed();

    if (nf == NULL || !entity_has_pos(&nf->header)) {
        return 0;
    }
    *out = nf->header.pos;
    return 1;
}

static uint32_t fold_main_mouse(uint32_t h) {
    const mouse_button_t *left = main_mouse_left();
    vec_t ra;

    if (pos_of_main_mouse(&ra)) {
        h = fnv(h, (uint32_t)ra.x);
        h = fnv(h, (uint32_t)ra.y);
    } else {
        h = fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)held_of(left));
    h = fnv(h, (uint32_t)pressed_of(left));
    return fnv(h, (uint32_t)released_of(left));
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static p8_input_frame_t g_frames[64];

/* Points the dispatch input at the first frames of g_frames, positioned at
 * 0. */
static void script(int n) {
    input.frames = g_frames;
    input.frame_count = n;
    input.current_frame = 0;
    dispatch_context_set_input(&input);
}

static void unscript(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
}

static void frame(int i, int x, int y, int buttons) {
    memset(&g_frames[i], 0, sizeof g_frames[i]);
    g_frames[i].mouse_x = x;
    g_frames[i].mouse_y = y;
    g_frames[i].mouse_buttons = buttons;
}

/* `reset_world()` builds the main mouse: among the live
 * entities only, its position the zero default, the left button state present
 * and still empty. */
static void test_reset_world_builds_a_mouse_reader(void) {
    const void *je;
    vec_t ra;

    reset_world(NULL);
    je = main_mouse();
    assert(entity_list_listed(live_entities(), je) && !in_any_entities_by_message(je));
    assert(pos_of_main_mouse(&ra) && ra.x == 0 && ra.y == 0);
    assert(main_mouse_left() != NULL);
    assert(held_of(main_mouse_left()) == -1 && pressed_of(main_mouse_left()) == -1 && released_of(main_mouse_left()) == -1);
}

/* Left alone, then the other bits, which the left button ignores, and releases, moving each tick. */
static void test_scripted_ticks(void) {
    static const int buttons[SCRIPTED_TICKS] = {0, 1, 1, 0, 2, 2, 4, 0, 3, 1, 2, 0};
    uint32_t h = 2166136261u;
    int i;

    reset_world(NULL);
    for (i = 0; i < SCRIPTED_TICKS; i++) {
        frame(i, 7 + 9 * i, 120 - 5 * i, buttons[i]);
    }
    script(SCRIPTED_TICKS);
    for (i = 0; i < SCRIPTED_TICKS; i++) {
        update_entities();
        h = fold_main_mouse(h);
        if (i < SCRIPTED_TICKS - 1) {
            p8_input_step(&input);
        }
    }
    unscript();
    check("scripted", h, SCRIPTED_HASH);
}

/* Every mouse shares the left button state: a left button held across
 * `reset_world()` gives the new main mouse no fresh press. */
static void test_buttons_survive_reset_world(void) {
    const mouse_button_t *l_before;

    reset_world(NULL);
    frame(0, 40, 40, 1);
    frame(1, 41, 41, 1);
    frame(2, 42, 42, 1);
    script(3);
    update_entities();
    p8_input_step(&input);
    update_entities();
    assert(held_of(main_mouse_left()) == 1);
    l_before = main_mouse_left();

    reset_world(NULL);
    p8_input_step(&input);
    update_entities();
    unscript();
    assert(main_mouse_left() == l_before);
    assert(pressed_of(main_mouse_left()) == 0 && held_of(main_mouse_left()) == 1);
}

/* `mouse_init()` starts the left button state afresh. */
static void test_mouse_init_starts_the_tables_afresh(void) {
    reset_world(NULL);
    frame(0, 10, 10, 1);
    script(1);
    update_entities();
    unscript();
    assert(held_of(main_mouse_left()) == 1);

    mouse_init();
    reset_world(NULL);
    assert(held_of(main_mouse_left()) == -1);
}

/* After `ui_init()`, the main mouse is the member-less placeholder: no
 * buttons, no position, and a ui_update on it hands over no press. */
static void test_placeholder_main_mouse(void) {
    ui_t *ok;
    vec_t ra;

    reset_world(NULL);
    ui_init();
    assert(main_mouse_left() == NULL && !pos_of_main_mouse(&ra));
    ok = ui_new();
    ui_update(&memory, ok);
    assert(ui_left_held(ok) == NULL);
    assert(ok->has & UI_HAS_MOUSE_WORLD);
}

/* The title screen's main mouse, then cart_start_game starting a game with
 * the left button still held: the new main mouse sees it held. */
static void test_launch_with_left_held(void) {
    const void *first_main_mouse;

    cart_init();
    cart_flush_exit_queue();
    first_main_mouse = main_mouse();
    frame(0, 64, 64, 1);
    frame(1, 64, 64, 1);
    frame(2, 64, 64, 1);
    script(3);
    update_entities();
    p8_input_step(&input);
    cart_start_game(NULL);
    assert(main_mouse() != first_main_mouse);
    p8_input_step(&input);
    update_entities();
    unscript();
    assert(held_of(main_mouse_left()) == 1);
}

static int is_a_mouse_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == mouse_class();
}

/* Built by `mouse_new`, a mouse is a struct among the live entities only;
 * its update is its method, `draw` is absent, and every mouse shares the
 * class's left button state. */
static void test_mouse_is_a_struct_sharing_its_buttons(void) {
    void *built;
    void *generic;

    reset_world(NULL);
    built = mouse_new();
    generic = mouse_new();

    assert(is_a_mouse_struct(built) && is_a_mouse_struct(generic) && is_a_mouse_struct(main_mouse()));
    assert(entity_list_listed(live_entities(), built) && !in_any_entities_by_message(built));
    assert(entity_list_listed(live_entities(), generic) && !in_any_entities_by_message(generic));
    assert(((const entity_t *)generic)->class->states[ENTITY_STATE_NORMAL] != NULL);
    assert(((const entity_t *)generic)->class->draw == NULL);
    assert(mouse_left_button() != NULL);
    assert(entity_has_pos((const entity_t *)built));
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

    test_reset_world_builds_a_mouse_reader();
    test_scripted_ticks();
    test_buttons_survive_reset_world();
    test_mouse_init_starts_the_tables_afresh();
    test_placeholder_main_mouse();
    test_launch_with_left_held();
    test_mouse_is_a_struct_sharing_its_buttons();

    printf("test_mouse: all tests passed\n");
    return 0;
}
