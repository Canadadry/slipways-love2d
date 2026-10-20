/* The particle burst, read and written on its struct members and the
 * generated defaults (game/game_data.h), built by its three roads
 * (planet_apply_project on the harvest recipe, protostar_construct,
 * burst_new(NULL)) and driven through the real class methods and
 * draw_layers. The planet's methods are named by planet_class(). No events
 * file. */

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
#include "game/cart.h"
#include "engine/entity_id.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "game/layout.h"
#include "p8/vram.h"
#include "game/particle.h"
#include "game/ui_zone.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "game/ui.h"
#include "game/visual_random.h"
#include "engine/vec.h"
#include "engine/game_zone.h"

/* The burst copies the planet's position and map position, values and presence. */
/* The emitter's particle list, NULL without EMITTER_HAS_PARTICLES. */
static pointer_list_t *particles_of(const emitter_t *emitter) {
    return (emitter->has & EMITTER_HAS_PARTICLES) ? (pointer_list_t *)&emitter->particles : NULL;
}

static int same_positions(const entity_t *a, const entity_t *b) {
    return entity_has_pos(a) == entity_has_pos(b) && entity_has_map_pos(a) == entity_has_map_pos(b) && a->pos.x == b->pos.x && a->pos.y == b->pos.y && a->map_pos.x == b->map_pos.x && a->map_pos.y == b->map_pos.y;
}

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}



#define BUILT_HASH 0xA539DD6Du
#define LIFE_HASH 0x6BB38879u
#define BURST_FRAMES 84

/* The tag a layout row's placed items carried as a tagged list, which the
 * frozen hashes fold: any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const tag_t tags[] = {TAG_PLANET, TAG_NODE, TAG_ASCENSION, TAG_STRUCTURE};
static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};

static int tag_count(unsigned tags) {
    int t, n = 0;

    for (t = 0; t < TAG_COUNT; t++) {
        n += (tags & TAG(t)) != 0;
    }
    return n;
}

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

static uint32_t fold_absent(uint32_t h) {
    return fnv(h, 0u);
}

static uint32_t fold_fixed_member(uint32_t h, p8_fixed_t v) {
    return fnv(fnv(fnv(h, 1u), 0xF1u), (uint32_t)v);
}

static uint32_t fold_bool_member(uint32_t h, int b) {
    return fnv(fnv(fnv(h, 1u), 0xB0u), (uint32_t)(b != 0));
}

/* A NULL string folds as absent. */
static uint32_t fold_string_member(uint32_t h, const char *s) {
    return s == NULL ? fold_absent(h) : fnv_str(fnv(fnv(h, 1u), 0x57u), s);
}

static uint32_t fold_is_planet(uint32_t h, const planet_t *jo) {
    return jo->archetype->is_planet ? fold_fixed_member(h, p8_fixed_from_int(1)) : fold_bool_member(h, 0);
}

static uint32_t fold_is_dead(uint32_t h, const planet_t *jo) {
    return (jo->header.is_dead != 0) ? fold_bool_member(h, jo->header.is_dead) : fold_absent(h);
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

static uint32_t fold_ref_member(uint32_t h, const void *ref) {
    assert(ref == NULL && "fold_ref_member: no test folds a set ref: to fold one, fold 1 then its rank in the live entities (0x4E, rank), or outside them 0xEEEE then the frozen struct tag 0x9E5C57C7, as the frozen hashes did");
    return fold_absent(h);
}

/* The item count of a list member, -1 when NULL. */
static int pointer_list_size(const pointer_list_t *l) {
    return l == NULL ? -1 : l->count;
}

static int id_list_count(const entity_id_list_t *l) {
    return l == NULL ? -1 : l->count;
}

/* The words the frozen hashes fold before a value: 0xF1 a number, 0xB0 a
 * boolean (then 0 or 1), 0x57 a string; any other moves every *_HASH. */
static uint32_t fold_fixed(uint32_t h, p8_fixed_t x) {
    return fnv(fnv(h, 0xF1u), (uint32_t)x);
}

static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, 0xB0u), b ? 1u : 0u);
}

static uint32_t fold_string(uint32_t h, const char *s) {
    return fnv_str(fnv(h, 0x57u), s);
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
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static mouse_pos_t mouse_world_only(vec_t world) {
    return (mouse_pos_t){.world = world, .has = MOUSE_POS_HAS_WORLD};
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static vec_t near_main_camera(int x, int y) {
    const vec_t *main_camera_pos;

    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    return vec_add(*main_camera_pos, vec_xy(x + 16, y - 4));
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

/* The class name is "burst" on a burst and on nothing else. */
static int is_burst(const void *e) {
    return e != NULL && class_of(e) == burst_class() && ((const planet_t *)e)->archetype->class_name != NULL &&
           strcmp(((const planet_t *)e)->archetype->class_name, "burst") == 0;
}

static planet_t *as_burst(void *e) {
    assert(e != NULL && class_of(e) == burst_class());
    return e;
}

/* The first live burst at the given rank or later, NULL when none. */
static void *burst_from(int from) {
    const entity_list_t *hv = live_entities();
    int i;
    for (i = from; i < hv->count; i++) {
        if (is_burst(hv->items[i])) {
            return hv->items[i];
        }
    }
    return NULL;
}

/* The research project whose class name is "burst": the harvest. */
static void *harvest_recipe(void) {
    void *found = NULL;
    int i;

    for (i = 1; i <= research_projects_count(); i++) {
        research_project_t *hp = research_projects_at(i);
        if (hp->class_name != NULL && strcmp(hp->class_name, "burst") == 0) {
            assert(found == NULL || found == hp);
            found = hp;
        }
    }
    assert(found != NULL);
    return found;
}

/* First road: a bare planet on planet_id, map_pos and pos, harvested by
 * planet_apply_project; the burst is the one the live entities gained. */
static void *harvest(int x, int y, const char *id) {
    planet_keys_t keys = {0};
    planet_t *planet;
    void *made;
    int before = live_entities()->count;

    if (id != NULL) {
        planet_keys_set_id(&keys, id);
    }
    keys.map_pos = vec_xy(x / 2, y / 2);
    keys.has_map_pos = 1;
    keys.pos = near_main_camera(x, y);
    keys.has_pos = 1;
    planet = planet_new(&keys);
    assert(!is_burst(planet) && live_entities()->count == before + 1);
    planet_apply_project(planet, harvest_recipe());
    made = burst_from(before + 1);
    assert(made != NULL && live_entities()->count == before + 2 && burst_from(0) == made);
    assert(same_positions(&as_burst(made)->header, &planet->header));
    return made;
}

/* Second road: protostar_new on map_pos, pos and planet_id; its burst is
 * registered just before the protostar. */
static void *spawn(int x, int y, const char *id, void **protostar) {
    planet_keys_t keys = {0};
    void *pm;
    int at;

    if (id != NULL) {
        planet_keys_set_id(&keys, id);
    }
    keys.map_pos = vec_xy(x / 2, y / 2);
    keys.has_map_pos = 1;
    keys.pos = near_main_camera(x, y);
    keys.has_pos = 1;
    pm = protostar_new(&keys);
    at = entity_list_rank_in(live_entities(), pm);
    assert(at >= 1 && is_burst(live_entities()->items[at - 1]));
    {
        const vec_t *ra = pos_ref(&as_burst(live_entities()->items[at - 1])->header);
        vec_t want = vec_add(keys.pos, ((planet_t *)pm)->archetype->spawn_offset);
        assert(ra != NULL && ra->x == want.x && ra->y == want.y);
    }
    if (protostar != NULL) {
        *protostar = pm;
    }
    return live_entities()->items[at - 1];
}

/* A burst as its road and burst_construct, planet_construct_in and
 * entity_construct left it: its scalars, which of its own and the planet's
 * names it has, its 12 methods (the planet's very functions but draw and
 * spawn_particle), which default tables it shares with the burst and planet
 * archetypes, where entity_register filed it -- among the live entities
 * and the messages, under no tag -- and the camera's shake. */
static uint32_t fold_burst(uint32_t hash, void *entity, int from_protostar) {
    const planet_t *burst = as_burst(entity);
    const emitter_t *emitter = &burst->emitter;
    const planet_t *planet = burst;
    int inner_index;

    assert(is_burst(entity));
    hash = fold_string_member(hash, burst->archetype->class_name);
    hash = fold_fixed_member(hash, planet->archetype->layer);
    hash = fold_fixed_member(hash, emitter->archetype->spawn_rate);
    hash = fold_fixed_member(hash, emitter->matrix_xx);
    hash = fold_fixed_member(hash, emitter->matrix_xy);
    hash = fold_fixed_member(hash, emitter->matrix_yx);
    hash = fold_fixed_member(hash, emitter->matrix_yy);
    hash = fold_fixed_member(hash, burst->acceleration_factor);
    hash = fold_fixed_member(hash, emitter->speed_factor);
    hash = fold_fixed_member(hash, burst->speed);
    hash = fold_is_planet(hash, planet);
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_fixed_member(hash, planet->header.shake);
    hash = fold_is_dead(hash, planet);
    hash = fold_vec(hash, pos_ref(&planet->header));
    hash = fold_vec(hash, map_pos_ref(&planet->header));
    hash = fnv(hash, (uint32_t)(emitter->trail_colors == NULL ? -1 : emitter->trail_colors->count));
    hash = fnv(hash, (uint32_t)tag_count(planet->header.class->tags));
    hash = fnv(hash, (uint32_t)trade_route_list_count(&planet->outgoing_routes));
    hash = fnv(hash, (uint32_t)pointer_list_size(particles_of(emitter)));
    hash = fnv(hash, (uint32_t)id_list_count(&planet->slipways));
    assert(main_camera_typed() != NULL);
    hash = fold_fixed_member(hash, main_camera_typed()->header.shake);
    /* The 1s of spawn_rate, the four matrix entries, acceleration_factor,
     * speed_factor, speed (the burst's) and of is_planet, trade_multiplier, layer,
     * shake, back_color (the planet's): constant, so BUILT_HASH proves the reads. */
    hash = fnv(hash, (uint32_t)(burst->archetype->class_name != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(emitter->trail_colors != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(particles_of(emitter) != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(planet->archetype->ring_colors != NULL));
    hash = fnv(hash, (uint32_t)(planet->archetype->mood_rows != NULL));
    hash = fnv(hash, (uint32_t)(planet->archetype->made_resources != NULL));
    hash = fnv(hash, (uint32_t)(planet_production_read(planet) != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(planet->archetype->spacer != NULL));
    hash = fnv(hash, (uint32_t)(planet_id_or_null(planet) != NULL));
    hash = fnv(hash, (uint32_t)(map_pos_ref(&planet->header) != NULL));
    hash = fnv(hash, (uint32_t)(pos_ref(&planet->header) != NULL));
    hash = fnv(hash, (uint32_t)(planet->slipways.zone != NULL));
    hash = fnv(hash, (uint32_t)(research_project_get(planet->project) != NULL));
    hash = fnv(hash, (uint32_t)(planet->description != NULL));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_NEEDS) != 0));
    hash = fnv(hash, (uint32_t)(planet->planet_type != NULL));
    hash = fnv(hash, (uint32_t)(planet->header.fold_anchor != 0));
    hash = fnv(hash, (uint32_t)(planet->layout_row != NULL));
    hash = fnv(hash, (uint32_t)(entity_get(planet->targeted_by) != NULL));
    hash = fnv(hash, (uint32_t)(planet->has_production_bonus != 0));
    hash = fnv(hash, (uint32_t)(planet->header.is_dead != 0));
    hash = fnv(hash, (uint32_t)(planet->has_net_income != 0));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_INCOMING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_OUTGOING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)(planet->has_development_tier != 0));
    hash = fnv(hash, (uint32_t)(planet->header.is_hovered != 0));
    assert(!(planet->has & PLANET_HAS_NEEDS) && !planet->has_net_income);
    assert(class_of(entity)->attach_slipway == planet_class()->attach_slipway && class_of(entity)->offer_trade == planet_class()->offer_trade &&
           class_of(entity)->attach_slipway != NULL && class_of(entity)->offer_trade != NULL && planet_is_family(entity));
    assert(class_of(entity)->spawn_particle != planet_class()->spawn_particle && class_of(entity)->spawn_particle != NULL);
    assert(class_of(entity)->draw != NULL && class_of(entity)->draw != planet_class()->draw);
    assert(class_of(entity)->hit_test == planet_class()->hit_test && class_of(entity)->left_press == planet_class()->left_press &&
           class_of(entity)->refresh == planet_class()->refresh && class_of(entity)->accept_drop == planet_class()->accept_drop &&
           class_of(entity)->new_year == planet_class()->new_year &&
           class_of(entity)->states[ENTITY_STATE_VANISHING] == planet_class()->states[ENTITY_STATE_VANISHING]);
    assert(class_of(entity)->hit_test != NULL && class_of(entity)->left_press != NULL && class_of(entity)->refresh != NULL && class_of(entity)->accept_drop != NULL &&
           class_of(entity)->new_year != NULL && class_of(entity)->states[ENTITY_STATE_VANISHING] != NULL);
    assert(class_of(entity)->states[ENTITY_STATE_NORMAL] == NULL && class_of(entity)->draw_overlay == NULL &&
           class_of(entity)->states[ENTITY_STATE_PLACING] == NULL && class_of(entity)->states[ENTITY_STATE_UNFOLDING] == NULL &&
           class_of(entity)->states[ENTITY_STATE_SETTLED] == NULL);
    assert(class_of(entity)->draw_at == NULL && class_of(entity)->drag == NULL && class_of(entity)->drop == NULL);
    assert(planet->header.class->tags == 0);
    assert(planet->archetype == &burst_archetype);
    assert(planet->has & PLANET_HAS_OUTGOING_ROUTES);
    if (from_protostar) {
        assert(emitter->trail_colors == &protostar_burst_trail_colors_seq);
    } else {
        assert(emitter->trail_colors == &burst_trail_colors_seq);
    }
    assert(planet->archetype == &burst_archetype);
    assert(!(planet->has & PLANET_HAS_PRODUCTION));
    assert(planet->archetype->spacer == &planet_spacer_row);
    assert(planet->archetype->ring_colors == &planet_ring_colors_seq);
    assert(planet->archetype->mood_rows == planet_mood_rows_rows);
    assert(planet->archetype->made_resources == &planet_made_resources_seq);
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), entity));
    for (inner_index = 0; inner_index < (int)(sizeof messages / sizeof messages[0]); inner_index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[inner_index]), entity));
    }
    for (inner_index = 0; inner_index < (int)(sizeof tags / sizeof tags[0]); inner_index++) {
        assert(!entity_list_listed(entities_by_tag(tags[inner_index]), entity));
    }
    return hash;
}

/* One burst by each road, the first two on screen. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;
    void *c;

    start_game();
    assert(burst_from(0) == NULL);
    a = harvest(40, 40, "s61");
    hash = fold_burst(hash, a, 0);
    b = spawn(80, 70, "s62", NULL);
    hash = fold_burst(hash, b, 1);
    c = burst_new(NULL);
    hash = fold_burst(hash, c, 0);
    assert(a != b && b != c && a != c);
    assert(particles_of(&as_burst(a)->emitter)->zone != particles_of(&as_burst(b)->emitter)->zone &&
           particles_of(&as_burst(b)->emitter)->zone != particles_of(&as_burst(c)->emitter)->zone);
    assert(as_burst(a)->slipways.zone != as_burst(b)->slipways.zone);
    assert((as_burst(a)->has & PLANET_HAS_OUTGOING_ROUTES) && (as_burst(b)->has & PLANET_HAS_OUTGOING_ROUTES) &&
           (as_burst(c)->has & PLANET_HAS_OUTGOING_ROUTES));
    assert(trade_route_list_count(&as_burst(a)->outgoing_routes) == 0 && trade_route_list_count(&as_burst(b)->outgoing_routes) == 0 &&
           trade_route_list_count(&as_burst(c)->outgoing_routes) == 0);
    assert(pos_ref(&as_burst(c)->header) == NULL && map_pos_ref(&as_burst(c)->header) == NULL && planet_id_or_null(as_burst(c)) == NULL);
    assert(as_burst(a)->archetype->layer == p8_fixed_from_int(3) && as_burst(a)->speed == p8_fixed_from_int(3));
    assert(as_burst(a)->archetype->is_planet == 0);
    assert(as_burst(b)->speed == P8_FIXED_LIT(0x00018000, 1.5));
    check("built", hash, BUILT_HASH);
}

static uint32_t fold_particle(uint32_t h, const particle_t *q) {
    assert(q != NULL);
    assert(q->has_acceleration);
    h = fnv(fnv(h, (uint32_t)q->pos.x), (uint32_t)q->pos.y);
    h = fnv(fnv(h, (uint32_t)q->velocity.x), (uint32_t)q->velocity.y);
    h = fnv(fnv(h, (uint32_t)q->acceleration.x), (uint32_t)q->acceleration.y);
    return fnv(h, (uint32_t)q->life);
}

/* A placed item as drawn: its kind, then its text and color or its sprite
 * and palette block, then its offset. */
static uint32_t fold_layout_row_placed_item(uint32_t h, const layout_row_t *lq, int i) {
    layout_row_item_t item = lq->placed_items[i];

    h = fnv(fnv(h, (uint32_t)(i + 1)), (uint32_t)item.kind);
    switch (item.kind) {
    case LAYOUT_ROW_ITEM_TEXT:
        h = fold_fixed(fold_string(h, item.text), item.color);
        break;
    case LAYOUT_ROW_ITEM_SPRITE:
        h = fold_fixed(fold_fixed(h, item.sprite_id), item.sprite_palette_block);
        break;
    default:
        break;
    }
    return fold_fixed(h, item.offset.x);
}

/* The row planet_inspection_panel built into the layout row: its placed
 * items, color, score, width. */
static uint32_t fold_layout_row(uint32_t h, void *e) {
    const layout_row_t *lq = as_burst(e)->layout_row;
    int n = 0;
    int k;
    int i;

    assert(lq != NULL &&
           lq->header.class == layout_row_class());
    assert(!lq->has_background);
    assert(lq->placed_items != NULL);
    k = lq->placed_items_count;
    for (i = 0; i < k; i++) {
        h = fold_layout_row_placed_item(h, lq, i);
    }
    if (lq->has_color) {
        h = fold_fixed(fnv_str(h, "color"), lq->color);
        n++;
    }
    if (lq->has_score) {
        h = fold_fixed(fnv_str(h, "score"), lq->score);
        n++;
    }
    assert(pos_ref(&lq->header) == NULL && "fold_layout_row: no test folds a layout row whose pos is set: to fold one, fold the key \"pos\", then 0xEEEE then 0, count it with the other keys, as the frozen hashes did");
    /* As fold_ref folded the tagged list placed_items was: never live, so the tag branch. */
    h = fnv(fnv(fnv_str(h, "placed_items"), 0xEEEEu), FROZEN_LIST_TAG);
    h = fold_fixed(fnv_str(h, "width"), lq->width);
    return fnv(h, (uint32_t)(k + n + 2));
}

/* Cuts the particle list to the given count, giving back the others. */
static void cut_particles(pointer_list_t *qy, int n) {
    while (qy->count > n) {
        allocator_release(particle_zone(PARTICLE_SLOT_PARTICLE), qy->items[qy->count - 1]);
        qy->count--;
    }
}

/* The draw method on a burst whose particle list is cut in place to the given count: is_dead after. */
static uint32_t fold_cut(uint32_t h, void *e, int n) {
    planet_t *g = as_burst(e);
    pointer_list_t *qy = particles_of(&g->emitter);

    assert(qy != NULL && qy->count >= n);
    cut_particles(qy, n);
    class_of(e)->draw(e, entity_pos(&g->header));
    h = fold_is_dead(h, g);
    return fnv(h, (uint32_t)qy->count);
}

/* spawn_particle on each burst; draw_layers(ENTITY_MESSAGE_DRAW) three times, then three
 * more with the protostar's burst cut to 3 particles, so that its circles,
 * drawn in its own trail ramp, are not all covered by emitter_update's
 * dots; is_dead once the particle list holds 3,
 * 2, 0 particles; hit_test on and off the burst, planet_is_colonized;
 * refresh on the planet's shared production (no row), then on one with a
 * resource, before and after two routes land in its own outgoing
 * routes; new_year with no net income. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *first;
    void *second;
    void *third;
    void *protostar;
    trade_route_list_t *outgoing;
    static const trade_route_t no_route = {0};
    static production_t production;
    uint32_t row_hash;
    int live_entities_count;
    int index;

    start_game();
    first = harvest(40, 40, "s71");
    second = spawn(84, 60, "s72", &protostar);
    third = burst_new(NULL);
    hash = fold_particle(hash, class_of(first)->spawn_particle(first));
    hash = fold_particle(hash, class_of(second)->spawn_particle(second));
    hash = fold_particle(hash, class_of(third)->spawn_particle(third));
    for (index = 0; index < 3; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
        hash = fold_is_dead(hash, as_burst(first));
        hash = fold_is_dead(hash, as_burst(second));
        hash = fold_is_dead(hash, as_burst(third));
        hash = fnv(hash, (uint32_t)pointer_list_size(particles_of(&as_burst(second)->emitter)));
    }

    cut_particles(particles_of(&as_burst(second)->emitter), 3);
    for (index = 0; index < 3; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
    }

    hash = fold_cut(hash, first, 3);
    assert(!as_burst(first)->header.is_dead);
    hash = fold_cut(hash, first, 2);
    assert(as_burst(first)->header.is_dead);
    hash = fold_cut(hash, first, 0);
    assert(as_burst(first)->header.is_dead);

    hash = fold_bool(hash, class_of(second)->hit_test(second, mouse_pos(*pos_ref(&as_burst(second)->header), *pos_ref(&as_burst(second)->header))));
    hash = fold_bool(hash, class_of(second)->hit_test(second, mouse_pos(near_main_camera(0, 0), near_main_camera(0, 0))));
    hash = fnv(fnv(hash, 0xB0u), planet_is_colonized(second) ? 1u : 0u);
    assert(planet_is_colonized(first) == 0);

    class_of(first)->refresh(first);
    hash = fold_ref_member(hash, as_burst(first)->layout_row);
    assert(as_burst(first)->layout_row == NULL);
    production.has_resource = 1;
    production.resource = p8_fixed_from_int(12);
    production.amount = p8_fixed_from_int(2);
    as_burst(first)->production = production;
    as_burst(first)->has |= PLANET_HAS_PRODUCTION;
    class_of(first)->refresh(first);
    row_hash = fold_layout_row(2166136261u, first);
    hash = fnv(hash, row_hash);
    outgoing = &as_burst(first)->outgoing_routes;
    assert(trade_route_list_count(outgoing) == 0 && outgoing->values.zone == NULL);
    trade_route_list_init(outgoing, game_zone());
    trade_route_list_add(outgoing, &no_route);
    trade_route_list_add(outgoing, &no_route);
    class_of(first)->refresh(first);
    assert(fold_layout_row(2166136261u, first) != row_hash);
    hash = fold_layout_row(hash, first);
    assert(trade_route_list_count(&as_burst(second)->outgoing_routes) == 0 && trade_route_list_count(&as_burst(third)->outgoing_routes) == 0);
    /* planet_rebuild_trades works on the outgoing routes held: routes kept, in place. */
    assert(planet_rebuild_trades(as_burst(first)) == outgoing && trade_route_list_count(outgoing) == 2);

    live_entities_count = live_entities()->count;
    class_of(second)->new_year(second, p8_fixed_from_int(0));
    assert(live_entities()->count == live_entities_count && !as_burst(second)->has_net_income);
    hash = fnv(hash, screen_hash());
    check("life", hash, LIFE_HASH);
}

/* A harvest played frame by frame: one live burst at a time, gone after
 * BURST_FRAMES frames. */
static void test_burst(void) {
    int frames = 0;
    void *e;

    start_game();
    e = harvest(40, 40, "s64");
    assert(burst_from(0) == e);
    while (burst_from(0) != NULL) {
        e = burst_from(0);
        assert(burst_from(entity_list_rank_in(live_entities(), e) + 1) == NULL);
        cart_update60();
        cart_draw();
        frames++;
        assert(frames < 600);
    }
    check_int("burst_frames", frames, BURST_FRAMES);
    cart_flush_exit_queue();
    check_int("burst slots after the burst", particle_zone_live(PARTICLE_SLOT_BURST), 0);
}

/* emitter_update moves a particle of the list in place: its pos by the old
 * velocity, the velocity turned (its second row reads the x just written),
 * then accelerated by its acceleration, and its life less 0.14. A
 * protostar's burst turns (matrix_yx is not 0). */
static void test_emitter_update_moves_a_particle_in_place(void) {
    particle_t *particle = particle_take();
    vec_t p0 = vec_xy(3, -2);
    vec_t v0 = vec_make(P8_FIXED_LIT(0x00018000, 1.5), P8_FIXED_LIT(0x00004000, 0.25));
    vec_t a0 = vec_make(P8_FIXED_LIT(0x00002000, 0.125), P8_FIXED_LIT(0xFFFF8000, -0.5));
    pointer_list_t *qy;
    p8_fixed_t vx;
    p8_fixed_t vy;
    void *e;
    const emitter_t *em;

    start_game();
    e = spawn(84, 60, "s73", NULL);
    em = &as_burst(e)->emitter;
    assert(em->matrix_yx != 0);
    particle->pos = p0;
    particle->velocity = v0;
    particle->has_acceleration = 1;
    particle->acceleration = a0;
    particle->life = p8_fixed_from_int(5);
    qy = particles_of(em);
    cut_particles(qy, 0);
    pointer_list_add(qy, particle);

    emitter_update(&graphics, &memory, e);

    vx = p8_fixed_add(p8_fixed_mul(em->matrix_xx, v0.x), p8_fixed_mul(em->matrix_xy, v0.y));
    vy = p8_fixed_add(p8_fixed_mul(em->matrix_yx, vx), p8_fixed_mul(em->matrix_yy, v0.y));
    assert(qy->count >= 1 && qy->items[0] == particle);
    assert(particle->pos.x == p8_fixed_add(p0.x, v0.x) && particle->pos.y == p8_fixed_add(p0.y, v0.y));
    assert(particle->velocity.x == p8_fixed_add(vx, a0.x) && particle->velocity.y == p8_fixed_add(vy, a0.y));
    assert(particle->acceleration.x == a0.x && particle->acceleration.y == a0.y);
    assert(particle->life == p8_fixed_sub(p8_fixed_from_int(5), P8_FIXED_LIT(0x000023d7, 0.14)));
}

static void check_ramp(const int_seq_t *ramp, const int *want, int count) {
    int index;

    assert(ramp != NULL && ramp->count == count);
    for (index = 0; index < count; index++) {
        assert(ramp->items[index] == want[index]);
    }
}

/* The color of a circle burst_draw draws for one particle of radius 10 and
 * life 6 (ramp index 3), read opposite the particle's own trail dot. */
static int circle_color(void *e) {
    particle_t *particle = particle_take();
    pointer_list_t *qy = particles_of(&as_burst(e)->emitter);

    particle->pos = vec_xy(10, 0);
    particle->velocity = vec_xy(0, 0);
    particle->has_acceleration = 0;
    particle->life = p8_fixed_from_int(6);
    cut_particles(qy, 0);
    pointer_list_add(qy, particle);
    p8_camera_reset(&graphics);
    p8_cls(&memory);
    burst_draw(&graphics, &memory, as_burst(e), vec_xy(64, 64));
    return memory.screen[64][54];
}

static void test_a_burst_draws_its_circles_in_its_own_ramp(void) {
    static const int ordinary[] = {1, 2, 4, 9, 9, 10, 10, 7, 7, 7, 7, 7};
    static const int pink[] = {1, 2, 8, 14, 14, 14, 14, 8, 8, 2, 2, 1, 1};
    void *plain;
    void *from_protostar;

    start_game();
    plain = burst_new(NULL);
    from_protostar = spawn(84, 60, "s74", NULL);
    check_ramp(as_burst(plain)->emitter.trail_colors, ordinary, 12);
    check_ramp(as_burst(from_protostar)->emitter.trail_colors, pink, 13);
    check_int("ordinary burst circle", circle_color(plain), 4);
    check_int("protostar burst circle", circle_color(from_protostar), 8);
}

static int is_a_burst_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == burst_class();
}

/* Built by any road, a burst is a struct whose class holds the planet's
 * very entries but its own draw, plus its own spawn_particle. */
static void test_burst_is_a_struct(void) {
    const entity_class_t *cls = burst_class();
    const entity_class_t *jo = planet_class();
    void *roads[3];
    size_t i;
    int j;

    start_game();
    roads[0] = harvest(40, 40, "s81");
    roads[1] = spawn(80, 70, "s82", NULL);
    roads[2] = burst_new(NULL);
    for (i = 0; i < 3; i++) {
        assert(is_a_burst_struct(roads[i]));
    }
    assert(cls->draw != NULL && cls->draw != jo->draw);
    assert(cls->draw_overlay == jo->draw_overlay && cls->hit_test == jo->hit_test && cls->new_year == jo->new_year && cls->refresh == jo->refresh &&
           cls->has_refresh == jo->has_refresh && cls->left_press == jo->left_press && cls->accept_drop == jo->accept_drop);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == jo->states[j]);
    }
    assert(cls->spawn_particle == burst_spawn_particle && jo->spawn_particle == NULL);
    assert(cls->drag == jo->drag && cls->drop == jo->drop && cls->attach_slipway == jo->attach_slipway && cls->offer_trade == jo->offer_trade && cls->draw_at == jo->draw_at);
    assert(burst_ready());
    assert(planet_ready());
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

/* What an emitter holds in the particle zone, by slot kind. */
typedef struct {
    int particle;
    int particles;
    int particles_help_screen;
    int burst;
    int rows;
} held_t;

static held_t held_now(void) {
    held_t h;
    h.particle = particle_zone_live(PARTICLE_SLOT_PARTICLE);
    h.particles = particle_zone_live(PARTICLE_SLOT_LIST);
    h.particles_help_screen = particle_zone_live(PARTICLE_SLOT_HELP_LIST);
    h.burst = particle_zone_live(PARTICLE_SLOT_BURST);
    h.rows = ui_zone_live(SLOT_KIND_ROW);
    return h;
}

/* A burst's particles are visual only: its spawn loop and each particle
 * draw from the visual stream and leave the game stream where it was. */
static void test_burst_particles_draw_from_the_visual_stream(void) {
    planet_t *e;
    uint32_t game;
    uint32_t visual;

    start_game();
    e = as_burst(harvest(40, 40, "s74"));
    game = p8_fixed_rnd_state();
    visual = visual_random_state();
    emitter_spawn(&e->header, p8_fixed_from_int(3));
    assert(p8_fixed_rnd_state() == game && "a burst's spawn loop draws no game number");
    assert(visual_random_state() != visual);

    visual = visual_random_state();
    (void)burst_spawn_particle(&e->header);
    assert(p8_fixed_rnd_state() == game && "a burst particle draws no game number");
    assert(visual_random_state() != visual);
}

/* A burst that leaves (entity_unregister, then the exit queue's flush)
 * gives back the particles it still holds, its particle list, then its own
 * slot. */
static void test_leaving_gives_back_its_particles(void) {
    held_t built;
    held_t after;
    planet_t *e;
    int count;

    start_game();
    cart_flush_exit_queue();
    e = as_burst(harvest(40, 40, "s74"));
    count = particles_of(&e->emitter)->count;
    assert(count == 140);
    built = held_now();
    entity_unregister(&e->header);
    cart_flush_exit_queue();
    after = held_now();
    assert(after.particle == built.particle - count && after.particles == built.particles - 1);
    assert(after.burst == built.burst - 1);
}

/* The largest bursts: a protostar's, which copies its position into its
 * slot, and a harvest's, which borrows the planet's; each with its slipways
 * list. Neither stops on a full burst slot kind. */
static void test_the_largest_bursts_fit_their_slot(void) {
    held_t before;

    start_game();
    cart_flush_exit_queue();
    before = held_now();
    (void)harvest(40, 40, "s84");
    (void)spawn(80, 70, "s83", NULL);
    assert(held_now().burst == before.burst + 2);
    printf("burst slot: %lu bytes at most\n", (unsigned long)particle_zone_max_bytes(PARTICLE_SLOT_BURST));
}

/* burst_new copies the id and positions of its planet keys: overwriting the keys afterwards changes nothing. */
static void test_burst_copies_its_keys(void) {
    burst_keys_t keys = {0};
    planet_t *made;

    start_game();
    planet_keys_set_id(&keys.planet, "s70");
    keys.planet.map_pos = vec_xy(5, 6);
    keys.planet.has_map_pos = 1;
    keys.planet.pos = vec_xy(7, 8);
    keys.planet.has_pos = 1;
    made = burst_new(&keys);
    keys.planet = (planet_keys_t){0};
    planet_keys_set_id(&keys.planet, "s1");
    assert(strcmp(planet_id_or_null(made), "s70") == 0);
    assert(pos_ref(&made->header) != NULL && pos_ref(&made->header)->x == p8_fixed_from_int(7) && pos_ref(&made->header)->y == p8_fixed_from_int(8));
    assert(map_pos_ref(&made->header) != NULL && map_pos_ref(&made->header)->x == p8_fixed_from_int(5));
}

/* A burst can be hovered, so a dragged slipway can end on it; the flush
 * that gives the burst back lets go of that end too. */
static void test_leaving_lets_go_of_a_dragged_slipway(void) {
    planet_keys_t keys = {0};
    slipway_keys_t slipway_keys;
    slipway_t *jc;
    vec_t mp = vec_xy(40, 40);
    void *e;

    start_game();
    cart_flush_exit_queue();
    planet_keys_set_id(&keys, "s85");
    keys.map_pos = vec_xy(30, 30);
    keys.has_map_pos = 1;
    keys.pos = near_main_camera(60, 60);
    keys.has_pos = 1;
    slipway_keys.start = entity_id_link(&planet_new(&keys)->header);
    jc = slipway_new(&slipway_keys);
    e = harvest(40, 40, "s86");
    main_ui()->left_held = entity_id_link(&jc->header);
    main_ui()->hovered = entity_id_link(e);
    slipway_drag(jc, mouse_world_only(mp));
    assert(slipway_end(jc) == e);
    entity_unregister(e);
    cart_flush_exit_queue();
    assert(slipway_end(jc) == NULL);
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
    in_fresh_child(test_life);
    in_fresh_child(test_burst);
    in_fresh_child(test_emitter_update_moves_a_particle_in_place);
    in_fresh_child(test_a_burst_draws_its_circles_in_its_own_ramp);
    in_fresh_child(test_burst_is_a_struct);
    in_fresh_child(test_leaving_gives_back_its_particles);
    in_fresh_child(test_burst_particles_draw_from_the_visual_stream);
    in_fresh_child(test_the_largest_bursts_fit_their_slot);
    in_fresh_child(test_leaving_lets_go_of_a_dragged_slipway);
    in_fresh_child(test_burst_copies_its_keys);

    printf("test_burst: all tests passed\n");
    return 0;
}
