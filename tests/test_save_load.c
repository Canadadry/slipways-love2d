#include "devtools/save_check.h"

#include "engine/dispatch_context.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "game/background.h"
#include "game/cart.h"
#include "game/economy.h"
#include "game/game_data.h"
#include "game/main_economy.h"
#include "game/map_tables.h"
#include "game/particle_zone.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/research_display.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/structure_icon.h"
#include "game/ui.h"
#include "game/ui_button.h"
#include "game/ui_zone.h"
#include "game/wormhole.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static save_check_snapshot_t g_saved;
static save_check_snapshot_t g_resumed;
static char g_report[512];

static void play_frame(void) {
    cart_update60();
    cart_draw();
}

static void play_compared_frame(save_check_snapshot_t *snapshot) {
    save_check_hide_particles(1);
    play_frame();
    save_check_capture_screen(snapshot);
    save_check_hide_particles(0);
}

static void load_cart_to_title(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    cart_init();
}

static void start_game_on_reasonable(void) {
    button_t reasonable = {0};
    reasonable.name = difficulty_options_table[1].name;
    reasonable.has_trade = 1;
    reasonable.trade = p8_fixed_from_int(difficulty_options_table[1].trade);
    reasonable.has_type_bias = 1;
    reasonable.type_bias = difficulty_options_table[1].type_bias;
    cart_start_game(&reasonable);
}

static void assert_equal(const save_check_snapshot_t *a, const save_check_snapshot_t *b) {
    if (save_check_compare(a, b, g_report, sizeof g_report) != 0) {
        fprintf(stderr, "test_save_load: %s\n", g_report);
        assert(0 && "the resumed game differs from the saved one");
    }
}

static void test_a_just_started_game_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    start_game_on_reasonable();
    play_frame();
    play_frame();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

enum { REVEALED_COUNT = 5, QUIET_FRAMES = 15 };

static int64_t squared_distance_to_screen_centre(const entity_t *planet) {
    vec_t camera = entity_pos(main_camera());
    vec_t pos = entity_pos(planet);
    int64_t dx = p8_fixed_flr_int(pos.x) - (p8_fixed_flr_int(camera.x) + P8_SCREEN_WIDTH / 2);
    int64_t dy = p8_fixed_flr_int(pos.y) - (p8_fixed_flr_int(camera.y) + P8_SCREEN_HEIGHT / 2);
    return dx * dx + dy * dy;
}

/* Fills out with the REVEALED_COUNT unknown planets nearest the screen
   centre, nearest first. */
static void nearest_unknown_planets(planet_t *out[REVEALED_COUNT]) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    int found = 0;

    for (int i = 0; i < planets->count; i++) {
        entity_t *candidate = planets->items[i];
        int slot;
        if (candidate->state != ENTITY_STATE_UNKNOWN) {
            continue;
        }
        slot = found < REVEALED_COUNT ? found++ : REVEALED_COUNT;
        while (slot > 0 && squared_distance_to_screen_centre(&out[slot - 1]->header) > squared_distance_to_screen_centre(candidate)) {
            if (slot < REVEALED_COUNT) {
                out[slot] = out[slot - 1];
            }
            slot--;
        }
        if (slot < REVEALED_COUNT) {
            out[slot] = (planet_t *)candidate;
        }
    }
    assert(found == REVEALED_COUNT && "the galaxy holds enough unknown planets");
}

static void colonize(planet_t *planet) {
    available_projects_kept_t offered = research_display_available_projects(&planet->header);
    research_project_t *project;
    int paid;

    assert(offered.count > 0 && "a revealed planet offers a colony project");
    project = research_project_get(offered.items[0]);
    paid = economy_spend(main_economy_typed(), research_project_cost(project));
    assert(paid && "the starting credits pay for a colony");
    planet_apply_project(planet, project);
}

static void play_one_colony_game(void) {
    planet_t *revealed[REVEALED_COUNT];
    planet_t *colony = NULL;
    int vanishing = 0;
    vec_t camera_start;

    start_game_on_reasonable();
    play_frame();
    play_frame();
    camera_start = entity_map_pos(main_camera());
    camera_pan(main_camera_typed(), 37, -23);
    assert(entity_map_pos(main_camera()).x != camera_start.x && "a moved camera keeps the camera restore under test");
    nearest_unknown_planets(revealed);
    for (int i = 0; i < REVEALED_COUNT; i++) {
        planet_roll_type(revealed[i]);
        if (revealed[i]->header.state == ENTITY_STATE_VANISHING) {
            vanishing++;
        } else if (colony == NULL) {
            colony = revealed[i];
        }
    }
    assert(colony != NULL && "one revealed planet is not vanishing");
    assert(vanishing > 0 && "a vanishing planet keeps the state_time restore under test; reveal more planets if the galaxy changed");
    colonize(colony);
    assert(planet_is_colonized(colony));
    for (int i = 0; i < QUIET_FRAMES; i++) {
        play_frame();
    }
}

static void test_a_one_colony_game_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_one_colony_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

static planet_t *on_screen_planet(state_t state) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    for (int i = 0; i < planets->count; i++) {
        planet_t *planet = (planet_t *)planets->items[i];
        if (planet->header.state == state && entity_geom_is_on_screen(&planet->header)) {
            return planet;
        }
    }
    return NULL;
}

static void test_a_pad_selection_resumes_equal_from_the_title(void) {
    planet_t *selected;
    load_cart_to_title();
    play_one_colony_game();
    selected = on_screen_planet(ENTITY_STATE_KNOWN);
    assert(selected != NULL && "a revealed planet is on screen to select");
    ui_set_pad_selection(main_ui(), &selected->header, 0);
    ui_set_pad_tab(main_ui(), &selected->header);
    save_check_drop_unsaved_state();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

enum { FLICKER_FIRST_STATE_TIME = 40, FLICKER_LAST_STATE_TIME = 55 };

/* Resumes once per frame of the flicker: a single frame's flicker matches by chance half the time. */
static void test_a_flickering_vanishing_planet_resumes_equal_from_the_title(void) {
    planet_t *vanishing;
    vec_t vanishing_pos;
    load_cart_to_title();
    play_one_colony_game();
    vanishing = on_screen_planet(ENTITY_STATE_VANISHING);
    assert(vanishing != NULL && "a vanishing planet is on screen");
    vanishing_pos = entity_pos(&vanishing->header);
    while (vanishing->header.state_time < p8_fixed_from_int(FLICKER_FIRST_STATE_TIME)) {
        play_frame();
    }
    while (vanishing->header.state_time < p8_fixed_from_int(FLICKER_LAST_STATE_TIME)) {
        assert(entity_geom_is_on_screen(&vanishing->header) && !vanishing->header.is_dead && "the planet flickers on screen");
        save_check_capture(&g_saved);
        play_compared_frame(&g_saved);

        cart_init();
        play_frame();
        save_check_resume(&g_saved);
        save_check_capture(&g_resumed);
        play_compared_frame(&g_resumed);
        assert_equal(&g_saved, &g_resumed);
        vanishing = on_screen_planet(ENTITY_STATE_VANISHING);
        assert(vanishing != NULL && vanishing->header.pos.x == vanishing_pos.x && vanishing->header.pos.y == vanishing_pos.y &&
               "the resumed game shows the same vanishing planet first");
    }
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t WORMHOLE_GAME_SEED = 0x5EED2026u;

enum { QUIET_FRAME_CAP = 150, WORMHOLE_PLACING_FRAMES = 16, PROBE_SCREEN_X = 90, PROBE_SCREEN_Y = 50, WORMHOLE_OFFSET_X = 30 };

static vec_t layer_camera(const entity_t *e) {
    p8_fixed_t parallax;
    vec_t camera;
    assert(game_data_fixed_seq_at(layer_fades_table, LAYER_FADES_COUNT, p8_fixed_flr_int(e->layer), &parallax));
    camera = vec_mul(entity_pos(main_camera()), parallax);
    return vec_make(p8_fixed_flr(camera.x), p8_fixed_flr(camera.y));
}

static int is_on_screen(const entity_t *e) {
    vec_t camera = layer_camera(e);
    int x = p8_fixed_flr_int(entity_pos(e).x) - p8_fixed_flr_int(camera.x);
    int y = p8_fixed_flr_int(entity_pos(e).y) - p8_fixed_flr_int(camera.y);
    return x >= 0 && x < P8_SCREEN_WIDTH && y >= 0 && y < P8_SCREEN_HEIGHT;
}

static int slipways_faded_in(void) {
    const entity_list_t *slipways = entities_by_tag(TAG_SLIPWAY);
    for (int i = 0; slipways != NULL && i < slipways->count; i++) {
        if (((const slipway_t *)slipways->items[i])->fade_in_step != p8_fixed_from_int(20)) {
            return 0;
        }
    }
    return 1;
}

static int is_quiet(void) {
    const entity_list_t *live = live_entities();
    const entity_list_t *wormholes = entities_by_tag(TAG_WORMHOLE);
    const entity_list_t *boxes = entities_by_tag(TAG_MESSAGE_BOX);
    if (main_camera()->shake != 0 || particle_zone_live(PARTICLE_SLOT_BURST) != 0) {
        return 0;
    }
    if ((wormholes != NULL && wormholes->count > 0) || (boxes != NULL && boxes->count > 0) || !slipways_faded_in()) {
        return 0;
    }
    for (int i = 0; i < live->count; i++) {
        const entity_t *e = live->items[i];
        if (e->shake != 0 && entity_has_pos(e) && is_on_screen(e)) {
            return 0;
        }
    }
    return 1;
}

/* Plays QUIET_FRAMES, then until nothing transient is left on screen. */
static void play_until_quiet(void) {
    int played = 0;
    while (played < QUIET_FRAMES || !is_quiet()) {
        assert(played < QUIET_FRAME_CAP && "the game gets quiet after its last action");
        play_frame();
        played++;
    }
}

static entity_t *press_background(void) {
    down_t made = background_left_press((background_t *)background_live());
    assert(made.stop && "a background press makes a probe, then wormholes");
    return entity_get(made.held);
}

static mouse_pos_t mouse_at_screen(const entity_t *held, int x, int y) {
    vec_t screen = vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
    return (mouse_pos_t){.world = vec_add(layer_camera(held), screen), .screen = screen,
                         .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static int vanishing_on_screen_count(void) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    int count = 0;
    for (int i = 0; i < planets->count; i++) {
        const entity_t *planet = planets->items[i];
        count += planet->state == ENTITY_STATE_VANISHING && is_on_screen(planet);
    }
    return count;
}

/* The probe on screen, then a wormhole dropped on it and played until it
   has revealed its area and died. */
static void play_wormhole_game(void) {
    entity_t *probe;
    entity_t *wormhole;
    mouse_pos_t mouse;

    p8_fixed_set_rnd_state(WORMHOLE_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    probe = press_background();
    assert(probe->class == probe_class());
    probe->class->drag(probe, mouse_at_screen(probe, PROBE_SCREEN_X, PROBE_SCREEN_Y));
    assert(targeting_confirm(probe) && "the probe drops on an empty spot of the screen");
    play_frame();

    wormhole = press_background();
    assert(wormhole->class == wormhole_class() && wormhole->state == ENTITY_STATE_PLACING);
    mouse = mouse_at_screen(wormhole, PROBE_SCREEN_X, PROBE_SCREEN_Y);
    /* Not on the probe itself: the nearest-anchor search skips an anchor at the very same map position. */
    mouse.world = vec_add(vec_mul(entity_map_pos(probe), p8_fixed_from_int(128)), vec_make(p8_fixed_from_int(WORMHOLE_OFFSET_X), 0));
    wormhole->class->drag(wormhole, mouse);
    for (int i = 0; i < WORMHOLE_PLACING_FRAMES; i++) {
        play_frame();
    }
    wormhole_drop((wormhole_t *)wormhole);
    assert(!wormhole->is_dead && wormhole->state == ENTITY_STATE_UNFOLDING && "the wormhole unfolds next to the probe");
    play_until_quiet();
    assert(vanishing_on_screen_count() > 0 && "a vanishing planet flickers on the compared frame; move the probe if the galaxy changed");
    assert(((probe_t *)probe)->emitter.particles.count > 0 && "the probe emits particles on the compared frame");
}

static void test_a_revealed_wormhole_area_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_wormhole_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t COLONIES_GAME_SEED = 0xC0105EEDu;

enum { LINKED_CREDITS = 500 };

static int is_terraformable(const planet_t *planet) {
    const research_project_t *terraform = research_projects_find("terraform");
    assert(terraform != NULL && planet->planet_type != NULL);
    return strchr(terraform->planet_codes, planet->planet_type->name[0]) != NULL;
}

/* Paid like a drag onto end: slipway_drop charges and attaches both ends. */
static int link_nodes(planet_t *start, planet_t *end) {
    slipway_keys_t keys = {.start = entity_id_link(&start->header)};
    slipway_t *slipway = slipway_new(&keys);
    vec_t over_end = vec_add(vec_mul(entity_map_pos(&end->header), p8_fixed_from_int(128)), vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1)));

    main_ui()->hovered = entity_id_link(&end->header);
    slipway_class()->drag(&slipway->header, (mouse_pos_t){.world = over_end, .has = MOUSE_POS_HAS_WORLD});
    main_ui()->hovered = entity_id_link(NULL);
    if (targeting_target(&slipway->targeting) != NULL) {
        slipway->header.is_dead = 1;
        return 0;
    }
    main_economy_typed()->year_ticks = 0;
    slipway_drop(slipway, &end->header);
    return 1;
}

static int outgoing_route_count(void) {
    const entity_list_t *nodes = entities_by_tag(TAG_NODE);
    int count = 0;
    for (int i = 0; i < nodes->count; i++) {
        const planet_t *node = (const planet_t *)nodes->items[i];
        if (node->has & PLANET_HAS_OUTGOING_ROUTES) {
            count += trade_route_list_count(&node->outgoing_routes);
        }
    }
    return count;
}

/* Every unvanishing revealed planet colonized, one terraformed first, then
   chained by slipways where nothing is in the way. */
static void play_linked_colonies_game(void) {
    planet_t *revealed[REVEALED_COUNT];
    planet_t *colonies[REVEALED_COUNT];
    int colony_count = 0;
    int terraformed = 0;
    int linked = 0;

    p8_fixed_set_rnd_state(COLONIES_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    main_economy_typed()->credits = p8_fixed_from_int(LINKED_CREDITS);
    nearest_unknown_planets(revealed);
    for (int i = 0; i < REVEALED_COUNT; i++) {
        planet_roll_type(revealed[i]);
        if (revealed[i]->header.state == ENTITY_STATE_VANISHING) {
            continue;
        }
        if (!terraformed && is_terraformable(revealed[i])) {
            const planet_types_entry_t *old_type = revealed[i]->planet_type;
            main_economy_typed()->year_ticks = 0;
            planet_apply_project(revealed[i], research_projects_find("terraform"));
            assert(revealed[i]->planet_type != old_type && revealed[i]->description == old_type->description);
            terraformed = 1;
        }
        main_economy_typed()->year_ticks = 0;
        colonize(revealed[i]);
        colonies[colony_count++] = revealed[i];
    }
    assert(terraformed && "a terraformed colony keeps its old description; reveal more planets if the galaxy changed");
    for (int i = 1; i < colony_count; i++) {
        linked += link_nodes(colonies[i - 1], colonies[i]);
    }
    assert(linked >= 2 && "a colony with two slipways keeps the attach order under test");
    assert(outgoing_route_count() > 0 && "linked colonies trade; reveal more planets if the galaxy changed");
    play_until_quiet();
}

static void test_linked_colonies_resume_equal_from_the_title(void) {
    load_cart_to_title();
    play_linked_colonies_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t STRUCTURES_GAME_SEED = 0x57AC7123u;

enum { STRUCTURE_CREDITS = 5000, STRUCTURE_RING = 16, STRUCTURE_SPACING = 12, RING_DIRECTIONS = 8, RING_COUNT = 3 };

static structure_icon_t *icon_of(const entity_kind_t *kind) {
    const entity_list_t *icons = entities_by_message(ENTITY_MESSAGE_REFRESH);
    for (int i = 0; i < icons->count; i++) {
        entity_t *e = icons->items[i];
        if (e->class == structure_icon_class() && ((structure_icon_t *)e)->kind == kind) {
            return (structure_icon_t *)e;
        }
    }
    assert(0 && "every structure kind has its icon");
    return NULL;
}

static vec_t screen_pos_of(const entity_t *e) {
    return vec_sub(entity_pos(e), layer_camera(e));
}

/* The nearest-node search of a drag skips a node at the very same spot. */
static int is_crowded(const entity_t *held, int x, int y) {
    const entity_list_t *nodes = entities_by_tag(TAG_NODE);
    for (int i = 0; i < nodes->count; i++) {
        const entity_t *node = nodes->items[i];
        vec_t pos;
        int dx, dy;
        if (node == held || !entity_has_pos(node)) {
            continue;
        }
        pos = screen_pos_of(node);
        dx = p8_fixed_flr_int(pos.x) - x;
        dy = p8_fixed_flr_int(pos.y) - y;
        if (dx * dx + dy * dy < STRUCTURE_SPACING * STRUCTURE_SPACING) {
            return 1;
        }
    }
    return 0;
}

/* Bought from its icon and confirmed on the first free spot of three
   rings around near. */
static planet_t *build_structure(const entity_kind_t *kind, const planet_t *near) {
    static const int ring[RING_DIRECTIONS][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
    down_t made = structure_icon_left_press(icon_of(kind));
    entity_t *held = entity_get(made.held);
    vec_t centre = screen_pos_of(&near->header);

    assert(held != NULL && held->state == ENTITY_STATE_PLACING && "an icon press holds a structure in placing");
    for (int i = 0; i < RING_COUNT * RING_DIRECTIONS; i++) {
        int radius = STRUCTURE_RING + STRUCTURE_SPACING * (i / RING_DIRECTIONS) / 2;
        int x = p8_fixed_flr_int(centre.x) + ring[i % RING_DIRECTIONS][0] * radius;
        int y = p8_fixed_flr_int(centre.y) + ring[i % RING_DIRECTIONS][1] * radius;
        if (x < 8 || x >= P8_SCREEN_WIDTH - 8 || y < 8 || y >= P8_SCREEN_HEIGHT - 24 || is_crowded(held, x, y)) {
            continue;
        }
        held->class->drag(held, mouse_at_screen(held, x, y));
        if (targeting_target(&((planet_t *)held)->targeting) == NULL) {
            main_economy_typed()->year_ticks = 0;
            assert(targeting_confirm(held) && "the credits pay for every structure");
            return (planet_t *)held;
        }
    }
    assert(0 && "a structure finds a free spot next to its node; move it if the galaxy changed");
    return NULL;
}

/* The unvanishing planets among the nearest unknown ones, colonized. */
static int colonize_nearest(planet_t *colonies[REVEALED_COUNT]) {
    planet_t *revealed[REVEALED_COUNT];
    int count = 0;
    nearest_unknown_planets(revealed);
    for (int i = 0; i < REVEALED_COUNT; i++) {
        planet_roll_type(revealed[i]);
        if (revealed[i]->header.state == ENTITY_STATE_VANISHING) {
            continue;
        }
        main_economy_typed()->year_ticks = 0;
        colonize(revealed[i]);
        colonies[count++] = revealed[i];
    }
    return count;
}

static const slipway_t *slipway_between(const planet_t *a, const planet_t *b) {
    const entity_list_t *slipways = entities_by_tag(TAG_SLIPWAY);
    for (int i = 0; i < slipways->count; i++) {
        const slipway_t *slipway = (const slipway_t *)slipways->items[i];
        const entity_t *start = slipway_start(slipway);
        const entity_t *end = slipway_end(slipway);
        if ((start == &a->header && end == &b->header) || (start == &b->header && end == &a->header)) {
            return slipway;
        }
    }
    return NULL;
}

enum { BOB_STRUCTURES_CAP = 16 };

static int is_bobbing(const entity_t *e) {
    return (e->class->lineage & LINEAGE(LINEAGE_STRUCTURE)) && e->state == ENTITY_STATE_SETTLED && entity_has_pos(e) && is_on_screen(e);
}

/* Plays until a structure's bob, which turns with its state_time, moves it
   to the next pixel row on the coming frame: a resumed clock that restarts
   at 0 then shows on the compared frame. Toasts go first: a toast removed
   mid-walk skips the next entity's update, so its bob that frame. */
static void play_until_a_bob_shows(void) {
    const entity_list_t *nodes = entities_by_tag(TAG_NODE);
    p8_fixed_t before[BOB_STRUCTURES_CAP];

    for (int played = 0; played < QUIET_FRAME_CAP; played++) {
        int count = 0;
        if (ui_zone_live(SLOT_KIND_TOAST) > 0) {
            play_frame();
            continue;
        }
        for (int i = 0; i < nodes->count && count < BOB_STRUCTURES_CAP; i++) {
            before[count] = nodes->items[i]->pos.y;
            count += is_bobbing(nodes->items[i]);
        }
        play_frame();
        count = 0;
        for (int i = 0; i < nodes->count && count < BOB_STRUCTURES_CAP; i++) {
            p8_fixed_t y = nodes->items[i]->pos.y;
            p8_fixed_t half_step;
            if (!is_bobbing(nodes->items[i])) {
                continue;
            }
            half_step = p8_fixed_div(p8_fixed_sub(y, before[count++]), p8_fixed_from_int(2));
            if (p8_fixed_flr_int(p8_fixed_add(y, half_step)) != p8_fixed_flr_int(y)) {
                return;
            }
        }
    }
    assert(0 && "a structure's bob crosses a pixel row within the cap");
}

/* One structure of every kind but the slipgate and the synthesizer, the
   infraspace gate linked to a colony, and a processor linked to a structure
   while that structure was not anchored yet. */
static void play_every_structure_game(void) {
    planet_t *colonies[REVEALED_COUNT];
    planet_t *structure;
    planet_t *processor;
    planet_t *trading_hub;
    planet_t *ascension;
    planet_t *gate;
    const slipway_t *infraspace_slipway;

    p8_fixed_set_rnd_state(STRUCTURES_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    main_economy_typed()->credits = p8_fixed_from_int(STRUCTURE_CREDITS);
    assert(colonize_nearest(colonies) >= 2 && "two colonies host the structures; reveal more planets if the galaxy changed");

    /* A frame apart, so that their bobs turn out of step. */
    structure = build_structure(structure_kind(), colonies[0]);
    play_frame();
    processor = build_structure(processor_kind(), structure);
    play_frame();
    trading_hub = build_structure(trading_hub_kind(), colonies[1]);
    play_frame();
    ascension = build_structure(ascension_kind(), colonies[1]);
    play_frame();
    gate = build_structure(infraspace_gate_kind(), colonies[0]);

    assert(link_nodes(processor, structure) && link_nodes(structure, colonies[0]) && "a structure links to a structure, then to a colony");
    assert(processor->header.fold_anchor == 0 && structure->header.fold_anchor != 0 &&
           "an unanchored node next to an anchored one keeps the fold anchor restore under test");
    assert(link_nodes(gate, colonies[0]) && "the infraspace gate links to its colony");
    infraspace_slipway = slipway_between(gate, colonies[0]);
    assert(infraspace_slipway != NULL && entity_has(&infraspace_slipway->header, ENTITY_HAS_INFRASPACE) &&
           infraspace_slipway->header.layer == p8_fixed_from_int(2) && "a gate's slipway is an infraspace slipway");
    (void)link_nodes(trading_hub, colonies[1]);
    (void)link_nodes(ascension, colonies[1]);
    play_until_quiet();
    play_until_a_bob_shows();
    assert(is_quiet() && "the save point is still quiet after the bob wait");
}

static void test_every_structure_kind_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_every_structure_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    /* A launch, not cart_init: the structure id counter outlives a restart,
       so only a fresh cart shows a missing restore. */
    load_cart_to_title();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t SLIPGATE_GAME_SEED = 0x6A7E6A7Eu;

static int needs_what_makes(const planet_t *consumer, const planet_t *producer) {
    if (!(consumer->has & PLANET_HAS_NEEDS) || !producer->production.has_resource) {
        return 0;
    }
    for (int i = 0; i < consumer->needs.count; i++) {
        const need_t *need = consumer->needs.items[i];
        if (need->row.has_resource && need->row.resource == producer->production.resource) {
            return 1;
        }
    }
    return 0;
}

static int longest_route_slipways(void) {
    const entity_list_t *nodes = entities_by_tag(TAG_NODE);
    int longest = 0;
    for (int i = 0; i < nodes->count; i++) {
        const planet_t *node = (const planet_t *)nodes->items[i];
        for (int k = 0; (node->has & PLANET_HAS_OUTGOING_ROUTES) && k < trade_route_list_count(&node->outgoing_routes); k++) {
            int count = trade_route_list_at(&node->outgoing_routes, k)->slipways.count;
            longest = count > longest ? count : longest;
        }
    }
    return longest;
}

/* Two colonies that trade, joined only through two slipgates. */
static void play_slipgate_chain_game(void) {
    planet_t *colonies[REVEALED_COUNT];
    planet_t *producer = NULL;
    planet_t *consumer = NULL;
    planet_t *near_gate;
    planet_t *far_gate;
    int count;

    p8_fixed_set_rnd_state(SLIPGATE_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    main_economy_typed()->credits = p8_fixed_from_int(STRUCTURE_CREDITS);
    count = colonize_nearest(colonies);
    for (int i = 0; i < count && producer == NULL; i++) {
        for (int k = 0; k < count && producer == NULL; k++) {
            if (i != k && needs_what_makes(colonies[k], colonies[i])) {
                producer = colonies[i];
                consumer = colonies[k];
            }
        }
    }
    assert(producer != NULL && "two colonies trade; reveal more planets if the galaxy changed");

    near_gate = build_structure(slipgate_kind(), producer);
    far_gate = build_structure(slipgate_kind(), consumer);
    assert(link_nodes(producer, near_gate) && link_nodes(near_gate, far_gate) && link_nodes(far_gate, consumer) &&
           "the slipgates chain the two colonies");
    assert(slipway_between(producer, consumer) == NULL && "the colonies trade through the slipgates only");
    assert(longest_route_slipways() >= 3 && "a route runs through both slipgates");
    play_until_quiet();
}

static void test_a_slipgate_chain_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_slipgate_chain_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t SPENT_PRODUCTION_GAME_SEED = 0xDAA66D13u;

/* Two colonies need what a third makes, and both are linked to it: its
   whole production goes out on one route, so the second slipway carries none. */
static void play_spent_production_game(void) {
    planet_t *colonies[REVEALED_COUNT];
    planet_t *producer = NULL;
    planet_t *consumers[2] = {NULL, NULL};
    int count;

    p8_fixed_set_rnd_state(SPENT_PRODUCTION_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    main_economy_typed()->credits = p8_fixed_from_int(STRUCTURE_CREDITS);
    count = colonize_nearest(colonies);
    for (int i = 0; i < count && consumers[1] == NULL; i++) {
        int found = 0;
        for (int k = 0; k < count && found < 2; k++) {
            if (i != k && needs_what_makes(colonies[k], colonies[i])) {
                consumers[found++] = colonies[k];
            }
        }
        if (found == 2) {
            producer = colonies[i];
        } else {
            consumers[0] = consumers[1] = NULL;
        }
    }
    assert(producer != NULL && "two colonies need what a third makes; reveal more planets if the galaxy changed");
    assert(link_nodes(producer, consumers[0]) && link_nodes(producer, consumers[1]) && "the producer links to both consumers");
    play_until_quiet();
    assert(producer->slipways.count == 2 && trade_route_list_count(&producer->outgoing_routes) == 1 &&
           "the producer's whole production goes out on one of its two slipways");
}

static void test_a_spent_production_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_spent_production_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t SYNTHESIZER_GAME_SEED = 0x5E7E5153u;

static int is_spent(const planet_t *synthesizer) {
    return synthesizer->synthesizable_resources != synthesizer_archetype.synthesizable_resources;
}

static const need_t *need_of(const planet_t *planet, p8_fixed_t resource) {
    for (int i = 0; (planet->has & PLANET_HAS_NEEDS) && i < planet->needs.count; i++) {
        const need_t *need = planet->needs.items[i];
        if (need->row.has_resource && need->row.resource == resource) {
            return need;
        }
    }
    return NULL;
}

static int is_satisfied(const planet_t *planet, int resource) {
    const need_t *need = need_of(planet, p8_fixed_from_int(resource));
    return need != NULL && need->has_satisfied && need->satisfied != 0;
}

enum { ORE = 21, FOOD = 28, ORE_MAKER = 0, ORE_AND_FOOD_EATER = 3 };

/* One synthesizer attached once ore was met, so it makes food; a second
   attached once food was met too, so it stays unspent. Reattached on fresh
   needs, both would pick ore. */
static void play_synthesizer_game(void) {
    planet_t *colonies[REVEALED_COUNT];
    planet_t *eater;
    planet_t *spent;
    planet_t *unspent;

    p8_fixed_set_rnd_state(SYNTHESIZER_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    main_economy_typed()->credits = p8_fixed_from_int(STRUCTURE_CREDITS);
    assert(colonize_nearest(colonies) > ORE_AND_FOOD_EATER && "four colonies; reveal more planets if the galaxy changed");
    eater = colonies[ORE_AND_FOOD_EATER];
    assert(colonies[ORE_MAKER]->production.resource == p8_fixed_from_int(ORE) && need_of(eater, p8_fixed_from_int(ORE)) == eater->needs.items[0] &&
           need_of(eater, p8_fixed_from_int(FOOD)) != NULL && "one colony makes ore, another needs ore first, then food; pick others if the galaxy changed");
    assert(link_nodes(colonies[ORE_MAKER], eater) && is_satisfied(eater, ORE) && "the ore maker feeds the eater");

    spent = build_structure(synthesizer_kind(), eater);
    assert(link_nodes(spent, eater) && is_spent(spent) && spent->production.resource == p8_fixed_from_int(FOOD) &&
           "a synthesizer skips a met need and makes the first unmet one it can");
    assert(is_satisfied(eater, FOOD));
    unspent = build_structure(synthesizer_kind(), eater);
    assert(link_nodes(unspent, eater) && !is_spent(unspent) && "a synthesizer with no unmet need to make stays unspent");
    play_until_quiet();
}

static void test_a_synthesizer_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_synthesizer_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t PROTOSTAR_GAME_SEED = 0x9A07057Au;

static int is_collapsible(const planet_t *planet) {
    const research_project_t *collapse = research_projects_find("collapse");
    assert(collapse != NULL && planet->planet_type != NULL);
    return strchr(collapse->planet_codes, planet->planet_type->name[0]) != NULL;
}

/* A revealed planet collapsed into a protostar that feeds a colony, played
   until its burst is gone and the camera is still. */
static void play_protostar_game(void) {
    planet_t *revealed[REVEALED_COUNT];
    research_project_t *collapse = research_projects_find("collapse");
    planet_t *collapsed = NULL;
    planet_t *protostar;
    planet_t *fed = NULL;
    const entity_list_t *nodes;

    p8_fixed_set_rnd_state(PROTOSTAR_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    nearest_unknown_planets(revealed);
    for (int i = 0; i < REVEALED_COUNT; i++) {
        planet_roll_type(revealed[i]);
        if (collapsed == NULL && revealed[i]->header.state != ENTITY_STATE_VANISHING && is_collapsible(revealed[i])) {
            collapsed = revealed[i];
        }
    }
    assert(collapsed != NULL && "a revealed planet collapses; pick another seed if the galaxy changed");
    assert(economy_spend(main_economy_typed(), research_project_cost(collapse)) && "the starting credits pay for a collapse");
    main_economy_typed()->year_ticks = 0;
    planet_apply_project(collapsed, collapse);
    nodes = entities_by_tag(TAG_NODE);
    protostar = (planet_t *)nodes->items[nodes->count - 1];
    assert(protostar->header.class == protostar_class() && "the collapse makes a protostar");
    assert(particle_zone_live(PARTICLE_SLOT_BURST) > 0 && main_camera()->shake != 0 && "the collapse bursts and shakes the camera");
    main_economy_typed()->credits = p8_fixed_from_int(STRUCTURE_CREDITS);
    for (int i = 0; i < REVEALED_COUNT && fed == NULL; i++) {
        if (revealed[i] != collapsed && revealed[i]->header.state != ENTITY_STATE_VANISHING) {
            main_economy_typed()->year_ticks = 0;
            colonize(revealed[i]);
            fed = link_nodes(protostar, revealed[i]) ? revealed[i] : NULL;
        }
    }
    assert(fed != NULL && fed->has_production_bonus && "a colony fed by the protostar keeps the production bonus restore under test");
    play_until_quiet();
}

static void test_a_protostar_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_protostar_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

/* Fixes the galaxy, whatever the cases before this one drew. */
static const uint32_t RESEARCH_GAME_SEED = 0x7EC4C0DEu;

enum { RESEARCH_POINTS = 200, RESEARCH_YEAR = 3405, RESEARCH_YEAR_TICKS = 7, RESEARCH_ERA = 3 };

/* Not in the icons' registration order (slipgate, infraspace gate, trading
   hub, synthesizer): their slots then depend on the invent order. */
static const char *const research_invent_order[] = {"void synthesis", "trade league", "slipgates", "space folding"};

/* Four techs invented out of icon order, past two era thresholds, with
   research points, year and year ticks away from a fresh game's. */
static void play_research_game(void) {
    economy_t *economy;

    p8_fixed_set_rnd_state(RESEARCH_GAME_SEED);
    start_game_on_reasonable();
    play_frame();
    play_frame();
    economy = main_economy_typed();
    /* Before the invents: the score panel shows them from the next recompute on. */
    economy->research_points = p8_fixed_from_int(RESEARCH_POINTS);
    economy->year = p8_fixed_from_int(RESEARCH_YEAR);
    economy->year_ticks = p8_fixed_from_int(RESEARCH_YEAR_TICKS);
    for (size_t i = 0; i < sizeof research_invent_order / sizeof research_invent_order[0]; i++) {
        const technologies_entry_t *tech = map_tables_technologies_lookup(research_invent_order[i]);
        assert(tech != NULL && "the invent order names technologies_table entries");
        economy_invent(economy, tech);
    }
    assert(economy->era == p8_fixed_from_int(RESEARCH_ERA) && economy->era != economy_archetype.era && "the invents cross two era thresholds");
    assert(economy->research_points > 0 && "research points are left over, so their restore stays under test");
    assert(economy->year != economy_archetype.year && economy->year_ticks != economy_archetype.year_ticks &&
           "year and year ticks differ from a fresh game's, so their restores stay under test");
    assert(structure_icon_is_revealed(&icon_of(synthesizer_kind())->header) && !structure_icon_is_revealed(&icon_of(infraspace_gate_kind())->header) &&
           "only the invented techs reveal their icons");
    assert(icon_of(synthesizer_kind())->header.pos.x > icon_of(slipgate_kind())->header.pos.x &&
           "the synthesizer, invented first, takes the slot a registration-order replay gives the slipgate");
    play_until_quiet();
}

static void test_research_resumes_equal_from_the_title(void) {
    load_cart_to_title();
    play_research_game();
    save_check_capture(&g_saved);
    play_compared_frame(&g_saved);

    cart_init();
    play_frame();
    save_check_resume(&g_saved);
    save_check_capture(&g_resumed);
    play_compared_frame(&g_resumed);
    assert_equal(&g_saved, &g_resumed);
}

int main(void) {
    test_a_just_started_game_resumes_equal_from_the_title();
    test_a_one_colony_game_resumes_equal_from_the_title();
    test_a_pad_selection_resumes_equal_from_the_title();
    test_a_flickering_vanishing_planet_resumes_equal_from_the_title();
    test_a_revealed_wormhole_area_resumes_equal_from_the_title();
    test_linked_colonies_resume_equal_from_the_title();
    test_every_structure_kind_resumes_equal_from_the_title();
    test_a_slipgate_chain_resumes_equal_from_the_title();
    test_a_spent_production_resumes_equal_from_the_title();
    test_a_synthesizer_resumes_equal_from_the_title();
    test_a_protostar_resumes_equal_from_the_title();
    test_research_resumes_equal_from_the_title();
    printf("test_save_load: all tests passed\n");
    return 0;
}
