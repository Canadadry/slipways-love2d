/* The kernel's component links on entities whose constructor calls
 * `malloc` without zeroing. The binary runs itself again with `malloc` filling every
 * block it returns (MallocScribble on macOS; MALLOC_PERTURB_ on glibc, whose
 * tcache hands a freed block back unfilled, hence the tcache turned off), so
 * a link no init set is not NULL. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/mouse.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/wormhole.h"
#include "heap_allocator.h"

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static void rerun_with_malloc_filling(char **argv) {
    if (getenv("MALLOC_FILLS") != NULL) {
        return;
    }
    setenv("MALLOC_PERTURB_", "90", 1);
    setenv("GLIBC_TUNABLES", "glibc.malloc.tcache_count=0", 1);
    setenv("MallocScribble", "1", 1);
    setenv("MALLOC_FILLS", "1", 1);
    execv(argv[0], argv);
    perror("execv");
    exit(1);
}

/* Reads a recycled block on purpose: through `volatile`, which the compiler
 * does not flag as uninitialized. */
static void assert_malloc_fills(void) {
    void *block = calloc(1, 64);
    const volatile unsigned char *bytes;
    int i;

    assert(block != NULL);
    free(block);
    block = malloc(64);
    bytes = block;
    assert(block != NULL);
    for (i = 0; i < 64; i++) {
        assert(bytes[i] != 0 && bytes[i] == bytes[0] && "malloc must fill its block, or this test proves nothing");
    }
    free(block);
}

static int same_vec(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}

/* Each value whose bit `want.has` sets; its `class`, `state` and
 * `state_time` are not compared. */
static void assert_links(const entity_t *h, entity_t want) {
    assert(h->has == want.has);
    assert(!entity_has(h, ENTITY_HAS_LAYER) || h->layer == want.layer);
    assert(!entity_has(h, ENTITY_HAS_POS) || same_vec(h->pos, want.pos));
    assert(!entity_has(h, ENTITY_HAS_MAP_POS) || same_vec(h->map_pos, want.map_pos));
    assert(!entity_has(h, ENTITY_HAS_HIT_BOX) || (h->hit_box.left == want.hit_box.left && h->hit_box.top == want.hit_box.top &&
                                                  h->hit_box.right == want.hit_box.right && h->hit_box.bottom == want.hit_box.bottom));
    assert(!entity_has(h, ENTITY_HAS_INFRASPACE) || h->infraspace == want.infraspace);
    assert(!entity_has(h, ENTITY_HAS_SHAKE) || h->shake == want.shake);
    assert(!entity_has(h, ENTITY_HAS_FOLD_ANCHOR) || h->fold_anchor == want.fold_anchor);
    assert(!entity_has(h, ENTITY_HAS_IS_DEAD) || h->is_dead == want.is_dead);
    assert(!entity_has(h, ENTITY_HAS_IS_HOVERED) || h->is_hovered == want.is_hovered);
}

/* The components targeting_of and emitter_of find: `targeting` and `emitter`
 * inside the host, or NULL. */
static void assert_components(entity_t *h, const targeting_t *targeting, const emitter_t *emitter) {
    assert(targeting_of(h) == targeting);
    assert(emitter_of(h) == emitter);
}

/* Non-carriers: no value but `pos`, at most: the mouse starts at (0,0), a
 * row built without keys has none. The mouse point a slipway's `drag` builds
 * has its own test below. */
static void test_non_carriers_have_null_links(void) {
    structure_icon_t *ie = structure_icon_new(NULL);
    structure_icon_kind_t view = structure_icon_kind_view(ie);
    mouse_t *nf = mouse_new();
    layout_row_t *lq = layout_row_new(&heap_allocator, NULL);

    assert_links(&nf->header, (entity_t){.has = ENTITY_HAS_POS, .pos = vec_make(0, 0)});
    assert_links(&lq->header, (entity_t){0});
    assert_links(&tooltip_new(&heap_allocator, NULL)->header, (entity_t){0});
    assert_links(&research_project_new(NULL)->header, (entity_t){0});
    assert_links(&economy_new()->header, (entity_t){0});
    assert_links(&view.header, (entity_t){0});
    assert_components(&nf->header, NULL, NULL);
    assert_components(&lq->header, NULL, NULL);
    assert_components(&ie->header, NULL, NULL);
}

/* Carriers: the archetype's field for a class that never writes it, the
 * instance's otherwise. */
static void test_carriers_link_their_storage(void) {
    background_t *background = background_new();
    button_t *button = button_new(NULL);
    structure_icon_t *structure_icon = structure_icon_new(NULL);
    modal_backdrop_t *modal_backdrop = modal_backdrop_new(NULL);
    ui_t *built_ui = ui_new();
    camera_t *camera = camera_new();
    message_box_t *message_box = message_box_new(NULL);
    toast_t *toast = toast_new(NULL);
    probe_t *help_screen = help_screen_new();
    planet_t *planet = planet_new(NULL);
    planet_t *infraspace_gate = infraspace_gate_new(NULL);
    planet_t *infraspace_gate_planet = infraspace_gate;
    const mouse_t *mouse = main_mouse_typed();
    const camera_t *main = main_camera_typed();
    vec_t toast_pos = vec_sub(vec_add(mouse != NULL ? entity_pos(&mouse->header) : vec_make(0, 0), main != NULL ? entity_pos(&main->header) : vec_make(0, 0)),
                              vec_make(0, p8_fixed_from_int(5)));

    assert_links(&background->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_HOVERED, .layer = background->archetype->layer});
    assert_links(&button->header,
                 (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_HIT_BOX | ENTITY_HAS_IS_HOVERED,
                            .layer = button_archetype.layer,
                            .hit_box = {p8_fixed_from_int(0), p8_fixed_from_int(-1), button->width, p8_fixed_from_int(11)}});
    assert_links(&structure_icon->header,
                 (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_POS | ENTITY_HAS_HIT_BOX | ENTITY_HAS_IS_HOVERED, .layer = structure_icon->archetype->layer, .pos = structure_icon->archetype->pos, .hit_box = structure_icon->archetype->hit_box});
    assert_links(&modal_backdrop->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_DEAD | ENTITY_HAS_IS_HOVERED, .layer = modal_backdrop->archetype->layer});
    assert_links(&built_ui->header, (entity_t){.has = ENTITY_HAS_LAYER, .layer = built_ui->archetype->layer});
    assert_links(&camera->header, (entity_t){.has = ENTITY_HAS_SHAKE | ENTITY_HAS_POS | ENTITY_HAS_MAP_POS, .pos = camera->archetype->pos, .map_pos = camera->archetype->pos_before_shake, .shake = camera->archetype->shake});
    assert_links(&message_box->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_DEAD, .layer = message_box->archetype->layer});
    assert_links(&toast->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_DEAD | ENTITY_HAS_POS, .layer = toast->archetype->layer, .pos = toast_pos});
    assert_links(&help_screen->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_POS | ENTITY_HAS_HIT_BOX | ENTITY_HAS_FOLD_ANCHOR | ENTITY_HAS_IS_DEAD | ENTITY_HAS_IS_HOVERED,
                                                         .layer = help_screen->archetype->layer,
                                                         .pos = help_screen->archetype->pos,
                                                         .hit_box = help_screen->archetype->hit_box,
                                                         .fold_anchor = help_screen->archetype->fold_anchor});
    assert_components(&help_screen->header, &help_screen->targeting, &help_screen->emitter);
    assert(help_screen->header.fold_anchor == p8_fixed_from_int(1) &&
           "a probe's fold_anchor is its archetype's 1.0: 0 would read as absent");
    assert_links(&planet->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_HIT_BOX | ENTITY_HAS_FOLD_ANCHOR | ENTITY_HAS_SHAKE | ENTITY_HAS_IS_DEAD | ENTITY_HAS_IS_HOVERED,
                                                      .layer = planet->archetype->layer,
                                                      .hit_box = planet->archetype->hit_box, .shake = planet->archetype->shake,
                                                      .fold_anchor = 0});
    assert_components(&planet->header, NULL, NULL);
    assert_links(&infraspace_gate_planet->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_HIT_BOX | ENTITY_HAS_INFRASPACE | ENTITY_HAS_FOLD_ANCHOR | ENTITY_HAS_SHAKE | ENTITY_HAS_IS_DEAD | ENTITY_HAS_IS_HOVERED,
                                                         .layer = infraspace_gate_planet->archetype->layer,
                                                         .hit_box = infraspace_gate_planet->archetype->hit_box,
                                                         .infraspace = infraspace_gate_planet->archetype->infraspace, .shake = infraspace_gate_planet->archetype->shake,
                                                         .fold_anchor = 0});
    assert_components(&infraspace_gate->header, &infraspace_gate->targeting, NULL);
}

static entity_t planet_links(planet_t *jo) {
    return (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_HIT_BOX | ENTITY_HAS_FOLD_ANCHOR | ENTITY_HAS_SHAKE | ENTITY_HAS_IS_DEAD | ENTITY_HAS_IS_HOVERED,
                                   .layer = jo->archetype->layer,
                                   .hit_box = jo->archetype->hit_box, .shake = jo->archetype->shake,
                                   .fold_anchor = 0};
}

static void assert_structure_links(planet_t *qh, emitter_t *emitter) {
    assert_links(&qh->header, planet_links(qh));
    assert_components(&qh->header, &qh->targeting, emitter);
}

/* Emitter and targeting carriers: each component is the one the instance
 * stores in itself. */
static void test_component_carriers_link_their_own(void) {
    probe_t *g_ = probe_new(NULL);
    planet_t *nl = trading_hub_new(NULL);
    planet_t *os = processor_new(NULL);
    planet_t *hj = ascension_new(NULL);
    planet_t *pm = protostar_new(NULL);
    planet_t *mx = synthesizer_new(NULL);
    planet_t *nr = slipgate_new(NULL);
    planet_t *ga = burst_new(NULL);
    wormhole_t *mz = wormhole_new();

    assert_links(&g_->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_FOLD_ANCHOR | ENTITY_HAS_IS_DEAD,
                                                      .layer = g_->archetype->layer,
                                                      .fold_anchor = g_->archetype->fold_anchor});
    assert_components(&g_->header, &g_->targeting, &g_->emitter);
    assert(g_->header.fold_anchor == p8_fixed_from_int(1) &&
           "a probe's fold_anchor is its archetype's 1.0: 0 would read as absent");
    assert_structure_links(structure_new(NULL), NULL);
    assert_structure_links(nl, NULL);
    assert_structure_links(os, NULL);
    assert_structure_links(hj, &hj->emitter);
    assert_structure_links(pm, &pm->emitter);
    assert_structure_links(mx, &mx->emitter);
    assert_structure_links(nr, NULL);
    assert_links(&ga->header, planet_links(ga));
    assert_components(&ga->header, NULL, &ga->emitter);
    assert_links(&mz->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_DEAD,
                                                      .layer = mz->archetype->layer});
    assert_components(&mz->header, &mz->targeting, NULL);
}

static slipway_t *slipway_dragged_over_nothing(entity_t *qu) {
    slipway_keys_t keys = {0};
    mouse_pos_t mouse = {.world = vec_make(p8_fixed_from_int(-640), p8_fixed_from_int(-640)), .has = MOUSE_POS_HAS_WORLD};
    slipway_t *jc;

    keys.start = entity_id_link(qu);
    jc = slipway_new(&keys);
    main_ui()->hovered = entity_id_link(NULL);
    slipway_class()->drag(&jc->header, mouse);
    return jc;
}

/* The point a slipway's `drag` builds over nothing carries `pos` and
 * `map_pos` alone, the mouse's and its map copy; the slipway's `infraspace`
 * is its start's, else absent. */
static void test_mouse_point_has_null_links(void) {
    slipway_t *jc = slipway_dragged_over_nothing(&planet_new(NULL)->header);
    slipway_mouse_point_t *point = (slipway_mouse_point_t *)slipway_end(jc);
    slipway_t *from_infraspace_gate = slipway_dragged_over_nothing(&infraspace_gate_new(NULL)->header);

    assert(point != NULL && point->header.class == slipway_mouse_point_class());
    assert_links(&point->header, (entity_t){.has = ENTITY_HAS_POS | ENTITY_HAS_MAP_POS,
                                            .pos = vec_make(p8_fixed_from_int(-640), p8_fixed_from_int(-640)),
                                            .map_pos = vec_make(p8_fixed_from_int(-5), p8_fixed_from_int(-5))});
    assert_links(&jc->header, (entity_t){.has = ENTITY_HAS_LAYER | ENTITY_HAS_IS_DEAD, .layer = p8_fixed_from_int(4)});
    assert_components(&jc->header, &jc->targeting, NULL);
    assert_links(&from_infraspace_gate->header,
                 (entity_t){
                     .has = ENTITY_HAS_LAYER | ENTITY_HAS_INFRASPACE | ENTITY_HAS_IS_DEAD,
                     .layer = p8_fixed_from_int(2),
                     .infraspace = infraspace_gate_archetype.infraspace});
    assert_components(&from_infraspace_gate->header, &from_infraspace_gate->targeting, NULL);
}

/* A position reads as the value the class holds, and as the zero vector
 * with its presence test false when the class or the instance has none. */
static void test_position_readers(void) {
    camera_t *camera = camera_new();
    wormhole_t *wormhole = wormhole_new();
    const entity_t *tooltip = &tooltip_new(&heap_allocator, NULL)->header;

    assert(entity_has_pos(&camera->header) && entity_has_map_pos(&camera->header));
    assert(same_vec(entity_pos(&camera->header), camera->header.pos));
    assert(same_vec(camera_pos_before_shake(camera), camera->header.map_pos));
    assert(!entity_has_map_pos(&wormhole->header));
    assert(same_vec(entity_map_pos(&wormhole->header), vec_make(0, 0)));
    assert(!entity_has_pos(tooltip) && !entity_has_map_pos(tooltip));
    assert(same_vec(entity_pos(tooltip), vec_make(0, 0)) && same_vec(entity_map_pos(tooltip), vec_make(0, 0)));
}

/* The cursor test reads the screen point from layer 10 up and the world
 * point below it, and an absent point reads as zero, whatever it holds. */
static void test_contains_cursor_picks_the_point_by_layer(void) {
    static const entity_class_t plain_class = {0};
    vec_t inside = vec_make(p8_fixed_from_int(3), p8_fixed_from_int(3));
    vec_t outside = vec_make(p8_fixed_from_int(50), p8_fixed_from_int(50));
    mouse_pos_t world_inside = {.world = inside, .screen = outside, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
    mouse_pos_t screen_inside = {.world = outside, .screen = inside, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
    mouse_pos_t absent_outside = {.world = outside, .screen = outside, .has = 0};
    entity_t e;

    entity_init_header(&e, &plain_class);
    e.hit_box = (box_t){p8_fixed_from_int(-1), p8_fixed_from_int(-1), p8_fixed_from_int(4), p8_fixed_from_int(4)};
    e.has |= ENTITY_HAS_HIT_BOX | ENTITY_HAS_LAYER;
    e.layer = p8_fixed_from_int(9);
    assert(entity_geom_contains_cursor(&e, world_inside));
    assert(!entity_geom_contains_cursor(&e, screen_inside));
    assert(entity_geom_contains_cursor(&e, absent_outside));
    e.layer = p8_fixed_from_int(10);
    assert(!entity_geom_contains_cursor(&e, world_inside));
    assert(entity_geom_contains_cursor(&e, screen_inside));
    assert(entity_geom_contains_cursor(&e, absent_outside));
    entity_set_pos(&e, vec_make(p8_fixed_from_int(10), p8_fixed_from_int(10)));
    assert(!entity_geom_contains_cursor(&e, absent_outside));
}

/* The cursor test as the cart wrote it: mid() on each axis of the point
 * taken by layer, minus the position, each absent part reading as zero. */
static int cart_contains_cursor(const entity_t *e, mouse_pos_t mouse) {
    p8_fixed_t layer = entity_has(e, ENTITY_HAS_LAYER) ? e->layer : 0;
    int screen = layer >= p8_fixed_from_int(10);
    int has_point = (mouse.has & (screen ? MOUSE_POS_HAS_SCREEN : MOUSE_POS_HAS_WORLD)) != 0;
    vec_t chosen = has_point ? (screen ? mouse.screen : mouse.world) : vec_make(0, 0);
    vec_t point = vec_sub(chosen, entity_has(e, ENTITY_HAS_POS) ? e->pos : vec_make(0, 0));

    return entity_has(e, ENTITY_HAS_HIT_BOX) && p8_fixed_mid(e->hit_box.left, e->hit_box.right, point.x) == point.x &&
           p8_fixed_mid(e->hit_box.top, e->hit_box.bottom, point.y) == point.y;
}

/* The inline cursor test the hover loop runs, and the function wrapping it,
 * answer as the cart's test on and around a reversed box's edges, with and
 * without layer, position, hit box or mouse points. */
static void test_inline_contains_cursor_answers_as_the_function(void) {
    static const entity_class_t plain_class = {0};
    static const uint16_t parts[] = {0, ENTITY_HAS_HIT_BOX, ENTITY_HAS_HIT_BOX | ENTITY_HAS_LAYER,
                                     ENTITY_HAS_HIT_BOX | ENTITY_HAS_LAYER | ENTITY_HAS_POS};
    static const int layers[] = {9, 10};
    static const uint16_t mice[] = {0, MOUSE_POS_HAS_WORLD, MOUSE_POS_HAS_SCREEN, MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
    entity_t e;
    size_t p;
    size_t l;
    size_t m;
    int dx;
    int dy;
    int agreed_inside = 0;

    assert(!entity_geom_contains_cursor_inline(NULL, (mouse_pos_t){0}));
    for (p = 0; p < sizeof parts / sizeof parts[0]; p++) {
        for (l = 0; l < sizeof layers / sizeof layers[0]; l++) {
            entity_init_header(&e, &plain_class);
            e.hit_box = (box_t){p8_fixed_from_int(4), p8_fixed_from_int(-2), p8_fixed_from_int(-3), p8_fixed_from_int(5)};
            e.pos = vec_make(P8_FIXED_LIT(0x00018000, 1.5), p8_fixed_from_int(-7));
            e.layer = p8_fixed_from_int(layers[l]);
            e.has |= parts[p];
            for (m = 0; m < sizeof mice / sizeof mice[0]; m++) {
                for (dy = -10 * 65536; dy <= 10 * 65536; dy += 32768 + 1) {
                    for (dx = -10 * 65536; dx <= 10 * 65536; dx += 32768 - 1) {
                        vec_t a = vec_make(dx, dy);
                        vec_t b = vec_make(dy, dx);
                        mouse_pos_t mouse = {.world = a, .screen = b, .has = mice[m]};
                        int want = cart_contains_cursor(&e, mouse);
                        assert(entity_geom_contains_cursor_inline(&e, mouse) == want);
                        assert(entity_geom_contains_cursor(&e, mouse) == want);
                        agreed_inside += want;
                    }
                }
            }
        }
    }
    assert(agreed_inside > 0);
}

int main(int argc, char **argv) {
    (void)argc;
    rerun_with_malloc_filling(argv);
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    reset_world(NULL);

    assert_malloc_fills();
    test_non_carriers_have_null_links();
    test_carriers_link_their_storage();
    test_component_carriers_link_their_own();
    test_mouse_point_has_null_links();
    test_position_readers();
    test_contains_cursor_picks_the_point_by_layer();
    test_inline_contains_cursor_answers_as_the_function();

    printf("test_entity_header: all tests passed\n");
    return 0;
}
