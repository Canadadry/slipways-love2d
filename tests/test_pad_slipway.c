#include "game/pad_slipway.h"

#include "engine/entity_id.h"
#include "engine/frame_zone.h"
#include "engine/entity_id_list.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/probe.h"
#include "game/slipway.h"
#include "heap_allocator.h"

#include <assert.h>
#include <stdio.h>

static planet_t *node(planet_t *storage, const entity_class_t *cls, state_t state) {
    entity_init_header(&storage->header, cls);
    storage->header.state = state;
    entity_id_take(&storage->header);
    entity_id_list_init(&storage->slipways, &heap_allocator);
    return storage;
}

static void test_settled_nodes_start_and_others_do_not(void) {
    static planet_t colony, structure, known, dead;
    static entity_t probe;
    assert(pad_slipway_can_start(&node(&colony, planet_class(), ENTITY_STATE_SETTLED)->header));
    assert(pad_slipway_can_start(&node(&structure, structure_class(), ENTITY_STATE_SETTLED)->header));
    assert(!pad_slipway_can_start(&node(&known, planet_class(), ENTITY_STATE_KNOWN)->header));
    node(&dead, planet_class(), ENTITY_STATE_SETTLED)->header.is_dead = 1;
    assert(!pad_slipway_can_start(&dead.header));
    entity_init_header(&probe, probe_class());
    probe.state = ENTITY_STATE_SETTLED;
    assert(!pad_slipway_can_start(&probe));
    assert(!pad_slipway_can_start(NULL));
}

enum { MAX_SLIPWAYS = 16 };

/* A slipway from start to end, listed on start as planet_attach_slipway lists it. */
static entity_t *link(planet_t *start, planet_t *end) {
    static slipway_t slipways[MAX_SLIPWAYS];
    static int used;
    assert(used < MAX_SLIPWAYS && "link: raise MAX_SLIPWAYS");
    slipway_t *s = &slipways[used++];
    entity_init_header(&s->header, slipway_class());
    s->header.state = ENTITY_STATE_NORMAL;
    s->start = entity_id_link(&start->header);
    s->end = entity_id_link(&end->header);
    entity_id_list_add(&start->slipways, entity_id_take(&s->header));
    return &s->header;
}

static void test_a_slipgate_starts_until_it_holds_three(void) {
    static planet_t gate, infraspace, a, b, c;
    node(&gate, slipgate_class(), ENTITY_STATE_SETTLED);
    node(&infraspace, infraspace_gate_class(), ENTITY_STATE_SETTLED);
    node(&a, planet_class(), ENTITY_STATE_SETTLED);
    node(&b, planet_class(), ENTITY_STATE_SETTLED);
    node(&c, planet_class(), ENTITY_STATE_SETTLED);
    link(&gate, &a);
    link(&gate, &b);
    link(&infraspace, &a);
    link(&infraspace, &b);
    assert(pad_slipway_can_start(&gate.header) && pad_slipway_can_start(&infraspace.header));
    link(&gate, &c);
    link(&infraspace, &c);
    assert(!pad_slipway_can_start(&gate.header) && !pad_slipway_can_start(&infraspace.header));
}

static void test_candidates_drop_the_start_and_keep_the_order(void) {
    entity_t a, start, b, c;
    entity_init_header(&a, planet_class());
    entity_init_header(&start, planet_class());
    entity_init_header(&b, planet_class());
    entity_init_header(&c, planet_class());
    entity_t *items[] = {&a, &start, &b, &c};
    entity_list_t candidates = {.items = items, .count = 4, .capacity = 4};
    entity_list_t out = pad_slipway_candidates(frame_zone(), &candidates, &start);
    assert(out.count == 3 && out.zone == frame_zone());
    assert(out.items[0] == &a && out.items[1] == &b && out.items[2] == &c);

    entity_t elsewhere;
    entity_init_header(&elsewhere, planet_class());
    out = pad_slipway_candidates(frame_zone(), &candidates, &elsewhere);
    assert(out.count == 4 && out.items[0] == &a && out.items[1] == &start && out.items[3] == &c);

    candidates.count = 0;
    assert(pad_slipway_candidates(frame_zone(), &candidates, &start).count == 0);
}

static void test_between_finds_the_joining_slipway_from_either_end(void) {
    static planet_t start, target, other;
    node(&start, planet_class(), ENTITY_STATE_SETTLED);
    node(&target, planet_class(), ENTITY_STATE_SETTLED);
    node(&other, planet_class(), ENTITY_STATE_SETTLED);
    assert(pad_slipway_between(&start.header, &target.header) == NULL);
    link(&start, &other);
    assert(pad_slipway_between(&start.header, &target.header) == NULL);
    entity_t *joining = link(&target, &start);
    assert(pad_slipway_between(&target.header, &start.header) == joining);
    entity_id_list_add(&start.slipways, entity_id_link(joining));
    assert(pad_slipway_between(&start.header, &target.header) == joining);
}

static void test_built_only_when_a_slipway_joins_them_now_and_none_did_before(void) {
    static planet_t start, target;
    node(&start, planet_class(), ENTITY_STATE_SETTLED);
    node(&target, planet_class(), ENTITY_STATE_KNOWN);
    assert(pad_slipway_outcome(&start.header, &target.header, 0) == PAD_SLIPWAY_REFUSED);
    link(&start, &target);
    assert(pad_slipway_outcome(&start.header, &target.header, 0) == PAD_SLIPWAY_BUILT);
    assert(pad_slipway_outcome(&start.header, &target.header, 1) == PAD_SLIPWAY_REFUSED);
    assert(pad_slipway_outcome(&start.header, NULL, 0) == PAD_SLIPWAY_REFUSED);
}

static void test_init_holds_nothing(void) {
    pad_slipway_t slipway = {.phase = PAD_SLIPWAY_HELD, .linked_before = 1};
    pad_slipway_init(&slipway);
    assert(slipway.phase == PAD_SLIPWAY_NONE && !slipway.linked_before);
    assert(entity_id_is_none(slipway.start) && entity_id_is_none(slipway.target));
}

static pad_slipway_t aimed(entity_id_t start, entity_id_t target) {
    pad_slipway_t slipway;
    pad_slipway_init(&slipway);
    slipway.start = start;
    slipway.target = target;
    slipway.phase = PAD_SLIPWAY_HELD;
    return slipway;
}

static void test_release_records_the_link_and_waits_for_the_drop(void) {
    entity_id_t start = {1, 1}, target = {2, 1};
    pad_slipway_t slipway = aimed(start, target);
    pad_slipway_release(&slipway, 1);
    assert(slipway.phase == PAD_SLIPWAY_RELEASED && slipway.linked_before);
    assert(entity_id_equal(slipway.start, start) && entity_id_equal(slipway.target, target));
}

static void test_built_continues_from_the_target_and_refused_from_the_start(void) {
    entity_id_t start = {1, 1}, target = {2, 1};
    pad_slipway_t built = aimed(start, target);
    pad_slipway_release(&built, 0);
    pad_slipway_rebase(&built, PAD_SLIPWAY_BUILT);
    assert(built.phase == PAD_SLIPWAY_PRESSED);
    assert(entity_id_equal(built.start, target) && entity_id_is_none(built.target));

    pad_slipway_t refused = aimed(start, target);
    pad_slipway_release(&refused, 0);
    pad_slipway_rebase(&refused, PAD_SLIPWAY_REFUSED);
    assert(refused.phase == PAD_SLIPWAY_PRESSED);
    assert(entity_id_equal(refused.start, start) && entity_id_is_none(refused.target));
}

int main(void) {
    entity_id_table_init(&heap_allocator);
    test_settled_nodes_start_and_others_do_not();
    test_a_slipgate_starts_until_it_holds_three();
    test_candidates_drop_the_start_and_keep_the_order();
    test_between_finds_the_joining_slipway_from_either_end();
    test_built_only_when_a_slipway_joins_them_now_and_none_did_before();
    test_init_holds_nothing();
    test_release_records_the_link_and_waits_for_the_drop();
    test_built_continues_from_the_target_and_refused_from_the_start();
    printf("test_pad_slipway: ok\n");
    return 0;
}
