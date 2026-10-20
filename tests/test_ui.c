/* The click dispatcher `ui_t`, the singleton `main_ui()`, built by its only
 * road (`reset_world()`, `ui_new()`, on a game started directly: no events
 * file), compared with the archetype `ui_archetype`, and driven through the real update_entities,
 * ui_update, draw_overlay, ui_send_to_hovered, ui_clear_ephemerals and the
 * opening of a button list. */

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
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/frame_zone.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "p8/vram.h"
#include "game/menu.h"
#include "game/mouse.h"
#include "game/message_box.h"
#include "game/particle.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/score_panel.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/ui_zone.h"
#include "engine/vec.h"
#include "game/wormhole.h"

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


/* Ticks after the slipway is paid, long enough for the colonizing and
 * paying effects to leave the live entities: update_entities skips the
 * entity after one it takes out. */
#define SETTLE_TICKS 120

#define BUILT_HASH 0x86CE257Cu
#define SHARED_HASH 0x690E4805u
#define FRESH_HASH 0x8669AF3Au
#define HOVER_HASH 0x5F61B759u
#define LAYERS_HASH 0x2C9F6F7Bu
#define LEFT_HASH 0xCD10022Au
#define CLEAR_EPHEMERALS_HASH 0xD215FB81u
#define STALE_HASH 0x5B066E86u
#define ENTITY_STATE_HASH 0xD49B8748u
#define ENTITY_STATE_TIME_HASH 0xDE09A985u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t not_listed[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_PLANET, TAG_PROBE, TAG_STRUCTURE, TAG_SLIPWAY, TAG_WORMHOLE, TAG_MESSAGE_BOX, TAG_NODE, TAG_ASCENSION, TAG_SLIPGATE};

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static int entity_list_rank_in(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return i;
        }
    }
    return -1;
}

static int entity_list_listed(const entity_list_t *list, const void *e) {
    return entity_list_rank_in(list, e) >= 0;
}

static const entity_id_list_t *ephemerals_of(const ui_t *ok) {
    return &ok->ephemerals;
}

static int owns_ephemerals(const ui_t *ok) {
    return (ok->has & UI_HAS_OWN_EPHEMERALS) != 0;
}

static vec_t vec_at(const vec_t *p) {
    assert(p != NULL);
    return *p;
}

static const mouse_t *checked_main_mouse(void) {
    const mouse_t *je = main_mouse_typed();
    assert(je != NULL);
    return je;
}

static const camera_t *checked_main_camera(void) {
    const camera_t *mo = main_camera_typed();
    assert(mo != NULL);
    return mo;
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

/* 0 absent or false, 2 true. */
static uint32_t is_hovered_state(const planet_t *jo) {
    return jo->header.is_hovered ? 2 : 0;
}

static uint32_t fold_vec(uint32_t h, vec_t v) {
    return fnv(fnv(h, (uint32_t)v.x), (uint32_t)v.y);
}

/* Presence, then the rank of an entity among the live entities. */
static uint32_t fold_entity(uint32_t h, const void *ref) {
    h = fnv(h, (uint32_t)(ref != NULL));
    return fnv(h, (uint32_t)entity_list_rank_in(live_entities(), ref));
}

static int layer_count(const ui_t *ok, int mf) {
    return ui_hovered_layer(ok, mf)->count;
}

static int in_layer(const ui_t *ok, int mf, const void *e) {
    return entity_id_list_has(ui_hovered_layer(ok, mf), entity_id_of(e));
}

static uint32_t fold_layers(uint32_t h, const ui_t *ok) {
    int mf;
    for (mf = 0; mf <= 15; mf++) {
        h = fnv(h, (uint32_t)layer_count(ok, mf));
    }
    return h;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
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

/* Draws the overlay alone on a cleared screen. */
static void draw_overlay_alone(void) {
    p8_cls(&memory);
    ui_draw_overlay(&graphics, &memory, main_ui());
}

static void assert_overlay_draws_nothing(void) {
    draw_overlay_alone();
    uint32_t drawn = screen_hash();
    p8_cls(&memory);
    assert(drawn == screen_hash());
}

static int is_of(const void *e, const entity_class_t *cls) {
    return e != NULL && ((const entity_t *)e)->class == cls;
}

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static ui_t *start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
    cart_flush_exit_queue();
    assert(main_ui() != NULL);
    return main_ui();
}

static vec_t pos_of(void *e) {
    return vec_at(map_pos_ref(&as_planet(e)->header));
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

static int placed(void *e) {
    return map_pos_ref(&as_planet(e)->header) != NULL;
}

/* The planet nearest to the given one, among those with a `map_pos`. */
static void *nearest_to(void *qu) {
    const entity_list_t *jos = entities_by_tag(TAG_PLANET);
    void *re = NULL;
    int i;

    for (i = 0; i < jos->count; i++) {
        void *e = jos->items[i];
        if (e != qu && placed(e) && (re == NULL || dist2(pos_of(e), pos_of(qu)) < dist2(pos_of(re), pos_of(qu)))) {
            re = e;
        }
    }
    assert(re != NULL);
    return re;
}

static p8_input_frame_t g_mouse;

/* One frame's mouse, read by the main mouse through update_entities. */
static void mouse_at(int x, int y, int buttons) {
    memset(&g_mouse, 0, sizeof g_mouse);
    g_mouse.mouse_x = x;
    g_mouse.mouse_y = y;
    g_mouse.mouse_buttons = buttons;
    input.frames = &g_mouse;
    input.frame_count = 1;
    input.current_frame = 0;
    dispatch_context_set_input(&input);
    update_entities();
}

static void *the_background(void) {
    const entity_list_t *hv = live_entities();
    void *bg = NULL;
    int i;
    for (i = 0; i < hv->count; i++) {
        if (is_of(hv->items[i], background_class())) {
            assert(bg == NULL);
            bg = hv->items[i];
        }
    }
    assert(bg != NULL);
    return bg;
}

/* The game's `main_ui()` as `reset_world()` and entity_register left it:
 * where it registered, its defaults, the names only read elsewhere, its one
 * method, an empty ephemerals list it does not own, and the class's panel
 * box by identity. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    ui_t *ok = start_game();
    size_t index;

    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), ok));
    hash = fnv(hash, (uint32_t)live_entities()->count);
    assert(entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), ok));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), ok));
    for (index = 0; index < sizeof not_listed / sizeof not_listed[0]; index++) {
        assert(!entity_list_listed(entities_by_message(not_listed[index]), ok));
    }
    for (index = 0; index < sizeof tags / sizeof tags[0]; index++) {
        assert(!entity_list_listed(entities_by_tag(tags[index]), ok));
    }

    assert(ok->archetype->layer == p8_fixed_from_int(15));
    assert(ok->header.state == ENTITY_STATE_NORMAL);
    /* The presence word is constant by the struct's shape, kept so that
     * BUILT_HASH still proves the typed read. */
    hash = fnv(fnv(hash, 1u), (uint32_t)ok->header.state_time);
    assert(!(ok->has & UI_HAS_MOUSE_WORLD) && !(ok->has & UI_HAS_HOVERED_BY_LAYER) && ui_hovered(ok) == NULL && ui_previous_hovered(ok) == NULL && ui_left_held(ok) == NULL);
    assert(ui_class()->draw_overlay != NULL);
    assert(ui_class()->states[ENTITY_STATE_NORMAL] == NULL && ui_class()->left_press == NULL &&
           ui_class()->hit_test == NULL);

    assert(!owns_ephemerals(ok));
    assert(ok->archetype == &ui_archetype);
    assert(ok->archetype->panel_box.left == p8_fixed_from_int(10) && ok->archetype->panel_box.top == p8_fixed_from_int(10));
    assert(ok->archetype->panel_box.right == p8_fixed_from_int(P8_SCREEN_WIDTH - 10) &&
           ok->archetype->panel_box.bottom == p8_fixed_from_int(P8_SCREEN_HEIGHT - 10));
    assert(ephemerals_of(ok)->count == 0);
    check("built", hash, BUILT_HASH);
}

static void call_galaxy_menu_on_click(int i) {
    galaxy_menu_on_click(i)(NULL);
}

/* A second `ui_t` starts on an empty list it does not own until its own
 * ui_clear_ephemerals; a button list replaces those of `main_ui()`, and a
 * help screen empties them. */
static void test_shared(void) {
    uint32_t h = 2166136261u;
    ui_t *ok = start_game();
    ui_t *other = ui_new();

    assert(other != NULL && other != ok);
    assert(!owns_ephemerals(other) && !owns_ephemerals(ok) && ephemerals_of(other)->count == 0 && ephemerals_of(ok)->count == 0);
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), other));
    h = fnv(h, (uint32_t)entity_list_rank_in(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), other));

    ui_clear_ephemerals(other);
    assert(owns_ephemerals(other) && ephemerals_of(other)->count == 0);
    assert(!owns_ephemerals(ok) && ephemerals_of(ok)->count == 0);

    call_galaxy_menu_on_click(4);
    assert(owns_ephemerals(ok) && other->ephemerals.zone != ok->ephemerals.zone);
    h = fnv(h, (uint32_t)ephemerals_of(ok)->count);
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), entity_id_list_at(ephemerals_of(ok), 0)));
    assert(ephemerals_of(other)->count == 0);

    (void)help_screen_new();
    assert(ephemerals_of(ok)->count == 0 && owns_ephemerals(ok));
    h = fnv(h, (uint32_t)live_entities()->count);
    check("shared", h, SHARED_HASH);
}

/* A fresh game's first ui_update: `mouse_world` is the mouse position plus
 * the camera position, `hovered_by_layer` built, `previous_hovered` is
 * `hovered`, and the overlay draws nothing. */
static void test_fresh(void) {
    uint32_t h = 2166136261u;
    ui_t *ok = start_game();
    vec_t want;

    mouse_at(80, 96, 0);
    ui_update(&memory, main_ui());
    want = vec_add(vec_at(pos_ref(&checked_main_mouse()->header)), vec_at(pos_ref(&checked_main_camera()->header)));
    assert((ok->has & UI_HAS_MOUSE_WORLD) && ok->mouse_world.x == want.x && ok->mouse_world.y == want.y);
    assert((ok->has & UI_HAS_HOVERED_BY_LAYER) && ui_hovered(ok) != NULL);
    assert(ui_previous_hovered(ok) == ui_hovered(ok));
    assert((void *)ui_hovered(ok) == the_background());
    h = fold_vec(h, ok->mouse_world);
    h = fold_layers(h, ok);
    h = fold_entity(h, ui_hovered(ok));
    assert_overlay_draws_nothing();
    assert(!owns_ephemerals(ok));
    check("fresh", h, FRESH_HASH);
}

/* The first planet whose screen position is well inside. */
static planet_t *planet_on_screen(int *sx, int *sy) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    vec_t cam = vec_at(pos_ref(&checked_main_camera()->header));
    int i;

    for (i = 0; jo != NULL && i < jo->count; i++) {
        vec_t at = vec_at(pos_ref(&as_planet(jo->items[i])->header));
        int x = p8_fixed_flr_int(p8_fixed_sub(at.x, cam.x));
        int y = p8_fixed_flr_int(p8_fixed_sub(at.y, cam.y));
        if (x >= 32 && x <= 128 && y >= 12 && y <= 108) {
            *sx = x;
            *sy = y;
            return (planet_t *)jo->items[i];
        }
    }
    assert(0 && "no planet on screen");
    return NULL;
}

/* The mouse over a planet: `hovered` is the planet, marked `is_hovered`,
 * `previous_hovered` on the next pass; the mouse away unmarks it. */
static void test_hover(void) {
    uint32_t h = 2166136261u;
    ui_t *ok = start_game();
    int x;
    int y;
    planet_t *planet;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    planet = planet_on_screen(&x, &y);
    mouse_at(x, y, 0);
    ui_update(&memory, ok);
    assert((void *)ui_hovered(ok) == planet);
    assert(is_hovered_state(planet) == 2);
    assert(in_layer(ok, 6, planet) && in_layer(ok, 1, the_background()));
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), planet));
    h = fold_layers(h, ok);

    ui_update(&memory, ok);
    assert((void *)ui_previous_hovered(ok) == planet && (void *)ui_hovered(ok) == planet);
    assert(is_hovered_state(planet) == 2);
    assert_overlay_draws_nothing();

    mouse_at(x + 40 > 143 ? x - 40 : x + 40, y, 0);
    ui_update(&memory, ok);
    assert((void *)ui_hovered(ok) != planet);
    assert(is_hovered_state(planet) == 0);
    h = fold_entity(h, ui_hovered(ok));
    check("hover", h, HOVER_HASH);
}

/* A layer the previous pass wrote and this pass does not reads empty. */
static void test_unwritten_layer_reads_empty(void) {
    ui_t *ok = start_game();
    planet_t *planet;
    int x;
    int y;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    planet = planet_on_screen(&x, &y);
    mouse_at(x, y, 0);
    ui_update(&memory, ok);
    assert(layer_count(ok, 6) == 1 && in_layer(ok, 6, planet));

    mouse_at(x + 40 > 143 ? x - 40 : x + 40, y, 0);
    ui_update(&memory, ok);
    assert(!in_layer(ok, 6, planet));
    assert(layer_count(ok, 1) == 1 && in_layer(ok, 1, the_background()));
}

/* ui_send_to_hovered walks the layers from 15 down: over a planet, the left
 * press answers from the planet (layer 6) before the background (layer 1). */
static void test_layers(void) {
    uint32_t h = 2166136261u;
    ui_t *ok = start_game();
    entity_t *kv = NULL;
    down_t kn;
    mouse_pos_t mouse;
    planet_t *planet;
    int x;
    int y;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    planet = planet_on_screen(&x, &y);
    mouse_at(x, y, 0);
    ui_update(&memory, ok);
    assert(ok->has & UI_HAS_MOUSE_WORLD);
    mouse = mouse_pos(ok->mouse_world, main_mouse_typed()->header.pos);

    entity_set_state(&planet->header, ENTITY_STATE_KNOWN);
    assert(ui_send_to_hovered(main_ui(), SEND_TO_HOVERED_LEFT_PRESS, mouse, NULL, &kv, &kn));
    assert((void *)kv == planet && kn.stop && entity_get(kn.held) == NULL);
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), kv));
    check("layers", h, LAYERS_HASH);
}

static void test_held_links_read_null_after_the_flush(void) {
    ui_t *ok = start_game();
    entity_t *probe;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    mouse_at(80, 96, 1);
    ui_update(&memory, ok);
    probe = ui_left_held(ok);
    assert(is_of(probe, probe_class()));
    entity_unregister(probe);
    assert(ui_left_held(ok) == probe);
    cart_flush_exit_queue();
    assert(ui_left_held(ok) == NULL);
}

static int slipways_count(void *planet) {
    const entity_id_list_t *oe = &as_planet(planet)->slipways;
    return oe == NULL ? 0 : oe->count;
}

/* Where the entity is on screen under the main camera. */
static void screen_of(void *e, int *x, int *y) {
    vec_t at = vec_sub(vec_at(pos_ref(&as_planet(e)->header)), vec_at(pos_ref(&checked_main_camera()->header)));

    *x = p8_fixed_flr_int(at.x);
    *y = p8_fixed_flr_int(at.y);
}

static int on_screen(void *e) {
    int x;
    int y;

    screen_of(e, &x, &y);
    return x >= 24 && x < 136 && y >= 4 && y < 116;
}

/* A started game's first planet on screen whose nearest one is on screen
 * too, both colonized by a bare project, then the mouse by hand: the left
 * button on the one holds a new slipway in `left_held`; released over the
 * other, the drop gets that planet and `left_held` goes. */
static void test_left(void) {
    uint32_t hash = 2166136261u;
    const entity_list_t *planets;
    ui_t *dispatcher;
    void *held;
    void *start_planet = NULL;
    void *target;
    void *other = NULL;
    int pixel_x;
    int pixel_y;
    int index;

    dispatcher = start_game();
    planets = entities_by_tag(TAG_PLANET);
    for (index = 0; index < planets->count && other == NULL; index++) {
        void *e = planets->items[index];
        if (on_screen(e) && on_screen(nearest_to(e))) {
            start_planet = e;
            other = nearest_to(e);
        }
    }
    assert(other != NULL);
    planet_apply_project(start_planet, research_project_new(NULL));
    planet_apply_project(other, research_project_new(NULL));
    for (index = 0; index < SETTLE_TICKS; index++) {
        update_entities();
    }

    screen_of(start_planet, &pixel_x, &pixel_y);
    mouse_at(pixel_x, pixel_y, 0);
    ui_update(&memory, dispatcher);
    mouse_at(pixel_x, pixel_y, 1);
    ui_update(&memory, dispatcher);
    held = ui_left_held(dispatcher);
    assert(held != NULL && is_of(held, slipway_class()));
    assert((void *)slipway_start(((const slipway_t *)held)) == start_planet && slipways_count(start_planet) == 0);
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), held));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), start_planet));
    assert_overlay_draws_nothing();

    screen_of(other, &pixel_x, &pixel_y);
    mouse_at(pixel_x, pixel_y, 1);
    ui_update(&memory, dispatcher);
    assert((void *)ui_left_held(dispatcher) == held);
    target = ui_hovered(dispatcher);
    assert(target == other);
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), target));

    mouse_at(pixel_x, pixel_y, 0);
    ui_update(&memory, dispatcher);
    assert(ui_left_held(dispatcher) == NULL);
    assert(slipways_count(start_planet) == 1 && slipways_count(target) == 1);
    hash = fnv(hash, (uint32_t)entities_by_tag(TAG_SLIPWAY)->count);
    check("left", hash, LEFT_HASH);
}

/* ui_clear_ephemerals unregisters every entity of the old list and starts
 * a new one. */
static void test_clear_ephemerals(void) {
    uint32_t h = 2166136261u;
    ui_t *ok = start_game();
    entity_id_list_t before;
    const entity_id_list_t *le = &before;
    entity_t *buttons[16];
    uint16_t serial;
    int count;
    int i;

    call_galaxy_menu_on_click(4);
    before = ok->ephemerals;
    serial = ok->ephemerals_serial;
    count = le->count;
    assert(count > 0 && count <= 16);
    for (i = 0; i < count; i++) {
        buttons[i] = entity_id_list_at(le, i);
        assert(entity_list_listed(live_entities(), buttons[i]));
        h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), buttons[i]));
    }
    h = fnv(h, (uint32_t)live_entities()->count);
    ui_clear_ephemerals(main_ui());
    assert(ok->ephemerals_serial != serial && ephemerals_of(ok)->count == 0 && owns_ephemerals(ok));
    assert(ok->ephemerals.zone != before.zone);
    for (i = 0; i < count; i++) {
        assert(!entity_list_listed(live_entities(), buttons[i]));
    }
    h = fnv(h, (uint32_t)live_entities()->count);
    check("clear_ephemerals", h, CLEAR_EPHEMERALS_HASH);
}

/* The pause menu's list of 4 buttons takes its 8 places at creation: its
 * slot holds 128 bytes (arena header 96, 8 ids x 4, x86-64), no block
 * left dead by a growth; its 4 elements are recorded when the ui gets it. */
static void test_ephemerals_reserve_at_creation(void) {
    ui_t *ok = start_game();

    call_galaxy_menu_on_click(4);
    assert(ephemerals_of(ok)->count == 4);
    assert(ephemerals_of(ok)->capacity == 8);
    assert(ui_zone_max_bytes(SLOT_KIND_EPHEMERALS) == 128);
    assert(ui_zone_max_ephemerals() == 4);
}

/* Eight elements fit the reserve: neither the list nor its slot grows. */
static void test_ephemerals_eight_fit_the_reserve(void) {
    entity_id_list_t le;
    int i;

    (void)start_game();
    le = ui_ephemerals_new();
    for (i = 0; i < 8; i++) {
        entity_id_list_add(&le, (entity_id_t){(uint16_t)(i + 1), 1});
    }
    assert(le.count == 8 && le.capacity == 8);
    assert(ui_zone_max_bytes(SLOT_KIND_EPHEMERALS) == 128);
}

/* On the title screen, a click on a difficulty button only requests the
 * game from inside ui_update, which ends on the title's ui. The generation
 * frame's update runs `cart_start_game()` outside ui_update: `reset_world()`
 * rebuilds `main_ui()`, and the old one's ephemerals are cleared and its
 * links, of the old world, read NULL. */
static void test_stale(void) {
    uint32_t h = 2166136261u;
    ui_t *old;
    void *button;
    uint16_t serial;

    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    old = main_ui();
    mouse_at(76, 71, 0);
    ui_update(&memory, old);
    button = ui_hovered(old);
    serial = old->ephemerals_serial;
    assert(button != NULL && is_of(button, button_class()) && ephemerals_of(old)->count == 0);
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), button));

    mouse_at(76, 71, 1);
    ui_update(&memory, old);
    assert(main_ui() == old && cart_loading() && "the click only requests the game");
    assert(old->ephemerals_serial != serial && ephemerals_of(old)->count == 0);
    cart_update60();
    assert(main_ui() == old && cart_loading());
    cart_update60();
    assert(!cart_loading() && main_ui() != old && main_ui() != NULL);
    assert(ui_previous_hovered(old) == NULL && ui_hovered(old) == NULL);
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), ui_previous_hovered(old)));
    assert(ephemerals_of(old)->count == 0);
    assert(ui_previous_hovered(main_ui()) == NULL && !(main_ui()->has & UI_HAS_MOUSE_WORLD));
    h = fnv(h, (uint32_t)ephemerals_of(main_ui())->count);
    h = fnv(h, (uint32_t)!owns_ephemerals(main_ui()));
    h = fnv(h, (uint32_t)entity_list_rank_in(live_entities(), main_ui()));
    h = fnv(h, (uint32_t)live_entities()->count);
    check("stale", h, STALE_HASH);
}

static void test_reset_world_drops_the_old_world_ids(void) {
    ui_t *old = start_game();
    entity_t *kv = NULL;
    down_t kn;
    int mf;

    call_galaxy_menu_on_click(4);
    mouse_at(80, 96, 0);
    ui_update(&memory, old);
    assert(ephemerals_of(old)->count == 4 && layer_count(old, 1) == 1);
    reset_world(NULL);
    assert(main_ui() != old && ephemerals_of(old)->count == 0);
    for (mf = 0; mf < UI_HOVER_LAYERS; mf++) {
        assert(layer_count(old, mf) == 0);
    }
    assert(!ui_send_to_hovered(old, SEND_TO_HOVERED_LEFT_PRESS, (mouse_pos_t){0}, NULL, &kv, &kn));
    ui_clear_ephemerals(old);
    assert(ephemerals_of(old)->count == 0);
}

/* Releasing a ui gives back the slot its ephemerals live in. */
static void test_release_frees_the_owned_slot(void) {
    ui_t *ok = start_game();
    int live;

    call_galaxy_menu_on_click(4);
    cart_flush_exit_queue();
    live = ui_zone_live(SLOT_KIND_EPHEMERALS);
    assert(owns_ephemerals(ok) && live >= 1);
    ui_class()->release(&ok->header);
    assert(ui_zone_live(SLOT_KIND_EPHEMERALS) == live - 1 && !owns_ephemerals(ok) && ephemerals_of(ok)->count == 0);
}

static void must_abort(void (*write)(void *), void *e) {
    pid_t child;
    int status;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        write(e);
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

/* The game's `main_ui()` is a struct whose class holds only draw_overlay;
 * it takes no construction key. Every new `ui_t` starts on the class's
 * archetype and an empty ephemerals list it does not own. */
static void test_ui_is_a_struct(void) {
    const entity_class_t *cls = ui_class();
    ui_t *ok = start_game();
    ui_t *other;
    int j;

    assert(is_of(ok, cls));
    assert(cls->draw_overlay != NULL && cls->draw == NULL && cls->hit_test == NULL && cls->new_year == NULL && cls->refresh == NULL &&
           cls->left_press == NULL && cls->accept_drop == NULL && cls->drag == NULL && cls->drop == NULL &&
           cls->attach_slipway == NULL && cls->offer_trade == NULL && cls->draw_at == NULL && cls->spawn_particle == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == NULL);
    }
    assert(ui_ready());
    assert(ok->archetype == &ui_archetype);
    assert(ui_panel_box() == &ok->archetype->panel_box);
    assert(!owns_ephemerals(ok) && ephemerals_of(ok)->count == 0);
    other = ui_new();
    assert(is_of(other, cls) && !owns_ephemerals(other) && ephemerals_of(other)->count == 0 && other->archetype == ok->archetype);
}

typedef struct {
    entity_t header;
} giver_t;

static entity_t *g_given;
static int g_giver_hovered = 1;

static down_t giver_down(entity_t *self, mouse_pos_t mouse) {
    down_t out;

    (void)self;
    (void)mouse;
    out.stop = g_given != NULL;
    out.held = entity_id_link(g_given);
    return out;
}

static mouse_pos_t g_giver_mouse;

static int giver_hit_test(entity_t *self, mouse_pos_t mouse) {
    (void)self;
    g_giver_mouse = mouse;
    return g_giver_hovered;
}

static const entity_class_t giver_class_def = {
    .hit_test = giver_hit_test,
    .left_press = giver_down,
};

static giver_t g_giver = {{.class = &giver_class_def, .has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_HOVERED, .layer = 0xf0000}};

static void press(void *ok, int buttons) {
    mouse_at(64, 100, 0);
    ui_update(&memory, ok);
    mouse_at(64, 100, buttons);
    ui_update(&memory, ok);
}

static void hold_given(void *ok) {
    press(ok, 1);
}

static void test_ui_held_aborts_on_other_classes(void) {
    ui_t *ok = start_game();
    void *left[8];
    size_t i;

    entity_construct(&g_giver.header);
    left[0] = protostar_new(NULL);
    left[1] = burst_new(NULL);
    left[2] = entities_by_tag(TAG_PLANET)->items[0];
    left[3] = the_background();
    left[4] = help_screen_new();
    left[5] = button_new(NULL);
    left[6] = structure_icon_new(NULL);
    left[7] = wormhole_too_far_target_wrapper();

    for (i = 0; i < 8; i++) {
        g_given = left[i];
        must_abort(hold_given, ok);
    }
}

static void set_left_held_to_a_planet(void *ok) {
    ui_set_left_held(ok, entities_by_tag(TAG_PLANET)->items[0]);
}

static void test_set_left_held_sets_and_clears(void) {
    ui_t *ok = start_game();
    planet_t *structure = structure_new(NULL);

    ui_set_left_held(ok, &structure->header);
    assert(ui_left_held(ok) == &structure->header);
    ui_set_left_held(ok, NULL);
    assert(ui_left_held(ok) == NULL);
    must_abort(set_left_held_to_a_planet, ok);
}

static int same_point(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}

/* The hover hands each hit test the ui's world mouse and the main mouse's
 * raw header position, each with its presence bit, even when the mouse
 * header has lost its position bit. */
static void test_hover_hands_the_mouse_by_value(void) {
    ui_t *ok = start_game();
    mouse_t *mouse;

    entity_construct(&g_giver.header);
    g_given = NULL;
    mouse_at(64, 100, 0);
    ui_update(&memory, ok);
    mouse = main_mouse_typed();
    assert(mouse != NULL && (ok->has & UI_HAS_MOUSE_WORLD));
    assert(mouse_pos_has_world(g_giver_mouse) && same_point(g_giver_mouse.world, ok->mouse_world));
    assert(mouse_pos_has_screen(g_giver_mouse) && same_point(g_giver_mouse.screen, mouse->header.pos));

    entity_clear_pos(&mouse->header);
    ui_update_hover(ok);
    assert(mouse_pos_has_screen(g_giver_mouse) && same_point(g_giver_mouse.screen, mouse->header.pos));
    assert(!same_point(g_giver_mouse.screen, vec_make(0, 0)));
}

/* The hover a test walk expects: every entity of the hit-test bucket, in
 * order, through its class's hit test. */
typedef struct {
    const entity_t *items[UI_HOVER_LAYERS][64];
    int counts[UI_HOVER_LAYERS];
    const entity_t *hovered;
} hover_walk_t;

static hover_walk_t walk_every_hit_test(mouse_pos_t mouse) {
    const entity_list_t *bucket = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    hover_walk_t out;
    int i;

    memset(&out, 0, sizeof out);
    for (i = 0; i < bucket->count; i++) {
        entity_t *e = bucket->items[i];
        if (e->class->hit_test != NULL && e->class->hit_test(e, mouse)) {
            if (entity_has(e, ENTITY_HAS_LAYER)) {
                int layer = p8_fixed_flr_int(e->layer);
                assert(out.counts[layer] < 64);
                out.items[layer][out.counts[layer]++] = e;
            }
            out.hovered = e;
        }
    }
    return out;
}

static void assert_hover_at(ui_t *ok, vec_t world, vec_t screen) {
    mouse_t *mouse = main_mouse_typed();
    hover_walk_t want;
    frame_zone_mark_t mark = frame_zone_mark();
    int layer;
    int i;

    entity_set_pos(&mouse->header, screen);
    ok->mouse_world = world;
    ok->has |= UI_HAS_MOUSE_WORLD;
    want = walk_every_hit_test(mouse_pos(world, screen));
    ui_update_hover(ok);
    assert((const entity_t *)ui_hovered(ok) == want.hovered);
    assert(want.hovered == NULL || want.hovered->is_hovered);
    for (layer = 0; layer < UI_HOVER_LAYERS; layer++) {
        assert(layer_count(ok, layer) == want.counts[layer]);
        for (i = 0; i < want.counts[layer]; i++) {
            assert((const entity_t *)entity_id_list_at(ui_hovered_layer(ok, layer), i) == want.items[layer][i]);
        }
    }
    frame_zone_rewind(mark);
}

/* Raw offsets around each box edge, so the inclusive bounds are crossed. */
static void assert_hover_around(ui_t *ok, const entity_t *e, int on_screen) {
    vec_t base = e->pos;
    p8_fixed_t xs[7];
    p8_fixed_t ys[7];
    vec_t cam = checked_main_camera()->header.pos;
    int i;
    int j;

    xs[0] = e->hit_box.left - 1;
    xs[1] = e->hit_box.left;
    xs[2] = e->hit_box.left + 1;
    xs[3] = (e->hit_box.left + e->hit_box.right) / 2;
    xs[4] = e->hit_box.right - 1;
    xs[5] = e->hit_box.right;
    xs[6] = e->hit_box.right + 1;
    ys[0] = e->hit_box.top - 1;
    ys[1] = e->hit_box.top;
    ys[2] = e->hit_box.top + 1;
    ys[3] = (e->hit_box.top + e->hit_box.bottom) / 2;
    ys[4] = e->hit_box.bottom - 1;
    ys[5] = e->hit_box.bottom;
    ys[6] = e->hit_box.bottom + 1;
    for (i = 0; i < 7; i++) {
        for (j = 0; j < 7; j++) {
            vec_t at = vec_add(base, vec_make(xs[i], ys[j]));
            if (on_screen) {
                assert_hover_at(ok, vec_add(at, cam), at);
            } else {
                assert_hover_at(ok, at, vec_sub(at, cam));
            }
        }
    }
}

static void assert_hover_over_the_screen(ui_t *ok) {
    vec_t cam = checked_main_camera()->header.pos;
    int x;
    int y;

    for (y = -4; y < P8_SCREEN_HEIGHT + 4; y += 3) {
        for (x = -4; x < P8_SCREEN_WIDTH + 4; x += 3) {
            vec_t screen = vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
            assert_hover_at(ok, vec_add(screen, cam), screen);
        }
    }
}

/* Over planets, two overlapping planet boxes, their edge points, a button
 * above them and then a backdrop above everything, under a camera off the
 * origin: the hover pass matches a walk calling every entity's hit test. */
static void test_hover_matches_a_walk_of_every_hit_test(void) {
    ui_t *ok = start_game();
    camera_t *camera;
    planet_t *planet;
    planet_t *other;
    button_t *button;
    int x;
    int y;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    camera = main_camera_typed();
    assert(camera != NULL);
    entity_set_pos(&camera->header, vec_add(camera->header.pos, vec_make(P8_FIXED_LIT(0x00038000, 3.5), P8_FIXED_LIT(0x00024000, 2.25))));
    assert(camera->header.pos.x != 0 && camera->header.pos.y != 0);
    planet = planet_on_screen(&x, &y);
    other = nearest_to(planet);
    entity_set_pos(&other->header, vec_add(planet->header.pos, vec_make(p8_fixed_from_int(3), p8_fixed_from_int(2))));
    button = button_new(NULL);
    entity_set_pos(&button->header, vec_sub(planet->header.pos, camera->header.pos));
    entity_registry_set_layer(&button->header, p8_fixed_from_int(12));
    assert(entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), button));

    assert_hover_around(ok, &planet->header, 0);
    assert_hover_around(ok, &other->header, 0);
    assert_hover_around(ok, &button->header, 1);
    assert_hover_over_the_screen(ok);

    (void)modal_backdrop_new(NULL);
    assert_hover_around(ok, &planet->header, 0);
    assert_hover_over_the_screen(ok);
}

typedef struct {
    entity_t header;
} replacer_t;

static down_t replacer_left_press(entity_t *self, mouse_pos_t mouse) {
    entity_id_list_t list = ui_ephemerals_new();
    down_t out = {1, ENTITY_ID_NONE};

    (void)mouse;
    entity_id_list_add(&list, entity_id_link(self));
    ui_set_ephemerals(main_ui(), list);
    return out;
}

static const entity_class_t replacer_class_def = {
    .hit_test = giver_hit_test,
    .left_press = replacer_left_press,
};

static replacer_t g_replacer = {{.class = &replacer_class_def, .has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_HOVERED, .layer = 0xf0000}};

/* A left press whose handler hands the ui a new ephemerals list keeps that
 * list: ui_update only clears a list the frame did not replace. */
static void test_press_that_replaces_the_ephemerals_keeps_them(void) {
    ui_t *ok = start_game();
    uint16_t serial;

    entity_construct(&g_replacer.header);
    g_giver_hovered = 1;
    mouse_at(64, 100, 0);
    ui_update(&memory, ok);
    serial = ok->ephemerals_serial;
    mouse_at(64, 100, 1);
    ui_update(&memory, ok);
    assert(ok->ephemerals_serial == (uint16_t)(serial + 1) && "only the press replaced the list");
    assert(ephemerals_of(ok)->count == 1 && entity_id_list_at(ephemerals_of(ok), 0) == &g_replacer.header);
}

/* entity_set_state takes a struct or NULL, and does nothing on NULL. */
static void test_entity_closed(void) {
    (void)start_game();
    entity_set_state(NULL, ENTITY_STATE_NORMAL);
}

/* Every entity struct begins with its header. */
static entity_t *entity_of(void *e) {
    assert(e != NULL);
    return e;
}

static uint32_t fold_string(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0u);
}

static uint32_t fold_set_state_back(uint32_t h, void *e) {
    assert(entity_of(e)->state_time == 0);
    return fnv(h, (uint32_t)entity_of(e)->state_time);
}

/* The component a host stores in itself: not shared with `other`, of the
 * same class, and inside the host's own `size` bytes. */
static void assert_own_emitter(entity_t *self, entity_t *other, size_t size) {
    entity_t *e = self;
    entity_t *o = other;
    const char *at = (const char *)emitter_of(e);

    assert(e->class == o->class);
    assert((const void *)at != (const void *)emitter_of(o));
    assert(at >= (const char *)self && at < (const char *)self + size);
}

static void assert_own_probe(entity_t *self, entity_t *other, size_t size) {
    entity_t *e = self;
    entity_t *o = other;
    const char *at = (const char *)targeting_of(e);

    assert(e->class == o->class);
    assert((const void *)at != (const void *)targeting_of(o));
    assert(at >= (const char *)self && at < (const char *)self + size);
}

/* A second help, built once `first` gave back its list: the particle zone
 * holds one help screen list at a time. */
static entity_t *help_alone(entity_t *first) {
    static entity_t *previous;
    entity_t *fresh;

    emitter_release_particles(emitter_of(first));
    if (previous != NULL) {
        emitter_release_particles(emitter_of(previous));
    }
    fresh = &help_screen_new()->header;
    previous = fresh;
    return fresh;
}

/* Which of the 24 classes carry a particle emitter (exactly ascension,
 * protostar, burst, synthesizer, probe, help screen) and a targeting
 * (the structure family, probe, help screen, slipway, wormhole), each in its
 * own storage. */
static void test_components_carried(void) {
    static const int emitter[24] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    static const int probe[24] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1};
    void *entities[24];
    entity_t *other_help;
    entity_t *other_protostar;
    size_t index;

    (void)start_game();
    entities[0] = background_new();
    entities[1] = mouse_new();
    entities[2] = message_box_new(NULL);
    entities[3] = toast_new(NULL);
    entities[4] = planet_new(NULL);
    entities[5] = structure_new(NULL);
    entities[6] = trading_hub_new(NULL);
    entities[7] = processor_new(NULL);
    entities[8] = ascension_new(NULL);
    entities[9] = protostar_new(NULL);
    entities[10] = burst_new(NULL);
    entities[11] = slipgate_new(NULL);
    entities[12] = infraspace_gate_new(NULL);
    entities[13] = synthesizer_new(NULL);
    entities[14] = probe_new(NULL);
    entities[15] = help_screen_new();
    entities[16] = structure_icon_new(NULL);
    entities[17] = score_panel_new();
    entities[18] = camera_new();
    entities[19] = slipway_new(NULL);
    entities[20] = ui_new();
    entities[21] = button_new(NULL);
    entities[22] = modal_backdrop_new(NULL);
    entities[23] = wormhole_new();
    for (index = 0; index < 24; index++) {
        entity_t *header = entities[index];

        assert((emitter_of(header) != NULL) == emitter[index]);
        assert((targeting_of(header) != NULL) == probe[index]);
    }

    assert_own_emitter(entities[8], &ascension_new(NULL)->header, sizeof(planet_t));
    other_protostar = &protostar_new(NULL)->header;
    assert_own_emitter(entities[9], other_protostar, sizeof(planet_t));
    assert_own_emitter(entities[10], &burst_new(NULL)->header, sizeof(planet_t));
    assert_own_emitter(entities[13], &synthesizer_new(NULL)->header, sizeof(planet_t));
    assert_own_emitter(entities[14], &probe_new(NULL)->header, sizeof(probe_t));
    other_help = help_alone(entities[15]);
    assert_own_emitter(entities[15], other_help, sizeof(probe_t));
    assert_own_probe(entities[5], &structure_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[6], &trading_hub_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[7], &processor_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[8], &ascension_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[9], other_protostar, sizeof(planet_t));
    assert_own_probe(entities[11], &slipgate_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[12], &infraspace_gate_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[13], &synthesizer_new(NULL)->header, sizeof(planet_t));
    assert_own_probe(entities[14], &probe_new(NULL)->header, sizeof(probe_t));
    assert_own_probe(entities[15], other_help, sizeof(probe_t));
    assert_own_probe(entities[19], &slipway_new(NULL)->header, sizeof(slipway_t));
    assert_own_probe(entities[23], &wormhole_new()->header, sizeof(wormhole_t));
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1 };

static void test_entity_state_by_name(void) {
    static const state_t want[24] = {
        ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_UNKNOWN, ENTITY_STATE_PLACING,
        ENTITY_STATE_PLACING, ENTITY_STATE_PLACING, ENTITY_STATE_PLACING, ENTITY_STATE_SETTLED, ENTITY_STATE_UNKNOWN, ENTITY_STATE_PLACING,
        ENTITY_STATE_PLACING, ENTITY_STATE_PLACING, ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,
        ENTITY_STATE_NORMAL,  ENTITY_STATE_PLACING, ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_NORMAL,  ENTITY_STATE_PLACING};
    void *entities[24];
    const entity_list_t *live;
    const char *state;
    uint32_t hash = 2166136261u;
    size_t index;

    (void)start_game();
    live = live_entities();
    assert(live != NULL);
    hash = fnv(hash, (uint32_t)live->count);
    /* The kind word is constant by the struct's shape (the state is a name
     * or absent), kept so that ENTITY_STATE_HASH still proves the reads. */
    for (index = 0; index < (size_t)live->count; index++) {
        state = entity_state_name(entity_of(live->items[index])->state);
        if (state != NULL) {
            hash = fold_string(fnv(fnv(hash, 1u), (uint32_t)STRING_KIND), state);
        } else {
            hash = fnv(hash, 0u);
        }
    }

    entities[0] = background_new();
    entities[1] = mouse_new();
    entities[2] = message_box_new(NULL);
    entities[3] = toast_new(NULL);
    entities[4] = planet_new(NULL);
    entities[5] = structure_new(NULL);
    entities[6] = trading_hub_new(NULL);
    entities[7] = processor_new(NULL);
    entities[8] = ascension_new(NULL);
    entities[9] = protostar_new(NULL);
    entities[10] = burst_new(NULL);
    entities[11] = slipgate_new(NULL);
    entities[12] = infraspace_gate_new(NULL);
    entities[13] = synthesizer_new(NULL);
    entities[14] = probe_new(NULL);
    entities[15] = help_screen_new();
    entities[16] = structure_icon_new(NULL);
    entities[17] = score_panel_new();
    entities[18] = camera_new();
    entities[19] = slipway_new(NULL);
    entities[20] = ui_new();
    entities[21] = button_new(NULL);
    entities[22] = modal_backdrop_new(NULL);
    entities[23] = wormhole_new();
    for (index = 0; index < 24; index++) {
        assert(entity_of(entities[index])->state == want[index]);
        hash = fold_string(fnv(hash, (uint32_t)STRING_KIND), entity_state_name(want[index]));
    }

    /* Six words constant by the struct's shape, kept for the frozen hash. */
    for (index = 0; index < 6; index++) {
        hash = fnv(hash, 0u);
    }

    entity_set_state(entities[0], ENTITY_STATE_VANISHING);
    assert(entity_of(entities[0])->state == ENTITY_STATE_VANISHING);
    hash = fold_set_state_back(fold_string(fnv(hash, 1u), entity_state_name(ENTITY_STATE_VANISHING)), entities[0]);
    entity_set_state(entities[0], ENTITY_STATE_NONE);
    assert(entity_of(entities[0])->state == ENTITY_STATE_NONE);
    hash = fold_set_state_back(fnv(hash, 0u), entities[0]);
    entity_set_state(entities[0], ENTITY_STATE_NORMAL);
    assert(entity_of(entities[0])->state == ENTITY_STATE_NORMAL);
    hash = fold_set_state_back(fold_string(fnv(hash, 1u), entity_state_name(ENTITY_STATE_NORMAL)), entities[0]);

    entity_set_state(entities[9], ENTITY_STATE_VANISHING);
    assert(entity_of(entities[9])->state == ENTITY_STATE_VANISHING);
    hash = fold_set_state_back(fold_string(fnv(hash, 1u), entity_state_name(ENTITY_STATE_VANISHING)), entities[9]);

    check("entity_state", hash, ENTITY_STATE_HASH);
}

/* The kind word is constant by the struct's shape (the state time is always
 * a fixed), kept so that ENTITY_STATE_TIME_HASH still proves the reads. */
static uint32_t fold_state_time(uint32_t h, void *e) {
    return fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)entity_of(e)->state_time);
}

static uint32_t fold_live_entities_state_time(uint32_t h) {
    const entity_list_t *hv = live_entities();
    int i;

    assert(hv != NULL);
    h = fnv(h, (uint32_t)hv->count);
    for (i = 0; i < hv->count; i++) {
        h = fold_state_time(h, hv->items[i]);
    }
    return h;
}

static void assert_state_time(void *e, int want) {
    assert(entity_of(e)->state_time == p8_fixed_from_int(want));
}

static void test_entity_state_time_by_name(void) {
    void *entities[24];
    probe_keys_t keyed_keys = {0};
    probe_keys_t zero_keys = {0};
    probe_t *keyed;
    probe_t *zero;
    uint32_t hash = 2166136261u;
    size_t index;

    (void)start_game();
    hash = fold_live_entities_state_time(hash);
    update_entities();
    hash = fold_live_entities_state_time(hash);

    entities[0] = background_new();
    entities[1] = mouse_new();
    entities[2] = message_box_new(NULL);
    entities[3] = toast_new(NULL);
    entities[4] = planet_new(NULL);
    entities[5] = structure_new(NULL);
    entities[6] = trading_hub_new(NULL);
    entities[7] = processor_new(NULL);
    entities[8] = ascension_new(NULL);
    entities[9] = protostar_new(NULL);
    entities[10] = burst_new(NULL);
    entities[11] = slipgate_new(NULL);
    entities[12] = infraspace_gate_new(NULL);
    entities[13] = synthesizer_new(NULL);
    entities[14] = probe_new(NULL);
    entities[15] = help_screen_new();
    entities[16] = structure_icon_new(NULL);
    entities[17] = score_panel_new();
    entities[18] = camera_new();
    entities[19] = slipway_new(NULL);
    entities[20] = ui_new();
    entities[21] = button_new(NULL);
    entities[22] = modal_backdrop_new(NULL);
    entities[23] = wormhole_new();
    for (index = 0; index < 24; index++) {
        assert_state_time(entities[index], 0);
        hash = fold_state_time(hash, entities[index]);
    }

    /* Six words constant by the struct's shape, kept for the frozen hash. */
    for (index = 0; index < 6; index++) {
        hash = fnv(hash, 0u);
    }

    keyed_keys.has_angle_slices = 1;
    keyed_keys.angle_slices = p8_fixed_from_int(7);
    keyed_keys.state_time = p8_fixed_from_int(3);
    keyed_keys.has_base_radius = 1;
    keyed_keys.base_radius = p8_fixed_from_int(10);
    keyed_keys.has_speed_factor = 1;
    keyed_keys.speed_factor = p8_fixed_from_int(1);
    keyed = probe_new(&keyed_keys);
    assert_state_time(keyed, 3);
    assert(keyed->emitter.angle_slices == p8_fixed_from_int(7));
    assert(keyed->emitter.base_radius == p8_fixed_from_int(10));
    assert(keyed->emitter.speed_factor == p8_fixed_from_int(1));
    hash = fold_state_time(hash, keyed);

    zero_keys.state_time = 0;
    zero = probe_new(&zero_keys);
    assert_state_time(zero, 0);
    assert(zero->emitter.angle_slices == probe_archetype.emitter.angle_slices);
    assert(zero->emitter.base_radius == probe_archetype.emitter.base_radius);
    assert(zero->emitter.speed_factor == probe_archetype.emitter.speed_factor);
    hash = fold_state_time(hash, zero);

    entity_set_state(&keyed->header, ENTITY_STATE_VANISHING);
    assert_state_time(keyed, 0);
    assert(keyed->header.state == ENTITY_STATE_VANISHING);
    hash = fold_string(fold_state_time(hash, keyed), entity_state_name(keyed->header.state));

    entity_set_state(entities[9], ENTITY_STATE_VANISHING);
    assert_state_time(entities[9], 0);
    hash = fold_state_time(hash, entities[9]);

    check("entity_state_time", hash, ENTITY_STATE_TIME_HASH);
}

static void test_probe_state_time_key_at_zero(void) {
    probe_keys_t keys = {0};
    void *made;

    (void)start_game();
    keys.state_time = 0;
    made = probe_new(&keys);
    assert_state_time(made, 0);
}

/* Over a planet or not, the overlay draws the colour-7 crosshair at the
 * screen centre while it is set, and nothing otherwise. */
static void test_overlay_draws_only_the_crosshair(void) {
    ui_t *ok = start_game();
    int x;
    int y;
    int lit = 0;

    mouse_at(80, 96, 0);
    ui_update(&memory, ok);
    planet_on_screen(&x, &y);
    mouse_at(x, y, 0);
    ui_update(&memory, ok);
    assert(ui_hovered(ok) != NULL && (void *)ui_hovered(ok) != the_background());
    assert_overlay_draws_nothing();

    ui_set_pad_crosshair(ok, 1);
    draw_overlay_alone();
    for (int py = 0; py < P8_SCREEN_HEIGHT; py++) {
        for (int px = 0; px < P8_SCREEN_WIDTH; px++) {
            int dx = px - P8_SCREEN_WIDTH / 2;
            int dy = py - P8_SCREEN_HEIGHT / 2;
            int on_arm = (dx == 0) != (dy == 0) && abs(dx + dy) <= UI_PAD_CROSSHAIR_ARM;
            assert(memory.screen[py][px] == (on_arm ? UI_PAD_CROSSHAIR_COLOUR : 0));
            lit += on_arm;
        }
    }
    assert(lit == 4 * UI_PAD_CROSSHAIR_ARM);

    ui_set_pad_crosshair(ok, 0);
    assert_overlay_draws_nothing();
}

/* Each scenario runs in a child forked right after cart_load(), as a
 * fresh process would start it: the rnd state is the same for all. */
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
    in_fresh_child(test_built);
    in_fresh_child(test_shared);
    in_fresh_child(test_fresh);
    in_fresh_child(test_hover);
    in_fresh_child(test_unwritten_layer_reads_empty);
    in_fresh_child(test_overlay_draws_only_the_crosshair);
    in_fresh_child(test_layers);
    in_fresh_child(test_left);
    in_fresh_child(test_held_links_read_null_after_the_flush);
    in_fresh_child(test_clear_ephemerals);
    in_fresh_child(test_ephemerals_reserve_at_creation);
    in_fresh_child(test_ephemerals_eight_fit_the_reserve);
    in_fresh_child(test_stale);
    in_fresh_child(test_reset_world_drops_the_old_world_ids);
    in_fresh_child(test_release_frees_the_owned_slot);
    in_fresh_child(test_press_that_replaces_the_ephemerals_keeps_them);
    in_fresh_child(test_hover_hands_the_mouse_by_value);
    in_fresh_child(test_hover_matches_a_walk_of_every_hit_test);
    in_fresh_child(test_ui_is_a_struct);
    in_fresh_child(test_ui_held_aborts_on_other_classes);
    in_fresh_child(test_set_left_held_sets_and_clears);
    in_fresh_child(test_entity_closed);
    in_fresh_child(test_entity_state_by_name);
    in_fresh_child(test_components_carried);
    in_fresh_child(test_entity_state_time_by_name);
    in_fresh_child(test_probe_state_time_key_at_zero);

    printf("test_ui: all tests passed\n");
    return 0;
}
