#include "game/pad_target.h"

#include "p8/fixed.h"

#include <assert.h>
#include <stdio.h>

enum { MAX_LAYOUT = 8 };

typedef struct {
    entity_t *items[MAX_LAYOUT];
    entity_list_t list;
} layout_t;

/* Coordinates are in 1/16 of a map unit, so every value is exact in fixed point. */
static void place(entity_t *e, int x, int y) {
    entity_init_header(e, NULL);
    entity_set_map_pos(e, vec_make(p8_fixed_from_int(x) / 16, p8_fixed_from_int(y) / 16));
}

static const entity_list_t *gather(layout_t *layout, entity_t **entities, int count) {
    assert(count <= MAX_LAYOUT && "gather: the layout holds at most MAX_LAYOUT entities");
    for (int i = 0; i < count; i++) {
        layout->items[i] = entities[i];
    }
    layout->list = (entity_list_t){.items = layout->items, .count = count, .capacity = MAX_LAYOUT};
    return &layout->list;
}

static void test_one_candidate_per_direction(void) {
    entity_t origin, up, down, left, right;
    place(&origin, 0, 0);
    place(&up, 0, -5);
    place(&down, 0, 5);
    place(&left, -5, 0);
    place(&right, 5, 0);
    entity_t *all[] = {&up, &down, &left, &right};
    layout_t layout;
    const entity_list_t *candidates = gather(&layout, all, 4);
    assert(pad_target_pick(&origin, PAD_DIRECTION_UP, candidates) == &up);
    assert(pad_target_pick(&origin, PAD_DIRECTION_DOWN, candidates) == &down);
    assert(pad_target_pick(&origin, PAD_DIRECTION_LEFT, candidates) == &left);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, candidates) == &right);
}

/* Right: the near off-axis object loses to the far on-axis one at weight 2 (12 against 10), and would win at weight 1. */
static void test_score_breaks_far_on_axis_against_near_off_axis(void) {
    entity_t origin, far, near;
    place(&origin, 0, 0);
    place(&far, 10, 0);
    place(&near, 6, 3);
    entity_t *all[] = {&near, &far};
    layout_t layout;
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, all, 2)) == &far);

    place(&near, 7, 1);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, all, 2)) == &near);
}

static void test_nothing_in_the_cone(void) {
    entity_t origin, sideways, behind, beside, same_point;
    place(&origin, 0, 0);
    place(&sideways, 3, 4);
    place(&behind, -5, 0);
    place(&beside, 0, 5);
    place(&same_point, 0, 0);
    entity_t *all[] = {&sideways, &behind, &beside, &same_point};
    layout_t layout;
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, all, 4)) == NULL);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, all, 0)) == NULL);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, NULL) == NULL);
}

static void test_origin_is_never_returned(void) {
    entity_t origin, right;
    place(&origin, 0, 0);
    place(&right, 5, 0);
    layout_t layout;
    entity_t *alone[] = {&origin};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, alone, 1)) == NULL);
    entity_t *both[] = {&origin, &right};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, both, 2)) == &right);
    assert(pad_target_pick(&origin, PAD_DIRECTION_LEFT, gather(&layout, both, 2)) == NULL);
}

static void test_cone_edge_is_inside(void) {
    entity_t origin, below_edge, above_edge;
    place(&origin, 0, 0);
    place(&below_edge, 4, 4);
    place(&above_edge, -3, -3);
    entity_t *all[] = {&below_edge, &above_edge};
    layout_t layout;
    const entity_list_t *candidates = gather(&layout, all, 2);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, candidates) == &below_edge);
    assert(pad_target_pick(&origin, PAD_DIRECTION_DOWN, candidates) == &below_edge);
    assert(pad_target_pick(&origin, PAD_DIRECTION_LEFT, candidates) == &above_edge);
    assert(pad_target_pick(&origin, PAD_DIRECTION_UP, candidates) == &above_edge);
}

static void test_equal_scores_go_on_axis_then_first_listed(void) {
    entity_t origin, on_axis, off_axis, upper, lower;
    place(&origin, 0, 0);
    place(&on_axis, 6, 0);
    place(&off_axis, 4, 1);
    place(&upper, 4, -1);
    place(&lower, 4, 1);
    layout_t layout;
    entity_t *off_first[] = {&off_axis, &on_axis};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, off_first, 2)) == &on_axis);
    entity_t *on_first[] = {&on_axis, &off_axis};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, on_first, 2)) == &on_axis);

    entity_t *upper_first[] = {&upper, &lower};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, upper_first, 2)) == &upper);
    entity_t *lower_first[] = {&lower, &upper};
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, lower_first, 2)) == &lower);
}

/* 40/16 map units is 320 world pixels, well past one 128-pixel screen. */
static void test_whole_map_without_wrap(void) {
    entity_t origin, far, left;
    place(&origin, 0, 0);
    place(&far, 40, 2);
    place(&left, -20, 0);
    entity_t *all[] = {&origin, &far, &left};
    layout_t layout;
    const entity_list_t *candidates = gather(&layout, all, 3);
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, candidates) == &far);
    assert(pad_target_pick(&far, PAD_DIRECTION_RIGHT, candidates) == NULL);
}

static void test_judged_by_map_pos_not_pos(void) {
    entity_t origin, bobbing, steady;
    place(&origin, 0, 0);
    place(&bobbing, 5, 0);
    place(&steady, 8, 0);
    entity_set_pos(&origin, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(64)));
    entity_set_pos(&bobbing, vec_make(p8_fixed_from_int(64), p8_fixed_from_int(10)));
    entity_set_pos(&steady, vec_make(p8_fixed_from_int(70), p8_fixed_from_int(64)));
    entity_t *all[] = {&steady, &bobbing};
    layout_t layout;
    assert(pad_target_pick(&origin, PAD_DIRECTION_RIGHT, gather(&layout, all, 2)) == &bobbing);
}

static const entity_class_t planet_family_class = {.lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE)};
static const entity_class_t probe_family_class = {.lineage = LINEAGE(LINEAGE_PROBE)};

static int is_candidate_as(const entity_class_t *cls, state_t state, int is_dead) {
    entity_t e;
    entity_init_header(&e, cls);
    e.state = state;
    e.is_dead = is_dead;
    return pad_target_is_candidate(&e);
}

static void test_candidates_are_live_known_or_settled_planets(void) {
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_KNOWN, 0) == 1);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_SETTLED, 0) == 1);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_UNKNOWN, 0) == 0);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_VANISHING, 0) == 0);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_PLACING, 0) == 0);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_UNFOLDING, 0) == 0);
    assert(is_candidate_as(&planet_family_class, ENTITY_STATE_KNOWN, 1) == 0);
    assert(is_candidate_as(&probe_family_class, ENTITY_STATE_KNOWN, 0) == 0);
    assert(is_candidate_as(&probe_family_class, ENTITY_STATE_SETTLED, 0) == 0);
    assert(pad_target_is_candidate(NULL) == 0);
}

int main(void) {
    test_one_candidate_per_direction();
    test_score_breaks_far_on_axis_against_near_off_axis();
    test_nothing_in_the_cone();
    test_origin_is_never_returned();
    test_cone_edge_is_inside();
    test_equal_scores_go_on_axis_then_first_listed();
    test_whole_map_without_wrap();
    test_judged_by_map_pos_not_pos();
    test_candidates_are_live_known_or_settled_planets();
    printf("test_pad_target: ok\n");
    return 0;
}
