/* The probe, read through its struct, its defaults read on its archetype,
 * built by its three roads (the background's left_press, probe_new(NULL),
 * probe_new with the 4 keys of test_probe_spawn_particle_order.c) and
 * driven through its class's methods, draw_layers and update_entities. Its
 * methods are compared to the public functions. The help screen, of the
 * probe's lineage, is read on the title screen. No events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "game/help.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "p8/vram.h"
#include "game/particle.h"
#include "game/ui_zone.h"
#include "game/planet.h"
#include "game/probe.h"
#include "game/slipway.h"
#include "game/planet_family.h"
#include "game/wormhole.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "game/visual_random.h"
#include "engine/vec.h"

/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


static int pos_copies(const vec_t *ra, vec_t mp) {
    return ra != NULL && ra->x == mp.x && ra->y == mp.y;
}


/* The tag class instances carried, which the frozen hashes fold: any
 * other value moves every *_HASH. */
#define FROZEN_INSTANCE_TAG 0x9E5C1451u

#define BUILT_HASH 0xD2BD6AF7u
#define LIFE_HASH 0x61934B9Du
#define DROP_ON_HASH 0x49056D9Bu
#define DROP_OFF_HASH 0x3EA86A53u
#define HELP_SCREEN_HASH 0x32425337u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

static uint32_t fnv_str(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFu);
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

static int entity_list_list_count(const entity_list_t *list) {
    return list != NULL ? list->count : 0;
}

static int is_state(const entity_t *ke, state_t want) {
    return ke->state == want;
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

static int is_targeting_track_target(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == targeting_track_target_class();
}

/* The throwaway target's targeted_by, NULL when absent. */
static const void *throwaway_targeted_by(const void *t) {
    assert(is_targeting_track_target(t));
    return entity_get(((const targeting_track_target_t *)t)->targeted_by);
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    /* The frozen hashes folded the INSTANCE tag the throwaway had. */
    assert(is_targeting_track_target(ref) && "no test folds an entity outside the live entities that is not the throwaway target: to fold one, fold 0xEEEE then the frozen struct tag 0x9E5C57C7, as test_trading_hub.c does");
    return fnv(fnv(h, 0xEEEEu), FROZEN_INSTANCE_TAG);
}

/* The words the frozen hashes fold before a value: 0xF1 a number, 0x57 a
 * string; any other moves every *_HASH. */
static uint32_t fold_fixed(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(h, 0xF1u), (uint32_t)x);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return fnv_str(fnv(h, 0x57u), s);
}

/* A member the reader always answers: its presence bit and kind byte are
 * constant by the struct's shape, kept so that the hashes still prove the
 * typed reads. */
static uint32_t fold_fixed_member(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(fnv(h, 1u), 0xF1u), (uint32_t)x);
}

static uint32_t fold_bool_member(uint32_t h, int b) {
    return fnv(fnv(fnv(h, 1u), 0xB0u), (uint32_t)b);
}

static uint32_t fold_string_member(uint32_t h, const char *s) {
    return s == NULL ? fnv(h, 0u) : fnv_str(fnv(fnv(h, 1u), 0x57u), s);
}

/* A pointer that carried no tag: the frozen hashes fold 0. */
static uint32_t fold_untagged(uint32_t h, const void *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, 0xEEEEu), 0u);
}

static uint32_t fold_ref_member(uint32_t h, const void *ref) {
    return ref == NULL ? fnv(h, 0u) : fold_untagged(fnv(h, 1u), ref);
}

static uint32_t fold_is_dead(uint32_t h, const probe_t *g_) {
    return (g_->header.is_dead != 0) ? fold_bool_member(h, g_->header.is_dead != 0) : fnv(h, 0u);
}

/* A probe's 18 archetype defaults, in archetype order. */
static uint32_t fold_defaults(uint32_t h, const probe_t *g_) {
    const emitter_t *em = &g_->emitter;
    const targeting_t *pr = &g_->targeting;

    h = fold_ref_member(h, &g_->header.class->tags);
    h = fold_fixed_member(h, g_->archetype->fold_anchor);
    h = fold_string_member(h, entity_state_name(pr->archetype->confirmed_state));
    h = fold_fixed_member(h, pr->cost);
    h = fold_bool_member(h, pr->archetype->clock_ticks != 0);
    h = fold_fixed_member(h, g_->archetype->layer);
    h = fold_fixed_member(h, pr->archetype->attach_distance);
    h = fold_fixed_member(h, em->speed_factor);
    h = fold_fixed_member(h, em->archetype->lifetime);
    h = fold_fixed_member(h, em->angle_slices);
    h = fold_fixed_member(h, em->archetype->quarter_turn);
    h = fold_fixed_member(h, em->archetype->spawn_rate);
    h = fold_ref_member(h, em->trail_colors);
    h = fold_fixed_member(h, em->matrix_xx);
    h = fold_fixed_member(h, em->matrix_xy);
    h = fold_fixed_member(h, em->matrix_yx);
    h = fold_fixed_member(h, em->matrix_yy);
    return fold_fixed_member(h, em->base_radius);
}

static const economy_t *checked_main_economy(void) {
    const economy_t *hy = main_economy_typed();
    assert(hy != NULL);
    return hy;
}

/* A class's tags, in enum order: each position, then its name, then
 * the count. */
static uint32_t fold_tags(uint32_t h, unsigned tags) {
    static const char *const names[TAG_COUNT] = {
        [TAG_STRUCTURE] = "structure", [TAG_ASCENSION] = "ascension", [TAG_SLIPGATE] = "slipgate",
        [TAG_PROBE] = "probe", [TAG_MESSAGE_BOX] = "message_box", [TAG_SLIPWAY] = "slipway",
        [TAG_WORMHOLE] = "wormhole", [TAG_PLANET] = "planet", [TAG_NODE] = "node",
    };
    uint32_t k = 0;
    int t;

    for (t = 0; t < TAG_COUNT; t++) {
        if (tags & TAG(t)) {
            k++;
            h = fnv(h, k);
            h = fold_string(h, names[t]);
        }
    }
    return fnv(h, k);
}

/* An integer sequence: each position, then its value, then the count. */
static uint32_t fold_seq(uint32_t h, const int_seq_t *s) {
    int i;

    for (i = 0; i < s->count; i++) {
        h = fnv(h, (uint32_t)(i + 1));
        h = fold_fixed(h, p8_fixed_from_int(s->items[i]));
    }
    return fnv(h, (uint32_t)s->count);
}

/* The item count of a list member, -1 when NULL. */
static int size_of(const pointer_list_t *list) {
    return list == NULL ? -1 : list->count;
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

/* A game started as cart_start_game starts it, with no difficulty picked:
 * a NULL difficulty reads as difficulty 0. */
static void start_game(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    cart_start_game(NULL);
    cart_flush_exit_queue();
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static mouse_pos_t mouse_screen_only(vec_t screen) {
    return (mouse_pos_t){.screen = screen, .has = MOUSE_POS_HAS_SCREEN};
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

/* The background is the one among the hit_test entities whose archetype's
 * hit_test and layer are both 1. */
static background_t *the_background(void) {
    const entity_list_t *hd = entities_by_message(ENTITY_MESSAGE_HIT_TEST);
    background_t *found = NULL;
    int count = 0;
    int i;

    for (i = 0; hd != NULL && i < hd->count; i++) {
        const entity_t *e = hd->items[i];
        background_t *bg;

        if (e->class != background_class()) {
            continue;
        }
        bg = (background_t *)e;
        if (bg->archetype->hit_test == p8_fixed_from_int(1) && bg->archetype->layer == p8_fixed_from_int(1)) {
            found = bg;
            count++;
        }
    }
    assert(count == 1);
    return found;
}

/* A probe, whatever its form: the probe's tags and layer 2 (the help screen
 * has 10). */
static int is_probe(entity_t *e) {
    const probe_t *g_ =
        e != NULL && (e->class->lineage & LINEAGE(LINEAGE_PROBE)) ? (const probe_t *)e : NULL;
    return g_ != NULL && g_->header.class->tags == TAG(TAG_PROBE) && g_->archetype->layer == p8_fixed_from_int(2);
}

static probe_t *as_probe(void *e) {
    assert(e != NULL &&
           ((const entity_t *)e)->class == probe_class());
    return e;
}

/* First road: the background's left_press while the probe tag is empty. */
static probe_t *left_press(void) {
    background_t *bg = the_background();
    down_t out;

    assert(entity_list_list_count(entities_by_tag(TAG_PROBE)) == 0);
    out = class_of(bg)->left_press(&bg->header, mouse_pos(vec_xy(1, 1), vec_xy(2, 2)));
    assert(out.stop && entity_get(out.held) != NULL && is_probe(entity_get(out.held)));
    return as_probe(entity_get(out.held));
}

/* Third road: the 4 keys of test_probe_spawn_particle_order.c. */
static probe_t *keyed(void) {
    probe_keys_t keys = {0};

    keys.has_angle_slices = 1;
    keys.angle_slices = p8_fixed_from_int(7);
    keys.state_time = p8_fixed_from_int(3);
    keys.has_base_radius = 1;
    keys.base_radius = p8_fixed_from_int(10);
    keys.has_speed_factor = 1;
    keys.speed_factor = p8_fixed_from_int(1);
    return probe_new(&keys);
}

/* A probe as its road, emitter_init and entity_construct left it: its 18
 * defaults, state, state time, its tables' contents, its empty particle
 * list; none of the fields only targeting_track and targeting_confirm
 * write; its 5 methods, the public functions; where it was registered. */
static uint32_t fold_probe(uint32_t hash, probe_t *entity) {
    const emitter_t *emitter = &entity->emitter;
    const targeting_t *targeting = &entity->targeting;
    size_t index;

    assert(is_probe(&entity->header));
    hash = fold_fixed_member(hash, entity->archetype->fold_anchor);
    hash = fold_string_member(hash, entity_state_name(targeting->archetype->confirmed_state));
    hash = fold_fixed_member(hash, targeting->cost);
    hash = fold_bool_member(hash, targeting->archetype->clock_ticks != 0);
    hash = fold_fixed_member(hash, entity->archetype->layer);
    hash = fold_fixed_member(hash, targeting->archetype->attach_distance);
    hash = fold_fixed_member(hash, emitter->speed_factor);
    hash = fold_fixed_member(hash, emitter->archetype->lifetime);
    hash = fold_fixed_member(hash, emitter->angle_slices);
    hash = fold_fixed_member(hash, emitter->archetype->quarter_turn);
    hash = fold_fixed_member(hash, emitter->archetype->spawn_rate);
    hash = fold_fixed_member(hash, emitter->matrix_xx);
    hash = fold_fixed_member(hash, emitter->matrix_xy);
    hash = fold_fixed_member(hash, emitter->matrix_yx);
    hash = fold_fixed_member(hash, emitter->matrix_yy);
    hash = fold_fixed_member(hash, emitter->base_radius);
    hash = fold_string_member(hash, entity_state_name(entity->header.state));
    hash = fold_fixed_member(hash, entity->header.state_time);
    assert(targeting->archetype->confirmed_state != ENTITY_STATE_NONE && emitter->trail_colors != NULL);
    hash = fold_tags(hash, entity->header.class->tags);
    hash = fold_seq(hash, emitter->trail_colors);
    assert(entity->header.class->tags == TAG(TAG_PROBE));
    assert(emitter->trail_colors == &probe_trail_colors_seq);
    hash = fnv(hash, (uint32_t)size_of(particles_of(emitter)));
    assert(size_of(particles_of(emitter)) == 0);
    assert(pos_ref(&entity->header) == NULL && map_pos_ref(&entity->header) == NULL && targeting_target(targeting) == NULL && !entity->header.is_dead);
    assert(class_of(entity)->hit_test == NULL && class_of(entity)->new_year == NULL && class_of(entity)->refresh == NULL &&
           class_of(entity)->states[ENTITY_STATE_SETTLED] == NULL && class_of(entity)->states[ENTITY_STATE_NORMAL] == NULL);
    assert(class_of(entity)->drag == targeting_track_method);
    assert(class_of(entity)->drop == targeting_confirm_method);
    assert(class_of(entity)->draw_overlay == targeting_tooltip_method);
    assert(class_of(entity)->draw == emitter_update_method);
    assert(class_of(entity)->spawn_particle == probe_spawn_particle);
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), entity));
    for (index = 0; index < sizeof messages / sizeof messages[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[index]), entity));
    }
    assert(entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW), entity) && entity_list_listed(entities_by_message(ENTITY_MESSAGE_DRAW_OVERLAY), entity));
    assert(!entity_list_listed(entities_by_message(ENTITY_MESSAGE_HIT_TEST), entity) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_NEW_YEAR), entity) && !entity_list_listed(entities_by_message(ENTITY_MESSAGE_REFRESH), entity));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(TAG_PROBE), entity));
    assert(entity_list_listed(entities_by_tag(TAG_PROBE), entity));
    return hash;
}

/* One probe by each road. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    probe_t *a;
    probe_t *b;
    probe_t *c;

    start_game();
    a = left_press();
    hash = fold_probe(hash, a);
    b = probe_new(NULL);
    hash = fold_probe(hash, b);
    c = keyed();
    hash = fold_probe(hash, c);
    assert(a != b && b != c && a != c);
    assert(particles_of(&a->emitter)->zone != particles_of(&b->emitter)->zone && particles_of(&b->emitter)->zone != particles_of(&c->emitter)->zone);
    assert(a->archetype->layer == p8_fixed_from_int(2) && a->emitter.archetype->lifetime == p8_fixed_from_int(9));
    assert(a->targeting.archetype->attach_distance == 0x666);
    assert(c->emitter.angle_slices == p8_fixed_from_int(7) && c->header.state_time == p8_fixed_from_int(3) &&
           c->emitter.base_radius == p8_fixed_from_int(10) && c->emitter.speed_factor == p8_fixed_from_int(1));
    assert(b->emitter.angle_slices == p8_fixed_from_int(3) && b->header.state_time == p8_fixed_from_int(0));
    check("built", hash, BUILT_HASH);
}

static uint32_t fold_particle(uint32_t h, const particle_t *q) {
    assert(q != NULL);
    assert(!q->has_acceleration);
    h = fnv(fnv(h, (uint32_t)q->pos.x), (uint32_t)q->pos.y);
    h = fnv(fnv(h, (uint32_t)q->velocity.x), (uint32_t)q->velocity.y);
    return fnv(h, (uint32_t)q->life);
}

/* The kind of the probe's target: NULL, a planet, a slipway, the throwaway
 * struct. */
static uint32_t target_kind(const probe_t *e) {
    const void *ref;

    if (targeting_target(&e->targeting) == NULL) {
        return 1;
    }
    ref = targeting_target(&e->targeting);
    if (entity_list_listed(entities_by_tag(TAG_NODE), ref)) {
        return 2;
    }
    if (ref != NULL &&
        ((const entity_t *)ref)->class == slipway_class()) {
        return 3;
    }
    assert(is_targeting_track_target(ref));
    return 4;
}

/* The targeted_by of the probe's target, a planet, a slipway or the
 * throwaway. */
static const void *target_targeted_by(const probe_t *e) {
    entity_t *et = targeting_target(&e->targeting);

    assert(et != NULL);
    if (planet_is_family(et)) {
        return entity_get(((const planet_t *)et)->targeted_by);
    }
    if (et->class == slipway_class()) {
        return entity_get(((const slipway_t *)et)->targeted_by);
    }
    return throwaway_targeted_by(et);
}

/* The target as the frozen hashes fold it: a false boolean when NULL, else
 * the target. */
static uint32_t fold_target(uint32_t h, const probe_t *e) {
    if (targeting_target(&e->targeting) == NULL) {
        return fnv(fnv(h, 0xB0u), 0u);
    }
    return fold_ref(h, targeting_target(&e->targeting));
}

/* The first planet of the node tag. */
static planet_t *a_planet(void) {
    entity_t *e;

    assert(entity_list_list_count(entities_by_tag(TAG_NODE)) > 0);
    e = entities_by_tag(TAG_NODE)->items[0];
    assert(planet_is_family(e));
    return (planet_t *)e;
}

static vec_t planet_map_pos(void) {
    const planet_t *planet = a_planet();
    assert(map_pos_ref(&planet->header) != NULL);
    return *map_pos_ref(&planet->header);
}

/* A slipway between two bare planets far from each other and from the
 * rest, its end set directly; its middle is the returned point. */
static slipway_t *a_slipway(vec_t *middle) {
    planet_keys_t keys = {0};
    slipway_keys_t slipway_keys = {0};
    void *qu;
    void *re;
    slipway_t *jc;
    vec_t base = planet_map_pos();
    vec_t start_p = vec_add(base, vec_xy(0, 1));
    vec_t end_p = vec_add(base, vec_xy(1, 1));

    keys.map_pos = start_p;
    keys.has_map_pos = 1;
    planet_keys_set_id(&keys, "s91");
    qu = planet_new(&keys);
    keys.map_pos = end_p;
    keys.has_map_pos = 1;
    planet_keys_set_id(&keys, "s92");
    re = planet_new(&keys);
    slipway_keys.start = entity_id_link(qu);
    jc = slipway_new(&slipway_keys);
    jc->end = entity_id_link(re);
    *middle = vec_div(vec_add(start_p, end_p), p8_fixed_from_int(2));
    return jc;
}

/* The mouse one pixel off the given point (map units, as map_pos):
 * entity_geom_nearest skips an entity at the very position. */
static vec_t mouse_at(vec_t at) {
    return vec_add(vec_mul(at, p8_fixed_from_int(128)), vec_xy(1, 1));
}

/* The drag at the given point, the screen mouse at the given coordinate:
 * the target's kind, its targeted_by, pos a copy of what was passed,
 * map_pos. */
static uint32_t fold_drag(uint32_t h, probe_t *e, vec_t at, int rp, uint32_t want_kind) {
    vec_t mp = mouse_at(at);
    uint32_t kind;

    class_of(e)->drag(&e->header, mouse_pos(mp, vec_xy(rp, rp)));
    kind = target_kind(e);
    printf("[drag] target kind %u\n", (unsigned)kind);
    fflush(stdout);
    assert(kind == want_kind);
    assert(pos_copies(pos_ref(&e->header), mp));
    assert(kind == 1 || target_targeted_by(e) == e);
    h = fnv(h, kind);
    h = fold_target(h, e);
    h = fold_vec(h, map_pos_ref(&e->header));
    return h;
}

/* spawn_particle; the drag on a planet, a slipway, the void; draw_overlay
 * with a target and without; draw_layers(ENTITY_MESSAGE_DRAW) with the particle lists'
 * sizes; update_entities steps, then is_dead raised by the drop taking it
 * out of the live entities and the tags. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    probe_t *first;
    probe_t *second;
    slipway_t *slipway;
    vec_t slipway_center;
    int index;

    start_game();
    first = left_press();
    second = probe_new(NULL);
    slipway = a_slipway(&slipway_center);
    hash = fold_particle(hash, class_of(first)->spawn_particle(&first->header));
    hash = fold_particle(hash, class_of(second)->spawn_particle(&second->header));

    hash = fold_drag(hash, first, planet_map_pos(), 64, 2);
    hash = fold_drag(hash, first, slipway_center, 64, 3);
    assert((void *)entity_get(slipway->targeted_by) == first);
    hash = fold_drag(hash, first, vec_add(slipway_center, vec_xy(0, 3)), 64, 1);
    hash = fold_drag(hash, first, vec_add(slipway_center, vec_xy(0, 3)), 2, 4);

    entity_set_state(&first->header, ENTITY_STATE_PLACING);
    entity_set_state(&second->header, ENTITY_STATE_PLACING);
    class_of(first)->draw_overlay(&first->header, (vec_t){0, 0});
    hash = fnv(hash, screen_hash());
    class_of(second)->draw_overlay(&second->header, (vec_t){0, 0});
    hash = fnv(hash, screen_hash());
    class_of(first)->drag(&first->header, mouse_pos(mouse_at(planet_map_pos()), vec_xy(64, 64)));
    class_of(first)->draw_overlay(&first->header, (vec_t){0, 0});
    hash = fnv(hash, screen_hash());

    for (index = 0; index < 3; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
        hash = fnv(hash, (uint32_t)size_of(particles_of(&first->emitter)));
        hash = fnv(hash, (uint32_t)size_of(particles_of(&second->emitter)));
    }
    for (index = 0; index < 3; index++) {
        update_entities();
        hash = fold_fixed_member(hash, first->header.state_time);
        hash = fold_fixed_member(hash, second->header.state_time);
        hash = fold_string_member(hash, entity_state_name(first->header.state));
        assert(entity_list_listed(live_entities(), first) && entity_list_listed(live_entities(), second));
    }
    assert(class_of(first)->drop == targeting_confirm_method);
    assert(targeting_confirm(&first->header) == 0);
    assert(first->header.is_dead);
    update_entities();
    assert(!entity_list_listed(live_entities(), first) && !entity_list_listed(entities_by_tag(TAG_PROBE), first));
    assert(entity_list_listed(live_entities(), second) && entity_list_listed(entities_by_tag(TAG_PROBE), second));
    hash = fnv(hash, (uint32_t)live_entities()->count);
    check("life", hash, LIFE_HASH);
}

/* The drop with a target: is_dead raised, 0 returned. */
static void test_drop_on(void) {
    uint32_t h = 2166136261u;
    probe_t *a;
    int out;

    start_game();
    a = left_press();
    class_of(a)->drag(&a->header, mouse_pos(mouse_at(planet_map_pos()), vec_xy(64, 64)));
    assert(class_of(a)->drop == targeting_confirm_method);
    out = targeting_confirm(&a->header);
    assert(out == 0);
    assert(a->header.is_dead);
    h = fold_string_member(h, entity_state_name(a->header.state));
    h = fold_fixed_member(h, a->header.state_time);
    h = fold_is_dead(h, a);
    h = fold_fixed_member(h, checked_main_economy()->credits);
    check("drop_on", h, DROP_ON_HASH);
}

/* The drop with no target and enough credits: entity_set_state to the
 * confirmed state, 1 returned. */
static void test_drop_off(void) {
    uint32_t h = 2166136261u;
    probe_t *a;
    int out;

    start_game();
    a = left_press();
    assert(class_of(a)->drop == targeting_confirm_method);
    out = targeting_confirm(&a->header);
    assert(out == 1);
    assert(!a->header.is_dead);
    assert(is_state(&a->header, ENTITY_STATE_SETTLED));
    assert(a->header.state_time == p8_fixed_from_int(0));
    h = fold_string_member(h, entity_state_name(a->header.state));
    h = fold_fixed_member(h, a->header.state_time);
    h = fold_fixed_member(h, checked_main_economy()->credits);
    check("drop_off", h, DROP_OFF_HASH);
}

/* Without the clock, the drop charges and sets the confirmed state as
 * targeting_confirm does, but leaves the year and its ticks: past the
 * year's 15 ticks, targeting_confirm starts a new year even on 0 ticks. */
static void test_drop_off_without_clock(void) {
    probe_t *a;
    probe_t *b;
    economy_t *economy;
    p8_fixed_t credits;
    p8_fixed_t year;

    start_game();
    economy = main_economy_typed();
    assert(economy != NULL);
    a = left_press();
    year = economy->year;
    economy->year_ticks = p8_fixed_from_int(16);
    assert(targeting_confirm(&a->header) == 1);
    assert(economy->year == p8_fixed_add(year, p8_fixed_from_int(1)) && economy->year_ticks < p8_fixed_from_int(16));

    b = probe_new(NULL);
    credits = economy->credits;
    year = economy->year;
    economy->year_ticks = p8_fixed_from_int(16);
    assert(targeting_confirm_without_clock(&b->header) == 1);
    assert(!b->header.is_dead && is_state(&b->header, ENTITY_STATE_SETTLED));
    assert(economy->credits == p8_fixed_sub(credits, b->targeting.has_cost ? b->targeting.cost : 0));
    assert(economy->year == year && economy->year_ticks == p8_fixed_from_int(16));
}

/* The drop over the void, off the panel: the target is the throwaway struct,
 * not NULL, so the drop is refused (is_dead raised, 0 returned) and costs
 * nothing. */
static void test_drop_void(void) {
    probe_t *a;
    vec_t middle;
    p8_fixed_t nz;
    state_t me;
    int out;

    start_game();
    a = left_press();
    (void)a_slipway(&middle);
    class_of(a)->drag(&a->header, mouse_pos(mouse_at(vec_add(middle, vec_xy(0, 3))), vec_xy(2, 2)));
    assert(target_kind(a) == 4);
    assert(target_targeted_by(a) == a);
    assert(!a->header.is_dead);
    nz = checked_main_economy()->credits;
    me = a->header.state;
    assert(class_of(a)->drop == targeting_confirm_method);
    out = targeting_confirm(&a->header);
    assert(out == 0);
    assert(a->header.is_dead);
    assert(checked_main_economy()->credits == nz);
    assert(a->header.state == me);
    assert(target_kind(a) == 4);
    printf("[drop void] refused\n");
}

/* The help screen: the probe's tags and layer 10. */
static int is_help(entity_t *e) {
    const probe_t *g_ =
        e != NULL && (e->class->lineage & LINEAGE(LINEAGE_PROBE)) ? (const probe_t *)e : NULL;
    return g_ != NULL && g_->header.class->tags == TAG(TAG_PROBE) && g_->archetype->layer == p8_fixed_from_int(10);
}

/* The title screen's help screen, of the probe's lineage: its tags,
 * fold_anchor and confirmed_state inherited, draw_overlay and
 * spawn_particle the probe's functions, a draw of its own. */
static void test_help_screen(void) {
    uint32_t hash = 2166136261u;
    const entity_list_t *live;
    entity_t *help_entity = NULL;
    const probe_t *help_screen;
    int index;

    p8_input_init(&input);
    dispatch_context_set_input(&input);
    cart_init();
    live = live_entities();
    for (index = 0; index < live->count; index++) {
        entity_t *e = live->items[index];
        if (is_help(e)) {
            assert(help_entity == NULL);
            help_entity = e;
        }
    }
    assert(help_entity != NULL && !is_probe(help_entity));
    help_screen = (const probe_t *)help_entity;
    assert(help_screen->header.class->tags == TAG(TAG_PROBE));
    assert(help_screen->archetype->fold_anchor == p8_fixed_from_int(1));
    assert(help_screen->targeting.archetype->confirmed_state == ENTITY_STATE_SETTLED);
    assert(help_screen->archetype->layer == p8_fixed_from_int(10));
    assert(help_screen->emitter.trail_colors == &help_screen_trail_colors_seq);
    assert(help_screen->emitter.trail_colors != &probe_trail_colors_seq);
    assert(class_of(help_screen)->draw_overlay == targeting_tooltip_method);
    assert(class_of(help_screen)->spawn_particle == probe_spawn_particle);
    assert(class_of(help_screen)->drag == targeting_track_method);
    assert(class_of(help_screen)->drop == targeting_confirm_method);
    assert(class_of(help_screen)->draw != emitter_update_method);
    assert(class_of(help_screen)->draw == help_screen_draw_method);
    assert(class_of(help_screen)->hit_test != NULL);
    assert(probe_ready());
    assert(size_of(particles_of(&help_screen->emitter)) >= 0);
    hash = fnv(hash, (uint32_t)size_of(particles_of(&help_screen->emitter)));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live, help_screen));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(TAG_PROBE), help_screen));
    assert(entity_list_listed(entities_by_tag(TAG_PROBE), help_screen));
    hash = fold_defaults(hash, help_screen);
    for (index = 0; index < 3; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
        hash = fnv(hash, (uint32_t)size_of(particles_of(&help_screen->emitter)));
    }
    check("help_screen", hash, HELP_SCREEN_HASH);
}

static int is_a_probe_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == probe_class();
}

/* Built by any road, a probe is a struct whose class descriptor holds the
 * probe's 5 public functions and no other method. The title's help screen
 * is a struct of its own class. */
static void test_probe_is_a_struct(void) {
    const entity_class_t *descriptor = probe_class();
    const entity_list_t *live;
    probe_t *probes[3];
    size_t index;
    int j;

    start_game();
    probes[0] = left_press();
    probes[1] = probe_new(NULL);
    probes[2] = keyed();
    for (index = 0; index < 3; index++) {
        assert(is_a_probe_struct(probes[index]));
    }
    assert(descriptor->drag == targeting_track_method && descriptor->drop == targeting_confirm_method && descriptor->draw_overlay == targeting_tooltip_method &&
           descriptor->draw == emitter_update_method && descriptor->spawn_particle == probe_spawn_particle);
    assert(descriptor->attach_slipway == NULL && descriptor->offer_trade == NULL && descriptor->draw_at == NULL);
    assert(descriptor->hit_test == NULL && descriptor->new_year == NULL && descriptor->refresh == NULL && descriptor->has_refresh == NULL && descriptor->left_press == NULL &&
           descriptor->accept_drop == NULL);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(descriptor->states[j] == NULL);
    }
    assert(probe_ready());

    {
        probe_archetype_t layer_zero = probe_archetype;
        probe_t bare;

        layer_zero.layer = 0;
        probe_init_instance(&bare, descriptor, &layer_zero);
        assert(is_a_probe_struct(&bare) && !is_probe(&bare.header));
    }

    cart_init();
    live = live_entities();
    for (j = 0; j < live->count; j++) {
        entity_t *e = live->items[j];
        if (is_help(e)) {
            break;
        }
    }
    assert(j < live->count);
    assert(live->items[j] != NULL &&
           ((const entity_t *)live->items[j])->class == help_screen_class());
}

/* The throwaway target is a struct that keeps targeted_by, the probe aiming
 * at it, and has no draw_overlay. */
static void test_targeting_track_target_is_a_struct(void) {
    probe_t *a;
    void *et;
    vec_t middle;

    start_game();
    a = left_press();
    (void)a_slipway(&middle);
    class_of(a)->drag(&a->header, mouse_pos(mouse_at(vec_add(middle, vec_xy(0, 3))), vec_xy(2, 2)));
    assert(targeting_target(&a->targeting) != NULL);
    et = targeting_target(&a->targeting);
    assert(is_targeting_track_target(et) && throwaway_targeted_by(et) == a);
    assert(class_of(et)->draw_overlay == NULL);
}

/* Over the void, a drag released within 10 pixels of any screen edge has
 * the throwaway target, so the drop cancels; 15 pixels in, it has none, so
 * the drop places. */
static void test_drop_near_an_edge_cancels(void) {
    static const int screen_points[][2] = {
        {P8_SCREEN_WIDTH - 5, P8_SCREEN_HEIGHT / 2}, {P8_SCREEN_WIDTH / 2, P8_SCREEN_HEIGHT - 5},
        {5, P8_SCREEN_HEIGHT / 2}, {P8_SCREEN_WIDTH / 2, 5},
    };
    probe_t *a;
    vec_t middle;
    vec_t void_point;
    size_t i;

    start_game();
    a = left_press();
    (void)a_slipway(&middle);
    void_point = mouse_at(vec_add(middle, vec_xy(0, 3)));
    for (i = 0; i < sizeof screen_points / sizeof screen_points[0]; i++) {
        int x = screen_points[i][0];
        int y = screen_points[i][1];
        int inward_x = x < P8_SCREEN_WIDTH / 2 ? x + 10 : (x > P8_SCREEN_WIDTH / 2 ? x - 10 : x);
        int inward_y = y < P8_SCREEN_HEIGHT / 2 ? y + 10 : (y > P8_SCREEN_HEIGHT / 2 ? y - 10 : y);

        class_of(a)->drag(&a->header, mouse_pos(void_point, vec_xy(x, y)));
        assert(target_kind(a) == 4);
        class_of(a)->drag(&a->header, mouse_pos(void_point, vec_xy(inward_x, inward_y)));
        assert(target_kind(a) == 1);
    }
}

/* Each scenario runs in a child forked right after cart_load(), as a fresh
 * process would start it: the rnd state is the same for all. */
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

/* What an emitter holds in the particle zone, by kind, and the UI zone's
 * rows. */
typedef struct {
    int particle;
    int particles;
    int particles_help_screen;
    int rows;
} held_t;

static held_t held_now(void) {
    held_t h;
    h.particle = particle_zone_live(PARTICLE_SLOT_PARTICLE);
    h.particles = particle_zone_live(PARTICLE_SLOT_LIST);
    h.particles_help_screen = particle_zone_live(PARTICLE_SLOT_HELP_LIST);
    h.rows = ui_zone_live(SLOT_KIND_ROW);
    return h;
}

/* A probe that leaves (entity_unregister, then the exit queue's flush)
 * gives back its particles and its particle list, and keeps its struct. */
static void test_leaving_gives_back_its_particles(void) {
    held_t before;
    held_t after;
    probe_t *a;

    start_game();
    before = held_now();
    a = left_press();
    emitter_spawn(&a->header, p8_fixed_from_int(5));
    assert(particles_of(&a->emitter)->count == 5);
    assert(held_now().particle == before.particle + 5 && held_now().particles == before.particles + 1);
    entity_unregister(&a->header);
    cart_flush_exit_queue();
    after = held_now();
    assert(after.particle == before.particle && after.particles == before.particles);
    assert(particles_of(&a->emitter) == NULL);
}

/* A probe's particles are visual only: emitting them, through the spawn
 * loop or one particle at a time, draws from the visual stream and leaves
 * the game stream where it was. */
static void test_particles_draw_from_the_visual_stream(void) {
    probe_t *a;
    uint32_t game;
    uint32_t visual;

    start_game();
    a = left_press();
    game = p8_fixed_rnd_state();
    visual = visual_random_state();
    emitter_spawn(&a->header, p8_fixed_from_int(5));
    assert(particles_of(&a->emitter)->count == 5);
    assert(p8_fixed_rnd_state() == game && "the emitter's spawn loop draws no game number");
    assert(visual_random_state() != visual);

    visual = visual_random_state();
    (void)probe_spawn_particle(&a->header);
    assert(p8_fixed_rnd_state() == game && "a probe particle draws no game number");
    assert(visual_random_state() != visual);
}

static void test_target_links_resolve_until_the_flush(void) {
    probe_t *first;
    probe_t *second;
    planet_t *planet;

    start_game();
    first = left_press();
    class_of(first)->drag(&first->header, mouse_pos(mouse_at(planet_map_pos()), vec_xy(64, 64)));
    planet = a_planet();
    assert(targeting_target(&first->targeting) == &planet->header);
    assert(entity_get(planet->targeted_by) == &first->header);
    entity_unregister(&first->header);
    assert(entity_get(planet->targeted_by) == &first->header);
    cart_flush_exit_queue();
    assert(entity_get(planet->targeted_by) == NULL);

    second = probe_new(NULL);
    class_of(second)->drag(&second->header, mouse_pos(mouse_at(planet_map_pos()), vec_xy(64, 64)));
    assert(targeting_target(&second->targeting) == &planet->header);
    entity_unregister(&planet->header);
    assert(targeting_target(&second->targeting) == &planet->header);
    cart_flush_exit_queue();
    assert(targeting_target(&second->targeting) == NULL);
}

static int header_at(const entity_t *e, vec_t world) {
    vec_t map = vec_div(world, p8_fixed_from_int(128));

    return entity_has_pos(e) && e->pos.x == world.x && e->pos.y == world.y && entity_has_map_pos(e) &&
           e->map_pos.x == map.x && e->map_pos.y == map.y;
}

/* A drag writes the host's position and map position into its header; one
 * without a world point clears the position and puts the map position at
 * (0,0); the next drag overwrites both, a zero world point included. */
static void drag_writes_the_header_of(entity_t *host) {
    vec_t first = vec_xy(640, 320);
    vec_t second = vec_xy(256, 512);
    vec_t screen = vec_xy(64, 64);

    targeting_track(host, mouse_pos(first, screen));
    assert(header_at(host, first));
    targeting_track(host, mouse_screen_only(screen));
    assert(!entity_has_pos(host) && entity_has_map_pos(host) && host->map_pos.x == 0 && host->map_pos.y == 0);
    targeting_track(host, mouse_pos(second, screen));
    assert(header_at(host, second));
    targeting_track(host, mouse_pos(vec_xy(0, 0), screen));
    assert(header_at(host, vec_xy(0, 0)) && "a world point given as zero is not an absent one");
}

static void test_drag_writes_the_host_header(void) {
    start_game();
    drag_writes_the_header_of(&probe_new(NULL)->header);
    drag_writes_the_header_of(&help_screen_new()->header);
    drag_writes_the_header_of(&structure_new(NULL)->header);
    drag_writes_the_header_of(&wormhole_new()->header);
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
    in_fresh_child(test_target_links_resolve_until_the_flush);
    in_fresh_child(test_life);
    in_fresh_child(test_drop_on);
    in_fresh_child(test_drop_off);
    in_fresh_child(test_drop_off_without_clock);
    in_fresh_child(test_drop_void);
    in_fresh_child(test_help_screen);
    in_fresh_child(test_probe_is_a_struct);
    in_fresh_child(test_targeting_track_target_is_a_struct);
    in_fresh_child(test_drop_near_an_edge_cancels);
    in_fresh_child(test_leaving_gives_back_its_particles);
    in_fresh_child(test_particles_draw_from_the_visual_stream);
    in_fresh_child(test_drag_writes_the_host_header);

    printf("test_probe: all tests passed\n");
    return 0;
}
