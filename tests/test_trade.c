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
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy_tiers.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_trade.h"
#include "game/research.h"
#include "game/slipway.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "engine/exit_queue.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "game/ui.h"
#include "engine/vec.h"
#include "engine/frame_zone.h"
#include "heap_allocator.h"

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

/* Research project 3: an output and no inputs, so its planet exports to
 * any planet a slipway joins it to. */
#define EXPORTER 3

#define GAME_ROUTE_HASH 0x93A4DA45u

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

static int entity_list_rank_in(const entity_list_t *list, const void *e) {
    int i;
    for (i = 0; list != NULL && i < list->count; i++) {
        if (list->items[i] == e) {
            return i;
        }
    }
    return -1;
}

static planet_t *as_planet(void *e) {
    assert(planet_is_family(e));
    return e;
}

/* Both resources set, of one kind and one value. */
static int same_resource(const trade_route_t *a, const trade_route_t *b) {
    return a->has_resource && b->has_resource && a->resource_is_bool == b->resource_is_bool && a->resource == b->resource;
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, BOOL_KIND = 2, REF_KIND = 3 };

/* The kind word of a reference, then presence and the rank in the live
 * entities. */
static uint32_t fold_ref(uint32_t h, const void *p) {
    h = fnv(h, (uint32_t)REF_KIND);
    h = fnv(h, (uint32_t)(p != NULL));
    return fnv(h, (uint32_t)entity_list_rank_in(live_entities(), p));
}

/* A boolean or fixed value's kind word and payload; an unset resource
 * folds as absent. */
static uint32_t fold_resource(uint32_t h, const trade_route_t *t) {
    if (!t->has_resource) {
        return fnv(h, 0xFFFFFFFFu);
    }
    if (t->resource_is_bool) {
        return fnv(fnv(h, (uint32_t)BOOL_KIND), (uint32_t)(t->resource != 0));
    }
    return fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)t->resource);
}

static uint32_t fold_ranks(uint32_t h, const entity_id_list_t *l) {
    int i;
    h = fnv(h, (uint32_t)l->count);
    for (i = 0; i < l->count; i++) {
        h = fold_ref(h, entity_id_list_at(l, i));
    }
    return h;
}

/* A route: the live entity ranks of its planets and slipways, its
 * resource, and the rank of its last planet. */
static uint32_t fold_route(uint32_t h, const trade_route_t *t) {
    entity_t *to;
    h = fold_ranks(h, &t->planets);
    h = fold_ranks(h, &t->slipways);
    h = fold_resource(h, t);
    if (trade_last_planet(t, &to)) {
        return fold_ref(fnv(h, 1u), to);
    }
    return fnv(h, 0u);
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

/* A game started by cart_start_game with no difficulty picked: a NULL
 * difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
}

static vec_t pos_of(void *e) {
    assert(entity_has_map_pos(&as_planet(e)->header));
    return as_planet(e)->header.map_pos;
}

static p8_fixed_t dist2(vec_t a, vec_t b) {
    vec_t d = vec_sub(a, b);
    return p8_fixed_add(p8_fixed_mul(d.x, d.x), p8_fixed_mul(d.y, d.y));
}

static research_project_t *project(int k) {
    assert(research_projects_at(k) != NULL);
    return research_projects_at(k);
}

/* slipway_drag reads the hovered planet off the main user interface; the
 * slipway is paid by slipway_drop as a drag onto the end planet pays it. */
static void join(void *qu, void *re) {
    slipway_keys_t slipway_keys = {0};
    vec_t mp;
    slipway_t *jc;

    slipway_keys.start = entity_id_link(qu);
    jc = slipway_new(&slipway_keys);
    mp = vec_add(vec_mul(pos_of(re), p8_fixed_from_int(128)),
                       vec_make(p8_fixed_from_int(1), p8_fixed_from_int(1)));
    main_ui()->hovered = entity_id_link(re);
    slipway_class()->drag(&jc->header, mouse_world_only(mp));
    main_ui()->hovered = entity_id_link(NULL);
    while (jc->header.state_time < p8_fixed_from_int(15)) {
        update_entities();
    }
    slipway_class()->drop(&jc->header, re);
}

static entity_id_list_t *list_of(int n, void *const *items) {
    entity_id_list_t *l = malloc(sizeof *l);
    int i;
    entity_id_list_init(l, &heap_allocator);
    for (i = 0; i < n; i++) {
        entity_id_list_add(l, entity_id_of(items[i]));
    }
    return l;
}

/* NULL planets or slipways give an empty list; has_resource 0 is no resource. */
static trade_route_t route(entity_id_list_t *lp, entity_id_list_t *rl, int has_resource, int resource_is_bool, p8_fixed_t qe) {
    trade_keys_t keys = {0};

    if (lp != NULL) {
        keys.planets = *lp;
    }
    if (rl != NULL) {
        keys.slipways = *rl;
    }
    keys.has_resource = has_resource;
    keys.resource_is_bool = resource_is_bool;
    keys.resource = qe;
    return trade_new(&keys);
}

static const trade_route_t no_route = {0};

static void *planet(int i) {
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    assert(jo != NULL && i < jo->count);
    return jo->items[i];
}

/* A generated planet colonized by EXPORTER, its nearest one by a bare
 * project, the two joined by a paid slipway, then left to settle; and the
 * first route: the first outgoing route of the first planet (planet tag)
 * that exports. */
static trade_route_t game_route(void) {
    const entity_list_t *jo;
    void *qu;
    void *re = NULL;
    int i;

    start_game();
    jo = entities_by_tag(TAG_PLANET);
    assert(jo != NULL && jo->count > 3);
    qu = jo->items[0];
    for (i = 1; i < jo->count; i++) {
        void *e = jo->items[i];
        if (re == NULL || dist2(pos_of(e), pos_of(qu)) < dist2(pos_of(re), pos_of(qu))) {
            re = e;
        }
    }
    planet_apply_project(qu, project(EXPORTER));
    planet_apply_project(re, research_project_new(NULL));
    join(qu, re);
    for (i = 0; i < 120; i++) {
        update_entities();
    }
    for (i = 0; jo != NULL && i < jo->count; i++) {
        const planet_t *exporter = as_planet(jo->items[i]);
        const trade_route_list_t *by = &exporter->outgoing_routes;
        if ((exporter->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(by) > 0) {
            return *trade_route_list_at(by, 0);
        }
    }
    assert(0 && "no route in the game");
    return no_route;
}

/* A route built from the game's route's own planets, slipways and
 * resource shares their ids; with no key, or the planets alone, the others
 * are empty. */
static void test_construction(void) {
    uint32_t h = 2166136261u;
    trade_route_t g = game_route();
    entity_id_list_t lp = g.planets;
    entity_id_list_t rl = g.slipways;
    trade_route_t t;

    assert(lp.count > 0 && rl.count > 0 && g.has_resource);
    h = fold_route(h, &g);
    t = route(&lp, &rl, g.has_resource, g.resource_is_bool, g.resource);
    assert(t.planets.items == lp.items && t.planets.count == lp.count && t.slipways.items == rl.items && t.slipways.count == rl.count);
    assert(same_resource(&t, &g));
    h = fold_route(h, &t);

    t = trade_new(NULL);
    assert(t.planets.count == 0 && t.planets.items == NULL && t.slipways.count == 0 && t.slipways.items == NULL && !t.has_resource);
    t = route(&lp, NULL, 0, 0, 0);
    assert(t.planets.items == lp.items && t.slipways.count == 0 && !t.has_resource);
    check("game_route", h, GAME_ROUTE_HASH);
}

/* trade_last_planet is the last of the planets; an empty or missing list
 * gives nothing and leaves the output alone. */
static void test_last_planet(void) {
    static entity_t sentinel_target;
    entity_t *const sentinel = &sentinel_target;
    void *items[3];
    entity_t *out;
    trade_route_t t;

    (void)game_route();
    items[0] = planet(0);
    items[1] = planet(1);
    items[2] = planet(2);
    t = route(list_of(3, items), NULL, 0, 0, 0);
    assert(trade_last_planet(&t, &out) && out == planet(2));
    t = route(list_of(1, items), NULL, 0, 0, 0);
    assert(trade_last_planet(&t, &out) && out == planet(0));

    out = sentinel;
    t = route(list_of(0, items), NULL, 0, 0, 0);
    assert(!trade_last_planet(&t, &out) && out == sentinel);
    assert(!trade_last_planet(&no_route, &out) && out == sentinel);
    assert(!trade_last_planet(NULL, &out) && out == sentinel);
}

static int list_is(const entity_id_list_t *l, int n, entity_t *const *items) {
    int i;
    if (l == NULL || l->count != n) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        if (entity_id_list_at(l, i) != items[i]) {
            return 0;
        }
    }
    return 1;
}

static int same_ids(const entity_id_list_t *a, const entity_id_list_t *b) {
    int i;
    if (a->count != b->count) {
        return 0;
    }
    for (i = 0; i < a->count; i++) {
        if (!entity_id_equal(a->items[i], b->items[i])) {
            return 0;
        }
    }
    return 1;
}

/* trade_extend builds a new route and leaves `self` alone: the node
 * appended to the planets, the slipway to the slipways unless NULL, the
 * same resource; `self` with no resource gives the new route false. */
static void test_extend(void) {
    const trade_route_t built_route = game_route();
    const entity_id_list_t *planets = &built_route.planets;
    const entity_id_list_t *slipways = &built_route.slipways;
    const int planets_n = planets->count;
    const int slipways_n = slipways->count;
    void *node = planet(3);
    entity_t *slipway = entity_id_list_at(slipways, 0);
    entity_t *want[16];
    trade_route_t extended;
    trade_route_t bare;
    int i;

    assert(built_route.has_resource && planets_n + 1 < 16 && slipways_n + 1 < 16);
    extended = trade_extend(game_zone(), &built_route, node, slipway);
    assert(planets->count == planets_n && slipways->count == slipways_n);
    assert(extended.planets.items != planets->items && extended.slipways.items != slipways->items);
    for (i = 0; i < planets_n; i++) {
        want[i] = entity_id_list_at(planets, i);
    }
    want[planets_n] = node;
    assert(list_is(&extended.planets, planets_n + 1, want));
    for (i = 0; i < slipways_n; i++) {
        want[i] = entity_id_list_at(slipways, i);
    }
    want[slipways_n] = slipway;
    assert(list_is(&extended.slipways, slipways_n + 1, want));
    assert(same_resource(&extended, &built_route));

    extended = trade_extend(game_zone(), &built_route, node, NULL);
    assert(extended.planets.count == planets_n + 1 && same_ids(&extended.slipways, slipways));
    assert(extended.slipways.items != slipways->items);
    assert(same_resource(&extended, &built_route));

    extended = trade_extend(game_zone(), &built_route, NULL, NULL);
    assert(same_ids(&extended.planets, planets) && same_ids(&extended.slipways, slipways));

    bare = route((entity_id_list_t *)planets, NULL, 0, 0, 0);
    extended = trade_extend(game_zone(), &bare, node, NULL);
    assert(extended.planets.count == planets_n + 1 && extended.slipways.count == 0);
    assert(extended.has_resource && extended.resource_is_bool && extended.resource == 0);

    extended = trade_extend(game_zone(), &no_route, node, slipway);
    want[0] = node;
    assert(list_is(&extended.planets, 1, want));
    want[0] = slipway;
    assert(list_is(&extended.slipways, 1, want));
    assert(extended.has_resource && extended.resource_is_bool && extended.resource == 0);
}

/* trade_extend and trade_copy build their id lists in the zone they are
 * given: an arena taken from the game zone grows, the game zone itself does
 * not; trade_new allocates nothing. */
static void test_built_in_the_given_zone(void) {
    const trade_route_t g = game_route();
    const allocator_t *arena = arena_take(game_zone(), "routes", 4096);
    const size_t game_zone_used_before = game_zone_used();
    size_t taken = arena_used(arena);
    trade_route_t k;

    (void)trade_new(NULL);
    assert(arena_used(arena) == taken && game_zone_used() == game_zone_used_before);
    k = trade_extend(arena, &g, planet(3), entity_id_list_at(&g.slipways, 0));
    assert(arena_used(arena) > taken);
    taken = arena_used(arena);
    (void)trade_copy(arena, &k);
    assert(arena_used(arena) > taken);
    assert(game_zone_used() == game_zone_used_before);
}

/* A route made in the frame zone, copied into the game zone: the copy
 * shares no list with it, and reads back whole once the frame zone is
 * reset, which ASan poisons. */
static void test_copy_outlives_the_frame(void) {
    const trade_route_t g = game_route();
    const trade_route_t k = trade_extend(frame_zone(), &g, planet(3), entity_id_list_at(&g.slipways, 0));
    const trade_route_t c = trade_copy(game_zone(), &k);
    entity_t *planets_want[16];
    entity_t *slipways_want[16];
    const int planets_n = k.planets.count;
    const int slipways_n = k.slipways.count;
    const int resource_is_bool = k.resource_is_bool;
    const p8_fixed_t qe = k.resource;
    trade_route_t nil_copy;
    int i;

    assert(planets_n < 16 && slipways_n < 16 && k.has_resource);
    for (i = 0; i < planets_n; i++) {
        planets_want[i] = entity_id_list_at(&k.planets, i);
    }
    for (i = 0; i < slipways_n; i++) {
        slipways_want[i] = entity_id_list_at(&k.slipways, i);
    }
    assert(c.planets.items != k.planets.items && c.slipways.items != k.slipways.items);
    assert(trade_equal(&c, &k));

    frame_zone_reset();
    assert(list_is(&c.planets, planets_n, planets_want) && list_is(&c.slipways, slipways_n, slipways_want));
    assert(c.has_resource && c.resource_is_bool == resource_is_bool && c.resource == qe);

    nil_copy = trade_copy(game_zone(), &no_route);
    assert(nil_copy.planets.count == 0 && nil_copy.slipways.count == 0 && !nil_copy.has_resource);
}

/* Equal when they start and end at the same planet, whatever the length. */
static void test_equal(void) {
    void *a[2];
    void *b[3];
    void *c[2];
    void *d[2];
    trade_route_t ta;
    trade_route_t tb;
    trade_route_t other;

    (void)game_route();
    a[0] = planet(0);
    a[1] = planet(1);
    b[0] = planet(0);
    b[1] = planet(2);
    b[2] = planet(1);
    c[0] = planet(2);
    c[1] = planet(1);
    d[0] = planet(0);
    d[1] = planet(2);
    ta = route(list_of(2, a), NULL, 0, 0, 0);
    tb = route(list_of(3, b), NULL, 0, 0, 0);
    assert(trade_equal(&ta, &tb) && trade_equal(&tb, &ta) && trade_equal(&ta, &ta));
    other = route(list_of(2, a), list_of(0, a), 1, 1, 1);
    assert(trade_equal(&ta, &other));
    other = route(list_of(2, c), NULL, 0, 0, 0);
    assert(!trade_equal(&ta, &other));
    other = route(list_of(2, d), NULL, 0, 0, 0);
    assert(!trade_equal(&ta, &other));
    other = route(list_of(0, a), NULL, 0, 0, 0);
    assert(!trade_equal(&ta, &other) && !trade_equal(&other, &ta));
    assert(!trade_equal(&ta, &no_route) && !trade_equal(&no_route, &ta));
    assert(!trade_equal(&ta, NULL) && !trade_equal(NULL, &ta));
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

static void build_with_bool_resource_2(void *e) {
    (void)e;
    (void)route(NULL, NULL, 1, 1, 2);
}

static void build_with_nil_resource_holding_1(void *e) {
    (void)e;
    (void)route(NULL, NULL, 0, 0, 1);
}

static void build_with_nil_resource_as_bool(void *e) {
    (void)e;
    (void)route(NULL, NULL, 0, 1, 0);
}

static void resource_of_true_route(void *e) {
    trade_route_t t = route(NULL, NULL, 1, 1, 1);
    (void)e;
    (void)trade_resource(&t);
}

static void extend_of_null(void *e) {
    (void)e;
    (void)trade_extend(game_zone(), NULL, NULL, NULL);
}

static void copy_of_null(void *e) {
    (void)e;
    (void)trade_copy(game_zone(), NULL);
}

static void last_planet_after_its_flush(void *e) {
    void *items[1];
    trade_route_t t;
    entity_t *out;

    (void)e;
    start_game();
    items[0] = planet(0);
    t = route(list_of(1, items), NULL, 0, 0, 0);
    entity_unregister(planet(0));
    cart_flush_exit_queue();
    (void)trade_last_planet(&t, &out);
}

static void test_misuse_stops_the_run(void) {
    assert(trade_ready());
    must_abort(build_with_bool_resource_2, NULL);
    must_abort(build_with_nil_resource_holding_1, NULL);
    must_abort(build_with_nil_resource_as_bool, NULL);
    must_abort(resource_of_true_route, NULL);
    must_abort(extend_of_null, NULL);
    must_abort(copy_of_null, NULL);
    must_abort(last_planet_after_its_flush, NULL);
}

static void test_route_list_holds_copies(void) {
    trade_route_list_t list;
    trade_route_t g;
    const trade_route_t *stored;
    int i;

    g = game_route();
    trade_route_list_init(&list, game_zone());
    trade_route_list_add(&list, &g);
    g.resource += p8_fixed_from_int(1);
    g.planets = (entity_id_list_t){0};
    stored = trade_route_list_at(&list, 0);
    assert(trade_route_list_count(&list) == 1);
    assert(stored->planets.items != NULL && stored->slipways.items == g.slipways.items && stored->resource == g.resource - p8_fixed_from_int(1));
    g = *stored;
    for (i = 0; i < 8; i++) {
        trade_route_list_add(&list, trade_route_list_at(&list, 0));
    }
    assert(trade_route_list_count(&list) == 9);
    assert(trade_equal(trade_route_list_at(&list, 8), &g) && trade_route_list_at(&list, 8)->planets.items == g.planets.items);
    trade_route_list_clear(&list);
    assert(trade_route_list_count(&list) == 0);
    trade_route_list_free(&list);
}

static void test_rebuild_keeps_copies_and_shares_them_with_incoming(void) {
    const entity_list_t *jo;
    const trade_route_t *outgoing = NULL;
    entity_t *to;
    const trade_route_list_t *incoming;
    int i;
    int shared = 0;

    (void)game_route();
    jo = entities_by_tag(TAG_PLANET);
    for (i = 0; outgoing == NULL && i < jo->count; i++) {
        const planet_t *exporter = as_planet(jo->items[i]);
        const trade_route_list_t *by = &exporter->outgoing_routes;
        if ((exporter->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(by) > 0) {
            assert(by->values.zone == game_zone());
            outgoing = trade_route_list_at(by, 0);
        }
    }
    assert(outgoing != NULL && outgoing->planets.zone == game_zone() && outgoing->slipways.zone == game_zone());
    frame_zone_reset();
    assert(trade_last_planet(outgoing, &to));
    incoming = &as_planet(to)->incoming_routes;
    assert(as_planet(to)->has & PLANET_HAS_INCOMING_ROUTES);
    for (i = 0; i < trade_route_list_count(incoming); i++) {
        const trade_route_t *in = trade_route_list_at(incoming, i);
        shared += in->planets.items == outgoing->planets.items && in->slipways.items == outgoing->slipways.items;
    }
    assert(shared == 1);
}

/* planet_rebuild_trades works on the planet's own outgoing routes: it
 * keeps the routes already there, and gives a planet that held none an
 * empty list in the game zone. */
static void test_rebuild_updates_the_planet_in_place(void) {
    const trade_route_t g = game_route();
    const entity_list_t *jo = entities_by_tag(TAG_PLANET);
    planet_t *exporter = NULL;
    planet_t *fresh;
    int kept;
    int i;

    for (i = 0; exporter == NULL && i < jo->count; i++) {
        planet_t *p = as_planet(jo->items[i]);
        if ((p->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(&p->outgoing_routes) > 0) {
            exporter = p;
        }
    }
    assert(exporter != NULL && trade_equal(trade_route_list_at(&exporter->outgoing_routes, 0), &g));
    kept = trade_route_list_count(&exporter->outgoing_routes);
    assert(planet_rebuild_trades(exporter) == &exporter->outgoing_routes);
    assert(trade_route_list_count(&exporter->outgoing_routes) == kept && trade_equal(trade_route_list_at(&exporter->outgoing_routes, 0), &g));

    fresh = structure_new(NULL);
    assert(!(fresh->has & PLANET_HAS_OUTGOING_ROUTES));
    assert(planet_rebuild_trades(fresh) == &fresh->outgoing_routes);
    assert((fresh->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(&fresh->outgoing_routes) == 0 &&
           fresh->outgoing_routes.values.zone == game_zone());
    assert(planet_rebuild_trades(NULL) == NULL);
}

#define STDERR_LOG "tmp/test_trade_stderr.txt"

static void must_abort_saying(void (*write)(void), const char *message) {
    pid_t child;
    int status;
    char text[2048] = {0};
    FILE *log;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        if (freopen(STDERR_LOG, "w", stderr) == NULL) _exit(2);
        write();
        _exit(0);
    }
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
    log = fopen(STDERR_LOG, "r");
    assert(log != NULL);
    assert(fread(text, 1, sizeof text - 1, log) > 0);
    fclose(log);
    remove(STDERR_LOG);
    assert(strstr(text, message) != NULL);
}

static planet_t *linked_planet(void) {
    const trade_route_t g = game_route();
    entity_t *to;

    assert(trade_last_planet(&g, &to));
    return as_planet(to);
}

static void play_frames_then_rebuild(int n) {
    int i;
    for (i = 0; i < n; i++) {
        update_entities();
        cart_flush_exit_queue();
    }
    economy_tiers_rebuild_incoming_routes();
}

static void replace_a_linked_planet(void) {
    planet_t *linked = linked_planet();
    research_project_t *replacing = research_projects_find("harvest");

    assert(replacing != NULL && replacing->class_name != NULL);
    planet_apply_project(linked, replacing);
    play_frames_then_rebuild(120);
}

static void vanish_a_linked_planet(void) {
    planet_t *linked = linked_planet();

    planet_update_vanishing(linked, p8_fixed_from_int(61));
    play_frames_then_rebuild(120);
}

static void test_a_linked_planet_never_dies(void) {
    must_abort_saying(replace_a_linked_planet, "a linked planet never dies");
    must_abort_saying(vanish_a_linked_planet, "a linked planet never dies");
}

static int g_accept_calls = 0;

static void count_accept(trade_offer_t *self, const trade_route_t *a) {
    (void)self;
    (void)a;
    g_accept_calls++;
}

/* A need of any resource (code "?") takes the resource of a trade its
 * planet accepts (accepted_resources), then answers that trade once. */
static void test_offer_trade_wildcard_want(void) {
    trade_keys_t keys = {0};
    trade_offer_t lw = {0};
    pointer_list_t *jy = malloc(sizeof *jy);
    planet_t *qh;
    need_t *want;
    p8_fixed_t any;
    int any_resource;

    start_game();
    assert(resource_code_lookup("?", &any_resource));
    any = p8_fixed_from_int(any_resource);
    want = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = any});
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, want);
    qh = structure_new(NULL);
    qh->needs = *jy;
    qh->has |= PLANET_HAS_NEEDS;
    keys.has_resource = 1;
    keys.resource = p8_fixed_from_int(qh->archetype->accepted_resources->items[0]);
    lw.route = trade_new(&keys);
    lw.accept = count_accept;
    planet_offer_trade(qh, &lw);
    assert(want->row.has_resource && want->row.resource == keys.resource && g_accept_calls == 1);
}

/* A route through trade_extend holds false, not absent: it no longer
 * answers a need with no resource, as a route with no resource does. */
static void test_offer_trade_nil_want_after_extend(void) {
    trade_offer_t lw = {0};
    pointer_list_t *jy = malloc(sizeof *jy);
    planet_t *qh;
    trade_route_t nil_route;

    start_game();
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL}));
    qh = structure_new(NULL);
    qh->needs = *jy;
    qh->has |= PLANET_HAS_NEEDS;
    nil_route = trade_new(&(trade_keys_t){.planets = *list_of(0, NULL)});
    lw.accept = count_accept;
    lw.route = nil_route;
    planet_offer_trade(qh, &lw);
    assert(g_accept_calls == 1);
    lw.route = trade_extend(game_zone(), &nil_route, NULL, NULL);
    assert(lw.route.has_resource && lw.route.resource_is_bool && lw.route.resource == 0);
    planet_offer_trade(qh, &lw);
    assert(g_accept_calls == 1);
}

/* economy_tiers_compute_tier counts for a need only the routes whose
 * resource equals the need's: a route with no resource is skipped by the
 * has_resource guard, and one through trade_extend holds false, which is
 * not the absent resource of a need with none. */
static void test_compute_tier_counts_numbered_routes_only(void) {
    pointer_list_t *jy = malloc(sizeof *jy);
    trade_route_list_t *cc = malloc(sizeof *cc);
    planet_t *qh;
    need_t *nil_want;
    need_t *x_want;
    p8_fixed_t x = p8_fixed_from_int(2);
    trade_route_t nil_route;
    trade_route_t route_value;

    start_game();
    nil_want = need_new(game_zone(), (resource_t){.kind = RESOURCE_NIL});
    x_want = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = x});
    pointer_list_init(jy, &heap_allocator);
    pointer_list_add(jy, nil_want);
    pointer_list_add(jy, x_want);
    nil_route = trade_new(NULL);
    trade_route_list_init(cc, &heap_allocator);
    trade_route_list_add(cc, &nil_route);
    route_value = trade_extend(game_zone(), &nil_route, NULL, NULL);
    trade_route_list_add(cc, &route_value);
    route_value = trade_new(&(trade_keys_t){.has_resource = 1, .resource = x});
    trade_route_list_add(cc, &route_value);
    qh = structure_new(NULL);
    qh->needs = *jy;
    qh->has |= PLANET_HAS_NEEDS;
    qh->incoming_routes = *cc;
    qh->has |= PLANET_HAS_INCOMING_ROUTES;
    entity_set_state(&qh->header, ENTITY_STATE_SETTLED);
    (void)economy_tiers_compute_tier(&qh->header);
    assert(nil_want->has_satisfied && nil_want->satisfied == p8_fixed_from_int(0));
    assert(x_want->has_satisfied && x_want->satisfied == p8_fixed_from_int(1));
}

/* resource_equal is symmetric, `number` unread outside RESOURCE_FIXED; and
 * trade_resource reads absent, false (after trade_extend) and a number. */
static void test_resource_equal(void) {
    const resource_t nil = {.kind = RESOURCE_NIL};
    const resource_t no = {.kind = RESOURCE_FALSE};
    const resource_t n0 = {.kind = RESOURCE_FIXED, .number = 0};
    const resource_t n2 = {.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(2)};
    const resource_t n3 = {.kind = RESOURCE_FIXED, .number = p8_fixed_from_int(3)};
    const trade_route_t nil_route = trade_new(NULL);
    const trade_route_t two = trade_new(&(trade_keys_t){.has_resource = 1, .resource = p8_fixed_from_int(2)});
    trade_route_t extended;
    resource_t got;

    assert(resource_equal(nil, nil) && resource_equal(no, no) && resource_equal(n2, n2));
    assert(!resource_equal(nil, no) && !resource_equal(no, nil));
    assert(!resource_equal(n0, nil) && !resource_equal(nil, n0) && !resource_equal(n0, no) &&
           !resource_equal(no, n0));
    assert(!resource_equal(n2, n3) && !resource_equal(n3, n2));
    assert(resource_equal(nil, (resource_t){.kind = RESOURCE_NIL, .number = 5}) &&
           resource_equal((resource_t){.kind = RESOURCE_FALSE, .number = 7}, no));

    assert(trade_resource(NULL).kind == RESOURCE_NIL);
    assert(trade_resource(&nil_route).kind == RESOURCE_NIL);
    extended = trade_extend(game_zone(), &nil_route, NULL, NULL);
    assert(trade_resource(&extended).kind == RESOURCE_FALSE);
    got = trade_resource(&two);
    assert(got.kind == RESOURCE_FIXED && got.number == p8_fixed_from_int(2));
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
    in_fresh_child(test_construction);
    in_fresh_child(test_last_planet);
    in_fresh_child(test_extend);
    in_fresh_child(test_built_in_the_given_zone);
    in_fresh_child(test_copy_outlives_the_frame);
    in_fresh_child(test_equal);
    in_fresh_child(test_misuse_stops_the_run);
    in_fresh_child(test_route_list_holds_copies);
    in_fresh_child(test_a_linked_planet_never_dies);
    in_fresh_child(test_rebuild_keeps_copies_and_shares_them_with_incoming);
    in_fresh_child(test_rebuild_updates_the_planet_in_place);
    in_fresh_child(test_offer_trade_wildcard_want);
    in_fresh_child(test_offer_trade_nil_want_after_extend);
    in_fresh_child(test_compute_tier_counts_numbered_routes_only);
    in_fresh_child(test_resource_equal);

    printf("test_trade: all tests passed\n");
    return 0;
}
