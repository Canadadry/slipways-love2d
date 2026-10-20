/* The floating toast, through the real entity_register(), update_entities()
 * and draw_layers(ENTITY_MESSAGE_DRAW) it goes through in the game. The sequence and
 * screen hashes must never be re-taken: one that moves means the player
 * sees a different toast. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "p8/vram.h"
#include "game/ui.h"
#include "game/message_box.h"
#include "game/planet.h"
#include "engine/exit_queue.h"
#include "game/mouse.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "game/wormhole.h"
#include "engine/frame_zone.h"
#include "game/ui_zone.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}


#define NO_MONEY_TICKS 51
#define NO_MONEY_SEQUENCE_HASH 0xAD86C71Cu
#define NO_MONEY_FIRST_DRAW_HASH 0x7487F35Du
#define PRICE_TICKS 51
#define PRICE_SEQUENCE_HASH 0x843EB6FBu
#define PRICE_FIRST_DRAW_HASH 0xA9B7E9A6u

static int same_vec(vec_t a, vec_t b) {
    return a.x == b.x && a.y == b.y;
}

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

static uint32_t draw_hash(void) {
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    draw_layers(ENTITY_MESSAGE_DRAW);
    return screen_hash();
}

/* One toast's draw alone, as draw_layers(ENTITY_MESSAGE_DRAW) would place it. */
static uint32_t draw_hash_of(const toast_t *e) {
    p8_gfx_init(&graphics);
    p8_cls(&memory);
    toast_draw(&graphics, &memory, e, *pos_ref(&e->header));
    return screen_hash();
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

/* reset_world() again after cart_init(): the live entities hold only the
 * main camera, mouse and user interface, as at the start of a game. The
 * mouse is put mid-screen, where a toast placed off it is drawn under the
 * main camera. */
static void fresh_world(void) {
    reset_world(NULL);
    assert(main_mouse() != NULL && main_mouse()->class == mouse_class());
    entity_set_pos(main_mouse(), vec_make(p8_fixed_from_int(80), p8_fixed_from_int(60)));
}

static toast_t *build(const vec_t *ra, const char *text, int color) {
    toast_keys_t keys = {0};

    if (ra != NULL) {
        keys.pos = *ra;
        keys.has_pos = 1;
    }
    toast_keys_set_text(&keys, text);
    keys.color = p8_fixed_from_int(color);
    return toast_new(&keys);
}

/* Lives the toast through update_entities() until it leaves the live
 * entities, folding every tick's position, fade, state time, list
 * membership and draw_layers(ENTITY_MESSAGE_DRAW) screen into one hash. */
static void live(toast_t *e, int want_ticks, uint32_t want_sequence, uint32_t want_first_draw, const char *label) {
    uint32_t h = 2166136261u;
    uint32_t first_draw;
    uint32_t blank;
    int ticks = 0;

    p8_cls(&memory);
    blank = screen_hash();
    first_draw = draw_hash();

    while (entity_list_listed(live_entities(), e)) {
        vec_t ra;
        assert(pos_ref(&e->header) != NULL);
        ra = *pos_ref(&e->header);
        assert(ticks < 1000);
        h = fnv(h, (uint32_t)ra.x);
        h = fnv(h, (uint32_t)ra.y);
        h = fnv(h, (uint32_t)e->fade);
        h = fnv(h, (uint32_t)e->header.state_time);
        h = fnv(h, (uint32_t)entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), e));
        h = fnv(h, draw_hash());
        update_entities();
        ticks++;
    }
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), e));
    h = fnv(h, (uint32_t)e->fade);
    h = fnv(h, (uint32_t)e->header.state_time);

    printf("[%s] ticks=%d sequence=0x%08Xu first_draw=0x%08Xu\n", label, ticks, (unsigned)h, (unsigned)first_draw);
    fflush(stdout);
    assert(first_draw != blank);
    assert(ticks == want_ticks);
    assert(h == want_sequence);
    assert(first_draw == want_first_draw);
}

/* economy_spend()'s "-no money-": no position, so toast_construct() places
 * it off the main mouse and camera. */
static void test_no_money_toast_lives_as_before(void) {
    fresh_world();
    live(build(NULL, "-no money-", 8), NO_MONEY_TICKS, NO_MONEY_SEQUENCE_HASH, NO_MONEY_FIRST_DRAW_HASH, "no money");
}

/* planet_new_year()'s price: the planet's own position is kept. */
static void test_price_toast_lives_as_before(void) {
    vec_t ra = vec_make(p8_fixed_from_int(250), p8_fixed_from_int(260));
    fresh_world();
    live(build(&ra, "+12$", 11), PRICE_TICKS, PRICE_SEQUENCE_HASH, PRICE_FIRST_DRAW_HASH, "price");
}

/* A toast alive when a wormhole is built is scanned by its construction for
 * a fold anchor, which it does not have. */
static void test_wormhole_scan_skips_a_live_toast(void) {
    vec_t ra = vec_make(p8_fixed_from_int(40), p8_fixed_from_int(60));
    void *e;
    wormhole_t *mz;

    fresh_world();
    e = build(&ra, "+1$", 11);
    mz = wormhole_new();
    assert(mz->anchors.zone != NULL);
    assert(!entity_id_list_has(&mz->anchors, entity_id_of(e)));
    assert(mz->anchors.count == 0);
}

/* update_entities() on an entity struct: the method of its state is called
 * with the state time, the state time is bumped on the struct itself, and a
 * non zero is_dead takes it out of the live entities. */
typedef struct {
    entity_t header;
} fake_toast_t;

static int g_fake_update_calls = 0;
static p8_fixed_t g_fake_update_last;

static void fake_toast_update(entity_t *self, p8_fixed_t t) {
    fake_toast_t *f = (fake_toast_t *)self;
    g_fake_update_calls++;
    g_fake_update_last = t;
    if (t >= p8_fixed_from_int(2)) {
        f->header.is_dead = 1;
    }
}

static const entity_class_t fake_toast_class_def = {
    .states = {[ENTITY_STATE_NORMAL] = fake_toast_update},
};

static void test_update_entities_drives_an_entity_struct(void) {
    fake_toast_t *f = malloc(sizeof *f);
    int i;

    fresh_world();
    entity_init_instance(&f->header, &fake_toast_class_def, ENTITY_STATE_NORMAL, 0);
    f->header.has |= ENTITY_HAS_IS_DEAD;
    f->header.is_dead = 0;
    entity_construct(&f->header);
    assert(entity_list_listed(live_entities(), f));

    for (i = 0; i < 2; i++) {
        update_entities();
        assert(g_fake_update_calls == i + 1 && g_fake_update_last == p8_fixed_from_int(i));
        assert(f->header.state_time == p8_fixed_from_int(i + 1));
        assert(entity_list_listed(live_entities(), f));
    }
    update_entities();
    assert(g_fake_update_calls == 3 && g_fake_update_last == p8_fixed_from_int(2));
    assert(!entity_list_listed(live_entities(), f));
    assert(f->header.state_time == p8_fixed_from_int(3));
}

/* The struct's defaults are its archetype's, whose layer and velocity are
 * the spec's. */
static void test_defaults_match_the_parsed_spec(void) {
    const toast_t *e;

    fresh_world();
    e = build(NULL, "x", 7);
    assert(e->archetype == &toast_archetype);
    assert(e->archetype->velocity.x == 0 && e->archetype->velocity.y == -0x570a);
    assert(e->archetype->layer == p8_fixed_from_int(9));
    assert(e->fade == e->archetype->fade);
    assert(e->header.state == ENTITY_STATE_NORMAL);
    assert(e->header.state_time == 0);
    assert(build(NULL, "y", 7)->archetype == e->archetype);
    assert(toast_ready());
    toast_shutdown();
    assert(!toast_ready());
    toast_init();
}

/* Built directly or through its class, a toast is a struct that lands in
 * the live entities and the draw message list only. */
static void test_toast_is_a_struct_registered_under_draw(void) {
    const entity_message_t others[] = {ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
    toast_keys_t keys = {0};
    void *direct;
    void *generic;
    int i;

    fresh_world();
    toast_keys_set_text(&keys, "z");
    direct = build(NULL, "z", 7);
    generic = toast_new(&keys);

    assert(direct != NULL);
    assert(generic != NULL);
    assert(entity_list_listed(live_entities(), direct) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), direct));
    assert(entity_list_listed(live_entities(), generic) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), generic));
    for (i = 0; i < 4; i++) {
        assert(!entity_list_listed(entities_by_message(others[i]), direct) && !entity_list_listed(entities_by_message(others[i]), generic));
    }
}

/* A toast takes a slot; once update_entities() takes it out after its 50
 * ticks, emptying the exit queue gives the slot back. */
static void test_toast_gives_its_slot_back(void) {
    toast_t *e;
    int base;
    int ticks = 0;

    fresh_world();
    cart_flush_exit_queue();
    base = ui_zone_live(SLOT_KIND_TOAST);
    e = build(NULL, "-no money-", 8);
    assert(ui_zone_live(SLOT_KIND_TOAST) == base + 1);
    while (entity_list_listed(live_entities(), e)) {
        update_entities();
        assert(++ticks < 1000);
    }
    assert(ui_zone_live(SLOT_KIND_TOAST) == base + 1 && "out of the live entities, still read this frame");
    cart_flush_exit_queue();
    assert(ui_zone_live(SLOT_KIND_TOAST) == base);
}

/* The toast's text is made in the frame zone (the formatted price of
 * economy_spend() and planet_new_year()): the toast draws it on later
 * frames, after the zone is reset and reused. */
static void test_toast_keeps_a_copy_of_its_text(void) {
    vec_t ra = vec_make(p8_fixed_from_int(250), p8_fixed_from_int(260));
    uint32_t want;
    char *text;
    char *reuse;
    toast_t *e;

    fresh_world();
    want = draw_hash_of(build(&ra, "+12$", 11));
    fresh_world();
    text = (char *)allocator_alloc(frame_zone(), 5);
    memcpy(text, "+12$", 5);
    e = build(&ra, text, 11);
    frame_zone_reset();
    reuse = (char *)allocator_alloc(frame_zone(), 64);
    memset(reuse, 'X', 64);
    assert(strcmp(e->text, "+12$") == 0);
    assert(draw_hash_of(e) == want);
}

/* The keys hold the toast's text and position as values: rewriting them
 * after the toast is built changes nothing it shows. */
static void test_toast_copies_its_keys(void) {
    toast_keys_t keys = {0};
    vec_t at = vec_make(p8_fixed_from_int(250), p8_fixed_from_int(260));
    uint32_t want;
    toast_t *e;

    fresh_world();
    want = draw_hash_of(build(&at, "+12$", 11));
    fresh_world();
    keys.pos = at;
    keys.has_pos = 1;
    toast_keys_set_text(&keys, "+12$");
    keys.color = p8_fixed_from_int(11);
    e = toast_new(&keys);
    memset(&keys, 'X', sizeof keys);
    assert(same_vec(e->header.pos, at) && strcmp(e->text, "+12$") == 0);
    assert(draw_hash_of(e) == want);
}

/* A position given as zero is a position, not the mouse's. */
static void test_toast_at_zero_is_not_placed_off_the_mouse(void) {
    vec_t zero = vec_make(0, 0);
    toast_t *e;

    fresh_world();
    e = build(&zero, "x", 7);
    assert(same_vec(e->header.pos, zero));
    e = build(NULL, "x", 7);
    assert(!same_vec(e->header.pos, zero));
}

static void set_too_long_text(void) {
    toast_keys_t keys = {0};

    toast_keys_set_text(&keys, "12345678901234567");
}

/* A text past TOAST_TEXT_CAP stops on its assert. */
static void test_toast_text_past_its_cap_aborts(void) {
    int status;
    pid_t child;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        set_too_long_text();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

static planet_t *planet_at(vec_t at) {
    planet_keys_t keys = {0};

    keys.pos = at;
    keys.has_pos = 1;
    return planet_new(&keys);
}

static toast_t *toast_from(const planet_t *source) {
    toast_keys_t keys = {0};

    keys.source = entity_id_link(&source->header);
    toast_keys_set_text(&keys, "+1$");
    keys.color = p8_fixed_from_int(11);
    return toast_new(&keys);
}

/* planet_new_year() names the toast's source planet, which planet_bob()
 * moves in place in the next update_entities(): until its first update the
 * toast is drawn at the planet's position, and no longer follows after. */
static void test_toast_follows_its_source_until_its_first_update(void) {
    planet_t *planet;
    vec_t moved;
    vec_t after;
    toast_t *e;

    fresh_world();
    planet = planet_at(vec_make(p8_fixed_from_int(100), p8_fixed_from_int(100)));
    e = toast_from(planet);
    assert(same_vec(e->header.pos, planet->header.pos));
    planet_bob(&planet->header, p8_fixed_from_int(50));
    moved = planet->header.pos;
    assert(moved.y != p8_fixed_from_int(100));
    e->header.class->draw(&e->header, (vec_t){0, 0});
    assert(same_vec(e->header.pos, moved));
    planet_bob(&planet->header, p8_fixed_from_int(60));
    moved = planet->header.pos;
    toast_update(e);
    after = vec_add(moved, e->archetype->velocity);
    assert(same_vec(e->header.pos, after));
    planet_bob(&planet->header, p8_fixed_from_int(70));
    e->header.class->draw(&e->header, (vec_t){0, 0});
    assert(same_vec(e->header.pos, after));
}

/* A source flushed before the toast's first update leaves the toast where
 * it last saw it. */
static void test_toast_keeps_its_copy_once_its_source_is_flushed(void) {
    planet_t *planet;
    vec_t start = vec_make(p8_fixed_from_int(100), p8_fixed_from_int(100));
    toast_t *e;

    fresh_world();
    planet = planet_at(start);
    e = toast_from(planet);
    entity_unregister(&planet->header);
    exit_queue_flush();
    assert(entity_get(entity_id_of(&e->header)) == &e->header);
    toast_update(e);
    assert(same_vec(e->header.pos, vec_add(start, e->archetype->velocity)));
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
    cart_init();

    test_no_money_toast_lives_as_before();
    test_price_toast_lives_as_before();
    test_wormhole_scan_skips_a_live_toast();
    test_update_entities_drives_an_entity_struct();
    test_defaults_match_the_parsed_spec();
    test_toast_is_a_struct_registered_under_draw();
    test_toast_gives_its_slot_back();
    test_toast_keeps_a_copy_of_its_text();
    test_toast_copies_its_keys();
    test_toast_at_zero_is_not_placed_off_the_mouse();
    test_toast_text_past_its_cap_aborts();
    test_toast_follows_its_source_until_its_first_update();
    test_toast_keeps_its_copy_once_its_source_is_flushed();

    printf("test_toast: all tests passed\n");
    return 0;
}
