/* The background `background_t`, read and written on its members and
 * driven through the real on_click call, update_entities, draw_layers and
 * `main_ui()`, in games built directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "engine/vec.h"
#include "game/wormhole.h"


#define MAP_HASH 0xBBDC5BFAu
#define MAP_STARS 4
#define MAP_ZEROS 625
#define SECOND_MAP_HASH 0x2E2CECCAu
#define LEFT_PRESS_PROBE_BEFORE 0
#define LEFT_PRESS_PROBE_AFTER 1
#define LEFT_PRESS_WORMHOLE_BEFORE 0
#define LEFT_PRESS_WORMHOLE_AFTER 1
#define HOVER_HASH 0x56A42FA7u
#define DRAW_HASH 0xA9BBBF46u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const int want_tile_choices[15] = {0, 0, 0, 72, 73, 74, 75, 88, 89, 90, 91, 88, 91, 89, 91};

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static uint32_t screen_hash(void) {
    uint32_t h = 2166136261u;
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            h ^= (uint32_t)memory.screen[y][x];
            h *= 16777619u;
        }
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

static int entity_list_list_count(const entity_list_t *list) {
    return list == NULL ? 0 : list->count;
}

static int is_state(const entity_t *ke, state_t want) {
    return ke->state == want;
}

/* 0 absent or false, 2 true. */
static uint32_t is_hovered_state(const background_t *bg) {
    return bg->header.is_hovered ? 2 : 0;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

static void check_int(const char *label, int got, int want) {
    printf("[%s] %d\n", label, got);
    fflush(stdout);
    assert(got == want);
}

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
}

/* The background is the one among the hit_test entities whose hit test and
 * layer are both 1. */
static background_t *find_background(int *count) {
    const entity_list_t *hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    background_t *found = NULL;
    int i;

    *count = 0;
    for (i = 0; hd != NULL && i < hd->count; i++) {
        const entity_t *e = hd->items[i];
        background_t *bg;

        if (e->class != background_class()) {
            continue;
        }
        bg = (background_t *)e;
        if (bg->archetype->hit_test == p8_fixed_from_int(1) && bg->archetype->layer == p8_fixed_from_int(1)) {
            found = bg;
            (*count)++;
        }
    }
    return found;
}

static background_t *the_background(void) {
    int count;
    background_t *bg = find_background(&count);
    assert(count == 1 && bg != NULL);
    return bg;
}

static vec_t point(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static int mget(int x, int y) {
    return memory.map_cells[y][x];
}

/* A fresh background: the entity's state and state time, the archetype's
 * layer, hit test and tile choices, the four methods, nothing written yet,
 * registered among the live entities, under draw and hit_test only. */
static void check_fresh_background(const background_t *bg) {
    const entity_class_t *cls = bg->header.class;
    const int_seq_t *pl;
    size_t i;

    assert(is_state(&bg->header, ENTITY_STATE_NORMAL));
    assert(bg->header.state_time == 0);
    assert(bg->archetype == &background_archetype);
    assert(bg->archetype->layer == p8_fixed_from_int(1));
    assert(bg->archetype->hit_test == p8_fixed_from_int(1));
    pl = bg->archetype->tile_choices;
    assert(pl == &background_tile_choices_seq && pl->count == 15);
    for (i = 0; i < 15; i++) {
        assert(pl->items[i] == want_tile_choices[i]);
    }
    assert(!bg->header.is_hovered);
    assert(cls->accept_drop == NULL && cls->states[ENTITY_STATE_NORMAL] == NULL);
    assert(cls->draw != NULL && cls->left_press != NULL && cls->drag == NULL);
    assert(cls->drop == NULL);
    assert(entity_list_listed(live_entities(), bg) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), bg) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), bg));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), bg) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), bg) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), bg));
}

static void test_construction(void) {
    background_t *from_start_game;
    background_t *built;
    background_t *generic;

    start_game();
    from_start_game = the_background();
    check_fresh_background(from_start_game);
    built = background_new();
    check_fresh_background(built);
    generic = background_new();
    check_fresh_background(generic);
    assert(from_start_game->archetype == built->archetype && built->archetype == generic->archetype);
}

static int in_tile_choices(int tile) {
    int i;
    for (i = 0; i < 15; i++) {
        if (want_tile_choices[i] == tile) {
            return 1;
        }
    }
    return 0;
}

static uint32_t map_hash(int *stars, int *zeros) {
    uint32_t h = 2166136261u;
    int x, y;

    *stars = 0;
    *zeros = 0;
    for (y = 0; y < 32; y++) {
        for (x = 0; x < 32; x++) {
            int tile = mget(x, y);
            assert(tile == 0 || in_tile_choices(tile) || (tile >= 122 && tile <= 126));
            *stars += tile >= 122 && tile <= 126;
            *zeros += tile == 0;
            h = fnv(h, (uint32_t)tile);
        }
    }
    return h;
}

static void test_map(void) {
    int stars;
    int zeros;
    uint32_t first;

    start_game();
    first = map_hash(&stars, &zeros);
    check("map", first, MAP_HASH);
    check_int("map_stars", stars, MAP_STARS);
    check_int("map_zeros", zeros, MAP_ZEROS);
    (void)background_new();
    check("second_map", map_hash(&stars, &zeros), SECOND_MAP_HASH);
}

/* The left press builds a probe while the probe tag is empty, a wormhole
 * after. */
static void test_left_press(void) {
    void *bg;
    down_t out;
    int probes_before;
    int wormhole_before;

    start_game();
    bg = the_background();
    probes_before = entity_list_list_count(entities_by_tag(TAG_PROBE));
    wormhole_before = entity_list_list_count(entities_by_tag(TAG_WORMHOLE));
    check_int("left_press_probe_before", probes_before, LEFT_PRESS_PROBE_BEFORE);
    check_int("left_press_wormhole_before", wormhole_before, LEFT_PRESS_WORMHOLE_BEFORE);

    out = background_class()->left_press(bg, mouse_pos(point(1, 1), point(2, 2)));
    assert(out.stop && entity_get(out.held) != NULL);
    assert(entity_get(out.held)->class == probe_class());
    assert(entity_list_listed(entities_by_tag(TAG_PROBE), entity_get(out.held)));

    out = background_class()->left_press(bg, mouse_pos(point(1, 1), point(2, 2)));
    assert(out.stop && entity_get(out.held) != NULL);
    assert(entity_get(out.held)->class == wormhole_class());
    assert(entity_list_listed(entities_by_tag(TAG_WORMHOLE), entity_get(out.held)));
    assert(is_state(&((const wormhole_t *)entity_get(out.held))->header, ENTITY_STATE_PLACING));

    check_int("left_press_probe_after", entity_list_list_count(entities_by_tag(TAG_PROBE)), LEFT_PRESS_PROBE_AFTER);
    check_int("left_press_wormhole_after", entity_list_list_count(entities_by_tag(TAG_WORMHOLE)), LEFT_PRESS_WORMHOLE_AFTER);
}

static int in_layer(int layer, const void *e) {
    return entity_id_list_has(ui_hovered_layer(main_ui(), layer), entity_id_of(e));
}

/* Nothing else hovered: the background is `hovered`, marked `is_hovered`,
 * in layer 1, and answers the left press; a veil built over it takes
 * `hovered`, and the next hover pass unmarks it through `previous_hovered`. */
static p8_input_frame_t g_void;

static void test_hover(void) {
    uint32_t h = 2166136261u;
    background_t *bg;
    void *veil;
    entity_t *kv = NULL;
    down_t kn;

    start_game();
    bg = the_background();
    memset(&g_void, 0, sizeof g_void);
    g_void.mouse_x = 64;
    g_void.mouse_y = 100;
    input.frames = &g_void;
    input.frame_count = 1;
    input.current_frame = 0;
    dispatch_context_set_input(&input);
    update_entities();

    ui_update(&memory, main_ui());
    assert((void *)ui_hovered(main_ui()) == bg);
    assert(is_hovered_state(bg) == 2);
    assert(in_layer(1, bg));
    h = fnv(h, is_hovered_state(bg));

    assert(ui_send_to_hovered(main_ui(), SEND_TO_HOVERED_LEFT_PRESS, mouse_pos(point(1, 1), point(2, 2)), NULL, &kv, &kn));
    assert((void *)kv == bg && kn.stop && entity_get(kn.held) != NULL && (void *)entity_get(kn.held) != bg);

    veil = (void *)modal_backdrop_new(NULL);
    ui_update(&memory, main_ui());
    assert((void *)ui_hovered(main_ui()) == veil);
    assert(is_hovered_state(bg) == 0);
    h = fnv(h, is_hovered_state(bg));
    check("hover", h, HOVER_HASH);
}

static planet_t *first_planet_with_state(state_t state) {
    const entity_list_t *list = entities_by_tag(TAG_PLANET);
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        entity_t *e = list->items[i];

        assert(planet_is_family(e));
        if (is_state(&((const planet_t *)e)->header, state)) {
            return (planet_t *)e;
        }
    }
    return NULL;
}

/* Released over a colonized planet or a slipgate, accept_drop with the
 * background held reads a start planet off it, which it never has. */
static void test_accept_drop(void) {
    background_t *bg;
    planet_t *ly;
    void *nr;
    planet_keys_t keys = {0};
    down_t r;

    start_game();
    planet_apply_project((planet_t *)entities_by_tag(TAG_PLANET)->items[0], research_project_new(NULL));
    bg = the_background();
    ly = first_planet_with_state(ENTITY_STATE_SETTLED);
    assert(ly != NULL);
    r = planet_accept_drop(ly, &bg->header);
    assert(r.stop && entity_get(r.held) == NULL);
    assert(is_state(&ly->header, ENTITY_STATE_SETTLED));

    keys.map_pos = ly->header.map_pos;
    keys.has_map_pos = entity_has_map_pos(&ly->header);
    keys.pos = ly->header.pos;
    keys.has_pos = entity_has_pos(&ly->header);
    nr = slipgate_new(&keys);
    r = slipgate_accept_drop(nr, &bg->header);
    assert(r.stop && entity_get(r.held) == NULL);
}

/* The background alone under draw, drawn by draw_layers with the main
 * camera at its initial position. */
static void test_draw(void) {
    void *bg;

    reset_world(NULL);
    bg = (void *)background_new();
    assert(entity_list_list_count(entities_by_message(ENTITY_MESSAGE_DRAW)) == 1 && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), bg));
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    assert(main_camera()->class == camera_class());
    camera_apply(dispatch_context_gfx(), (camera_t *)main_camera(), p8_fixed_from_int(1));
    draw_layers(ENTITY_MESSAGE_DRAW);
    check("draw", screen_hash(), DRAW_HASH);
}

enum { HELD_ABSENT, HELD_PROBE_STRUCT, HELD_WORMHOLE_STRUCT, HELD_OTHER };

static uint32_t left_held_kind(void) {
    const void *held = ui_left_held(main_ui());

    if (held == NULL) {
        return HELD_ABSENT;
    }
    if (held != NULL &&
        ((const entity_t *)held)->class == probe_class()) {
        return HELD_PROBE_STRUCT;
    }
    if (held != NULL &&
        ((const entity_t *)held)->class == wormhole_class()) {
        return HELD_WORMHOLE_STRUCT;
    }
    return HELD_OTHER;
}

static p8_input_frame_t g_drag[8];

static void drag_frame(int i, int x, int y, int buttons) {
    memset(&g_drag[i], 0, sizeof g_drag[i]);
    g_drag[i].mouse_x = x;
    g_drag[i].mouse_y = y;
    g_drag[i].mouse_buttons = buttons;
}

/* Over empty space, the left button pressed holds the probe the
 * background's left press builds, and the drag keeps it held. */
static void test_scripted_drags(void) {
    background_t *bg;
    int i;

    start_game();
    bg = the_background();
    drag_frame(0, 64, 100, 0);
    drag_frame(1, 64, 100, 1);
    drag_frame(2, 70, 96, 1);
    input.frames = g_drag;
    input.frame_count = 3;
    input.current_frame = 0;
    dispatch_context_set_input(&input);
    for (i = 0; i < 3; i++) {
        int count;

        cart_update60();
        cart_draw();
        assert(find_background(&count) == bg && count == 1);
        assert((left_held_kind() == HELD_PROBE_STRUCT) == (i > 0));
        if (i < 2) {
            p8_input_step(&input);
        }
    }
    p8_input_init(&input);
    dispatch_context_set_input(&input);
}

static int is_a_background_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == background_class();
}

/* Built by `cart_start_game()` or `background_new`, a background is a
 * struct with draw and left press as methods. */
static void test_background_is_a_struct(void) {
    const entity_class_t *cls;
    void *from_start_game;
    void *built;
    void *generic;

    start_game();
    from_start_game = the_background();
    built = (void *)background_new();
    generic = (void *)background_new();
    assert(is_a_background_struct(from_start_game) && is_a_background_struct(built) && is_a_background_struct(generic));
    cls = ((const entity_t *)built)->class;
    assert(cls->draw != NULL && cls->left_press != NULL && cls->drag == NULL);
    assert(cls->states[ENTITY_STATE_NORMAL] == NULL && cls->accept_drop == NULL && cls->draw_overlay == NULL);
    assert(cls->drop == NULL && cls->draw_at == NULL);
}

/* A scenario depends on the rnd state: run each in a child forked right
 * after cart_load(), as a fresh process would start it. */
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

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    in_fresh_child(test_construction);
    in_fresh_child(test_map);
    in_fresh_child(test_left_press);
    in_fresh_child(test_hover);
    in_fresh_child(test_accept_drop);
    in_fresh_child(test_draw);
    in_fresh_child(test_scripted_drags);
    in_fresh_child(test_background_is_a_struct);

    printf("test_background: all tests passed\n");
    return 0;
}
