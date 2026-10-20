/* The slipgate, read and written on its struct members and the generated
 * defaults (game/game_data.h), built as the shop builds it (a left press on
 * its third icon, with a planet_id) and driven through the real class
 * methods, update_entities and draw_layers, in games started directly: no
 * events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy_score.h"
#include "game/economy_tiers.h"
#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "p8/fixed.h"
#include "game/main_economy.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
#include "engine/entity_id.h"
#include "game/trade.h"
#include "engine/game_zone.h"
#include "engine/vec.h"
#include "heap_allocator.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


static int pos_copies(const vec_t *ra, vec_t mp) {
    return ra != NULL && ra->x == mp.x && ra->y == mp.y;
}


#define BUILT_HASH 0x7BCCC2FEu
#define LIFE_HASH 0x33195DC7u

/* The tag planet_rebuild_trades's list carried as a tagged list, which the
 * frozen hashes fold: any other value moves every *_HASH. */
#define FROZEN_LIST_TAG 0x9E5C1157u

/* The tag entity structs carried, which the frozen hashes fold: any other
 * value moves every *_HASH. */
#define FROZEN_STRUCT_TAG 0x9E5C57C7u

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_SLIPGATE, TAG_NODE};

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

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
}

static uint32_t fold_ref(uint32_t h, const void *ref) {
    if (ref == NULL) {
        return fnv(h, 0xDEADu);
    }
    if (entity_list_listed(live_entities(), ref)) {
        return fnv(fnv(h, 0x4Eu), (uint32_t)entity_list_rank_in(live_entities(), ref));
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_STRUCT_TAG);
}

/* The word the frozen hashes fold before a boolean, 0xB0, then the boolean
 * as 0 or 1; any other moves every *_HASH. */
static uint32_t fold_bool(uint32_t h, int b) {
    return fnv(fnv(h, 0xB0u), b ? 1u : 0u);
}

/* The item count of a list member, -1 when NULL. */
static int id_list_count(const entity_id_list_t *l) {
    return l == NULL ? -1 : l->count;
}

/* The entity as a planet of the laboratory's lineage (asserted). */
static planet_t *as_structure(void *e) {
    const entity_t *h = e;

    assert(h != NULL && (h->class->lineage & LINEAGE(LINEAGE_STRUCTURE)));
    return e;
}

static uint32_t fold_structure(uint32_t h, const planet_t *qh) {
    return qh->archetype->is_structure ? fold_fixed_member(h, p8_fixed_from_int(1)) : fold_bool_member(h, 0);
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
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static vec_t vec_xy(int x, int y) {
    return vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

/* The members a planet of the laboratory's tree holds: draw, draw_overlay,
 * hit_test, new_year, refresh, left_press, accept_drop, drag, drop,
 * attach_slipway, offer_trade and the vanishing and settled states;
 * spawn_particle aside, no other. */
static void assert_structure_shape(const entity_class_t *c) {
    assert(c->draw != NULL && c->draw_overlay != NULL && c->hit_test != NULL && c->new_year != NULL && c->refresh != NULL && c->left_press != NULL &&
           c->accept_drop != NULL && c->states[ENTITY_STATE_VANISHING] != NULL && c->states[ENTITY_STATE_SETTLED] != NULL);
    assert(c->has_refresh == NULL && c->states[ENTITY_STATE_NORMAL] == NULL &&
           c->states[ENTITY_STATE_PLACING] == NULL && c->states[ENTITY_STATE_UNFOLDING] == NULL);
    assert(c->drag != NULL && c->drop != NULL && c->attach_slipway != NULL && c->offer_trade != NULL && c->draw_at == NULL);
}

/* The structure icon whose kind is the given one. */
static void *icon_of(const entity_kind_t *kind) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    int i;
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *e = hb->items[i];
        if (class_of(e) == structure_icon_class() && ((const structure_icon_t *)e)->kind == kind) {
            return e;
        }
    }
    return NULL;
}

/* A left press on the icon of the given kind, as the shop does. */
static void *build(const entity_kind_t *kind) {
    void *icon = icon_of(kind);
    down_t made;

    assert(icon != NULL);
    made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
    assert(made.stop && entity_get(made.held) != NULL);
    return entity_get(made.held);
}

static void *build_slipgate(void) {
    return build(slipgate_kind());
}

enum { SLIPGATE, INFRASPACE_GATE };

/* link_sprites is never written on an entity: one whose link_sprites is
 * the very table of the slipgate archetype is a slipgate, of the infraspace
 * gate archetype an infraspace gate. -1 for anything else. */
static int gate_kind(const void *e) {
    const void *hz = NULL;

    if (e != NULL && (class_of(e) == slipgate_class() || class_of(e) == infraspace_gate_class())) {
        hz = ((const planet_t *)e)->archetype->link_sprites;
    }
    if (hz == NULL) {
        return -1;
    }
    if (hz == &slipgate_link_sprites_seq) {
        return SLIPGATE;
    }
    if (hz == &infraspace_gate_link_sprites_seq) {
        return INFRASPACE_GATE;
    }
    return -1;
}

/* A slipgate as structure_icon_left_press and planet_construct left it:
 * its scalars, which of its own, the laboratory's and the planet's members
 * are set, its methods (the laboratory's very functions but its own
 * accept_drop, left_press, refresh, offer_trade), which default tables it
 * shares with the slipgate, laboratory and planet archetypes, and where
 * entity_register filed it -- under the slipgate, node and targeting tags,
 * never under the planet or structure tag. */
static uint32_t fold_slipgate(uint32_t hash, const void *entity) {
    const entity_class_t *structure_cls = structure_class();
    const planet_t *slipgate;
    const planet_t *structure;
    const planet_t *planet;
    size_t index;

    assert(gate_kind(entity) == SLIPGATE && class_of(entity) == slipgate_class());
    slipgate = entity;
    structure = slipgate;
    planet = structure;
    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, slipgate->archetype->required_tech);
    hash = fold_fixed_member(hash, structure->archetype->project_index);
    hash = fold_fixed_member(hash, structure->sprite);
    hash = fold_fixed_member(hash, structure->targeting.cost);
    hash = fold_fixed_member(hash, structure->targeting.archetype->clock_ticks);
    hash = fold_fixed_member(hash, structure->targeting.archetype->attach_distance);
    hash = fold_string_member(hash, entity_state_name(structure->targeting.archetype->confirmed_state));
    hash = fold_structure(hash, structure);
    hash = planet->has_development_tier ? fold_fixed_member(hash, planet->development_tier) : fold_absent(hash);
    hash = fold_is_planet(hash, planet);
    hash = fold_fixed_member(hash, planet->archetype->trade_multiplier);
    hash = fold_fixed_member(hash, planet->archetype->layer);
    hash = fold_fixed_member(hash, planet->header.shake);
    hash = fold_fixed_member(hash, planet->archetype->back_color);
    hash = fold_vec(hash, &planet->archetype->row_step);
    hash = fnv(hash, (uint32_t)slipgate->archetype->link_sprites->count);
    /* The 1s of project_index, is_structure, cost, clock_ticks, attach_distance,
     * sprite, is_planet, trade_multiplier, layer, shake and back_color: constant
     * by the struct's shape, kept so that BUILT_HASH still proves the typed reads. */
    hash = fnv(hash, (uint32_t)(slipgate->archetype->link_sprites != NULL));
    hash = fnv(hash, (uint32_t)(slipgate->archetype->required_tech != NULL));
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(structure->archetype->name != NULL));
    hash = fnv(hash, (uint32_t)(structure->targeting.archetype->confirmed_state != ENTITY_STATE_NONE));
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(structure->archetype->accepted_resources != NULL));
    hash = fnv(hash, targeting_target(&structure->targeting) != NULL ? 1u : 0u);
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
    hash = fnv(hash, (uint32_t)planet->has_production_bonus);
    hash = fnv(hash, (uint32_t)(planet->header.is_dead != 0));
    hash = fnv(hash, (uint32_t)planet->has_net_income);
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_INCOMING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_OUTGOING_ROUTES) != 0));
    hash = fnv(hash, (uint32_t)planet->has_development_tier);
    hash = fnv(hash, (uint32_t)(planet->header.is_hovered != 0));
    assert(class_of(entity)->offer_trade == slipgate_class()->offer_trade && class_of(entity)->offer_trade != structure_cls->offer_trade &&
           class_of(entity)->offer_trade != planet_class()->offer_trade);
    assert(class_of(entity)->attach_slipway == structure_cls->attach_slipway && class_of(entity)->drag == structure_cls->drag && class_of(entity)->drop == structure_cls->drop &&
           class_of(entity)->spawn_particle == NULL && planet_is_family(entity));
    assert_structure_shape(class_of(entity));
    assert(class_of(entity)->draw == structure_cls->draw && class_of(entity)->draw_overlay == structure_cls->draw_overlay && class_of(entity)->hit_test == structure_cls->hit_test &&
           class_of(entity)->new_year == structure_cls->new_year);
    assert(class_of(entity)->states[ENTITY_STATE_VANISHING] == structure_cls->states[ENTITY_STATE_VANISHING] &&
           class_of(entity)->states[ENTITY_STATE_SETTLED] == structure_cls->states[ENTITY_STATE_SETTLED]);
    assert(class_of(entity)->accept_drop != structure_cls->accept_drop && class_of(entity)->left_press != structure_cls->left_press && class_of(entity)->refresh != structure_cls->refresh);
    assert(class_of(entity)->accept_drop != planet_class()->accept_drop && class_of(entity)->left_press != planet_class()->left_press &&
           class_of(entity)->refresh != planet_class()->refresh);
    assert(planet->archetype == &slipgate_archetype);
    assert(planet->header.class->tags == (TAG(TAG_SLIPGATE) | TAG(TAG_NODE)) && slipgate->archetype->link_sprites == &slipgate_link_sprites_seq);
    assert(planet->header.class->tags != (TAG(TAG_STRUCTURE) | TAG(TAG_PLANET) | TAG(TAG_NODE)));
    assert(planet->archetype->hit_box.left != planet_archetype.hit_box.left);
    assert(structure->archetype->name == &structure_name_row && structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &slipgate_archetype);
    assert(planet->archetype->spacer == &planet_spacer_row && planet->archetype->ring_colors == &planet_ring_colors_seq && planet->archetype->mood_rows == planet_mood_rows_rows &&
           planet->archetype->made_resources == &planet_made_resources_seq);
    assert(planet->has & PLANET_HAS_PRODUCTION);
    assert(research_projects_at(1) != NULL);
    assert(research_project_get(planet->project) == research_projects_at(1));
    hash = fnv(hash, (uint32_t)id_list_count(&planet->slipways));
    hash = fnv(hash, (uint32_t)((planet->has & PLANET_HAS_NEEDS) ? planet->needs.count : -1));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), entity));
    for (index = 0; index < sizeof messages / sizeof messages[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[index]), entity));
    }
    for (index = 0; index < sizeof tags / sizeof tags[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(tags[index]), entity));
    }
    assert(entity_list_listed(entities_by_tag(TAG_SLIPGATE), entity) && !entity_list_listed(entities_by_tag(TAG_PLANET), entity) && !entity_list_listed(entities_by_tag(TAG_STRUCTURE), entity));
    return hash;
}

/* Two slipgates built by the shop's left press on its third icon. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *b;

    start_game();
    a = build_slipgate();
    b = build_slipgate();
    assert(a != b && as_structure(a)->slipways.zone != as_structure(b)->slipways.zone && planet_production_read(as_structure(a)) != planet_production_read(as_structure(b)));
    hash = fold_slipgate(hash, a);
    hash = fold_slipgate(hash, b);
    check("built", hash, BUILT_HASH);
}

static void set_credits(int n) {
    main_economy_typed()->credits = p8_fixed_from_int(n);
}

static uint32_t fold_pos_y(uint32_t h, const planet_t *qh) {
    const vec_t *ra = pos_ref(&qh->header);
    return ra == NULL ? fnv(h, 0xDEADu) : fnv(h, (uint32_t)ra->y);
}

/* update_entities once, then what the laboratory's settled update left. */
static uint32_t fold_update_entities(uint32_t h, void *e) {
    const planet_t *qh = as_structure(e);

    h = fold_pos_y(h, qh);
    update_entities();
    h = fold_fixed_member(h, qh->header.state_time);
    return fold_pos_y(h, qh);
}

/* planet_rebuild_trades's list, folded as fold_ref folded the tagged list
 * it was: never a live entity, so the tag branch. */
static uint32_t fold_rebuild_trades(uint32_t h, const trade_route_list_t *ib) {
    if (ib == NULL) {
        return fnv(h, 0xDEADu);
    }
    return fnv(fnv(h, 0xEEEEu), FROZEN_LIST_TAG);
}

/* What economy_tiers_compute_tier, planet_rebuild_trades,
 * planet_is_colonized and economy_score_planet_upkeep give on a slipgate. */
static uint32_t fold_economy(uint32_t h, void *e) {
    trade_route_list_t *ib = planet_rebuild_trades(e);
    h = fnv(h, (uint32_t)economy_tiers_compute_tier(e));
    h = fold_rebuild_trades(h, ib);
    h = fnv(fnv(h, 0xB0u), planet_is_colonized(e) ? 1u : 0u);
    h = fnv(h, (uint32_t)economy_score_planet_upkeep(e));
    return as_structure(e)->has_development_tier ? fold_fixed_member(h, as_structure(e)->development_tier) : fold_absent(h);
}

/* A list of the given count of copies of an item, for the slipways list:
 * accept_drop, left_press and refresh only count it. */
static entity_id_list_t *list_of(int n, entity_t *item) {
    entity_id_list_t *l = malloc(sizeof *l);
    int i;

    entity_id_list_init(l, &heap_allocator);
    for (i = 0; i < n; i++) {
        entity_id_list_add(l, entity_id_link(item));
    }
    return l;
}

/* Placed on screen by the drag method (targeting_track) and the drop method
 * (targeting_confirm); update_entities on one still in placing, then in
 * settled; draw through draw_layers; accept_drop against no argument,
 * another slipgate and a slipway drawn from itself; left_press with no link
 * (a slipway whose start is the slipgate) and with 3 (refused); refresh
 * with 0 to 3 links; an infraspace gate's refresh; slipgate_new with no
 * key, then with map_pos and pos. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *placed;
    void *idle;
    void *infraspace_gate;
    void *bare;
    vec_t mouse_world;
    down_t slipway;
    entity_id_list_t slipways;
    const vec_t *main_camera_pos;
    int index;

    start_game();
    idle = build_slipgate();
    hash = fold_string_member(hash, entity_state_name(as_structure(idle)->header.state));
    hash = fold_update_entities(hash, idle);

    placed = build_slipgate();
    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    mouse_world = vec_add(*main_camera_pos, vec_xy(28, 104));
    class_of(placed)->drag(placed, mouse_pos(mouse_world, vec_xy(64, 64)));
    assert(pos_copies(pos_ref(&as_structure(placed)->header), mouse_world));
    set_credits(200);
    assert(class_of(placed)->drop == targeting_confirm_method);
    hash = targeting_confirm(placed) ? fnv(fnv(hash, 0xB0u), 1u) : fold_ref(hash, NULL);
    hash = fold_string_member(hash, entity_state_name(as_structure(placed)->header.state));
    assert(as_structure(placed)->header.state == ENTITY_STATE_SETTLED);
    hash = fold_economy(hash, placed);
    for (index = 0; index < 3; index++) {
        hash = fold_update_entities(hash, placed);
    }
    for (index = 0; index < 2; index++) {
        draw_layers(ENTITY_MESSAGE_DRAW);
        hash = fnv(hash, screen_hash());
    }

    hash = fold_bool(hash, class_of(placed)->accept_drop(placed, NULL).stop);
    hash = fold_bool(hash, class_of(placed)->accept_drop(placed, idle).stop);
    slipway = class_of(placed)->left_press(placed, (mouse_pos_t){0});
    assert(slipway.stop && entity_get(slipway.held) != NULL);
    assert(class_of(entity_get(slipway.held)) == slipway_class() && slipway_start(((const slipway_t *)entity_get(slipway.held))) == placed);
    hash = fnv(hash, FROZEN_STRUCT_TAG);
    hash = fold_bool(hash, class_of(placed)->accept_drop(placed, entity_get(slipway.held)).stop);
    hash = fold_bool(hash, class_of(idle)->accept_drop(idle, entity_get(slipway.held)).stop);

    slipways = as_structure(placed)->slipways;
    for (index = 0; index <= 3; index++) {
        as_structure(placed)->slipways = *list_of(index, idle);
        class_of(placed)->refresh(placed);
        hash = fold_fixed_member(hash, as_structure(placed)->sprite);
        hash = fold_bool(hash, class_of(placed)->accept_drop(placed, NULL).stop);
    }
    assert(as_structure(placed)->sprite == p8_fixed_from_int(168));
    slipway = class_of(placed)->left_press(placed, (mouse_pos_t){0});
    assert(entity_get(slipway.held) == NULL);
    hash = fold_bool(hash, slipway.stop);
    as_structure(placed)->slipways = slipways;
    class_of(placed)->refresh(placed);
    hash = fold_fixed_member(hash, as_structure(placed)->sprite);
    hash = fold_economy(hash, placed);

    infraspace_gate = build(infraspace_gate_kind());
    assert(gate_kind(infraspace_gate) == INFRASPACE_GATE);
    hash = fold_fixed_member(hash, as_structure(infraspace_gate)->sprite);
    class_of(infraspace_gate)->refresh(infraspace_gate);
    hash = fold_fixed_member(hash, as_structure(infraspace_gate)->sprite);
    assert(as_structure(infraspace_gate)->sprite == p8_fixed_from_int(78));

    bare = slipgate_new(NULL);
    assert(gate_kind(bare) == SLIPGATE && planet_id_or_null(as_structure(bare)) == NULL && as_structure(bare)->slipways.zone != NULL &&
           (as_structure(bare)->has & PLANET_HAS_PRODUCTION));
    hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(TAG_SLIPGATE), bare));
    {
        planet_keys_t keys = {0};
        vec_t pos = vec_xy(3, 4);
        vec_t ra = vec_xy(5, 6);
        void *with;
        keys.map_pos = pos;
        keys.has_map_pos = 1;
        keys.pos = ra;
        keys.has_pos = 1;
        with = slipgate_new(&keys);
        assert(pos_copies(map_pos_ref(&as_structure(with)->header), pos) && pos_copies(pos_ref(&as_structure(with)->header), ra) && planet_id_or_null(as_structure(with)) == NULL);
        hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), with));
    }
    check("life", hash, LIFE_HASH);
}

static int is_a_slipgate_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == slipgate_class();
}

/* Built by structure_icon_left_press or slipgate_new, a slipgate is a
 * struct whose class holds the laboratory's very entries but its own
 * accept_drop, left_press, refresh and offer_trade. */
static void test_slipgate_is_a_struct(void) {
    const entity_class_t *cls = slipgate_class();
    const entity_class_t *qh = structure_class();
    void *a;
    void *fr;

    start_game();
    a = build_slipgate();
    assert(is_a_slipgate_struct(a) && cls != qh && entity_list_listed(entities_by_tag(TAG_SLIPGATE), a));
    assert(cls->offer_trade != qh->offer_trade && cls->attach_slipway == qh->attach_slipway && cls->drag == qh->drag && cls->drop == qh->drop && cls->spawn_particle == qh->spawn_particle);
    assert_structure_shape(cls);
    assert(cls->draw == qh->draw && cls->draw_overlay == qh->draw_overlay && cls->hit_test == qh->hit_test && cls->new_year == qh->new_year);
    assert(cls->states[ENTITY_STATE_VANISHING] == qh->states[ENTITY_STATE_VANISHING] &&
           cls->states[ENTITY_STATE_SETTLED] == qh->states[ENTITY_STATE_SETTLED]);
    assert(cls->accept_drop != qh->accept_drop && cls->left_press != qh->left_press && cls->refresh != qh->refresh);
    assert(cls->accept_drop != planet_class()->accept_drop && cls->left_press != planet_class()->left_press &&
           cls->refresh != planet_class()->refresh);
    assert(slipgate_ready() && infraspace_gate_ready());
    {
        planet_t *bare = slipgate_new(NULL);
        assert(is_a_slipgate_struct(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL && (bare->has & PLANET_HAS_PRODUCTION));
    }
    fr = build(infraspace_gate_kind());
    assert(fr != NULL &&
           ((const entity_t *)fr)->class == infraspace_gate_class());
    class_of(fr)->refresh(fr);
    assert(as_structure(fr)->sprite == p8_fixed_from_int(78));
}

/* Counts the offers offer_trade makes, taking each as planet_rebuild_trades's accept does. */
static int g_offers = 0;

static void take_offer(trade_offer_t *self, const trade_route_t *a) {
    trade_route_list_add(&self->accepted_routes, a);
    self->amount -= p8_fixed_from_int(1);
    g_offers++;
}

/* slipgate_offer_trade forwards a trade over each of its slipways but one
 * the trade's slipways already run through: over a slipway to a planet
 * wanting the trade's resource, a trade whose slipways hold it is not
 * offered, one whose slipways do not is, once, and the route it took comes
 * back in the caller's offer. */
static void test_offer_trade_skips_a_slipway_already_in_slipways(void) {
    planet_t *self;
    planet_t *other;
    slipway_keys_t slipway_keys = {0};
    slipway_t *slipway;
    pointer_list_t *needs = malloc(sizeof *needs);
    entity_id_list_t *with_w = malloc(sizeof *with_w);
    entity_id_list_t *without_w = malloc(sizeof *without_w);
    trade_keys_t keys = {0};
    trade_offer_t lw = {0};
    p8_fixed_t qe;
    int r_p;

    start_game();
    assert(resource_code_lookup("p", &r_p));
    qe = p8_fixed_from_int(r_p);
    self = slipgate_new(NULL);
    other = structure_new(NULL);
    pointer_list_init(needs, &heap_allocator);
    pointer_list_add(needs, need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = qe}));
    other->needs = *needs;
    other->has |= PLANET_HAS_NEEDS;
    slipway_keys.start = entity_id_link(&self->header);
    slipway = slipway_new(&slipway_keys);
    slipway->end = entity_id_link(&other->header);
    assert(self->slipways.zone != NULL && self->slipways.count == 0);
    entity_id_list_add(&self->slipways, entity_id_link(&slipway->header));

    entity_id_list_init(with_w, &heap_allocator);
    entity_id_list_add(with_w, entity_id_of(&slipway->header));
    trade_route_list_init(&lw.accepted_routes, &heap_allocator);
    keys.has_resource = 1;
    keys.resource = qe;
    keys.slipways = *with_w;
    lw.route = trade_new(&keys);
    lw.amount = p8_fixed_from_int(2);
    lw.accept = take_offer;
    slipgate_offer_trade(self, &lw);
    assert(g_offers == 0 && trade_route_list_count(&lw.accepted_routes) == 0 && lw.amount == p8_fixed_from_int(2));

    entity_id_list_init(without_w, &heap_allocator);
    keys.slipways = *without_w;
    lw.route = trade_new(&keys);
    slipgate_offer_trade(self, &lw);
    assert(g_offers == 1 && trade_route_list_count(&lw.accepted_routes) == 1 && lw.amount == p8_fixed_from_int(1));
    trade_route_list_free(&lw.accepted_routes);
}

/* The galaxy depends on the rnd state: run each scenario in a child forked
 * right after cart_load(), as a fresh process would start it. */
static pid_t spawn(void (*test)(void)) {
    pid_t child;

    fflush(stdout);
    child = fork();
    assert(child >= 0);
    if (child == 0) {
        test();
        fflush(stdout);
        _exit(0);
    }
    return child;
}

static void reap(pid_t child) {
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
}

static void in_fresh_child(void (*test)(void)) {
    reap(spawn(test));
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
    in_fresh_child(test_slipgate_is_a_struct);
    in_fresh_child(test_offer_trade_skips_a_slipway_already_in_slipways);

    printf("test_slipgate: all tests passed\n");
    return 0;
}
