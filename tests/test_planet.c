/* The planet, read on its struct members and its generated defaults
 * (game/game_data.h), and driven through the real galaxy_populate,
 * update_entities and draw_layers. Its subclasses are read the same way.
 * Games are built directly: no events file. */

#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "p8/sprite_sheet.h"
#include "engine/entity_id.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "game/economy.h"
#include "game/economy_formulas.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "game/help.h"
#include "game/hud.h"
#include "game/ui.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_display.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "game/research_display.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"
#include "game/structure_icon.h"
#include "game/map_tables.h"
#include "game/scroll.h"
#include "engine/entity_class.h"
#include "engine/vec.h"
#include "game/ui_zone.h"
#include "game/visual_random.h"

static const vec_t *pos_ref(const entity_t *e) {
    return entity_has_pos(e) ? &e->pos : NULL;
}

static const vec_t *map_pos_ref(const entity_t *e) {
    return entity_has_map_pos(e) ? &e->map_pos : NULL;
}


#define GALAXY_PLANETS 90
#define GALAXY_HASH 0xE3AFB4DEu
#define DAUGHTERS_HASH 0x668D1EBEu
#define NEW_PLANET_HASH 0x8E844279u
#define UPDATE_VANISHING_CALLS 62

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_message_t messages[] = {ENTITY_MESSAGE_DRAW, ENTITY_MESSAGE_DRAW_OVERLAY, ENTITY_MESSAGE_HIT_TEST, ENTITY_MESSAGE_NEW_YEAR, ENTITY_MESSAGE_REFRESH};
static const tag_t tags[] = {TAG_PLANET, TAG_NODE};

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

static const planet_t *as_planet(const void *e) {
    assert(planet_is_family(e));
    return e;
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

/* 0 absent, 1 false, 2 true. */
static uint32_t is_dead_state(const planet_t *jo) {
    return jo->header.is_dead ? 2u : 0u;
}

static uint32_t fold_vec(uint32_t h, const vec_t *p) {
    if (p == NULL) {
        return fnv(h, 0xDEADu);
    }
    h = fnv(h, (uint32_t)p->x);
    return fnv(h, (uint32_t)p->y);
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

static uint32_t fold_production(uint32_t h, const planet_t *jo) {
    const production_t *k = planet_production_read(jo);

    if (k == NULL) {
        return fnv(h, 0xA0u);
    }
    h = k->has_resource ? fold_fixed_member(h, k->resource) : fold_absent(h);
    h = fold_fixed_member(h, k->amount);
    return fnv(h, (uint32_t)(k == planet_shared_production()));
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

/* A cart_init on the title screen, without a frame: the previous help
 * screen gives back its help particle list slot, the only one, before the
 * new help screen takes it. */
static void leave_help_screen(void) {
    const entity_list_t *hv = live_entities();
    for (int i = 0; i < hv->count; i++) {
        if (hv->items[i]->class == help_screen_class()) {
            entity_unregister(hv->items[i]);
            cart_flush_exit_queue();
            return;
        }
    }
}

static void start_title(void) {
    p8_input_init(&input);
    dispatch_context_set_input(&input);
    leave_help_screen();
    cart_init();
    cart_flush_exit_queue();
}

/* A game started by cart_start_game with no difficulty picked: a NULL
 * difficulty reads as difficulty 0. */
static void start_game(void) {
    start_title();
    cart_start_game(NULL);
    cart_flush_exit_queue();
}

static mouse_pos_t mouse_pos(vec_t world, vec_t screen) {
    return (mouse_pos_t){.world = world, .screen = screen, .has = MOUSE_POS_HAS_WORLD | MOUSE_POS_HAS_SCREEN};
}

static const entity_class_t *class_of(const void *e) {
    return ((const entity_t *)e)->class;
}

/* The planet class's own members: hit_test, draw, left_press, refresh,
 * accept_drop, new_year, attach_slipway, offer_trade and the vanishing
 * state; no other. */
static void assert_planet_members(const entity_class_t *c) {
    const entity_class_t *jo = planet_class();

    assert(c->hit_test == jo->hit_test && c->draw == jo->draw && c->left_press == jo->left_press && c->refresh == jo->refresh && c->accept_drop == jo->accept_drop &&
           c->new_year == jo->new_year && c->states[ENTITY_STATE_VANISHING] == jo->states[ENTITY_STATE_VANISHING]);
    assert(c->hit_test != NULL && c->draw != NULL && c->left_press != NULL && c->refresh != NULL && c->accept_drop != NULL && c->new_year != NULL &&
           c->states[ENTITY_STATE_VANISHING] != NULL);
    assert(c->attach_slipway == jo->attach_slipway && c->offer_trade == jo->offer_trade && c->attach_slipway != NULL && c->offer_trade != NULL);
    assert(c->drag == NULL && c->drop == NULL && c->draw_at == NULL && c->spawn_particle == NULL);
    assert(c->draw_overlay == NULL && c->has_refresh == NULL && c->states[ENTITY_STATE_NORMAL] == NULL &&
           c->states[ENTITY_STATE_SETTLED] == NULL && c->states[ENTITY_STATE_PLACING] == NULL &&
           c->states[ENTITY_STATE_UNFOLDING] == NULL);
}

/* A bare planet as planet_new and planet_construct left it: the scalars,
 * which of its 31 members are set, its methods, which default tables it
 * shares with the class, and where entity_register registered it. */
static uint32_t fold_planet(uint32_t hash, const void *entity) {
    const planet_t *planet = as_planet(entity);
    size_t index;

    hash = fold_string_member(hash, entity_state_name(planet->header.state));
    hash = fold_fixed_member(hash, planet->header.state_time);
    hash = fold_string_member(hash, planet_id_or_null(planet));
    hash = fold_is_planet(hash, planet);
    hash = fold_fixed_member(hash, planet->archetype->trade_multiplier);
    hash = fold_fixed_member(hash, planet->archetype->layer);
    hash = fold_fixed_member(hash, planet->header.shake);
    hash = fold_fixed_member(hash, planet->archetype->back_color);
    hash = fold_vec(hash, map_pos_ref(&planet->header));
    hash = fold_vec(hash, pos_ref(&planet->header));
    hash = fold_vec(hash, &planet->archetype->row_step);
    /* The 1s of is_planet, trade_multiplier, layer, shake and back_color:
     * constant by the struct's shape, kept so that GALAXY_HASH and
     * NEW_PLANET_HASH still prove the typed reads. */
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
    assert_planet_members(class_of(entity));
    assert(planet->archetype == &planet_archetype);
    assert(planet->archetype == &planet_archetype);
    assert(planet->header.class->tags == (TAG(TAG_PLANET) | TAG(TAG_NODE)) && !(planet->has & PLANET_HAS_PRODUCTION) && planet->archetype->spacer == &planet_spacer_row);
    hash = fnv(hash, (uint32_t)entity_list_rank_in(live_entities(), entity));
    for (index = 0; index < sizeof messages / sizeof messages[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_message(messages[index]), entity));
    }
    for (index = 0; index < sizeof tags / sizeof tags[0]; index++) {
        hash = fnv(hash, (uint32_t)entity_list_rank_in(entities_by_tag(tags[index]), entity));
    }
    return hash;
}

/* After cart_start_game: the 90 planets galaxy_populate built. */
static void test_galaxy(void) {
    uint32_t h = 2166136261u;
    const entity_list_t *jo;
    int i;

    start_game();
    jo = entities_by_tag(TAG_PLANET);
    for (i = 0; i < jo->count; i++) {
        h = fold_planet(h, jo->items[i]);
    }
    check_int("galaxy_planets", jo->count, GALAXY_PLANETS);
    check("galaxy", h, GALAXY_HASH);
}

static uint32_t fold_live(uint32_t h, const void *e) {
    const planet_t *jo = as_planet(e);

    h = fold_string_member(h, entity_state_name(jo->header.state));
    h = fold_fixed_member(h, jo->header.state_time);
    h = fold_fixed_member(h, jo->header.shake);
    h = (jo->header.fold_anchor != 0) ? fold_bool_member(h, jo->header.fold_anchor) : fold_absent(h);
    h = jo->has_development_tier ? fold_fixed_member(h, jo->development_tier) : fold_absent(h);
    h = jo->has_net_income ? fold_fixed_member(h, jo->net_income) : fold_absent(h);
    h = fold_string_member(h, jo->description);
    h = fnv(h, is_dead_state(jo));
    h = fold_production(h, jo);
    h = fnv(h, (uint32_t)id_list_count(&jo->slipways));
    h = fnv(h, (uint32_t)((jo->has & PLANET_HAS_NEEDS) ? jo->needs.count : -1));
    h = fnv(h, (uint32_t)((jo->has & PLANET_HAS_OUTGOING_ROUTES) ? trade_route_list_count(&jo->outgoing_routes) : -1));
    h = fnv(h, (uint32_t)((jo->has & PLANET_HAS_INCOMING_ROUTES) ? trade_route_list_count(&jo->incoming_routes) : -1));
    h = fnv(h, (uint32_t)(entity_get(jo->targeted_by) != NULL));
    h = fnv(h, (uint32_t)(jo->planet_type != NULL));
    h = fnv(h, (uint32_t)(jo->layout_row != NULL));
    h = fnv(h, (uint32_t)(jo->header.is_hovered != 0));
    return h;
}

enum { STRUCTURE, TRADING_HUB, PROCESSOR, ASCENSION, PROTOSTAR, SYNTHESIZER, SLIPGATE, INFRASPACE_GATE, BURST, DAUGHTERS };

static int daughter_of(const void *e) {
    if (e != NULL &&
        ((const entity_t *)e)->class == structure_class()) {
        return STRUCTURE;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == trading_hub_class()) {
        return TRADING_HUB;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == processor_class()) {
        return PROCESSOR;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == ascension_class()) {
        return ASCENSION;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == protostar_class()) {
        return PROTOSTAR;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == synthesizer_class()) {
        return SYNTHESIZER;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == slipgate_class()) {
        return SLIPGATE;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == infraspace_gate_class()) {
        return INFRASPACE_GATE;
    }
    if (e != NULL &&
        ((const entity_t *)e)->class == burst_class()) {
        return BURST;
    }
    return -1;
}

/* What a subclass inherits from the planet: back_color, row_step, the
 * spacer row itself, and planet_is_colonized and the planet's hit_test
 * called on it. */
static uint32_t fold_daughter(uint32_t h, void *d) {
    const planet_t *jo = as_planet(d);
    vec_t ra;

    h = fold_fixed_member(h, jo->archetype->back_color);
    h = fold_vec(h, &jo->archetype->row_step);
    assert(jo->archetype->spacer == &planet_spacer_row);
    h = fnv(fnv(h, 0xB0u), planet_is_colonized(jo) ? 1u : 0u);
    assert(pos_ref(&jo->header) != NULL);
    ra = *pos_ref(&jo->header);
    h = fold_bool(h, class_of(d)->hit_test(d, mouse_pos(ra, ra)));
    ra = vec_add(ra, vec_make(p8_fixed_from_int(8), p8_fixed_from_int(8)));
    return fold_bool(h, class_of(d)->hit_test(d, mouse_pos(ra, ra)));
}

/* The shop's structure_icon_left_press on the icon of `kind`, as the
 * player buys it, then dragged to the middle of the screen. */
static void *bought(const entity_kind_t *kind) {
    const entity_list_t *hb = entities_by_message(ENTITY_MESSAGE_REFRESH);
    const camera_t *mo = main_camera_typed();
    const vec_t *main_camera_pos;
    int i;

    assert(mo != NULL && pos_ref(&mo->header) != NULL);
    main_camera_pos = pos_ref(&mo->header);
    for (i = 0; hb != NULL && i < hb->count; i++) {
        void *icon = hb->items[i];
        if (class_of(icon) == structure_icon_class() && ((const structure_icon_t *)icon)->kind == kind) {
            down_t made = class_of(icon)->left_press(icon, (mouse_pos_t){0});
            vec_t middle = vec_make(p8_fixed_from_int(80), p8_fixed_from_int(60));
            assert(made.stop && entity_get(made.held) != NULL);
            class_of(entity_get(made.held))->drag(entity_get(made.held), mouse_pos(vec_add(*main_camera_pos, middle), middle));
            return entity_get(made.held);
        }
    }
    assert(0 && "no icon for that kind");
    return NULL;
}

/* The research project whose class_name is the given one. */
static research_project_t *class_name_project(const char *cw) {
    int k;

    for (k = 1;; k++) {
        research_project_t *hp = research_projects_at(k);
        assert(hp != NULL);
        if (hp->class_name != NULL && strcmp(hp->class_name, cw) == 0) {
            return hp;
        }
    }
}

/* A generated planet turned by planet_apply_project into the subclass the
 * class name names: the newest live entity. */
static void *turned(int planet, const char *cw) {
    void *p = entities_by_tag(TAG_PLANET)->items[planet];

    planet_roll_type(p);
    planet_apply_project(p, class_name_project(cw));
    return live_entities()->items[live_entities()->count - 1];
}

/* Each subclass by the road a game takes to it: the seven of the shop
 * bought from their icons, protostar and burst turned from planets by the
 * collapse and harvest projects. */
static void test_daughters(void) {
    static const struct {
        int daughter;
        const entity_kind_t *(*kind)(void);
    } shop[] = {{STRUCTURE, structure_kind}, {PROCESSOR, processor_kind}, {SLIPGATE, slipgate_kind}, {INFRASPACE_GATE, infraspace_gate_kind},
                {TRADING_HUB, trading_hub_kind}, {SYNTHESIZER, synthesizer_kind}, {ASCENSION, ascension_kind}};
    void *made[DAUGHTERS] = {NULL};
    uint32_t h = 2166136261u;
    size_t i;

    start_game();
    for (i = 0; i < sizeof shop / sizeof shop[0]; i++) {
        made[shop[i].daughter] = bought(shop[i].kind());
    }
    made[PROTOSTAR] = turned(0, "protostar");
    made[BURST] = turned(1, "burst");
    for (i = 0; i < DAUGHTERS; i++) {
        assert(made[i] != NULL && daughter_of(made[i]) == (int)i && entity_list_listed(live_entities(), made[i]));
        h = fold_daughter(fnv(h, (uint32_t)i), made[i]);
    }
    check("daughters", h, DAUGHTERS_HASH);
}

static planet_keys_t planet_keys_at(const char *id, int x, int y) {
    planet_keys_t keys = {0};

    planet_keys_set_id(&keys, id);
    keys.map_pos = vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));
    keys.has_map_pos = 1;
    keys.pos = vec_make(p8_fixed_from_int(x * 128), p8_fixed_from_int(y * 128));
    keys.has_pos = 1;
    return keys;
}

static void *new_planet(const char *id, int x, int y) {
    planet_keys_t keys = planet_keys_at(id, x, y);
    return planet_new(&keys);
}

/* The planet copies its id and positions: overwriting the keys afterwards changes nothing. */
static void test_new_planet_copies_its_keys(void) {
    planet_keys_t keys;
    planet_t *made;

    start_title();
    keys = planet_keys_at("p77", 3, 4);
    made = planet_new(&keys);
    keys = planet_keys_at("p1", 9, 9);
    assert(strcmp(planet_id_or_null(made), "p77") == 0 && (made->has & PLANET_HAS_ID));
    assert(entity_map_pos(&made->header).x == p8_fixed_from_int(3) && entity_map_pos(&made->header).y == p8_fixed_from_int(4));
    assert(entity_pos(&made->header).x == p8_fixed_from_int(384) && entity_pos(&made->header).y == p8_fixed_from_int(512));
}

/* Keys without an id, nor positions, make a planet without them. */
static void test_new_planet_without_keys_has_no_id(void) {
    planet_keys_t keys = {0};
    planet_t *made;

    start_title();
    made = planet_new(&keys);
    assert(planet_id_or_null(made) == NULL && !(made->has & PLANET_HAS_ID));
    assert(!entity_has_pos(&made->header) && !entity_has_map_pos(&made->header));
}

static void spend_every_planet_synthesizable_resources(void) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    for (int i = 0; i < planets->count; i++) {
        ((planet_t *)planets->items[i])->synthesizable_resources = &synthesizer_synthesizable_resources_seq;
    }
}

static int planets_holding_their_archetype_synthesizable_resources(void) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    int held = 0;
    for (int i = 0; i < planets->count; i++) {
        const planet_t *planet = (const planet_t *)planets->items[i];
        held += planet->synthesizable_resources == planet->archetype->synthesizable_resources;
    }
    assert(planets->count == GALAXY_PLANETS);
    return held;
}

/* A restart builds the galaxy in game zone memory the previous games' planets
   held: a new planet starts with its archetype's synthesizable resources,
   not what the old one had spent. */
static void test_new_planet_in_reused_memory_resets_synthesizable_resources(void) {
    start_game();
    spend_every_planet_synthesizable_resources();
    start_game();
    assert(planets_holding_their_archetype_synthesizable_resources() == GALAXY_PLANETS);
    spend_every_planet_synthesizable_resources();
    start_game();
    assert(planets_holding_their_archetype_synthesizable_resources() == GALAXY_PLANETS);
}

static void set_too_long_id(void) {
    planet_keys_t keys = {0};
    assert(freopen("/dev/null", "w", stderr) != NULL);
    planet_keys_set_id(&keys, "p1234567");
}

static void test_new_planet(void) {
    uint32_t h = 2166136261u;
    void *a;

    start_title();
    a = new_planet("p900", 1, 2);
    assert(entity_list_listed(live_entities(), a) && entity_list_listed(entities_by_tag(TAG_PLANET), a));
    h = fold_planet(h, a);
    h = fold_live(h, a);
    check("new_planet", h, NEW_PLANET_HASH);
}

/* entities_by_tag answers NULL for a tag that no entity has carried since
 * entity_registry_init(), known or not, and keeps a list once emptied. */
static void test_entities_by_tag_null_until_seen(void) {
    void *a;

    start_title();
    entity_registry_init(NULL);
    assert(entities_by_tag(TAG_PLANET) == NULL && entities_by_tag(TAG_NODE) == NULL);
    a = new_planet("p902", 5, 6);
    assert(entities_by_tag(TAG_PLANET) != NULL && entities_by_tag(TAG_NODE) != NULL);
    assert(entities_by_tag(TAG_STRUCTURE) == NULL);
    entity_unregister(a);
    assert(entities_by_tag(TAG_PLANET) != NULL && entities_by_tag(TAG_PLANET)->count == 0);
    entity_registry_init(NULL);
    assert(entities_by_tag(TAG_PLANET) == NULL);
}

/* update_entities dispatches by state: a vanishing planet runs
 * planet_update_vanishing until its state time passes 60, then leaves the
 * live entities. */
static void test_update_vanishing(void) {
    planet_t *a;
    int calls = 0;

    start_title();
    a = new_planet("p901", 3, 4);
    update_entities();
    assert(is_dead_state(a) == 0);
    entity_set_state(&a->header, ENTITY_STATE_VANISHING);
    while (entity_list_listed(live_entities(), a) && calls < 200) {
        update_entities();
        calls++;
    }
    assert(is_dead_state(a) == 2);
    assert(!entity_list_listed(live_entities(), a) && !entity_list_listed(entities_by_tag(TAG_PLANET), a));
    check_int("update_vanishing_calls", calls, UPDATE_VANISHING_CALLS);
}

/* The flicker of a vanishing planet on screen is visual only: its roll
 * draws from the visual stream and leaves the game stream where it was. */
static void test_vanishing_flicker_draws_from_the_visual_stream(void) {
    planet_keys_t keys;
    planet_t *a;
    uint32_t game;
    uint32_t visual;

    start_game();
    keys = planet_keys_at("p902", 0, 0);
    keys.pos = vec_add(entity_pos(main_camera()), vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    a = planet_new(&keys);
    assert(entity_geom_is_on_screen(&a->header));
    entity_set_state(&a->header, ENTITY_STATE_VANISHING);
    a->header.state_time = p8_fixed_from_int(40);
    game = p8_fixed_rnd_state();
    visual = visual_random_state();
    planet_draw(&graphics, &memory, a, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    assert(p8_fixed_rnd_state() == game && "the vanishing flicker draws no game number");
    assert(visual_random_state() != visual);
}

static p8_vram_t memory_before;
static p8_vram_t memory_drawn;

static void paint_background(void) {
    int x;
    int y;

    for (y = 0; y < P8_SCREEN_HEIGHT; y++) {
        for (x = 0; x < P8_SCREEN_WIDTH; x++) {
            memory.screen[y][x] = (unsigned char)((x + 2 * y) % 15 + 1);
        }
    }
}

/* Today's planet_draw sequence of primitives, frozen as the oracle: disc, sprite, ring, marker,
 * pad circle. Only for a planet with no row, no shake and no targeting. */
static void draw_planet_with_primitives(const planet_t *a, int x, int y) {
    int spr_index = a->planet_type != NULL ? a->planet_type->sprite : 43;

    p8_circfill(&graphics, &memory, x, y, 7, 0);
    hud_select_palette_block(&memory, (a->header.class->lineage & LINEAGE(LINEAGE_TRADING_HUB)) ? a->palette_block : p8_fixed_from_int(0));
    p8_palt(&memory, 3, 0);
    p8_palt(&memory, 0, 1);
    p8_spr(&graphics, &memory, spr_index, x - 4, y - 4, 2, 2);
    hud_select_palette_block_default(&memory);
    if (planet_is_colonized(a)) {
        int tier = p8_fixed_flr_int(a->has_development_tier ? a->development_tier : p8_fixed_from_int(0));
        int color = 0;
        int_seq_at(planet_ring_colors()->items, planet_ring_colors()->count, tier, &color);
        p8_circ(&graphics, &memory, x, y, 6, color);
        p8_spr_default(&graphics, &memory, tier + 1, x - 3, y - 6);
    }
    if (ui_pad_circle(main_ui()) == &a->header) {
        p8_circ(&graphics, &memory, x, y, 9, 7);
    }
}

/* planet_draw at screen (x, y) leaves the same vram as the primitives, from the same start. */
static void check_planet_draws_like_primitives(planet_t *a, int x, int y) {
    vec_t screen = vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y));

    a->header.pos = vec_add(entity_pos(main_camera()), screen);
    assert(entity_geom_is_on_screen(&a->header));
    assert(entity_geom_is_on_screen(&a->header));
    assert(a->layout_row == NULL);
    paint_background();
    memory_before = memory;
    planet_draw(&graphics, &memory, a, screen);
    memory_drawn = memory;
    memory = memory_before;
    draw_planet_with_primitives(a, x, y);
    if (memcmp(&memory_drawn, &memory, sizeof memory) != 0) {
        fprintf(stderr, "planet_draw at (%d, %d), state %d, tier %d: vram differs from the primitives\n", x, y,
                (int)a->header.state, a->has_development_tier ? p8_fixed_flr_int(a->development_tier) : -1);
        assert(0 && "planet_draw draws the same pixels as its primitives");
    }
}

static void check_planet_draws_like_primitives_everywhere(planet_t *a) {
    enum { W = P8_SCREEN_WIDTH, H = P8_SCREEN_HEIGHT };
    static const int spots[][2] = {
        {W / 2, H / 2}, {0, H / 2}, {-6, H / 2}, {W - 1, H / 2}, {W + 5, H / 2},
        {W / 2, 0}, {W / 2, -6}, {W / 2, H - 1}, {W / 2, H + 5},
        {-3, -3}, {W + 2, H + 2}, {2, H - 3}, {W - 3, 2},
    };
    size_t i;

    for (i = 0; i < sizeof spots / sizeof spots[0]; i++) {
        check_planet_draws_like_primitives(a, spots[i][0], spots[i][1]);
    }
}

static planet_t *new_planet_on_screen(const char *id) {
    planet_keys_t keys = planet_keys_at(id, 0, 0);

    keys.pos = vec_add(entity_pos(main_camera()), vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    return planet_new(&keys);
}

/* A plain planet, a colony at every tier in and out of the ring colour table, and the planet under
 * the pad draw the pixels of today's primitives, at the centre and across every screen edge. */
static void test_planet_draw_matches_its_primitives(void) {
    planet_t *a;
    int tier;

    start_game();
    a = new_planet_on_screen("p903");
    check_planet_draws_like_primitives_everywhere(a);
    entity_set_state(&a->header, ENTITY_STATE_SETTLED);
    assert(planet_is_colonized(a));
    a->has_development_tier = 0;
    check_planet_draws_like_primitives_everywhere(a);
    a->has_development_tier = 1;
    for (tier = 0; tier <= PLANET_RING_COLORS_COUNT + 1; tier++) {
        a->development_tier = p8_fixed_from_int(tier);
        check_planet_draws_like_primitives_everywhere(a);
    }
    a->development_tier = p8_fixed_from_int(2);
    ui_set_pad_selection(main_ui(), &a->header, 0);
    assert(ui_pad_circle(main_ui()) == &a->header);
    check_planet_draws_like_primitives_everywhere(a);
}

/* With a fill pattern in force, the disc and the ring keep the pattern's phase on screen. */
static void test_planet_draw_under_a_fill_pattern_matches_its_primitives(void) {
    planet_t *a;
    int tier;

    start_game();
    a = new_planet_on_screen("p904");
    entity_set_state(&a->header, ENTITY_STATE_SETTLED);
    a->has_development_tier = 1;
    p8_fillp(&graphics, 0x5A5Au);
    for (tier = 1; tier <= PLANET_RING_COLORS_COUNT; tier++) {
        a->development_tier = p8_fixed_from_int(tier);
        check_planet_draws_like_primitives_everywhere(a);
    }
    p8_fillp_transparent(&graphics, 0xA5A5u);
    check_planet_draws_like_primitives_everywhere(a);
    p8_fillp_reset(&graphics);
    check_planet_draws_like_primitives_everywhere(a);
}

/* The first draw of a settled planet, the one that may prepare its images, draws no random number
 * from either stream. */
static void test_first_planet_draw_draws_no_random_number(void) {
    planet_t *a;
    uint32_t game;
    uint32_t visual;

    start_game();
    a = new_planet_on_screen("p905");
    entity_set_state(&a->header, ENTITY_STATE_SETTLED);
    a->has_development_tier = 1;
    a->development_tier = p8_fixed_from_int(3);
    game = p8_fixed_rnd_state();
    visual = visual_random_state();
    planet_draw(&graphics, &memory, a, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    assert(p8_fixed_rnd_state() == game && visual_random_state() == visual);
}

/* The production of a planet that never ran planet_install_project is the
 * class's shared one: an in-place write to it is seen by every such planet
 * and by the class, until the next planet_init() puts it back to zero. */
static void test_production_shared(void) {
    planet_t *a;
    planet_t *b;
    production_t *kw;
    research_project_t *hp;
    resource_map_t production_before;
    resource_map_t production_after;
    p8_fixed_t before = p8_fixed_from_int(0);
    p8_fixed_t after = p8_fixed_from_int(0);

    start_title();
    a = new_planet("p902", 5, 6);
    b = new_planet("p903", 7, 8);
    kw = planet_production(a);
    assert(!(a->has & PLANET_HAS_PRODUCTION) && kw == planet_shared_production() && planet_production(b) == kw);
    kw->amount = p8_fixed_from_int(7);
    assert(planet_production_read(b)->amount == p8_fixed_from_int(7));
    assert(planet_shared_production()->amount == p8_fixed_from_int(7));

    assert(research_projects_count() > 0);
    hp = research_projects_find("");
    assert(hp != NULL);
    planet_install_project(a, hp);
    assert(a->has & PLANET_HAS_PRODUCTION);
    assert(planet_production_read(a) == &a->production && a->production.amount == p8_fixed_from_int(0));
    assert(!(b->has & PLANET_HAS_PRODUCTION) && planet_production_read(b) == planet_shared_production());
    assert(research_project_get(a->project) == hp && ((a->has & PLANET_HAS_NEEDS) ? a->needs.count : -1) >= 0);

    economy_total_production(&production_before);
    kw->has_resource = 1;
    kw->resource = p8_fixed_from_int(28);
    economy_total_production(&production_after);
    resource_map_get(&production_before, kw->resource, &before);
    resource_map_get(&production_after, kw->resource, &after);
    assert(after == before);
    planet_init();
    assert(planet_production_read(b) == planet_shared_production());
    assert(!kw->has_resource && kw->amount == p8_fixed_from_int(0));
}

/* A planet holds needs only once a project is installed: one copy per
 * input of the project, owned by the planet. */
static void test_needs_installed(void) {
    planet_t *a;
    research_project_t *hp = NULL;
    int k;

    start_title();
    a = new_planet("p904", 5, 6);
    assert(!(a->has & PLANET_HAS_NEEDS) && a->needs.count == 0);
    for (k = 1; research_projects_at(k) != NULL && hp == NULL; k++) {
        if (research_projects_at(k)->inputs.count >= 2) hp = research_projects_at(k);
    }
    assert(hp != NULL);
    planet_install_project(a, hp);
    assert((a->has & PLANET_HAS_NEEDS) && a->needs.count == hp->inputs.count);
    for (k = 0; k < a->needs.count; k++) {
        assert(a->needs.items[k] != hp->inputs.items[k] && memcmp(a->needs.items[k], hp->inputs.items[k], sizeof(need_t)) == 0);
    }
}

/* planet_roll_type takes the "vanishing" planet type itself on a vanish
 * roll under 0.45, and its state for the planet's; any other draw is a
 * weighted picker entry, and the known state. */
static void test_roll_type_takes_vanishing(void) {
    char id[8];
    int took_vanishing = 0;
    int i;

    start_game();
    for (i = 0; i < 20 && !took_vanishing; i++) {
        planet_t *p;

        snprintf(id, sizeof id, "p%d", 950 + i);
        p = new_planet(id, i, i);
        planet_roll_type(p);
        assert(p->planet_type != NULL && p->description == p->planet_type->description);
        took_vanishing = p->planet_type == planet_types_lookup("vanishing");
        assert(p->header.state == (took_vanishing ? ENTITY_STATE_VANISHING : ENTITY_STATE_KNOWN));
    }
    assert(took_vanishing);
}

static void test_installed_randomness_is_read_back(void) {
    random_walk_t walk = {P8_FIXED_LIT(0x00008000, 0.5), 0};
    random_walk_t vanish = {P8_FIXED_LIT(0x0000cccc, 0.8), P8_FIXED_LIT(0x00008000, 0.5)};
    weighted_picker_t picker = {0};

    planet_init();
    assert(planet_type_picker() == NULL && planet_vanish_roll() == NULL);
    picker.random = random_walk_draw;
    picker.random_ctx = &walk;
    planet_set_type_picker(&picker);
    planet_set_vanish_roll(&vanish);
    assert(planet_type_picker() == &picker && planet_vanish_roll() == &vanish);
    planet_shutdown();
    assert(planet_type_picker() == NULL && planet_vanish_roll() == NULL);
}

static void test_started_game_has_reachable_randomness(void) {
    start_game();
    assert(planet_type_picker() != NULL && planet_vanish_roll() != NULL);
    assert(weighted_picker_walk(planet_type_picker()) != NULL);
}

static int is_a_planet_struct(const void *e) {
    return e != NULL &&
           ((const entity_t *)e)->class == planet_class();
}

/* Built by galaxy_populate or planet_new, a planet is a struct whose class
 * holds the planet's methods, and no other. */
static void test_planet_is_a_struct(void) {
    const entity_list_t *jo;
    void *a;
    int j;

    start_game();
    jo = entities_by_tag(TAG_PLANET);
    for (j = 0; j < jo->count; j++) {
        assert(is_a_planet_struct(jo->items[j]));
    }
    assert_planet_members(planet_class());
    assert(planet_ready());

    assert(planet_mood_rows() == planet_mood_rows_rows && planet_made_resources() == &planet_made_resources_seq &&
           planet_ring_colors() == &planet_ring_colors_seq && planet_spacer() == &planet_spacer_row);

    a = new_planet("p904", 9, 10);
    assert(is_a_planet_struct(a));
    {
        planet_t *bare = planet_new(NULL);
        assert(is_a_planet_struct(bare) && planet_id_or_null(bare) == NULL && pos_ref(&bare->header) == NULL && bare->slipways.zone != NULL);
    }

    (void)bought(structure_kind());
    jo = entities_by_tag(TAG_STRUCTURE);
    assert(jo != NULL && jo->count == 1);
    assert(jo->items[0] != NULL &&
           ((const entity_t *)jo->items[0])->class == structure_class());
}

static int row_slots(void) {
    return ui_zone_live(SLOT_KIND_ROW);
}

/* A galaxy planet given a production of its own with a resource, as
 * planet_install_project and update_tiers_visit would: planet_refresh then
 * gives it a row. */
static planet_t *planet_with_a_row(void) {
    planet_t *a = (planet_t *)entities_by_tag(TAG_PLANET)->items[0];

    a->has |= PLANET_HAS_PRODUCTION;
    a->production.has_resource = 1;
    a->production.resource = p8_fixed_from_int(28);
    a->production.amount = p8_fixed_from_int(2);
    return a;
}

/* planet_refresh rebuilds the planet's row in a slot of its own and gives
 * the replaced one back on the spot; once the planet leaves the live
 * entities, emptying the exit queue gives its row's slot back. */
static void test_refresh_row_slot(void) {
    planet_t *a;
    int once;
    int i;

    start_game();
    cart_flush_exit_queue();
    a = planet_with_a_row();
    class_of(a)->refresh(&a->header);
    once = row_slots();
    assert(a->layout_row != NULL && once > 0);
    for (i = 0; i < 5; i++) {
        class_of(a)->refresh(&a->header);
        assert(row_slots() == once && "the replaced row gives its place back on the spot");
    }
    entity_unregister(&a->header);
    assert(row_slots() == once && "out of the live entities, the planet is still read this frame");
    cart_flush_exit_queue();
    assert(row_slots() == once - 1 && a->layout_row == NULL);
}

/* A planet whose production has no resource gets no row from
 * research_display_planet_resource_row_keys, and no slot. */
static void test_refresh_without_resource_takes_no_slot(void) {
    planet_t *a;
    int before;

    start_title();
    a = new_planet("p903", 7, 8);
    assert(!planet_production_read(a)->has_resource);
    before = row_slots();
    class_of(a)->refresh(&a->header);
    assert(a->layout_row == NULL && row_slots() == before);
}

/* reset_world() hands every planet to the exit queue: a title after a game
 * whose planets all built a row holds as many row slots as before. */
static void test_rows_given_back_after_restart(void) {
    const entity_list_t *jo;
    int title;
    int i;

    start_title();
    cart_flush_exit_queue();
    title = row_slots();
    start_game();
    jo = entities_by_tag(TAG_PLANET);
    (void)planet_with_a_row();
    for (i = 0; i < jo->count; i++) {
        class_of(jo->items[i])->refresh(jo->items[i]);
    }
    assert(row_slots() > title);
    start_title();
    cart_flush_exit_queue();
    assert(row_slots() == title);
}

/* The largest row planet_refresh can build: luxury produced, so two
 * outputs, the most wants a project has, and an amount of 5. */
static planet_t *planet_with_the_largest_row(void) {
    planet_t *a = (planet_t *)entities_by_tag(TAG_PLANET)->items[0];
    const pointer_list_t *is = NULL;
    int r_w;
    int k;

    for (k = 1; research_projects_at(k) != NULL; k++) {
        const research_project_t *p = research_projects_at(k);
        if (is == NULL || p->inputs.count > is->count) is = &p->inputs;
    }
    assert(is != NULL && resource_code_lookup("w", &r_w));
    a->needs = *is;
    a->has |= PLANET_HAS_NEEDS;
    a->has |= PLANET_HAS_PRODUCTION;
    a->production.has_resource = 1;
    a->production.resource = p8_fixed_from_int(r_w);
    a->production.amount = p8_fixed_from_int(5);
    return a;
}

#define REFRESHES_IN_ONE_FRAME 200

/* One tick may rerun economy_recompute, so refresh every planet, several
 * times: a refresh leaves the frame zone as it found it, the row it builds
 * stays the same. */
static void test_refresh_many_times_in_one_frame(void) {
    layout_row_item_t first[16];
    int first_count;
    p8_fixed_t first_width;
    planet_t *a;
    int i;

    start_game();
    cart_flush_exit_queue();
    a = planet_with_the_largest_row();
    planet_refresh(a);
    first_count = a->layout_row->placed_items_count;
    assert(first_count <= 16);
    first_width = a->layout_row->width;
    memcpy(first, a->layout_row->placed_items, (size_t)first_count * sizeof *first);
    for (i = 0; i < REFRESHES_IN_ONE_FRAME; i++) planet_refresh(a);
    assert(a->layout_row->placed_items_count == first_count && a->layout_row->width == first_width);
    for (i = 0; i < first_count; i++) {
        const layout_row_item_t *now = &a->layout_row->placed_items[i];
        assert(now->kind == first[i].kind && now->sprite_id == first[i].sprite_id &&
               now->sprite_palette_block == first[i].sprite_palette_block &&
               now->offset.x == first[i].offset.x && now->offset.y == first[i].offset.y);
        assert(now->kind != LAYOUT_ROW_ITEM_TEXT || strcmp(now->text, first[i].text) == 0);
    }
}

/* The planet's row lays out the same items as a row built afresh from the
 * planet's needs, production and outgoing routes now. */
static void assert_row_is_a_fresh_build(const planet_t *a) {
    const production_t *kw = planet_production_read(a);
    const pointer_list_t *needs = (a->has & PLANET_HAS_NEEDS) ? &a->needs : NULL;
    int routes = (a->has & PLANET_HAS_OUTGOING_ROUTES) ? trade_route_list_count(&a->outgoing_routes) : 0;
    frame_zone_mark_t mark = frame_zone_mark();
    layout_row_keys_t keys;
    const layout_row_t *fresh;
    int i;

    if (!research_display_planet_resource_row_keys(needs, kw->has_resource, kw->resource, kw->amount, p8_fixed_from_int(routes),
                                                   p8_fixed_from_int(0), &keys)) {
        assert(a->layout_row == NULL);
        frame_zone_rewind(mark);
        return;
    }
    fresh = layout_row_new(frame_zone(), &keys);
    assert(a->layout_row != NULL && "a planet with an output always has a row");
    assert(a->layout_row->placed_items_count == fresh->placed_items_count && a->layout_row->width == fresh->width &&
           a->layout_row->has_color == fresh->has_color && a->layout_row->color == fresh->color);
    for (i = 0; i < fresh->placed_items_count; i++) {
        const layout_row_item_t *now = &a->layout_row->placed_items[i];
        const layout_row_item_t *want = &fresh->placed_items[i];
        assert(now->kind == want->kind && now->sprite_id == want->sprite_id && now->color == want->color &&
               now->sprite_palette_block == want->sprite_palette_block && now->offset.x == want->offset.x &&
               now->offset.y == want->offset.y);
        assert(now->kind != LAYOUT_ROW_ITEM_TEXT || strcmp(now->text, want->text) == 0);
    }
    frame_zone_rewind(mark);
}

static need_t *add_need(planet_t *a, p8_fixed_t resource) {
    need_t *need = need_new(game_zone(), (resource_t){.kind = RESOURCE_FIXED, .number = resource});

    if (!(a->has & PLANET_HAS_NEEDS)) {
        pointer_list_init(&a->needs, game_zone());
        a->has |= PLANET_HAS_NEEDS;
    }
    pointer_list_add(&a->needs, need);
    return need;
}

static void refresh_then_check(planet_t *a) {
    planet_refresh(a);
    assert_row_is_a_fresh_build(a);
}

/* Whatever changes between two refreshes, the row shows the planet as it is
 * now; with nothing changed, it shows the same row. */
static void test_refresh_row_follows_every_input(void) {
    planet_t *a;
    need_t *ore;
    trade_route_t route = trade_new(NULL);

    start_game();
    cart_flush_exit_queue();
    a = planet_with_a_row();
    refresh_then_check(a);
    refresh_then_check(a);

    ore = add_need(a, p8_fixed_from_int(21));
    refresh_then_check(a);
    (void)add_need(a, p8_fixed_from_int(22));
    refresh_then_check(a);
    pointer_list_add(&a->needs, NULL);
    refresh_then_check(a);

    ore->has_satisfied = 1;
    ore->satisfied = p8_fixed_from_int(1);
    refresh_then_check(a);
    ore->satisfied = 0;
    refresh_then_check(a);
    refresh_then_check(a);
    ore->satisfied = p8_fixed_from_int(1);
    refresh_then_check(a);
    ore->has_satisfied = 0;
    ore->satisfied = 0;
    refresh_then_check(a);
    ore->row.resource = p8_fixed_from_int(23);
    refresh_then_check(a);
    ore->row.has_resource = 0;
    refresh_then_check(a);
    ore->row.has_resource = 1;
    refresh_then_check(a);
    ore->row.resource = 0;
    refresh_then_check(a);
    ore->row.has_resource = 0;
    refresh_then_check(a);
    ore->row.has_resource = 1;
    ore->row.resource = p8_fixed_from_int(21);
    refresh_then_check(a);

    assert(pointer_list_del(&a->needs, ore));
    refresh_then_check(a);

    a->production.resource = p8_fixed_from_int(29);
    refresh_then_check(a);
    a->production.amount = p8_fixed_from_int(5);
    refresh_then_check(a);
    a->production.amount = p8_fixed_from_int(6);
    refresh_then_check(a);
    a->production.amount = p8_fixed_from_int(7);
    refresh_then_check(a);
    a->production.amount = p8_fixed_from_int(1);
    refresh_then_check(a);

    trade_route_list_init(&a->outgoing_routes, game_zone());
    a->has |= PLANET_HAS_OUTGOING_ROUTES;
    refresh_then_check(a);
    trade_route_list_add(&a->outgoing_routes, &route);
    refresh_then_check(a);
    trade_route_list_add(&a->outgoing_routes, &route);
    refresh_then_check(a);
    trade_route_list_clear(&a->outgoing_routes);
    refresh_then_check(a);

    a->production.has_resource = 0;
    refresh_then_check(a);
    a->production.has_resource = 1;
    refresh_then_check(a);
}

/* A released row is rebuilt by the next refresh, inputs unchanged or not. */
static void test_refresh_after_release_builds_the_row(void) {
    planet_t *a;

    start_game();
    cart_flush_exit_queue();
    a = planet_with_a_row();
    refresh_then_check(a);
    planet_release_row(&a->header);
    assert(a->layout_row == NULL);
    refresh_then_check(a);
}

/* More needs than any planet holds in a game: a change to the last one
 * still shows. */
static void test_refresh_row_with_many_needs(void) {
    need_t *needs[6];
    planet_t *a;
    int i;

    start_game();
    cart_flush_exit_queue();
    a = planet_with_a_row();
    for (i = 0; i < 6; i++) {
        needs[i] = add_need(a, p8_fixed_from_int(21 + i % 3));
        needs[i]->has_satisfied = 1;
        needs[i]->satisfied = p8_fixed_from_int(1);
    }
    refresh_then_check(a);
    refresh_then_check(a);
    needs[5]->satisfied = 0;
    refresh_then_check(a);
    needs[4]->satisfied = 0;
    refresh_then_check(a);
    needs[5]->satisfied = p8_fixed_from_int(1);
    refresh_then_check(a);
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

static void aborts_in_child(void (*test)(void)) {
    int status;
    pid_t child = spawn(test);
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT);
}

/* draw_layers culls an off-screen planet before its draw: only the classes whose draw is
 * planet_draw_method opt in; an ascension-family draw updates its emitter first, so it is never culled. */
static void test_only_planet_draw_classes_are_culled_at_dispatch(void) {
    const entity_class_t *culled[] = {planet_class(), structure_class(), trading_hub_class(), processor_class(),
                                      slipgate_class(), infraspace_gate_class()};
    const entity_class_t *dispatched[] = {ascension_class(), protostar_class(), synthesizer_class(), burst_class()};
    start_game();
    for (size_t i = 0; i < sizeof culled / sizeof culled[0]; i++) {
        assert(culled[i]->draw == planet_draw_method && culled[i]->skip_draw_off_screen);
    }
    for (size_t i = 0; i < sizeof dispatched / sizeof dispatched[0]; i++) {
        assert(dispatched[i]->draw != planet_draw_method && !dispatched[i]->skip_draw_off_screen);
    }
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
    in_fresh_child(test_vanishing_flicker_draws_from_the_visual_stream);
    in_fresh_child(test_planet_draw_matches_its_primitives);
    in_fresh_child(test_planet_draw_under_a_fill_pattern_matches_its_primitives);
    in_fresh_child(test_first_planet_draw_draws_no_random_number);
    in_fresh_child(test_galaxy);
    in_fresh_child(test_daughters);
    in_fresh_child(test_new_planet);
    in_fresh_child(test_new_planet_copies_its_keys);
    in_fresh_child(test_new_planet_without_keys_has_no_id);
    in_fresh_child(test_new_planet_in_reused_memory_resets_synthesizable_resources);
    aborts_in_child(set_too_long_id);
    in_fresh_child(test_entities_by_tag_null_until_seen);
    in_fresh_child(test_update_vanishing);
    in_fresh_child(test_production_shared);
    in_fresh_child(test_needs_installed);
    in_fresh_child(test_roll_type_takes_vanishing);
    in_fresh_child(test_installed_randomness_is_read_back);
    in_fresh_child(test_started_game_has_reachable_randomness);
    in_fresh_child(test_planet_is_a_struct);
    in_fresh_child(test_only_planet_draw_classes_are_culled_at_dispatch);
    in_fresh_child(test_refresh_row_slot);
    in_fresh_child(test_refresh_without_resource_takes_no_slot);
    in_fresh_child(test_rows_given_back_after_restart);
    in_fresh_child(test_refresh_many_times_in_one_frame);
    in_fresh_child(test_refresh_row_follows_every_input);
    in_fresh_child(test_refresh_after_release_builds_the_row);
    in_fresh_child(test_refresh_row_with_many_needs);

    printf("test_planet: all tests passed\n");
    return 0;
}
