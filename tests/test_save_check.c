#include "devtools/save_check.h"

#include "engine/dispatch_context.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/frame_zone.h"
#include "game/cart.h"
#include "game/game_data.h"
#include "game/main_economy.h"
#include "game/message_box.h"
#include "game/particle_zone.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/probe.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/ui.h"
#include "game/ui_zone.h"
#include "game/ui_button.h"
#include "game/visual_random.h"
#include "p8/sprite_sheet.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static save_check_snapshot_t g_first;
static save_check_snapshot_t g_second;
static char g_report[512];

static void play_frame(void) {
    cart_update60();
    cart_draw();
}

static void play_frame_with_particles_hidden(void) {
    save_check_hide_particles(1);
    play_frame();
}

static void start_game_from_seed(uint32_t seed) {
    button_t reasonable = {0};
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);
    cart_load();
    p8_fixed_set_rnd_state(seed);
    cart_init();
    reasonable.name = difficulty_options_table[1].name;
    reasonable.has_trade = 1;
    reasonable.trade = p8_fixed_from_int(difficulty_options_table[1].trade);
    reasonable.has_type_bias = 1;
    reasonable.type_bias = difficulty_options_table[1].type_bias;
    cart_start_game(&reasonable);
    play_frame();
    play_frame();
}

static void start_game_on_reasonable(void) {
    start_game_from_seed(p8_fixed_rnd_state());
}

static void test_two_snapshots_of_an_untouched_game_compare_equal(void) {
    start_game_on_reasonable();
    play_frame_with_particles_hidden();
    save_check_capture(&g_first);
    save_check_capture_screen(&g_first);
    save_check_capture(&g_second);
    save_check_capture_screen(&g_second);
    assert(g_first.byte_count > 0);
    assert(strstr(g_first.dump, "planet ") != NULL);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) == 0);
    save_check_hide_particles(0);
}

static void test_a_credit_change_is_reported_in_the_dump_and_the_bytes(void) {
    start_game_on_reasonable();
    save_check_capture(&g_first);
    main_economy_typed()->credits += p8_fixed_from_int(1);
    save_check_capture(&g_second);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) != 0);
    assert(strstr(g_report, "byte") != NULL);

    memcpy(g_second.bytes, g_first.bytes, sizeof g_first.bytes);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) != 0);
    assert(strstr(g_report, "resources credits") != NULL);
}

static void test_a_changed_pixel_is_reported_with_its_position(void) {
    start_game_on_reasonable();
    play_frame_with_particles_hidden();
    save_check_capture(&g_first);
    save_check_capture_screen(&g_first);
    memory.screen[37][101] ^= 1;
    save_check_capture(&g_second);
    save_check_capture_screen(&g_second);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) != 0);
    assert(strstr(g_report, "pixel 101 37") != NULL);
    save_check_hide_particles(0);
}

static void test_every_differing_part_is_reported(void) {
    start_game_on_reasonable();
    play_frame_with_particles_hidden();
    save_check_capture(&g_first);
    save_check_capture_screen(&g_first);
    main_economy_typed()->credits += p8_fixed_from_int(1);
    memory.screen[12][34] ^= 1;
    save_check_capture(&g_second);
    save_check_capture_screen(&g_second);
    save_check_hide_particles(0);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) != 0);
    assert(strstr(g_report, "byte ") != NULL);
    assert(strstr(g_report, "resources credits") != NULL);
    assert(strstr(g_report, "pixel 34 12") != NULL);
}

static void test_a_screen_recorded_on_one_side_only_is_not_compared(void) {
    start_game_on_reasonable();
    play_frame_with_particles_hidden();
    save_check_capture(&g_first);
    save_check_capture_screen(&g_first);
    save_check_hide_particles(0);
    memory.screen[0][0] ^= 1;
    save_check_capture(&g_second);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) == 0);
}

static void test_a_difference_confined_to_zone_and_cap_lines_compares_equal(void) {
    frame_zone_mark_t mark;
    start_game_on_reasonable();
    mark = frame_zone_mark();
    save_check_capture(&g_first);
    assert(allocator_alloc(frame_zone(), 4096) != NULL);
    save_check_capture(&g_second);
    frame_zone_rewind(mark);
    assert(save_check_compare(&g_first, &g_second, g_report, sizeof g_report) == 0);
    assert(strstr(g_first.dump, "zone ") == NULL && strstr(g_first.dump, "\ncap ") == NULL);
    assert(strstr(g_first.dump, "budget zones") != NULL);
}

enum { PROBE_FRAMES = 12 };

static unsigned char g_shown_screen[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];

static vec_t probe_layer_camera(const probe_t *probe) {
    p8_fixed_t parallax;
    vec_t camera;
    assert(game_data_fixed_seq_at(layer_fades_table, LAYER_FADES_COUNT, p8_fixed_flr_int(probe->header.layer), &parallax));
    camera = vec_mul(entity_pos(main_camera()), parallax);
    return vec_make(p8_fixed_flr(camera.x), p8_fixed_flr(camera.y));
}

static probe_t *drop_probe_on_screen(void) {
    probe_t *probe = probe_new(NULL);
    vec_t screen = vec_make(p8_fixed_from_int(44), p8_fixed_from_int(28));
    mouse_pos_t mouse = {.world = vec_add(probe_layer_camera(probe), screen), .screen = screen,
                         .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
    probe->header.class->drag(&probe->header, mouse);
    assert(targeting_confirm(&probe->header) && "the probe drops on an empty spot of the screen");
    return probe;
}

/* Plays the probe game from seed; the last frame is drawn with particles
   hidden or shown, and the switch is left off. */
static probe_t *play_probe_game(uint32_t seed, int hidden) {
    probe_t *probe;
    start_game_from_seed(seed);
    probe = drop_probe_on_screen();
    for (int i = 0; i < PROBE_FRAMES; i++) {
        play_frame();
    }
    assert(probe->emitter.particles.count > 0 && "the probe emits particles on the compared frame");
    save_check_hide_particles(hidden);
    play_frame();
    save_check_capture(hidden ? &g_first : &g_second);
    if (hidden) {
        save_check_capture_screen(&g_first);
    }
    save_check_hide_particles(0);
    return probe;
}

static int is_particle_pixel(const probe_t *probe, int x, int y) {
    vec_t camera = probe_layer_camera(probe);
    vec_t centre = entity_pos(&probe->header);
    const pointer_list_t *particles = &probe->emitter.particles;
    for (int i = 0; i < particles->count; i++) {
        const particle_t *particle = particles->items[i];
        int px = p8_fixed_flr_int(p8_fixed_add(centre.x, particle->pos.x)) - p8_fixed_flr_int(camera.x);
        int py = p8_fixed_flr_int(p8_fixed_add(centre.y, particle->pos.y)) - p8_fixed_flr_int(camera.y);
        if (px == x && py == y) {
            return 1;
        }
    }
    return 0;
}

static void test_hidden_particles_change_pixels_only(void) {
    uint32_t seed = p8_fixed_rnd_state();
    uint32_t visual_seed = visual_random_state();
    uint32_t hidden_rnd;
    uint32_t hidden_visual;
    probe_t *probe;
    int differing = 0;

    play_probe_game(seed, 1);
    hidden_rnd = p8_fixed_rnd_state();
    hidden_visual = visual_random_state();
    visual_random_set_state(visual_seed);
    probe = play_probe_game(seed, 0);
    memcpy(g_shown_screen, memory.screen, sizeof g_shown_screen);

    assert(p8_fixed_rnd_state() == hidden_rnd && "hiding particles draws no game number");
    assert(visual_random_state() == hidden_visual && "hiding particles draws no visual number");
    if (save_check_compare(&g_first, &g_second, g_report, sizeof g_report) != 0) {
        fprintf(stderr, "test_save_check: %s\n", g_report);
        assert(0 && "hiding particles changes no state");
    }
    for (int y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (int x = 0; x < P8_SCREEN_WIDTH; x++) {
            if (g_first.screen[y][x] != g_shown_screen[y][x]) {
                assert(is_particle_pixel(probe, x, y) && "a hidden frame differs only where a particle is drawn");
                differing++;
            }
        }
    }
    assert(differing > 0 && "a shown particle is on the screen");
}

static uint8_t g_raw_before[SAVE_STATE_MAX_BYTES];
static uint8_t g_raw_after[SAVE_STATE_MAX_BYTES];
static save_state_t g_raw_state;

/* The save bytes of the live game, without save_check_capture's assertions. */
static int encode_live_game(uint8_t *out) {
    save_state_capture(&g_raw_state);
    return save_state_encode(&g_raw_state, out);
}

static planet_t *harvest_a_revealed_planet(void) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    research_project_t *harvest = NULL;
    for (int i = 1; i <= research_projects_count(); i++) {
        research_project_t *project = research_projects_at(i);
        if (project->class_name != NULL && strcmp(project->class_name, "burst") == 0) {
            harvest = project;
        }
    }
    assert(harvest != NULL && "the harvest is the research project whose class name is \"burst\"");
    for (int i = 0; i < planets->count; i++) {
        planet_t *planet = (planet_t *)planets->items[i];
        if (planet->header.state == ENTITY_STATE_UNKNOWN) {
            planet_roll_type(planet);
            if (planet->header.state != ENTITY_STATE_VANISHING && planet->slipways.count == 0) {
                planet_apply_project(planet, harvest);
                return planet;
            }
        }
    }
    assert(0 && "the galaxy holds an unknown planet to harvest");
    return NULL;
}

static int every_shake_is_zero(void) {
    const entity_list_t *live = live_entities();
    for (int i = 0; i < live->count; i++) {
        const entity_t *e = live->items[i];
        if (entity_has(e, ENTITY_HAS_SHAKE) && e->shake != 0) {
            return 0;
        }
    }
    return main_camera()->shake == 0;
}

static void test_the_drop_removes_bursts_shakes_and_the_camera_drift_only(void) {
    int before;
    start_game_on_reasonable();
    harvest_a_revealed_planet();
    play_frame();
    main_camera()->shake = p8_fixed_from_int(3);
    entity_set_pos(main_camera(), vec_add(entity_map_pos(main_camera()), vec_make(p8_fixed_from_int(5), p8_fixed_from_int(-2))));
    assert(particle_zone_live(PARTICLE_SLOT_BURST) > 0 && "the harvest bursts");
    before = encode_live_game(g_raw_before);

    save_check_drop_unsaved_state();

    assert(particle_zone_live(PARTICLE_SLOT_BURST) == 0);
    assert(every_shake_is_zero());
    assert(main_camera()->pos.x == main_camera()->map_pos.x && main_camera()->pos.y == main_camera()->map_pos.y);
    assert(encode_live_game(g_raw_after) == before && memcmp(g_raw_before, g_raw_after, (size_t)before) == 0);
}

static void test_the_drop_removes_an_entity_being_placed(void) {
    const entity_list_t *icons;
    entity_t *held = NULL;
    entity_id_t held_id;
    int before;
    start_game_on_reasonable();
    icons = entities_by_message(ENTITY_MESSAGE_REFRESH);
    for (int i = 0; i < icons->count && held == NULL; i++) {
        entity_t *e = icons->items[i];
        if (e->class == structure_icon_class() && ((structure_icon_t *)e)->kind == structure_kind()) {
            held = entity_get(structure_icon_left_press((structure_icon_t *)e).held);
        }
    }
    assert(held != NULL && held->state == ENTITY_STATE_PLACING && "an icon press holds a structure in placing");
    held_id = entity_id_link(held);
    before = encode_live_game(g_raw_before);

    save_check_drop_unsaved_state();

    assert(entity_get(held_id) == NULL && "the structure being placed is gone");
    assert(encode_live_game(g_raw_after) == before && memcmp(g_raw_before, g_raw_after, (size_t)before) == 0);
}

static void test_the_drop_removes_live_toasts(void) {
    toast_keys_t keys = {0};
    int before;
    start_game_on_reasonable();
    keys.has_pos = 1;
    keys.pos = vec_make(p8_fixed_from_int(40), p8_fixed_from_int(30));
    toast_keys_set_text(&keys, "$1");
    assert(toast_new(&keys) != NULL);
    assert(ui_zone_live(SLOT_KIND_TOAST) > 0);
    before = encode_live_game(g_raw_before);

    save_check_drop_unsaved_state();

    assert(ui_zone_live(SLOT_KIND_TOAST) == 0 && "the save holds no toast");
    assert(encode_live_game(g_raw_after) == before && memcmp(g_raw_before, g_raw_after, (size_t)before) == 0);
}

static void test_the_drop_hides_the_pad_crosshair(void) {
    int before;
    start_game_on_reasonable();
    ui_set_pad_crosshair(main_ui(), 1);
    before = encode_live_game(g_raw_before);

    save_check_drop_unsaved_state();

    assert(main_ui()->pad_crosshair == 0 && "the crosshair follows the pad input, which the save never holds");
    assert(encode_live_game(g_raw_after) == before && memcmp(g_raw_before, g_raw_after, (size_t)before) == 0);
}

static void test_the_drop_removes_a_dead_entity_waiting_to_leave(void) {
    planet_t *harvested;
    entity_id_t harvested_id;
    int before;
    start_game_on_reasonable();
    harvested = harvest_a_revealed_planet();
    assert(harvested->header.is_dead && "a harvested planet dies at once and leaves on the next update");
    harvested_id = entity_id_link(&harvested->header);
    before = encode_live_game(g_raw_before);

    save_check_drop_unsaved_state();

    assert(entity_get(harvested_id) == NULL && "the dead planet is gone from the live list");
    assert(encode_live_game(g_raw_after) == before && memcmp(g_raw_before, g_raw_after, (size_t)before) == 0);
}

static void capture_a_screen_drawn_with_particles_shown(void) {
    start_game_on_reasonable();
    play_frame();
    save_check_capture_screen(&g_first);
}

static void test_a_screen_drawn_with_particles_shown_is_refused(void) {
    pid_t child;
    int status;
    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        capture_a_screen_drawn_with_particles_shown();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && "capture_screen refuses a frame drawn with particles shown");
}

static void capture_a_screen_with_a_message_box_open(void) {
    message_box_keys_t keys = {0};
    start_game_on_reasonable();
    play_frame_with_particles_hidden();
    assert(message_box_new(&keys) != NULL);
    save_check_capture_screen(&g_first);
}

static void test_a_message_box_keeps_the_dump_and_drops_the_screen(void) {
    message_box_keys_t keys = {0};
    pid_t child;
    int status;

    start_game_on_reasonable();
    assert(save_check_screen_comparable() == 1 && "a quiet game shows no box");
    assert(message_box_new(&keys) != NULL);
    save_check_capture(&g_first);
    assert(g_first.byte_count > 0 && strstr(g_first.dump, "planet ") != NULL);
    assert(save_check_screen_comparable() == 0);

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        capture_a_screen_with_a_message_box_open();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && "capture_screen refuses a frame with a message box open");
}

int main(void) {
    test_two_snapshots_of_an_untouched_game_compare_equal();
    test_a_credit_change_is_reported_in_the_dump_and_the_bytes();
    test_a_changed_pixel_is_reported_with_its_position();
    test_a_screen_recorded_on_one_side_only_is_not_compared();
    test_a_difference_confined_to_zone_and_cap_lines_compares_equal();
    test_hidden_particles_change_pixels_only();
    test_a_screen_drawn_with_particles_shown_is_refused();
    test_the_drop_removes_bursts_shakes_and_the_camera_drift_only();
    test_the_drop_removes_an_entity_being_placed();
    test_the_drop_removes_live_toasts();
    test_the_drop_hides_the_pad_crosshair();
    test_the_drop_removes_a_dead_entity_waiting_to_leave();
    test_a_message_box_keeps_the_dump_and_drops_the_screen();
    test_every_differing_part_is_reported();
    printf("test_save_check: all tests passed\n");
    return 0;
}
