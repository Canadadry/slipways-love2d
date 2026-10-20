/* The infraspace gate, read and written on its struct members and the
 * generated defaults (game/game_data.h), built as the shop builds it (a
 * left press on its fourth icon, with a planet_id) and driven through the
 * real class methods, update_entities and draw_layers. Its methods are
 * compared to those of a slipgate built in the same game, started directly:
 * no events file. */

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
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/structure_icon.h"
#include "game/scroll.h"
#include "game/slipway.h"
#include "engine/entity_class.h"
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


#define BUILT_HASH 0x977F263Bu
#define LIFE_HASH 0x0E905697u

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

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
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

/* The link sprites of a slipgate or a gate; NULL on any other struct. */
static const int_seq_t *link_sprites_of(const void *e) {
    if (class_of(e) == slipgate_class() || class_of(e) == infraspace_gate_class()) {
        return ((const planet_t *)e)->archetype->link_sprites;
    }
    return NULL;
}

/* link_sprites is never written on an entity: one whose link_sprites is
 * the very table of the infraspace gate archetype is an infraspace gate. */
static int is_a_gate(const void *e) {
    return link_sprites_of(e) == &infraspace_gate_link_sprites_seq;
}

static void *build_gate(void) {
    void *e = build(infraspace_gate_kind());
    assert(is_a_gate(e));
    return e;
}

/* A gate as structure_icon_left_press and planet_construct left it: its
 * scalars, which of its own, the slipgate's, the laboratory's and the
 * planet's members are set, its methods (a slipgate's very ones), which
 * default tables it shares with the infraspace gate, slipgate, laboratory
 * and planet archetypes, and where entity_register filed it -- under the
 * slipgate, node and targeting tags, never under the planet or structure
 * tag. */
static uint32_t fold_gate(uint32_t hash, const void *entity, const void *slipgate) {
    const planet_t *infraspace_gate;
    const planet_t *gate;
    const planet_t *structure;
    const planet_t *planet;
    size_t index;

    assert(is_a_gate(entity) && !is_a_gate(slipgate) && class_of(entity) == infraspace_gate_class());
    infraspace_gate = entity;
    gate = infraspace_gate;
    structure = gate;
    planet = structure;
    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, gate->archetype->required_tech);
    hash = fold_fixed_member(hash, infraspace_gate->archetype->infraspace);
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
    hash = fnv(hash, (uint32_t)gate->archetype->link_sprites->count);
    /* The 1s of infraspace, project_index, is_structure, cost, clock_ticks,
     * attach_distance, sprite, is_planet, trade_multiplier, layer, shake and
     * back_color: constant by the shape, kept so BUILT_HASH proves the reads. */
    hash = fnv(hash, 1u);
    hash = fnv(hash, (uint32_t)(gate->archetype->link_sprites != NULL));
    hash = fnv(hash, (uint32_t)(gate->archetype->required_tech != NULL));
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
    assert(class_of(entity)->attach_slipway == class_of(slipgate)->attach_slipway && class_of(entity)->offer_trade == class_of(slipgate)->offer_trade &&
           class_of(entity)->drag == class_of(slipgate)->drag && class_of(entity)->drop == class_of(slipgate)->drop);
    assert(class_of(entity)->attach_slipway != NULL && class_of(entity)->offer_trade != NULL && class_of(entity)->drag != NULL && class_of(entity)->drop != NULL &&
           planet_is_family(entity));
    assert(class_of(entity)->hit_test == class_of(slipgate)->hit_test && class_of(entity)->draw == class_of(slipgate)->draw &&
           class_of(entity)->left_press == class_of(slipgate)->left_press && class_of(entity)->refresh == class_of(slipgate)->refresh &&
           class_of(entity)->accept_drop == class_of(slipgate)->accept_drop && class_of(entity)->new_year == class_of(slipgate)->new_year &&
           class_of(entity)->draw_overlay == class_of(slipgate)->draw_overlay);
    assert(class_of(entity)->states[ENTITY_STATE_VANISHING] == class_of(slipgate)->states[ENTITY_STATE_VANISHING] &&
           class_of(entity)->states[ENTITY_STATE_SETTLED] == class_of(slipgate)->states[ENTITY_STATE_SETTLED]);
    assert(class_of(entity)->hit_test != NULL && class_of(entity)->draw != NULL && class_of(entity)->left_press != NULL && class_of(entity)->refresh != NULL &&
           class_of(entity)->accept_drop != NULL && class_of(entity)->new_year != NULL && class_of(entity)->draw_overlay != NULL &&
           class_of(entity)->states[ENTITY_STATE_VANISHING] != NULL && class_of(entity)->states[ENTITY_STATE_SETTLED] != NULL);
    assert(class_of(entity)->draw_at == NULL && class_of(entity)->spawn_particle == NULL);
    assert(class_of(entity)->states[ENTITY_STATE_NORMAL] == NULL && class_of(entity)->states[ENTITY_STATE_PLACING] == NULL &&
           class_of(entity)->states[ENTITY_STATE_UNFOLDING] == NULL);
    assert(gate->archetype->link_sprites != &slipgate_link_sprites_seq);
    assert(planet->archetype == &infraspace_gate_archetype);
    assert(planet->header.class->tags == (TAG(TAG_SLIPGATE) | TAG(TAG_NODE)));
    assert(structure->archetype->name == &structure_name_row && structure->archetype->accepted_resources == &structure_accepted_resources_seq);
    assert(planet->archetype == &infraspace_gate_archetype);
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

/* Two infraspace gates built by the shop's left press on its fourth icon,
 * around a slipgate. */
static void test_built(void) {
    uint32_t hash = 2166136261u;
    void *a;
    void *slipgate;
    void *b;

    start_game();
    a = build_gate();
    slipgate = build(slipgate_kind());
    b = build_gate();
    assert(a != b && as_structure(a)->slipways.zone != as_structure(b)->slipways.zone && planet_production_read(as_structure(a)) != planet_production_read(as_structure(b)));
    hash = fold_gate(hash, a, slipgate);
    hash = fold_gate(hash, b, slipgate);
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
 * planet_is_colonized and economy_score_planet_upkeep give on a gate. */
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

/* The drag method, as the player holds a planet at the given offset from the camera. */
static void slot(void *e, int x, int y) {
    const vec_t *main_camera_pos;
    vec_t mp;

    assert(main_camera_typed() != NULL);
    main_camera_pos = pos_ref(&main_camera_typed()->header);
    assert(main_camera_pos != NULL);
    mp = vec_add(*main_camera_pos, vec_xy(x + 16, y - 4));
    class_of(e)->drag(e, mouse_pos(mp, vec_xy(64, 64)));
    assert(pos_copies(pos_ref(&as_structure(e)->header), mp));
}

/* A slipway drawn from the given planet by its left press, aimed by its
 * own drag at a point with no planet: its infraspace (then its layer)
 * comes from that planet alone. */
static uint32_t fold_route(uint32_t h, void *from) {
    down_t jc = class_of(from)->left_press(from, (mouse_pos_t){0});
    const vec_t *ra = pos_ref(&as_structure(from)->header);
    int from_has_infraspace = class_of(from) == infraspace_gate_class();
    const slipway_t *made;

    assert(jc.stop && entity_get(jc.held) != NULL && class_of(entity_get(jc.held)) == slipway_class());
    made = (const slipway_t *)entity_get(jc.held);
    assert(slipway_start(made) == from);
    assert(ra != NULL);
    class_of(entity_get(jc.held))->drag(entity_get(jc.held), mouse_world_only(vec_add(*ra, vec_xy(20, 0))));
    assert(entity_has(&made->header, ENTITY_HAS_INFRASPACE) == from_has_infraspace);
    assert(made->header.layer == p8_fixed_from_int(from_has_infraspace ? 2 : 4));
    h = entity_has(&made->header, ENTITY_HAS_INFRASPACE) ? fold_fixed_member(h, made->header.infraspace) : fold_absent(h);
    return fold_fixed_member(h, made->header.layer);
}

/* Placed on screen by the drag method (targeting_track) and the drop method
 * (targeting_confirm); update_entities on one still in placing, then in
 * settled; draw through draw_layers; accept_drop against no argument,
 * another gate and a slipway drawn from itself; left_press with no link (a
 * slipway whose start is the gate) and with 3 (refused); refresh with 0 to
 * 3 links; the infraspace of a slipway drawn from a gate and from a
 * slipgate. */
static void test_life(void) {
    uint32_t hash = 2166136261u;
    void *placed;
    void *idle;
    void *slipgate;
    down_t slipway;
    entity_id_list_t slipways;
    int index;

    start_game();
    idle = build_gate();
    hash = fold_string_member(hash, entity_state_name(as_structure(idle)->header.state));
    hash = fold_update_entities(hash, idle);

    placed = build_gate();
    slot(placed, 12, 108);
    set_credits(999);
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
    assert(as_structure(placed)->sprite == p8_fixed_from_int(76));
    slipway = class_of(placed)->left_press(placed, (mouse_pos_t){0});
    assert(entity_get(slipway.held) == NULL);
    hash = fold_bool(hash, slipway.stop);
    as_structure(placed)->slipways = slipways;
    class_of(placed)->refresh(placed);
    hash = fold_fixed_member(hash, as_structure(placed)->sprite);
    assert(as_structure(placed)->sprite == p8_fixed_from_int(78));
    hash = fold_economy(hash, placed);

    hash = fold_route(hash, placed);
    assert(is_a_gate(placed) && ((const planet_t *)placed)->archetype->infraspace == p8_fixed_from_int(1));
    slipgate = build(slipgate_kind());
    slot(slipgate, 40, 108);
    hash = fold_route(hash, slipgate);
    assert(class_of(slipgate) != infraspace_gate_class());
    check("life", hash, LIFE_HASH);
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

static void build_with_unknown_key(void *e) {
    planet_keys_t keys = {0};
    (void)e;
    keys.map_pos = vec_xy(1, 1);
    keys.has_map_pos = 1;
    (void)infraspace_gate_new(&keys);
}

static int is_an_infraspace_gate_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == infraspace_gate_class();
}

/* Built by structure_icon_left_press or infraspace_gate_new, a gate is a
 * struct whose class holds the slipgate's very entries; a construction key
 * other than planet_id stops the run. */
static void test_infraspace_gate_is_a_struct(void) {
    const entity_class_t *cls = infraspace_gate_class();
    const entity_class_t *nr = slipgate_class();
    void *a;
    int j;

    start_game();
    a = build_gate();
    assert(is_an_infraspace_gate_struct(a) && cls != nr && entity_list_listed(entities_by_tag(TAG_SLIPGATE), a));
    assert(cls->draw == nr->draw && cls->draw_overlay == nr->draw_overlay && cls->hit_test == nr->hit_test && cls->new_year == nr->new_year && cls->refresh == nr->refresh &&
           cls->has_refresh == nr->has_refresh && cls->left_press == nr->left_press && cls->accept_drop == nr->accept_drop &&
           cls->drag == nr->drag && cls->drop == nr->drop && cls->attach_slipway == nr->attach_slipway && cls->offer_trade == nr->offer_trade && cls->draw_at == nr->draw_at &&
           cls->spawn_particle == nr->spawn_particle);
    for (j = 0; j < STATE_COUNT; j++) {
        assert(cls->states[j] == nr->states[j]);
    }
    assert(infraspace_gate_ready());

    must_abort(build_with_unknown_key, NULL);
    {
        planet_t *bare = infraspace_gate_new(NULL);
        assert(is_an_infraspace_gate_struct(bare) && is_a_gate(bare) && planet_id_or_null(bare) == NULL && bare->slipways.zone != NULL &&
               (bare->has & PLANET_HAS_PRODUCTION));
        assert(entity_list_listed(entities_by_tag(TAG_SLIPGATE), bare));
    }
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
    in_fresh_child(test_life);
    in_fresh_child(test_infraspace_gate_is_a_struct);

    printf("test_infraspace_gate: all tests passed\n");
    return 0;
}
