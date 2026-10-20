/* Live slots of the UI and particle zones, and the bytes of the game zone,
 * on replays of tests/e2e/ and tests/fixtures/ driven as src/cmd/replay.c
 * drives a frame (input_step_run, cart_update60, cart_draw): after a session each
 * slot kind returns to its starting level, and none rises during an idle
 * window. Each count is read after a flush of the exit queue. One replay per
 * child process: the zone maxima and module state last as long as the
 * process. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "devtools/replay_source.h"
#include "p8/sprite_sheet.h"
#include "game/background.h"
#include "game/cart.h"
#include "game/input_step.h"
#include "engine/entity_id.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/zone_sizes.h"
#include "game/main_economy.h"
#include "game/game_data.h"
#include "game/gen.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "game/layout.h"
#include "game/map_tables.h"
#include "p8/vram.h"
#include "game/mouse.h"
#include "game/particle.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_trade.h"
#include "game/probe.h"
#include "game/structure_icon.h"
#include "game/score_panel.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "game/trade.h"
#include "game/ui.h"
#include "game/wormhole.h"
#include "engine/allocator.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"
#include "game/particle_zone.h"
#include "engine/game_zone.h"

#define BEFORE_F0 (-1)
#define MAX_READINGS 4

static const char *const NAMES[SLOT_KIND_COUNT] = {"button", "modal_backdrop", "message_box", "toast", "row", "ephemerals"};

typedef struct {
    int frame;
    int live[SLOT_KIND_COUNT];
    int particle;
    int particles;
    int particles_help_screen;
    int burst;
    int transit_dots;
    size_t game_zone;
} reading_t;

/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
}

static void take_reading(reading_t *r, int frame) {
    cart_flush_exit_queue();
    r->frame = frame;
    for (int s = 0; s < SLOT_KIND_COUNT; s++) {
        r->live[s] = ui_zone_live((slot_kind_t)s);
    }
    r->particle = particle_zone_live(PARTICLE_SLOT_PARTICLE);
    r->particles = particle_zone_live(PARTICLE_SLOT_LIST);
    r->particles_help_screen = particle_zone_live(PARTICLE_SLOT_HELP_LIST);
    r->burst = particle_zone_live(PARTICLE_SLOT_BURST);
    r->transit_dots = particle_zone_live(PARTICLE_SLOT_TRANSIT_DOT);
    r->game_zone = game_zone_used();
}

/* Particles and lists held by the emitters among the live entities,
 * registered bursts, and transit dots of the slipways under the slipway tag:
 * what the particle zone counts must say. */
typedef struct {
    int particle;
    int lists;
    int bursts;
    int trails;
} registered_t;

static registered_t count_registered(void) {
    registered_t n = {0, 0, 0, 0};
    const entity_list_t *hv = live_entities();
    for (int i = 0; hv != NULL && i < hv->count; i++) {
        entity_t *e = hv->items[i];
        const emitter_t *emitter = emitter_of(e);
        if (emitter != NULL && particles_of(emitter) != NULL) {
            n.lists++;
            n.particle += particles_of(emitter)->count;
        }
        if (e->class == burst_class()) n.bursts++;
    }
    const entity_list_t *jc = entities_by_tag(TAG_SLIPWAY);
    for (int i = 0; jc != NULL && i < jc->count; i++) {
        const slipway_t *s = (const slipway_t *)jc->items[i];
        n.trails += s->transit_dots.count;
    }
    return n;
}

/* Read after a flush: a lost particle, list or burst breaks it on the very
 * frame it is lost. */
static void check_invariant(const char *e2e, int frame) {
    reading_t r;
    take_reading(&r, frame);
    registered_t n = count_registered();
    if (r.particle != n.particle || r.particles + r.particles_help_screen != n.lists || r.burst != n.bursts || r.transit_dots != n.trails) {
        fprintf(stderr, "  %s f%d: particle=%d (registered %d) particles+particles_help_screen=%d (lists %d) burst=%d (bursts %d) transit_dots=%d (trails %d)\n",
                e2e, frame, r.particle, n.particle, r.particles + r.particles_help_screen, n.lists, r.burst, n.bursts, r.transit_dots, n.trails);
        assert(0 && "a particle, particle list, burst or transit dot lives outside the registered emitters and the slipway tag");
    }
}

static void print_reading(const char *e2e, const reading_t *r) {
    printf("  %s f%d:", e2e, r->frame);
    for (int s = 0; s < SLOT_KIND_COUNT; s++) {
        printf(" %s=%d", NAMES[s], r->live[s]);
    }
    printf(" particle=%d particles=%d particles_help_screen=%d burst=%d game=%lu\n", r->particle, r->particles, r->particles_help_screen, r->burst,
           (unsigned long)r->game_zone);
}

static input_step_t g_step;

/* Runs cart_load and cart_init to replay the named e2e (NULL: no input; a
 * path when the name holds a '/') and returns its input, which is static. */
static p8_input_t *load_e2e(const char *e2e) {
    static p8_vram_t mem;
    static p8_graphics_t gfx;
    static p8_input_t input;
    char path[256];

    p8_vram_init(&mem);
    p8_gfx_init(&gfx);
    p8_sprite_sheet_load(&mem);
    static bindings_t bindings;
    p8_input_init(&input);
    bindings = bindings_compiled;
    int attract_after = ATTRACT_AFTER_DEFAULT;
    if (e2e != NULL) {
        snprintf(path, sizeof path, strchr(e2e, '/') != NULL ? "%s" : "tests/e2e/%s.json", e2e);
        assert(p8_input_load(&input, path));
        static replay_source_t source;
        static replay_source_bindings_report_t report;
        assert(replay_source_open(&source, path) == REPLAY_SOURCE_LOADED);
        assert(replay_source_seed_storage(&source) && "the header's initial_save is seeded as the replay tool seeds it");
        replay_source_bindings_status_t status = replay_source_read_bindings(&source, &bindings_compiled, &bindings, &report);
        if (status != REPLAY_SOURCE_BINDINGS_DEFAULT && status != REPLAY_SOURCE_BINDINGS_FIXTURE) fprintf(stderr, "%s\n", report.message);
        assert((status == REPLAY_SOURCE_BINDINGS_DEFAULT || status == REPLAY_SOURCE_BINDINGS_FIXTURE) &&
               "an e2e plays with its bindings fixture, as the replay tool plays it");
        attract_after = replay_source_attract_after(&source);
        replay_source_close(&source);
    }
    dispatch_context_set_gfx(&gfx);
    dispatch_context_set_mem(&mem);
    dispatch_context_set_input(&input);
    input_step_init(&g_step, &bindings);
    input_step_set_attract_after(&g_step, attract_after);
    cart_load();
    cart_init();
    return &input;
}

static void play_input(const p8_input_t *input) {
    input_step_run(&g_step, &input->frames[input->current_frame]);
}

/* Takes and prints a reading after each listed frame (BEFORE_F0: after
 * cart_init, before frame 0); every listed frame must lie in the replay. */
static void replay_e2e(const char *e2e, const int *frames, int n, reading_t *out) {
    p8_input_t *input = load_e2e(e2e);
    int k = 0;

    if (k < n && frames[k] == BEFORE_F0) {
        take_reading(&out[k], BEFORE_F0);
        print_reading(e2e, &out[k]);
        k++;
    }
    for (int i = 0; i < input->frame_count && k < n; i++) {
        play_input(input);
        cart_update60();
        cart_draw();
        if (frames[k] == i) {
            take_reading(&out[k], i);
            print_reading(e2e, &out[k]);
            k++;
        }
        if (i < input->frame_count - 1) p8_input_step(input);
    }
    assert(k == n && "a reading frame lies past the end of the replay");
}

/* Replays the whole e2e, checking the invariant after every frame and that
 * the replay starts a game. Returns 1 when the attract mode played a demo
 * frame, 0 otherwise. */
static int replay_under_invariant(const char *e2e) {
    p8_input_t *input = load_e2e(e2e);
    check_invariant(e2e, BEFORE_F0);
    int started = 0;
    int reached_demo = 0;
    for (int i = 0; i < input->frame_count; i++) {
        play_input(input);
        cart_update60();
        cart_draw();
        check_invariant(e2e, i);
        started |= main_economy_typed() != NULL;
        reached_demo |= input_step_attract_phase(&g_step) == ATTRACT_PHASE_PLAYING;
        if (i < input->frame_count - 1) p8_input_step(input);
    }
    assert(started && "the replay never leaves the title screen: its input is not played");
    return reached_demo;
}

/* Everything but the particle count, which an active emitter moves without
 * losing anything: the invariant follows it frame by frame. */
static int same_counts(const reading_t *a, const reading_t *b) {
    return memcmp(a->live, b->live, sizeof a->live) == 0 && a->particles == b->particles && a->particles_help_screen == b->particles_help_screen &&
           a->burst == b->burst;
}

static int none_rises(const reading_t *a, const reading_t *b) {
    for (int s = 0; s < SLOT_KIND_COUNT; s++) {
        if (b->live[s] > a->live[s]) return 0;
    }
    return 1;
}

static void in_child(void (*test)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0 && "a live slots reading failed");
}

/* Returns what the case returns, run in a child (below 255). */
static int count_in_child(int (*test)(void)) {
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        int n = test();
        fflush(stdout);
        _exit(n < 255 ? n : 255);
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) < 255 && "a counted reading failed");
    return WEXITSTATUS(status);
}

#define ASAN_LOG "tmp/test_live_slots_asan.txt"

static char *read_file(const char *path, size_t *len);

static void in_child_dying_by_asan(void (*test)(void)) {
    fflush(stdout);
    remove(ASAN_LOG);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        assert(freopen(ASAN_LOG, "w", stderr) != NULL);
        test();
        _exit(0);
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    size_t len;
    char *err = read_file(ASAN_LOG, &len);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        fprintf(stderr, "%s", err);
        assert(0 && "the released block reads back: its generation was not released");
    }
    if (strstr(err, "use-after-poison") == NULL) {
        fprintf(stderr, "%s", err);
        assert(0 && "expected use-after-poison on the released block");
    }
    printf("  reading the released block crashes (use-after-poison)\n");
    free(err);
    remove(ASAN_LOG);
}

static char g_e2e[128];

static int invariant_on_current_e2e(void) {
    return replay_under_invariant(g_e2e);
}

/* Every frame of every e2e of tests/e2e/; returns how many e2e ran, in *pad
 * how many of them are pad_ replays, and in *demo how many reach the demo. */
static int invariant_on_all_e2e(int *pad, int *demo) {
    DIR *dir = opendir("tests/e2e");
    struct dirent *ent;
    int n = 0;
    *pad = 0;
    *demo = 0;
    assert(dir != NULL);
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len <= 5 || strcmp(ent->d_name + len - 5, ".json") != 0) continue;
        assert(len - 5 < sizeof g_e2e);
        memcpy(g_e2e, ent->d_name, len - 5);
        g_e2e[len - 5] = '\0';
        *demo += count_in_child(invariant_on_current_e2e);
        n++;
        *pad += strncmp(g_e2e, "pad_", 4) == 0;
    }
    closedir(dir);
    return n;
}

/* f28: 'restart'; f39: title screen, before the f40 click. */
static void pad_pause_restart_returns_to_title(void) {
    const int frames[] = {BEFORE_F0, 39};
    reading_t r[2];
    replay_e2e("pad_pause_restart", frames, 2, r);
    assert(same_counts(&r[0], &r[1]) && "reset_world must give back every slot it drops");
    assert(r[0].particles_help_screen == 1 && r[0].particles == 0 && r[0].burst == 0);
    /* No help screen particle dies before 115 draws: 5 per draw, no draw
     * before f0, and the draws of f28 to f39 for the help screen of 'restart'. */
    assert(r[0].particle == 0 && r[1].particle == 5 * 12 && "reset_world must give back the help screen particles");
}

/* volatile: the optimizer must not drop the read of a released sentinel,
 * the very read the case observes. */
static volatile unsigned char g_spend;

static p8_input_t *g_input;
static int g_frame;

/* Without take_reading: its flush of the exit queue would release the
 * generation itself, instead of the cart_update60 of the frame after the
 * click. */
static void play_until(int last) {
    for (; g_frame <= last && g_frame < g_input->frame_count; g_frame++) {
        play_input(g_input);
        cart_update60();
        cart_draw();
        if (g_frame < g_input->frame_count - 1) p8_input_step(g_input);
    }
    assert(g_frame == last + 1 && "a sentinel frame lies past the end of the replay");
}

static void start_e2e(const char *e2e) {
    g_input = load_e2e(e2e);
    g_frame = 0;
}

static unsigned char *sentinel(void) {
    unsigned char *s = allocator_alloc(game_zone(), 64);
    for (int i = 0; i < 64; i++) s[i] = (unsigned char)i;
    return s;
}

static void check_sentinel(const unsigned char *s) {
    for (int i = 0; i < 64; i++) assert(s[i] == i && "the game zone sentinel changed");
}

/* f40: 'challenging' click, held through the loading screen; f42:
 * cart_start_game: the zone flips, and the generation of the f28 title
 * screen lives until the end of the frame. */
static void pad_pause_restart_keeps_title_during_click(void) {
    start_e2e("pad_pause_restart");
    play_until(39);
    assert(!game_zone_pending());
    unsigned char *s = sentinel();
    size_t used_before = game_zone_used();
    play_until(42);
    assert(game_zone_pending() && "the f42 generation flips the game zone");
    check_sentinel(s);
    assert(game_zone_used() >= used_before && "the used bytes count the quit game until it is released");
    printf("  pad_pause_restart f42: game=%lu, sentinel read\n", (unsigned long)game_zone_used());
}

/* f43: cart_update60 flushes the exit queue, then releases the title
 * generation. */
static void pad_pause_restart_releases_title_next_frame(void) {
    start_e2e("pad_pause_restart");
    play_until(39);
    unsigned char *s = sentinel();
    play_until(43);
    g_spend = s[63];
}

/* f28: 'restart' (cart_init) flips the zone, so the title screen has its
 * own generation; the quit game lives until the end of the click frame. */
static void pad_pause_restart_keeps_game_during_restart_click(void) {
    start_e2e("pad_pause_restart");
    play_until(27);
    assert(!game_zone_pending());
    unsigned char *s = sentinel();
    play_until(28);
    assert(game_zone_pending() && "'restart' flips the game zone");
    check_sentinel(s);
}

/* f29: cart_update60 releases the generation of the game quit at f28. */
static void pad_pause_restart_releases_game_after_restart(void) {
    start_e2e("pad_pause_restart");
    play_until(27);
    unsigned char *s = sentinel();
    play_until(29);
    assert(!game_zone_pending());
    g_spend = s[63];
}

/* f29: the quit game's economy and score panel died at the release; the
 * title screen must not keep them as main economy and main score panel,
 * which the exit-queue flush, the cheat and make state read. */
static void pad_pause_restart_title_drops_quit_economy_and_panel(void) {
    start_e2e("pad_pause_restart");
    play_until(29);
    assert(main_economy() == NULL && "the title after 'restart' keeps the released economy");
    assert(main_score_panel() == NULL && "the title after 'restart' keeps the released score panel");
}

static const planet_t *planet_of_first_game(void) {
    start_e2e("pad_pause_restart");
    play_until(27);
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    assert(jo != NULL && jo->count > 0 && "the first game has planets");
    return (const planet_t *)jo->items[0];
}

/* f28: the 'restart' click flips the zone; the planets of the quit game
 * still live. */
static void pad_pause_restart_keeps_quit_planets_during_click(void) {
    const planet_t *p = planet_of_first_game();
    play_until(28);
    assert(game_zone_pending());
    g_spend = (unsigned char)p->has_production_bonus;
}

/* f29: they die with their generation. */
static void pad_pause_restart_releases_quit_planets(void) {
    const planet_t *p = planet_of_first_game();
    play_until(29);
    g_spend = (unsigned char)p->has_production_bonus;
}

static const economy_t *economy_of_first_game(void) {
    start_e2e("pad_pause_restart");
    play_until(27);
    const economy_t *fm = main_economy_typed();
    assert(fm != NULL && "the first game has its main economy");
    return fm;
}

/* f28: its generation lives until the end of the frame. */
static void pad_pause_restart_keeps_quit_economy_during_click(void) {
    const economy_t *fm = economy_of_first_game();
    play_until(28);
    assert(game_zone_pending());
    g_spend = (unsigned char)fm->credits;
}

/* f29: it dies with its generation. */
static void pad_pause_restart_releases_quit_economy(void) {
    const economy_t *fm = economy_of_first_game();
    play_until(29);
    g_spend = (unsigned char)fm->credits;
}

/* f27: the live entities hold the structure icons of the first game; the
 * reset_world of cart_init removes them at f28. */
static const structure_icon_t *icon_of_first_game(void) {
    start_e2e("pad_pause_restart");
    play_until(27);
    const entity_list_t *hv = live_entities();
    for (int i = 0; hv != NULL && i < hv->count; i++) {
        if (hv->items[i]->class == structure_icon_class()) return (const structure_icon_t *)hv->items[i];
    }
    assert(0 && "the first game has structure icons among the live entities");
    return NULL;
}

static void pad_pause_restart_keeps_quit_icons_during_click(void) {
    const structure_icon_t *ie = icon_of_first_game();
    play_until(28);
    assert(game_zone_pending());
    g_spend = (unsigned char)ie->is_revealed;
}

static void pad_pause_restart_releases_quit_icons(void) {
    const structure_icon_t *ie = icon_of_first_game();
    play_until(29);
    g_spend = (unsigned char)ie->is_revealed;
}

static const probe_t *help_screen_in_live_entities(void) {
    const entity_list_t *hv = live_entities();
    for (int i = 0; hv != NULL && i < hv->count; i++) {
        if (hv->items[i]->class == help_screen_class()) return (const probe_t *)hv->items[i];
    }
    assert(0 && "the title screen has its help screen among the live entities");
    return NULL;
}

static const background_t *background_in_live_entities(void);

/* The first five live in the generation of the first game, read at f27 and
 * released at f29; the last four in that of the f28 title screen, read at
 * f39 and released at f43. */
enum {
    SINGLETON_GAME_CAMERA, SINGLETON_GAME_MOUSE, SINGLETON_GAME_UI, SINGLETON_BACKGROUND, SINGLETON_MAIN_SCORE_PANEL,
    SINGLETON_TITLE_CAMERA, SINGLETON_TITLE_MOUSE, SINGLETON_TITLE_UI, SINGLETON_HELP_SCREEN, SINGLETON_COUNT
};
static const char *const NAMES_OF_SINGLETONS[SINGLETON_COUNT] = {
    "game main_camera", "game main_mouse", "game main_ui", "background", "main_score_panel",
    "title main_camera", "title main_mouse", "title main_ui", "help_screen"};

static void singletons_of_quit_game(const void *out[SINGLETON_COUNT]) {
    start_e2e("pad_pause_restart");
    play_until(27);
    out[SINGLETON_GAME_CAMERA] = main_camera_typed();
    out[SINGLETON_GAME_MOUSE] = main_mouse_typed();
    out[SINGLETON_GAME_UI] = main_ui();
    out[SINGLETON_BACKGROUND] = background_in_live_entities();
    out[SINGLETON_MAIN_SCORE_PANEL] = main_score_panel();
    for (int k = 0; k < SINGLETON_TITLE_CAMERA; k++) assert(out[k] != NULL && "a singleton is missing at f27");
}

static void singletons_of_title(const void *out[SINGLETON_COUNT]) {
    singletons_of_quit_game(out);
    play_until(39);
    out[SINGLETON_TITLE_CAMERA] = main_camera_typed();
    out[SINGLETON_TITLE_MOUSE] = main_mouse_typed();
    out[SINGLETON_TITLE_UI] = main_ui();
    out[SINGLETON_HELP_SCREEN] = help_screen_in_live_entities();
    for (int k = SINGLETON_TITLE_CAMERA; k < SINGLETON_COUNT; k++) assert(out[k] != NULL && "a singleton is missing at f39");
}

/* f28: the quit main_ui finishes its update after the reset_world of
 * cart_init. */
static void pad_pause_restart_keeps_quit_singletons_during_click(void) {
    const void *s[SINGLETON_COUNT];
    singletons_of_quit_game(s);
    play_until(28);
    assert(game_zone_pending());
    assert(main_camera_typed() != s[SINGLETON_GAME_CAMERA] && main_ui() != s[SINGLETON_GAME_UI] &&
           "the f28 click makes new singletons");
    for (int k = 0; k < SINGLETON_TITLE_CAMERA; k++) g_spend = *(const unsigned char *)s[k];
}

/* f42: the title singletons outlive the reset_world of cart_start_game
 * until the end of the frame. */
static void pad_pause_restart_keeps_title_singletons_during_click(void) {
    const void *s[SINGLETON_COUNT];
    singletons_of_title(s);
    play_until(42);
    assert(game_zone_pending());
    assert(main_camera_typed() != s[SINGLETON_TITLE_CAMERA] && main_ui() != s[SINGLETON_TITLE_UI] &&
           "the f42 generation makes new singletons");
    for (int k = SINGLETON_TITLE_CAMERA; k < SINGLETON_COUNT; k++) g_spend = *(const unsigned char *)s[k];
}

static int g_singleton;

/* Each one dies with its generation. */
static void pad_pause_restart_releases_singleton(void) {
    const void *s[SINGLETON_COUNT];
    if (g_singleton < SINGLETON_TITLE_CAMERA) {
        singletons_of_quit_game(s);
        play_until(29);
    } else {
        singletons_of_title(s);
        play_until(43);
    }
    g_spend = *(const unsigned char *)s[g_singleton];
}

/* Bytes that galaxy_populate takes in the generation of each
 * cart_start_game: the type picker, its random walk and the vanish roll. */
static size_t populate_galaxy_states(void) {
    return (2 * ZONE_CELLS(sizeof(random_walk_t)) + ZONE_CELLS(sizeof(weighted_picker_t))) *
           sizeof(zone_cell_t);
}

/* Bytes a list has taken in its generation: its first block, of `first`
 * entries, then each doubling up to its capacity, as each growth copies and
 * leaves the old block dead until the release. A NULL list (a tag nothing
 * registered under yet) still holds its first block. */
static size_t list_bytes_from(const entity_list_t *list, int first) {
    size_t bytes = 0;
    const int capacity = list != NULL ? list->capacity : first;
    for (int k = first; k > 0 && k <= capacity; k *= 2) {
        bytes += ZONE_CELLS((size_t)k * sizeof(entity_t *)) * sizeof(zone_cell_t);
    }
    return bytes;
}

/* The registries of a game, each taken at its reserve by cart_start_game. */
static size_t registries_bytes(void) {
    const entity_registry_reserve_t *rv = &cart_game_registry_reserve;
    size_t r = ENTITY_TABLE_BYTES + list_bytes_from(live_entities(), rv->live);
    for (int m = 0; m < ENTITY_MESSAGE_COUNT; m++) {
        r += list_bytes_from(entities_by_message((entity_message_t)m), rv->by_message[m]);
    }
    for (int t = 0; t < TAG_COUNT; t++) r += list_bytes_from(entities_by_tag((tag_t)t), rv->by_tag[t]);
    return r;
}

static size_t block_bytes(size_t size) {
    return ZONE_CELLS(size) * sizeof(zone_cell_t);
}

/* Bytes of a planet list made in the generation: its container, then its
 * blocks as for a registry. */
static size_t id_list_bytes(const entity_id_list_t *list) {
    size_t bytes = 0;
    for (int k = 4; list != NULL && k <= list->capacity; k *= 2) bytes += block_bytes((size_t)k * sizeof(entity_id_t));
    return bytes;
}

static size_t route_list_bytes(const planet_t *planet, uint16_t present, const trade_route_list_t *list) {
    if (!(planet->has & present)) return 0;
    size_t bytes = 0;
    for (int k = 4; k <= list->values.capacity; k *= 2) bytes += block_bytes((size_t)k * sizeof(trade_route_t));
    return bytes;
}

/* Footprint of the planets under the planet tag: struct, and the slipways,
 * outgoing_routes and incoming_routes lists. No needs list nor copies of needs, which the planets of
 * galaxy_populate lack before any action. */
static size_t planets_bytes(void) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    size_t bytes = 0;
    for (int i = 0; jo != NULL && i < jo->count; i++) {
        const planet_t *p = (const planet_t *)jo->items[i];
        assert(!(p->has & PLANET_HAS_NEEDS) && "a planet has needs, which planets_bytes does not count");
        bytes += block_bytes(sizeof *p) + id_list_bytes(&p->slipways) +
                  route_list_bytes(p, PLANET_HAS_OUTGOING_ROUTES, &p->outgoing_routes) +
                  route_list_bytes(p, PLANET_HAS_INCOMING_ROUTES, &p->incoming_routes);
    }
    return bytes;
}

/* Bytes of the main economy made by cart_start_game: its struct, with no
 * invented items while nothing is invented. */
static size_t economy_bytes(void) {
    const economy_t *fm = main_economy_typed();
    assert(fm != NULL && "cart_start_game made the game economy");
    assert(fm->invented.count == 0 && fm->invented.items == NULL && "an invention, which economy_bytes does not count");
    return block_bytes(sizeof *fm);
}

/* Bytes of the seven structure icons among the live entities, in the order
 * of icon_tooltips_table: struct and tooltip (rebuilt in a frame zone arena
 * to measure it). */
static size_t icons_bytes(void) {
    const entity_list_t *hv = live_entities();
    const allocator_t *measure_arena = arena_take(frame_zone(), "tooltip-measure", 16384);
    const size_t head = arena_used(measure_arena);
    size_t bytes = 0;
    int k = 0;
    for (int i = 0; hv != NULL && i < hv->count; i++) {
        const structure_icon_t *ie = (const structure_icon_t *)hv->items[i];
        if (ie->header.class != structure_icon_class()) continue;
        assert(k < ICON_TOOLTIPS_COUNT && "more structure icons than icon tooltips");
        (void)tooltip_new(measure_arena, &icon_tooltips_table[k]);
        bytes += block_bytes(sizeof *ie);
        k++;
    }
    assert(k == ICON_TOOLTIPS_COUNT && "cart_start_game makes one structure icon per icon tooltip");
    return bytes + arena_used(measure_arena) - head;
}

static size_t research_and_economy_bytes(void) {
    return economy_bytes() + icons_bytes();
}

static const background_t *background_in_live_entities(void) {
    const entity_list_t *hv = live_entities();
    const background_t *bg = NULL;
    for (int i = 0; hv != NULL && i < hv->count; i++) {
        if (hv->items[i]->class != background_class()) continue;
        assert(bg == NULL && "the live entities hold two backgrounds");
        bg = (const background_t *)hv->items[i];
    }
    assert(bg != NULL && "cart_start_game made the game background");
    return bg;
}

static size_t singletons_bytes(void) {
    assert(main_camera_typed() != NULL && main_mouse_typed() != NULL && main_ui() != NULL && main_score_panel() != NULL);
    (void)background_in_live_entities();
    return block_bytes(sizeof(camera_t)) + block_bytes(sizeof(mouse_t)) +
           block_bytes(sizeof(ui_t)) + block_bytes(sizeof(background_t)) +
           block_bytes(sizeof(score_panel_t));
}

/* After the f43 release, the generation holds only what the f42
 * cart_start_game made and what has grown since: the galaxy_populate states,
 * the registries at their reserves, its planets, its economy and
 * structure icons, and its singletons. */
static void pad_pause_restart_bounds_dead_registry_blocks(void) {
    const size_t g = populate_galaxy_states();
    start_e2e("pad_pause_restart");
    play_until(43);
    const size_t r = registries_bytes();
    const size_t p = planets_bytes();
    printf("  pad_pause_restart f43: game=%lu, galaxy_populate states=%lu, registries=%lu, planets=%lu\n",
           (unsigned long)game_zone_used(), (unsigned long)g, (unsigned long)r, (unsigned long)p);
    assert(game_zone_used() == g + r + p + research_and_economy_bytes() + singletons_bytes() &&
           "the registries take their reserves and leave no dead block, and nothing else gets in");
}

/* f39: the title screen alone, built by cart_init with empty registries;
 * f43: the game the f42 cart_start_game started, its registries taken at
 * their reserves, none of which the pad_pause_restart game exceeds. */
static void pad_pause_restart_reserves_registries_at_game_start_only(void) {
    const entity_registry_reserve_t *rv = &cart_game_registry_reserve;
    start_e2e("pad_pause_restart");
    play_until(39);
    assert(live_entities()->capacity < rv->live && "the title screen takes the reserves of a game");
    play_until(43);
    assert(live_entities()->capacity == rv->live);
    for (int m = 0; m < ENTITY_MESSAGE_COUNT; m++) {
        assert(entities_by_message((entity_message_t)m)->capacity == rv->by_message[m]);
    }
    for (int t = 0; t < TAG_COUNT; t++) {
        const entity_list_t *list = entities_by_tag((tag_t)t);
        assert((list == NULL || list->capacity == rv->by_tag[t]) && "a registry of the game grew past its reserve");
    }
    printf("  pad_pause_restart f43: live %d/%d\n", live_entities()->count, live_entities()->capacity);
}

/* Only cart_load fills the load zone: two games add nothing to it. */
static void pad_pause_restart_leaves_load_zone_alone(void) {
    start_e2e("pad_pause_restart");
    play_until(1);
    const size_t f1 = load_zone_used();
    assert(f1 > 0 && "the research projects live in the load zone");
    play_until(g_input->frame_count - 1);
    printf("  pad_pause_restart: load=%lu at f1 and at the end of the replay\n", (unsigned long)f1);
    assert(load_zone_used() == f1 && "a game takes from the load zone");
    assert(game_zone_load_zone_max() == f1);
}

/* f39: the title screen alone, the quit game released at f29; f42: the
 * galaxy_populate of the new game takes its states beside the title
 * generation, released at f43. */
static void pad_pause_restart_counts_populate_galaxy_states(void) {
    const size_t g = populate_galaxy_states();
    start_e2e("pad_pause_restart");
    play_until(39);
    const size_t f39 = game_zone_used();
    play_until(42);
    const size_t f42 = game_zone_used();
    assert(f42 >= f39 + g && "the title generation lives beside the new game until the release");
    play_until(43);
    assert(game_zone_used() == g + registries_bytes() + planets_bytes() + research_and_economy_bytes() +
                                             singletons_bytes() &&
           "the f43 release leaves the states, registries, planets, economy and singletons of the new game");
    assert(game_zone_max() >= f42);
    printf("  pad_pause_restart f39/f42/f43: game=%lu/%lu/%lu, max=%lu\n", (unsigned long)f39, (unsigned long)f42,
           (unsigned long)game_zone_used(), (unsigned long)game_zone_max());
}

/* Bytes of the generation outside the registries, which grow whenever an
 * entity registers. */
static size_t game_without_registries(void) {
    return game_zone_used() - registries_bytes();
}

/* f46-93: 24 chained year jumps after the f45 hold's, so as many
 * economy_tiers_rebuild_incoming_routes calls, which must not grow the
 * generation: each incoming_routes list is emptied in place. */
static void pad_game_end_rebuild_does_not_grow_game(void) {
    start_e2e("pad_game_end");
    play_until(45);
    const size_t f45 = game_without_registries();
    play_until(93);
    const size_t f93 = game_without_registries();
    printf("  pad_game_end f45/f93: game - registries = %lu/%lu\n", (unsigned long)f45, (unsigned long)f93);
    assert(f45 == f93 && "an incoming routes rebuild grows the generation");
}

/* f509-557: the f509 year jump over six placed slipways, so
 * planet_rebuild_trades and planet_offer_trade, which offer known routes
 * again. The f508 rebuild keeps the route of the slipway placed at f476. The
 * presses of the f537 and f552 triangles reuse the slipway construction arena
 * the f477 re-press took and its cancel left. */
static void pad_tech_costs_route_rebuild_does_not_grow_game(void) {
    start_e2e("pad_tech_costs");
    play_until(476);
    const size_t f476 = game_without_registries();
    play_until(477);
    const size_t f477 = game_without_registries();
    assert(f477 == f476 + arena_size_for(SLIPWAY_CONSTRUCTION_BYTES) &&
           "the f477 re-press of p54 takes a slipway construction arena");
    play_until(508);
    const size_t f508 = game_without_registries();
    play_until(557);
    const size_t f557 = game_without_registries();
    printf("  pad_tech_costs f508/f557: game - registries = %lu/%lu\n", (unsigned long)f508,
           (unsigned long)f557);
    assert(f557 == f508 && "a rebuild over routes grows the generation, or a triangle press does not reuse the arena");
}

static const planet_t *planet_with_id(const char *id) {
    const entity_list_t *np = entities_by_tag(TAG_NODE);
    for (int i = 0; np != NULL && i < np->count; i++) {
        const planet_t *p = (const planet_t *)np->items[i];
        if (planet_id_or_null(p) != NULL && strcmp(planet_id_or_null(p), id) == 0) return p;
    }
    assert(0 && "planet not found under the node tag");
    return NULL;
}

/* A route of outgoing_routes kept by the f508 rebuild reads back at f557,
 * after 49 resets of the frame zone where its candidates are made. */
static void pad_tech_costs_kept_route_survives_frame(void) {
    entity_id_t lp[16];
    entity_id_t rl[16];
    start_e2e("pad_tech_costs");
    play_until(508);
    const planet_t *p54 = planet_with_id("p54");
    const trade_route_list_t *by = &p54->outgoing_routes;
    assert((p54->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(by) > 0 && "f508: p54 keeps a route");
    const trade_route_t *t = trade_route_list_at(by, trade_route_list_count(by) - 1);
    const int nlp = t->planets.count;
    const int nrl = t->slipways.count;
    assert(nlp <= 16 && nrl <= 16);
    memcpy(lp, t->planets.items, (size_t)nlp * sizeof *lp);
    memcpy(rl, t->slipways.items, (size_t)nrl * sizeof *rl);
    play_until(557);
    assert(t->planets.count == nlp && memcmp(lp, t->planets.items, (size_t)nlp * sizeof *lp) == 0);
    assert(t->slipways.count == nrl && memcmp(rl, t->slipways.items, (size_t)nrl * sizeof *rl) == 0);
    printf("  pad_tech_costs: p54 route (%d planets, %d slipways) read back at f557\n", nlp, nrl);
}

/* At f508, a rebuild of p54 over its kept routes gives back to the frame
 * zone every candidate offer it made, and the routes it hands back still
 * read the same. */
static void pad_tech_costs_rebuild_gives_back_frame(void) {
    entity_id_t lp[16];
    start_e2e("pad_tech_costs");
    play_until(508);
    planet_t *p54 = (planet_t *)planet_with_id("p54");
    assert(p54->slipways.zone != NULL && p54->slipways.count > 0 && "f508: p54 is linked");
    const trade_route_list_t *by = &p54->outgoing_routes;
    assert((p54->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(by) > 0 && "f508: p54 keeps a route");
    const int n = trade_route_list_count(by);
    const trade_route_t *t = trade_route_list_at(by, n - 1);
    const int nlp = t->planets.count;
    assert(nlp <= 16);
    memcpy(lp, t->planets.items, (size_t)nlp * sizeof *lp);
    const size_t before = frame_zone_used();
    trade_route_list_t *out = planet_rebuild_trades(p54);
    const size_t after = frame_zone_used();
    printf("  pad_tech_costs f508: frame %lu -> %lu across a rebuild of p54\n", (unsigned long)before,
           (unsigned long)after);
    assert(after == before && "planet_rebuild_trades keeps its candidate offers in the frame zone");
    assert(out == by && trade_route_list_count(out) == n && "the rebuild hands back the kept routes");
    assert(t->planets.count == nlp && memcmp(lp, t->planets.items, (size_t)nlp * sizeof *lp) == 0);
}

/* The transit dots still alive at the end of the game go back to the
 * particle zone through the release method of the slipway class, on the next
 * cart_load. */
static void pad_tech_costs_releases_trails_on_reload(void) {
    start_e2e("pad_tech_costs");
    play_until(g_input->frame_count - 1);
    const int end = particle_zone_live(PARTICLE_SLOT_TRANSIT_DOT);
    cart_load();
    const int after = particle_zone_live(PARTICLE_SLOT_TRANSIT_DOT);
    printf("  pad_tech_costs: transit_dots %d at the end of the replay, %d after reload (max %d, list max %d)\n", end, after,
           particle_zone_max_live(PARTICLE_SLOT_TRANSIT_DOT),
           particle_zone_max_elements(PARTICLE_SLOT_TRANSIT_DOT));
    assert(end > 0 && "pad_tech_costs ends with live transit dots");
    assert(after == 0 && "releasing a slipway gives back its transit dots");
}

#define GRABS "tests/fixtures/grabs_refused.json"

/* GRABS: f0-70 of pad_probe (probe at f28, fold f43-59, its wormhole dead
 * at f86), Down 10 so the centre lies 12 px from p47; then 7-frame grabs (L1,
 * X takes the basic structure at f+2, X at f+4 refuses it near p47, three
 * frames of waiting): twenty refused (f82-221), one carried Up+Left 10 and
 * placed at f237 (f222-239), Down+Right 10, one refused near p47 again
 * (f251-258);
 * 'restart' at f264 and a new game (X at f272); one grab (f278-284). */
static void refused_grabs_do_not_grow_game(void) {
    start_e2e(GRABS);
    play_until(88);
    const size_t a = game_without_registries();
    play_until(221);
    const size_t twenty = game_without_registries();
    printf("  refused grabs 1/20: game - registries = %lu/%lu\n", (unsigned long)a, (unsigned long)twenty);
    assert(a == twenty && "a refused grab leaves its structure in the generation");
}

/* The structure placed at f237, the only planet under the planet tag
 * whose identifier starts with an 's'. */
static const planet_t *placed_grab(void) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    const planet_t *placed = NULL;
    for (int i = 0; jo != NULL && i < jo->count; i++) {
        const planet_t *p = (const planet_t *)jo->items[i];
        if (planet_id_or_null(p)[0] != 's') continue;
        assert(placed == NULL && "a single grab was placed");
        placed = p;
    }
    assert(placed != NULL && placed->header.state == ENTITY_STATE_SETTLED && "the f237 grab was not placed");
    return placed;
}

/* The f43 wormhole's arena, free since f86, is too small for a structure:
 * the grabs take one new arena of the structure's size, full, and the
 * rebuild after the placement makes the outgoing_routes and incoming_routes
 * lists in the generation, outside the arena. */
static void placed_grab_does_not_fit_dead_wormhole_arena(void) {
    start_e2e(GRABS);
    play_until(221);
    const size_t before = game_without_registries();
    play_until(258);
    const size_t after = game_without_registries();
    const planet_t *placed = placed_grab();
    const allocator_t *arena = placed->slipways.zone;
    const size_t lists = route_list_bytes(placed, PLANET_HAS_OUTGOING_ROUTES, &placed->outgoing_routes) +
                         route_list_bytes(placed, PLANET_HAS_INCOMING_ROUTES, &placed->incoming_routes);
    printf("  placed grab: game - registries = %lu -> %lu (arena %lu, outgoing_routes/incoming_routes %lu)\n",
           (unsigned long)before, (unsigned long)after, (unsigned long)arena_size(arena), (unsigned long)lists);
    assert(arena_used(arena) == arena_size(arena) && "the placed structure fills its arena");
    assert(after == before + arena_size(arena) + lists &&
           "one new arena for the grabs past the f43 wormhole's, too small, and the placed grab's route lists");
}

/* One replay frame; when draw is 0, no draw method clears targeted_by. */
static void play_one_frame(int draw) {
    play_input(g_input);
    cart_update60();
    if (draw) cart_draw();
    if (g_frame < g_input->frame_count - 1) p8_input_step(g_input);
    g_frame++;
}

static planet_t *planet_targeted_by(const entity_t *held) {
    const entity_list_t *np = entities_by_tag(TAG_NODE);
    for (int i = 0; np != NULL && i < np->count; i++) {
        planet_t *p = (planet_t *)np->items[i];
        if (entity_get(p->targeted_by) == held) return p;
    }
    return NULL;
}

/* f85: the structure carried over p47 sets its targeted_by; p47 moves off
 * screen before the draw, and its draw returns without clearing targeted_by.
 * Refused at f86, the structure is freed by the f87 flush, and the f91 take
 * reuses its arena: targeted_by must not point at the new structure. */
static void offscreen_planet_forgets_refused_grab(void) {
    static vec_t far;
    start_e2e(GRABS);
    play_until(84);
    play_input(g_input);
    cart_update60();
    const entity_t *held = ui_left_held(main_ui());
    assert(held != NULL && "f85: a structure is held");
    planet_t *p47 = planet_targeted_by(held);
    assert(p47 != NULL && "f85: the held structure sets a planet's targeted_by");
    far = vec_add(entity_pos(main_camera()), vec_make(p8_fixed_from_int(500), 0));
    entity_set_pos(&p47->header, far);
    assert(!entity_geom_is_on_screen(&p47->header) && "p47 is off screen");
    cart_draw();
    assert(entity_get(p47->targeted_by) == held && "off screen, the draw leaves targeted_by set");
    if (g_frame < g_input->frame_count - 1) p8_input_step(g_input);
    g_frame++;
    while (g_frame <= 90) play_one_frame(1);
    play_one_frame(0);
    assert(ui_left_held(main_ui()) == held && "f91: the next grab reuses the arena, at the same address");
    assert(entity_get(p47->targeted_by) == NULL && "an off-screen planet's targeted_by points at a refused, recycled grab");
}

static void refused_grab_crosses_new_game(void) {
    start_e2e(GRABS);
    play_until(284);
}

#define SLIPWAYS "tests/fixtures/slipways_refused.json"

/* SLIPWAYS: f0-100 of pad_colonise (probe, fold, p47 colonised, but as
 * "dismantle" so that no trade route joins it to p25, its menu from jump
 * mode after the f88 square); square at f102, aim mode, panned onto p25;
 * square at f129, jump mode on p25, colonised from its menu (f130-135);
 * the basic structure s1 placed 40 px beyond p25 on the line from p47
 * (f140-171); circle, Down: jump mode on p25. Then 16-frame cycles from
 * f184: X starts the slipway from p25, Down aims it at p47, circle cancels
 * it, refused. Twenty refusals (f184-503), one built from p25 to p47
 * (f504-544, X at f544), one refusal from p47 aimed past p25 at s1, p25
 * crossed (f545-599, circle at f599); 'restart' at f608 and a new game (X
 * at f616); one icon grab (f622-628). A refusal is dropped on the 13th
 * frame of its cycle and freed by the flush of the 14th. */
static const slipway_t *held_slipway(void) {
    const entity_t *held = ui_left_held(main_ui());
    assert(held != NULL && held->class == slipway_class() && "a slipway is held");
    return (const slipway_t *)held;
}

static void held_slipway_does_not_grow_game(void) {
    start_e2e(SLIPWAYS);
    play_until(185);
    const slipway_t *held = held_slipway();
    const size_t f185 = game_zone_used();
    play_until(195);
    assert(held_slipway() == held && slipway_end(held) == &planet_with_id("p47")->header && "f195: held aimed at p47");
    printf("  held slipway f185/f195: game = %lu/%lu\n", (unsigned long)f185, (unsigned long)game_zone_used());
    assert(game_zone_used() == f185 && "a held slipway allocates every frame");
}

static void refused_slipways_do_not_grow_game(void) {
    start_e2e(SLIPWAYS);
    play_until(199);
    const size_t first_refusal = game_without_registries();
    play_until(503);
    const size_t twenty = game_without_registries();
    printf("  refused slipways 1/20: game - registries = %lu/%lu\n", (unsigned long)first_refusal, (unsigned long)twenty);
    assert(first_refusal == twenty && "a refused slipway leaves its arena in the generation");
}

static size_t slipways_of_p25_and_p47_bytes(void) {
    return id_list_bytes(&planet_with_id("p25")->slipways) + id_list_bytes(&planet_with_id("p47")->slipways);
}

/* The f43 wormhole's arena, free since f86, is too small for a slipway: the
 * placed slipway reuses the arena of the last refused one, the next refused
 * one takes a new arena of a slipway's size. The planet_attach_slipway of
 * the placement grows the slipways lists at both ends, in the generation,
 * outside the arena. */
static void placed_slipway_does_not_fit_dead_wormhole_arena(void) {
    start_e2e(SLIPWAYS);
    play_until(503);
    const size_t before = game_without_registries();
    const size_t slipways_before = slipways_of_p25_and_p47_bytes();
    play_until(602);
    const size_t after = game_without_registries();
    const size_t oe = slipways_of_p25_and_p47_bytes() - slipways_before;
    assert(entities_by_tag(TAG_SLIPWAY) != NULL && entities_by_tag(TAG_SLIPWAY)->count == 1 && "f544: one placed slipway");
    printf("  slipway placed then refused: game - registries = %lu -> %lu (slipways %lu)\n", (unsigned long)before,
           (unsigned long)after, (unsigned long)oe);
    assert(after == before + arena_size_for(SLIPWAY_CONSTRUCTION_BYTES) + oe &&
           "one new slipway arena past the f43 wormhole's, too small, and the slipways lists of the placement");
}

/* f598: the slipway aimed from p47 at s1 sets the targeted_by of p25, on
 * its way, which is pushed off screen: its draw no longer clears
 * targeted_by. Refused at f599, the slipway is freed by the f600 flush. */
static void offscreen_planet_forgets_refused_slipway(void) {
    static vec_t far;
    start_e2e(SLIPWAYS);
    play_until(597);
    play_input(g_input);
    cart_update60();
    const slipway_t *held = held_slipway();
    planet_t *p = planet_targeted_by(&held->header);
    assert(p != NULL && "f598: the held slipway sets a planet's targeted_by");
    far = vec_add(entity_pos(main_camera()), vec_make(p8_fixed_from_int(500), 0));
    entity_set_pos(&p->header, far);
    assert(!entity_geom_is_on_screen(&p->header) && "the planet is off screen");
    cart_draw();
    if (g_frame < g_input->frame_count - 1) p8_input_step(g_input);
    g_frame++;
    play_until(599);
    assert(entity_get(p->targeted_by) == &held->header && "off screen, the draw leaves targeted_by set");
    play_until(600);
    printf("  refused slipway: targeted_by of %s cleared by the f600 flush\n", planet_id_or_null(p));
    assert(entity_get(p->targeted_by) == NULL && "an off-screen planet's targeted_by points at a refused, released slipway");
}

static void refused_slipway_crosses_new_game(void) {
    start_e2e(SLIPWAYS);
    play_until(602);
    assert(entities_by_tag(TAG_SLIPWAY) != NULL && entities_by_tag(TAG_SLIPWAY)->count == 1);
    play_until(624);
    assert(entities_by_tag(TAG_SLIPWAY) == NULL || entities_by_tag(TAG_SLIPWAY)->count == 0);
    assert(ui_left_held(main_ui()) != NULL && "f624: a grab in the new game");
    play_until(628);
}

#define WORMHOLES "tests/fixtures/wormholes.json"

/* WORMHOLES: f0-42 of pad_probe (probe at f28, the centre moved off it);
 * then 7-frame cycles: X held four frames, three released, a fold abandoned
 * under 15 ticks. Twenty refused wormholes (f43-182), one held sixteen
 * frames (f183-198) and opened by the f199 release, freed at f226; one
 * refused press (f232-238); 'restart' at f244 and a new game (X at f252);
 * one probe moved off the planets and placed at f272, one refused wormhole
 * (f274-280). A refusal is released on the 5th frame of its cycle and freed
 * by the flush of the 6th. */
static const wormhole_t *held_wormhole(void) {
    const entity_t *held = ui_left_held(main_ui());
    assert(held != NULL && held->class == wormhole_class() && "a wormhole is held");
    return (const wormhole_t *)held;
}

static void held_wormhole_does_not_grow_game(void) {
    start_e2e(WORMHOLES);
    play_until(43);
    const wormhole_t *held = held_wormhole();
    const size_t f43 = game_zone_used();
    play_until(46);
    assert(held_wormhole() == held);
    printf("  held wormhole f43/f46: game = %lu/%lu\n", (unsigned long)f43, (unsigned long)game_zone_used());
    assert(game_zone_used() == f43 && "a held wormhole allocates every frame");
}

static void refused_wormholes_do_not_grow_game(void) {
    start_e2e(WORMHOLES);
    play_until(49);
    const size_t first_refusal = game_without_registries();
    play_until(182);
    const size_t twenty = game_without_registries();
    printf("  refused wormholes 1/20: game - registries = %lu/%lu\n", (unsigned long)first_refusal, (unsigned long)twenty);
    assert(first_refusal == twenty && "a refused wormhole leaves its arena in the generation");
}

static void open_wormhole_releases_arena_on_death(void) {
    start_e2e(WORMHOLES);
    play_until(182);
    const size_t before = game_without_registries();
    play_until(199);
    const entity_list_t *mz = entities_by_tag(TAG_WORMHOLE);
    assert(mz != NULL && mz->count == 1 && ((const wormhole_t *)mz->items[0])->header.state == ENTITY_STATE_UNFOLDING &&
           "f199: the wormhole is open");
    play_until(232);
    assert(entities_by_tag(TAG_WORMHOLE)->count == 1 && "f232: only the pressed wormhole lives");
    const size_t after = game_without_registries();
    printf("  open wormhole: game - registries = %lu -> %lu\n", (unsigned long)before, (unsigned long)after);
    assert(after == before && "an open wormhole keeps its arena after its death");
}

static void refused_wormhole_crosses_new_game(void) {
    start_e2e(WORMHOLES);
    play_until(274);
    held_wormhole();
    play_until(280);
}

#define PROBES "tests/fixtures/probes_refused.json"

/* PROBES: f0-14 of pad_probe ('reasonable' at f12), the centre 17 px from
 * p47; then 7-frame cycles there: X held four frames from the press, three
 * released, the probe refused near p47. Ten refused probes (f16-85); one
 * pressed at f86 and held four frames with Down, towards p47 (f86-92); Up
 * 12, away from p47, one probe placed by X at f106; one wormhole held
 * (f108-114); 'restart' at f120 and a new game (X at f128); one refusal
 * (f134-140). A refusal is released on the 5th frame of its cycle and freed
 * by the flush of the 6th. */
static const probe_t *held_probe(void) {
    const entity_t *held = ui_left_held(main_ui());
    assert(held != NULL && held->class == probe_class() && "a probe is held");
    return (const probe_t *)held;
}

static void refused_probes_do_not_grow_game(void) {
    start_e2e(PROBES);
    play_until(22);
    const size_t a = game_without_registries();
    play_until(85);
    const size_t ten = game_without_registries();
    printf("  refused probes 1/10: game - registries = %lu/%lu\n", (unsigned long)a, (unsigned long)ten);
    assert(a == ten && "a refused probe leaves its arena in the generation");
}

static void each_refused_probe_reuses_previous_arena(void) {
    start_e2e(PROBES);
    play_until(16);
    const probe_t *first = held_probe();
    for (int k = 1; k < 10; k++) {
        play_until(16 + 7 * k);
        assert(held_probe() == first && "the next probe reuses the refused one's arena, at the same address");
    }
    printf("  refused probes: ten probes at the same address\n");
}

/* f88: the probe held over p47 sets its targeted_by; p47 moves off screen
 * before the draw. Refused at f90, the probe is freed by the f91 flush, and
 * the f106 press, away from p47, reuses its arena. */
static void offscreen_planet_forgets_refused_probe(void) {
    static vec_t far;
    start_e2e(PROBES);
    play_until(87);
    play_input(g_input);
    cart_update60();
    const entity_t *held = &held_probe()->header;
    planet_t *p47 = planet_targeted_by(held);
    assert(p47 != NULL && "f88: the held probe sets a planet's targeted_by");
    far = vec_add(entity_pos(main_camera()), vec_make(p8_fixed_from_int(500), 0));
    entity_set_pos(&p47->header, far);
    assert(!entity_geom_is_on_screen(&p47->header) && "p47 is off screen");
    cart_draw();
    assert(entity_get(p47->targeted_by) == held && "off screen, the draw leaves targeted_by set");
    if (g_frame < g_input->frame_count - 1) p8_input_step(g_input);
    g_frame++;
    while (g_frame <= 105) play_one_frame(1);
    play_one_frame(0);
    assert(ui_left_held(main_ui()) == held && "f106: the next probe reuses the arena, at the same address");
    assert(entity_get(p47->targeted_by) == NULL && "an off-screen planet's targeted_by points at a refused, recycled probe");
}

/* f110: the anchors of the held wormhole keep only placed probes; the placed
 * probe keeps its arena, and the wormhole takes another one. */
static void wormhole_targets_only_placed_probe(void) {
    start_e2e(PROBES);
    play_until(110);
    const wormhole_t *mz = held_wormhole();
    const probe_t *placed = NULL;
    for (int i = 0; i < mz->anchors.count; i++) {
        const entity_t *e = entity_id_list_at(&mz->anchors, i);
        if (e->class != probe_class()) continue;
        assert(placed == NULL && e->state == ENTITY_STATE_SETTLED && "the anchors keep a single probe, a placed one");
        placed = (const probe_t *)e;
    }
    assert(placed != NULL && "f106: the placed probe is an anchor of the wormhole");
    assert(placed->grab_arena != NULL && placed->grab_arena != mz->anchors.zone && "the placed probe keeps its arena");
}

static void refused_probe_crosses_new_game(void) {
    start_e2e(PROBES);
    play_until(134);
    held_probe();
    play_until(140);
}

#define WORMHOLE_ABANDONED "tests/fixtures/wormhole_abandoned.json"

/* WORMHOLE_ABANDONED: f0-59 of pad_probe (wormhole opened at f59), then
 * Start and X: 'restart' at f65 abandons the wormhole and flips the game
 * zone. */
static const wormhole_t *abandoned_wormhole(void) {
    start_e2e(WORMHOLE_ABANDONED);
    play_until(61);
    const entity_list_t *mz = entities_by_tag(TAG_WORMHOLE);
    assert(mz != NULL && mz->count == 1 && "f61: a wormhole lives");
    return (const wormhole_t *)mz->items[0];
}

static void abandoned_wormhole_rereads_during_click(void) {
    const wormhole_t *mz = abandoned_wormhole();
    play_until(65);
    assert(game_zone_pending() && "the f65 'restart' flips the game zone");
    g_spend = (unsigned char)mz->header.is_dead;
}

static void abandoned_wormhole_dies_with_its_game(void) {
    const wormhole_t *mz = abandoned_wormhole();
    play_until(66);
    g_spend = (unsigned char)mz->header.is_dead;
}

/* f39: title after 'restart'. Reloading gives back the slots of the live
 * entities, and the reset_world of cart_init does not read back the
 * registries cleared by cart_load. */
static void pad_pause_restart_reloads_after_title(void) {
    reading_t fresh, title, reloads;
    start_e2e("pad_pause_restart");
    take_reading(&fresh, BEFORE_F0);
    play_until(39);
    take_reading(&title, 39);
    print_reading("pad_pause_restart", &title);
    cart_shutdown();
    cart_load();
    cart_init();
    cart_update60();
    cart_draw();
    take_reading(&reloads, 0);
    print_reading("pad_pause_restart reload", &reloads);
    assert(same_counts(&fresh, &reloads) && "reloading must give back the slots of the live entities");
}

/* After the f43 release, the vanish roll and the type picker must point
 * into the new generation, or ASan stops at the first read. */
static void pad_pause_restart_reveals_on_new_game_states(void) {
    start_e2e("pad_pause_restart");
    play_until(43);
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    const planet_types_entry_t *ll = planet_types_lookup("vanishing");
    int revealed = 0;
    int from_type_picker = 0;
    for (int i = 0; jo != NULL && i < jo->count && revealed < 5; i++) {
        planet_t *p = (planet_t *)jo->items[i];
        if (p->header.state != ENTITY_STATE_UNKNOWN) continue;
        planet_roll_type(p);
        revealed++;
        from_type_picker += p->planet_type != NULL && p->planet_type != ll;
    }
    assert(revealed > 0 && "the new game has unknown planets");
    assert(from_type_picker > 0 && "no reveal read the type picker");
    printf("  pad_pause_restart f43: %d planets revealed, %d through the type picker\n", revealed, from_type_picker);
}

/* f141-147: the year's message box kept open by its keep_open callback, with
 * no button. */
static void pad_message_box_idle_does_not_grow(void) {
    const int frames[] = {141, 147};
    reading_t r[2];
    replay_e2e("pad_message_box", frames, 2, r);
    assert(same_counts(&r[0], &r[1]) && r[0].particle == r[1].particle && "a slot kind takes a slot per tick");
    assert(r[0].game_zone == r[1].game_zone && "the game zone takes bytes every tick");
}

/* f142-143: the year's message box kept open, with no action. The boxes
 * remade every tick are rewritten in place: they keep their address. */
static void pad_message_box_rewrites_boxes_in_place(void) {
    start_e2e("pad_message_box");
    play_until(142);
    const camera_t *mo = main_camera_typed();
    const mouse_t *je = main_mouse_typed();
    const ui_t *ok = main_ui();
    assert(mo != NULL && je != NULL && ok != NULL);
    const void *before[3] = {&mo->header.map_pos, &mo->header.pos, &je->header.pos};
    size_t used_before = game_zone_used();
    play_until(143);
    assert(main_camera_typed() == mo && main_mouse_typed() == je && main_ui() == ok);
    const void *after[3] = {&mo->header.map_pos, &mo->header.pos, &je->header.pos};
    int stables = 0;
    for (int i = 0; i < 3; i++) stables += before[i] == after[i];
    printf("  pad_message_box f142-143: %d / 3 per-tick boxes at the same address\n", stables);
    fflush(stdout);
    assert(stables == 3 && "the camera's pos_before_shake and pos and the mouse's pos must be rewritten in place");
    assert(game_zone_used() == used_before && "the game zone takes bytes every tick");
}

static int idle_frame(const p8_input_t *input, int i) {
    return i > 0 && input->frames[i].pad_buttons == 0 && input->frames[i - 1].pad_buttons == 0;
}

/* Returns 1 when the window [start, last] holds two frames or more, after
 * checking that the generation at its second frame is the one at its last. */
static int close_window(int start, int last, size_t second, size_t used) {
    if (last <= start) return 0;
    if (used != second) {
        fprintf(stderr, "  %s f%d-%d: game %lu -> %lu\n", g_e2e, start, last, (unsigned long)second, (unsigned long)used);
        assert(0 && "the generation grows during an idle window");
    }
    return 1;
}

/* A window: at least two frames with no pad button, the frame before
 * included. The generation at its second frame (the
 * first may still flush what the action before left) is the one at its last.
 * A frame whose attract phase differs from the previous frame's (a demo start
 * or return flips the game zone) ends the window before it.
 * Without take_reading, whose flush would release the generation early. */
static int idle_windows_of_current_e2e(void) {
    start_e2e(g_e2e);
    int start = -1;
    size_t second = 0;
    size_t previous = 0;
    attract_phase_t phase = input_step_attract_phase(&g_step);
    int windows = 0;
    for (int i = 0; i < g_input->frame_count; i++) {
        play_until(i);
        size_t used = game_zone_used();
        attract_phase_t now = input_step_attract_phase(&g_step);
        int phase_changed = now != phase;
        phase = now;
        if (phase_changed && start >= 0) {
            windows += close_window(start, i - 1, second, previous);
            start = -1;
        }
        int idle = idle_frame(g_input, i) && !phase_changed;
        if (idle && start < 0) start = i;
        if (idle && i == start + 1) second = used;
        int end = idle && (i + 1 == g_input->frame_count || !idle_frame(g_input, i + 1));
        if (end) windows += close_window(start, i, second, used);
        if (!idle || end) start = -1;
        previous = used;
    }
    return windows;
}

static int idle_windows_on_all_e2e(void) {
    DIR *dir = opendir("tests/e2e");
    struct dirent *ent;
    int total = 0;
    assert(dir != NULL);
    while ((ent = readdir(dir)) != NULL) {
        size_t len = strlen(ent->d_name);
        if (len <= 5 || strcmp(ent->d_name + len - 5, ".json") != 0) continue;
        assert(len - 5 < sizeof g_e2e);
        memcpy(g_e2e, ent->d_name, len - 5);
        g_e2e[len - 5] = '\0';
        total += count_in_child(idle_windows_of_current_e2e);
    }
    closedir(dir);
    return total;
}

/* f100-135: p71 fades while only the camera pans; its released row may lower the row slot count. */
static void pad_empty_planet_does_not_grow(void) {
    const int frames[] = {100, 135};
    reading_t r[2];
    replay_e2e("pad_empty_planet", frames, 2, r);
    assert(none_rises(&r[0], &r[1]) && "a slot kind rises while p71 fades");
}

/* f101: 'harvest', p25 becomes a burst that lives to f185 and is gone at f186. */
static void pad_planet_projects_releases_burst(void) {
    const int frames[] = {100, 101, 185, 186};
    reading_t r[4];
    replay_e2e("pad_planet_projects", frames, 4, r);
    assert(r[0].burst == 0 && r[1].burst == 1 && "'harvest' makes a burst");
    assert(r[1].particle - r[0].particle >= 140 && "a burst is born with 140 particles");
    assert(r[2].burst == 1 && "the burst burns out at f186, not before");
    assert(r[3].burst == 0 && "a burnt-out burst must give back its slot");
    assert(r[3].particles == r[0].particles && "a burnt-out burst must give back its list");
}

/* With no input, the help screen emits 5 particles per draw and each lives
 * 115 draws: past that, the count levels off. */
static void idle_title_screen_settles(void) {
    int ceiling[4] = {0, 0, 0, 0};
    load_e2e(NULL);
    for (int i = 0; i < 400; i++) {
        cart_update60();
        cart_draw();
        check_invariant("title", i);
        assert(particle_zone_live(PARTICLE_SLOT_HELP_LIST) == 1 && "the help screen keeps a single list");
        int p = particle_zone_live(PARTICLE_SLOT_PARTICLE);
        if (p > ceiling[i / 100]) ceiling[i / 100] = p;
    }
    printf("  title without input, particle max per 100-frame window: %d %d %d %d\n", ceiling[0], ceiling[1],
           ceiling[2], ceiling[3]);
    assert(ceiling[3] <= ceiling[2] && "the help screen particle count still rises past their lifetime");
}

/* Allocated buffer, NUL-terminated, that the caller frees. */
static char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");
    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    long size = ftell(f);
    assert(size >= 0);
    rewind(f);
    char *buf = malloc((size_t)size + 1);
    assert(buf != NULL);
    assert(fread(buf, 1, (size_t)size, f) == (size_t)size);
    buf[size] = '\0';
    fclose(f);
    *len = (size_t)size;
    return buf;
}

static int same_bytes(const char *a_path, const char *b_path) {
    size_t a_len, b_len;
    char *a = read_file(a_path, &a_len);
    char *b = read_file(b_path, &b_len);
    int same = a_len == b_len && memcmp(a, b, a_len) == 0;
    free(a);
    free(b);
    return same;
}

#define DRIVER "./tmp/replay"
#define FILL "tmp/test_live_slots_fill.txt"
#define CAPTURE_A "tmp/test_live_slots_a.bin"
#define CAPTURE_B "tmp/test_live_slots_b.bin"
#define DUMP_A "tmp/test_live_slots_a.txt"
#define DUMP_B "tmp/test_live_slots_b.txt"

static void replay_writes_fill_at_session_end(void) {
    const int frames[] = {79};
    reading_t reading[1];
    replay_e2e("pad_pause_restart", frames, 1, reading);

    remove(FILL);
    assert(system(DRIVER " tests/e2e/pad_pause_restart.json " CAPTURE_A " " DUMP_A " > /dev/null") == 0);
    assert(system("REPLAY_FILL=" FILL " " DRIVER " tests/e2e/pad_pause_restart.json " CAPTURE_B " " DUMP_B
                  " > /dev/null") == 0);
    assert(same_bytes(CAPTURE_A, CAPTURE_B) && "the fill file does not change the capture");
    assert(same_bytes(DUMP_A, DUMP_B) && "the fill file does not change the dump");

    char expected[1024] = "";
    char line[48];
    for (int s = 0; s < SLOT_KIND_COUNT; s++) {
        snprintf(line, sizeof line, "%s %d\n", NAMES[s], reading[0].live[s]);
        strcat(expected, line);
    }
    static const char *const PARTICLE[PARTICLE_SLOT_COUNT] = {"particle", "particles", "particles_help_screen", "burst", "transit_dots"};
    for (int s = 0; s < PARTICLE_SLOT_COUNT; s++) {
        snprintf(line, sizeof line, "%s %d\n", PARTICLE[s],
                 particle_zone_live((particle_slot_kind_t)s));
        strcat(expected, line);
    }
    for (int s = 0; s < PARTICLE_SLOT_COUNT; s++) {
        snprintf(line, sizeof line, "max_live %s %d\n", PARTICLE[s],
                 particle_zone_max_live((particle_slot_kind_t)s));
        strcat(expected, line);
    }
    for (int s = PARTICLE_SLOT_LIST; s < PARTICLE_SLOT_COUNT; s++) {
        snprintf(line, sizeof line, "max_bytes %s %lu\n", PARTICLE[s],
                 (unsigned long)particle_zone_max_bytes((particle_slot_kind_t)s));
        strcat(expected, line);
    }
    for (int s = PARTICLE_SLOT_LIST; s < PARTICLE_SLOT_COUNT; s++) {
        if (s == PARTICLE_SLOT_BURST) continue;
        snprintf(line, sizeof line, "max_elements %s %d\n", PARTICLE[s],
                 particle_zone_max_elements((particle_slot_kind_t)s));
        strcat(expected, line);
    }
    snprintf(line, sizeof line, "game %lu\nmax_bytes game %lu\n", (unsigned long)game_zone_used(),
             (unsigned long)game_zone_max());
    strcat(expected, line);
    snprintf(line, sizeof line, "load %lu\n", (unsigned long)load_zone_used());
    strcat(expected, line);
    snprintf(line, sizeof line, "max_bytes load %lu\n", (unsigned long)game_zone_load_zone_max());
    strcat(expected, line);
    size_t len;
    char *writes = read_file(FILL, &len);
    printf("  fill file written by the replay:\n%s", writes);
    assert(strcmp(writes, expected) == 0 && "the replay writes the counts of the last frame, after a flush");
    free(writes);
    remove(FILL);
    remove(CAPTURE_A);
    remove(CAPTURE_B);
    remove(DUMP_A);
    remove(DUMP_B);
}

static long fill_value(const char *fill, const char *key) {
    char pattern[64];
    long n;
    snprintf(pattern, sizeof pattern, "\n%s ", key);
    const char *l = strstr(fill, pattern);
    assert(l != NULL && "key missing from the fill file");
    assert(sscanf(l + strlen(pattern), "%ld", &n) == 1);
    return n;
}

/* The f172 burst's 140 particles are born at once and dead by f256, before the end of the replay: a maximum read
 * only at the end of the session would stay under 140. */
static void replay_writes_session_peak(void) {
    remove(FILL);
    assert(system("REPLAY_FILL=" FILL " " DRIVER " tests/e2e/pad_planet_projects.json " CAPTURE_A
                  " " DUMP_A " > /dev/null") == 0);
    size_t len;
    char *writes = read_file(FILL, &len);
    long particle = fill_value(writes, "particle");
    long max_particle = fill_value(writes, "max_live particle");
    printf("  pad_planet_projects: particle %ld at the end of the session, max %ld; burst %ld, max %ld\n", particle,
           max_particle, fill_value(writes, "burst"), fill_value(writes, "max_live burst"));
    assert(fill_value(writes, "max_live burst") >= 1 && "'harvest' makes a burst");
    assert(max_particle >= 140 && particle < 140 && "the maximum is read at every take, not at the end of the session");
    free(writes);
    remove(FILL);
    remove(CAPTURE_A);
    remove(DUMP_A);
}

int main(void) {
    int pad_count;
    int demo_count;
    int e2e_count = invariant_on_all_e2e(&pad_count, &demo_count);
    printf("  invariant particle = sum of the registered #particles, transit_dots = sum of the #transit_dots of entities_by_tag(slipway): %d e2e (%d pad, %d reaching the demo), every frame\n", e2e_count, pad_count, demo_count);
    assert(pad_count >= 11 && "tests/e2e/ lost some of its 11 pad replays");
    assert(demo_count >= 1 && "tests/e2e/ lost its attract replay");
    in_child(pad_pause_restart_returns_to_title);
    in_child(pad_pause_restart_keeps_title_during_click);
    in_child_dying_by_asan(pad_pause_restart_releases_title_next_frame);
    in_child(pad_pause_restart_keeps_game_during_restart_click);
    in_child_dying_by_asan(pad_pause_restart_releases_game_after_restart);
    in_child(pad_pause_restart_title_drops_quit_economy_and_panel);
    in_child(pad_pause_restart_keeps_quit_planets_during_click);
    in_child_dying_by_asan(pad_pause_restart_releases_quit_planets);
    in_child(pad_pause_restart_keeps_quit_economy_during_click);
    in_child_dying_by_asan(pad_pause_restart_releases_quit_economy);
    in_child(pad_pause_restart_keeps_quit_icons_during_click);
    in_child_dying_by_asan(pad_pause_restart_releases_quit_icons);
    in_child(pad_pause_restart_keeps_quit_singletons_during_click);
    in_child(pad_pause_restart_keeps_title_singletons_during_click);
    for (g_singleton = 0; g_singleton < SINGLETON_COUNT; g_singleton++) {
        printf("  singleton %s:", NAMES_OF_SINGLETONS[g_singleton]);
        in_child_dying_by_asan(pad_pause_restart_releases_singleton);
    }
    in_child(pad_pause_restart_counts_populate_galaxy_states);
    in_child(pad_pause_restart_leaves_load_zone_alone);
    in_child(pad_pause_restart_reserves_registries_at_game_start_only);
    in_child(pad_pause_restart_bounds_dead_registry_blocks);
    in_child(pad_pause_restart_reloads_after_title);
    in_child(pad_pause_restart_reveals_on_new_game_states);
    in_child(pad_game_end_rebuild_does_not_grow_game);
    in_child(pad_tech_costs_route_rebuild_does_not_grow_game);
    in_child(pad_tech_costs_kept_route_survives_frame);
    in_child(pad_tech_costs_rebuild_gives_back_frame);
    in_child(pad_tech_costs_releases_trails_on_reload);
    in_child(refused_grabs_do_not_grow_game);
    in_child(placed_grab_does_not_fit_dead_wormhole_arena);
    in_child(refused_grab_crosses_new_game);
    in_child(offscreen_planet_forgets_refused_grab);
    in_child(held_slipway_does_not_grow_game);
    in_child(refused_slipways_do_not_grow_game);
    in_child(placed_slipway_does_not_fit_dead_wormhole_arena);
    in_child(refused_slipway_crosses_new_game);
    in_child(offscreen_planet_forgets_refused_slipway);
    in_child(held_wormhole_does_not_grow_game);
    in_child(refused_wormholes_do_not_grow_game);
    in_child(open_wormhole_releases_arena_on_death);
    in_child(refused_wormhole_crosses_new_game);
    in_child(refused_probes_do_not_grow_game);
    in_child(each_refused_probe_reuses_previous_arena);
    in_child(offscreen_planet_forgets_refused_probe);
    in_child(wormhole_targets_only_placed_probe);
    in_child(refused_probe_crosses_new_game);
    in_child(abandoned_wormhole_rereads_during_click);
    in_child_dying_by_asan(abandoned_wormhole_dies_with_its_game);
    in_child(pad_message_box_idle_does_not_grow);
    int windows = idle_windows_on_all_e2e();
    printf("  generation constant over %d idle windows of the %d e2e\n", windows, e2e_count);
    assert(windows > 0);
    in_child(pad_message_box_rewrites_boxes_in_place);
    in_child(pad_empty_planet_does_not_grow);
    in_child(pad_planet_projects_releases_burst);
    in_child(idle_title_screen_settles);
    in_child(replay_writes_fill_at_session_end);
    in_child(replay_writes_session_peak);
    printf("test_live_slots: all tests passed\n");
    return 0;
}
