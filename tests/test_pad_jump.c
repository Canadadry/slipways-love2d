#include "game/pad_jump.h"

#include "engine/entity_id.h"
#include "engine/frame_zone.h"
#include "game/bindings.h"
#include "game/planet.h"
#include "heap_allocator.h"

#include "p8/fixed.h"
#include "p8/input.h"

#include <assert.h>
#include <stdio.h>

static void test_init_hovers_nothing_unseeded_without_repeat(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_hovered(&jump) == NULL && !jump.seeded);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_4, 0) ==
           PAD_DIRECTION_NONE);
}

static void test_a_press_jumps_then_repeats_at_18_26_34(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_4, P8_PAD_BUTTON_4) ==
           PAD_DIRECTION_RIGHT);
    for (int frame = 1; frame <= 60; frame++) {
        pad_direction_t expected =
            (frame == 18 || frame == 26 || frame == 34 || frame == 42 || frame == 50 || frame == 58)
                ? PAD_DIRECTION_RIGHT
                : PAD_DIRECTION_NONE;
        assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_4, 0) == expected);
    }
}

static void hold_frames(pad_jump_t *jump, int pad_buttons, int frames) {
    for (int i = 0; i < frames; i++) {
        assert(pad_jump_repeat_step(jump, &bindings_compiled, BINDINGS_MODE_JUMP, pad_buttons, 0) ==
               PAD_DIRECTION_NONE);
    }
}

static void test_a_release_clears_and_a_new_press_restarts(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_3, P8_PAD_BUTTON_3);
    hold_frames(&jump, P8_PAD_BUTTON_3, 10);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, 0, 0) == PAD_DIRECTION_NONE);
    hold_frames(&jump, P8_PAD_BUTTON_3, 30);

    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_3, P8_PAD_BUTTON_3) ==
           PAD_DIRECTION_LEFT);
    hold_frames(&jump, P8_PAD_BUTTON_3, 10);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_3, P8_PAD_BUTTON_3) ==
           PAD_DIRECTION_LEFT);
    hold_frames(&jump, P8_PAD_BUTTON_3, 17);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_3, 0) ==
           PAD_DIRECTION_LEFT);
}

static void test_a_second_direction_takes_over_at_once(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_4, P8_PAD_BUTTON_4);
    hold_frames(&jump, P8_PAD_BUTTON_4, 10);
    int both = P8_PAD_BUTTON_4 | P8_PAD_BUTTON_2;
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, both, P8_PAD_BUTTON_2) ==
           PAD_DIRECTION_DOWN);
    hold_frames(&jump, both, 17);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, both, 0) == PAD_DIRECTION_DOWN);
}

static void test_simultaneous_presses_go_up_down_left_right(void) {
    pad_jump_t jump;
    int all = P8_PAD_BUTTON_1 | P8_PAD_BUTTON_2 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4;
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, all, all) == PAD_DIRECTION_UP);
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, all,
                                P8_PAD_BUTTON_2 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4) == PAD_DIRECTION_DOWN);
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, all,
                                P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4) == PAD_DIRECTION_LEFT);
}

static void test_the_directions_by_index_are_up_down_left_right(void) {
    static const pad_direction_t expected[PAD_JUMP_DIRECTIONS] = {PAD_DIRECTION_UP, PAD_DIRECTION_DOWN,
                                                                   PAD_DIRECTION_LEFT, PAD_DIRECTION_RIGHT};
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        assert(pad_jump_direction(i) == expected[i]);
    }
}

static void test_clear_stops_the_repeat(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_1, P8_PAD_BUTTON_1);
    hold_frames(&jump, P8_PAD_BUTTON_1, 10);
    pad_jump_repeat_clear(&jump);
    hold_frames(&jump, P8_PAD_BUTTON_1, 40);
}

static void test_other_buttons_neither_jump_nor_repeat(void) {
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &bindings_compiled, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6,
                                P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6) == PAD_DIRECTION_NONE);
    hold_frames(&jump, P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6, 30);
}

static bindings_t jump_directions_swapped(void) {
    bindings_t swapped;
    bindings_error_t error;
    assert(bindings_parse("swapped.cfg",
                          "jump.jump_up = 2\njump.jump_down = 1\njump.jump_left = 4\njump.jump_right = 3\n", &swapped,
                          &error));
    return swapped;
}

static void test_a_swapped_table_moves_the_jump_directions(void) {
    bindings_t swapped = jump_directions_swapped();
    static const int buttons[PAD_JUMP_DIRECTIONS] = {P8_PAD_BUTTON_2, P8_PAD_BUTTON_1, P8_PAD_BUTTON_4,
                                                     P8_PAD_BUTTON_3};
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        pad_jump_t jump;
        pad_jump_init(&jump);
        assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, buttons[i], buttons[i]) ==
               pad_jump_direction(i));
    }
    pad_jump_t jump;
    pad_jump_init(&jump);
    int left_and_right = P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4;
    assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, left_and_right, left_and_right) ==
           PAD_DIRECTION_LEFT);
}

static void test_a_swapped_direction_repeats_on_its_own_button_only(void) {
    bindings_t swapped = jump_directions_swapped();
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_2, P8_PAD_BUTTON_2) ==
           PAD_DIRECTION_UP);
    for (int frame = 1; frame <= PAD_JUMP_REPEAT_DELAY + 2 * PAD_JUMP_REPEAT_PERIOD; frame++) {
        int repeats = frame >= PAD_JUMP_REPEAT_DELAY && (frame - PAD_JUMP_REPEAT_DELAY) % PAD_JUMP_REPEAT_PERIOD == 0;
        pad_direction_t expected = repeats ? PAD_DIRECTION_UP : PAD_DIRECTION_NONE;
        assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_2, 0) == expected);
    }

    pad_jump_init(&jump);
    pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_2, P8_PAD_BUTTON_2);
    assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_1, 0) == PAD_DIRECTION_NONE);
    for (int frame = 0; frame < PAD_JUMP_REPEAT_DELAY + PAD_JUMP_REPEAT_PERIOD; frame++) {
        assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_JUMP, P8_PAD_BUTTON_2, 0) == PAD_DIRECTION_NONE);
    }
}

static void test_a_mode_left_unbound_neither_jumps_nor_repeats(void) {
    bindings_t swapped = jump_directions_swapped();
    int all = P8_PAD_BUTTON_1 | P8_PAD_BUTTON_2 | P8_PAD_BUTTON_3 | P8_PAD_BUTTON_4;
    for (int button = P8_PAD_BUTTON_1; button <= P8_PAD_BUTTON_4; button <<= 1) {
        pad_jump_t jump;
        pad_jump_init(&jump);
        assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_SLIPWAY, button, button) == PAD_DIRECTION_NONE);
        for (int frame = 0; frame < PAD_JUMP_REPEAT_DELAY + 2 * PAD_JUMP_REPEAT_PERIOD; frame++) {
            assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_SLIPWAY, button, 0) == PAD_DIRECTION_NONE);
        }
    }
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_repeat_step(&jump, &swapped, BINDINGS_MODE_SLIPWAY, all, all) == PAD_DIRECTION_NONE);
}

static void assert_scroll(int x, int y, int expected_dx, int expected_dy) {
    int dx = -99, dy = -99;
    pad_jump_scroll(x, y, &dx, &dy);
    assert(dx == expected_dx && dy == expected_dy);
}

static void test_the_inner_rectangle_bounds_are_inclusive(void) {
    assert(pad_jump_inside(80, 60) == 1);
    assert(pad_jump_inside(20, 20) == 1);
    assert(pad_jump_inside(139, 88) == 1);
    assert(pad_jump_inside(19, 60) == 0);
    assert(pad_jump_inside(140, 60) == 0);
    assert(pad_jump_inside(80, 19) == 0);
    assert(pad_jump_inside(80, 89) == 0);
}

static void test_scroll_is_the_distance_outside_clamped_to_8(void) {
    assert_scroll(80, 60, 0, 0);
    assert_scroll(20, 20, 0, 0);
    assert_scroll(139, 88, 0, 0);
    assert_scroll(15, 60, -5, 0);
    assert_scroll(144, 60, 5, 0);
    assert_scroll(80, 15, 0, -5);
    assert_scroll(80, 93, 0, 5);
    assert_scroll(-10, 60, -8, 0);
    assert_scroll(169, 60, 8, 0);
    assert_scroll(80, 99, 0, 8);
    assert_scroll(17, 91, -3, 3);
    assert_scroll(400, -300, 8, -8);
}

enum { MAX_LAYOUT = 8 };

typedef struct {
    entity_t *items[MAX_LAYOUT];
    entity_list_t list;
} layout_t;

/* Map coordinates in 1/16 of a map unit, which is 8 world pixels: every value is exact in fixed point. */
static void place(entity_t *e, const entity_class_t *cls, int x, int y) {
    entity_init_header(e, cls);
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

static void test_world_point_floors_map_pos_and_ignores_pos(void) {
    entity_t e;
    int x = -99, y = -99;
    place(&e, NULL, 3, -5);
    entity_set_pos(&e, vec_make(p8_fixed_from_int(7), p8_fixed_from_int(9)));
    pad_jump_world_point(&e, &x, &y);
    assert(x == 24 && y == -40);

    entity_set_map_pos(&e, vec_make(p8_fixed_from_int(1) / 3, -p8_fixed_from_int(1) / 3));
    pad_jump_world_point(&e, &x, &y);
    assert(x == 42 && y == -43);
}

static void test_nearest_of_several(void) {
    entity_t far, near, middle;
    place(&far, NULL, 10, 10);
    place(&near, NULL, 1, -1);
    place(&middle, NULL, -3, 0);
    entity_t *all[] = {&far, &middle, &near};
    layout_t layout;
    assert(pad_jump_nearest(0, 0, gather(&layout, all, 3)) == &near);
    assert(pad_jump_nearest(-24, 0, gather(&layout, all, 3)) == &middle);
    assert(pad_jump_nearest(5000, 5000, gather(&layout, all, 3)) == &far);
}

static void test_nearest_tie_goes_to_the_earlier(void) {
    entity_t left, right;
    place(&left, NULL, -2, 0);
    place(&right, NULL, 2, 0);
    layout_t layout;
    entity_t *left_first[] = {&left, &right};
    assert(pad_jump_nearest(0, 0, gather(&layout, left_first, 2)) == &left);
    entity_t *right_first[] = {&right, &left};
    assert(pad_jump_nearest(0, 0, gather(&layout, right_first, 2)) == &right);
}

static void test_nearest_of_nothing_is_null(void) {
    layout_t layout;
    assert(pad_jump_nearest(0, 0, NULL) == NULL);
    assert(pad_jump_nearest(0, 0, gather(&layout, NULL, 0)) == NULL);
}

static const entity_class_t structure_class = {.lineage = LINEAGE(LINEAGE_PLANET) | LINEAGE(LINEAGE_STRUCTURE)};

static void place_in_state(entity_t *e, const entity_class_t *cls, state_t state, int x, int y) {
    place(e, cls, x, y);
    e->state = state;
}

static void test_seed_prefers_the_first_colony(void) {
    entity_t known, first_colony, second_colony;
    place_in_state(&known, planet_class(), ENTITY_STATE_KNOWN, 0, 0);
    place_in_state(&first_colony, planet_class(), ENTITY_STATE_SETTLED, 20, 20);
    place_in_state(&second_colony, planet_class(), ENTITY_STATE_SETTLED, 1, 1);
    entity_t *all[] = {&known, &first_colony, &second_colony};
    layout_t layout;
    assert(pad_jump_seed(gather(&layout, all, 3), 0, 0) == &first_colony);
}

static void test_seed_without_colony_takes_the_nearest(void) {
    entity_t structure, far_known, near_known;
    place_in_state(&structure, &structure_class, ENTITY_STATE_SETTLED, 0, 0);
    place_in_state(&far_known, planet_class(), ENTITY_STATE_KNOWN, 10, 0);
    place_in_state(&near_known, planet_class(), ENTITY_STATE_KNOWN, 4, 0);
    entity_t *all[] = {&structure, &far_known, &near_known};
    layout_t layout;
    assert(pad_jump_seed(gather(&layout, all, 3), 80, 0) == &far_known);
    assert(pad_jump_seed(gather(&layout, all, 3), 40, 0) == &near_known);
    assert(pad_jump_seed(gather(&layout, all, 3), 0, 0) == &structure);
}

static void test_seed_of_nothing_is_null(void) {
    layout_t layout;
    assert(pad_jump_seed(gather(&layout, NULL, 0), 80, 60) == NULL);
    assert(pad_jump_seed(NULL, 80, 60) == NULL);
}

/* With an id, as entity_get needs. */
static void place_live(entity_t *e, state_t state, int x, int y) {
    place_in_state(e, planet_class(), state, x, y);
    entity_id_take(e);
}

static void test_hovered_is_the_hovered_object_while_it_is_a_candidate(void) {
    entity_t known, unknown;
    place_live(&known, ENTITY_STATE_KNOWN, 0, 0);
    place_live(&unknown, ENTITY_STATE_UNKNOWN, 1, 0);
    pad_jump_t jump;
    pad_jump_init(&jump);
    pad_jump_hover(&jump, &known);
    assert(pad_jump_hovered(&jump) == &known && !jump.seeded);
    known.is_dead = 1;
    assert(pad_jump_hovered(&jump) == NULL);
    pad_jump_hover(&jump, &unknown);
    assert(pad_jump_hovered(&jump) == NULL);
    pad_jump_hover(&jump, NULL);
    assert(pad_jump_hovered(&jump) == NULL && !jump.seeded);
}

static void test_hover_seeded_seeds_only_on_an_object(void) {
    entity_t known;
    place_live(&known, ENTITY_STATE_KNOWN, 0, 0);
    pad_jump_t jump;
    pad_jump_init(&jump);
    pad_jump_hover_seeded(&jump, NULL);
    assert(pad_jump_hovered(&jump) == NULL && !jump.seeded);
    pad_jump_hover_seeded(&jump, &known);
    assert(pad_jump_hovered(&jump) == &known && jump.seeded);
    pad_jump_hover_seeded(&jump, NULL);
    assert(pad_jump_hovered(&jump) == NULL && jump.seeded);
}

static void test_reseed_takes_the_first_colony_once_then_the_nearest(void) {
    entity_t near_known, colony;
    place_live(&near_known, ENTITY_STATE_KNOWN, 0, 0);
    place_live(&colony, ENTITY_STATE_SETTLED, 20, 20);
    entity_t *all[] = {&near_known, &colony};
    layout_t layout;
    pad_jump_t jump;
    pad_jump_init(&jump);
    assert(pad_jump_reseed(&jump, gather(&layout, NULL, 0), 0, 0) == NULL && !jump.seeded);
    assert(pad_jump_reseed(&jump, gather(&layout, all, 2), 0, 0) == &colony && jump.seeded);
    assert(pad_jump_hovered(&jump) == &colony);
    assert(pad_jump_reseed(&jump, gather(&layout, all, 2), 0, 0) == &near_known);
    assert(pad_jump_hovered(&jump) == &near_known && jump.seeded);
}

static void test_candidates_keep_known_and_settled_in_order(void) {
    entity_t unknown, colony, probe, known, dead, structure, vanishing;
    place_in_state(&unknown, planet_class(), ENTITY_STATE_UNKNOWN, 0, 0);
    place_in_state(&colony, planet_class(), ENTITY_STATE_SETTLED, 1, 0);
    place_in_state(&probe, probe_class(), ENTITY_STATE_KNOWN, 2, 0);
    place_in_state(&known, planet_class(), ENTITY_STATE_KNOWN, 3, 0);
    place_in_state(&dead, planet_class(), ENTITY_STATE_KNOWN, 4, 0);
    dead.is_dead = 1;
    place_in_state(&structure, &structure_class, ENTITY_STATE_SETTLED, 5, 0);
    place_in_state(&vanishing, planet_class(), ENTITY_STATE_VANISHING, 6, 0);
    entity_t *all[] = {&unknown, &colony, &probe, &known, &dead, &structure, &vanishing};
    layout_t layout;
    entity_list_t out = pad_jump_candidates(frame_zone(), gather(&layout, all, 7));
    assert(out.count == 3 && out.zone == frame_zone());
    assert(out.items[0] == &colony && out.items[1] == &known && out.items[2] == &structure);
}

static void test_candidates_of_nothing_are_empty(void) {
    layout_t layout;
    assert(pad_jump_candidates(frame_zone(), gather(&layout, NULL, 0)).count == 0);
    assert(pad_jump_candidates(frame_zone(), NULL).count == 0);
}

static void test_any_candidate_needs_a_live_known_or_settled_object(void) {
    layout_t layout;
    assert(!pad_jump_any_candidate(NULL));
    assert(!pad_jump_any_candidate(gather(&layout, NULL, 0)));

    entity_t unknown, dead;
    place_in_state(&unknown, planet_class(), ENTITY_STATE_UNKNOWN, 0, 0);
    place_in_state(&dead, planet_class(), ENTITY_STATE_KNOWN, 1, 0);
    dead.is_dead = 1;
    entity_t *nothing_known[] = {&unknown, &dead};
    assert(!pad_jump_any_candidate(gather(&layout, nothing_known, 2)));

    entity_t known;
    place_in_state(&known, planet_class(), ENTITY_STATE_KNOWN, 2, 0);
    entity_t *one_known[] = {&unknown, &dead, &known};
    assert(pad_jump_any_candidate(gather(&layout, one_known, 3)));

    entity_t structure;
    place_in_state(&structure, &structure_class, ENTITY_STATE_SETTLED, 3, 0);
    entity_t *one_structure[] = {&unknown, &structure};
    assert(pad_jump_any_candidate(gather(&layout, one_structure, 2)));
}

int main(void) {
    entity_id_table_init(&heap_allocator);
    test_init_hovers_nothing_unseeded_without_repeat();
    test_seed_prefers_the_first_colony();
    test_seed_without_colony_takes_the_nearest();
    test_seed_of_nothing_is_null();
    test_hovered_is_the_hovered_object_while_it_is_a_candidate();
    test_hover_seeded_seeds_only_on_an_object();
    test_reseed_takes_the_first_colony_once_then_the_nearest();
    test_candidates_keep_known_and_settled_in_order();
    test_candidates_of_nothing_are_empty();
    test_any_candidate_needs_a_live_known_or_settled_object();
    test_world_point_floors_map_pos_and_ignores_pos();
    test_nearest_of_several();
    test_nearest_tie_goes_to_the_earlier();
    test_nearest_of_nothing_is_null();
    test_the_inner_rectangle_bounds_are_inclusive();
    test_scroll_is_the_distance_outside_clamped_to_8();
    test_a_press_jumps_then_repeats_at_18_26_34();
    test_a_release_clears_and_a_new_press_restarts();
    test_a_second_direction_takes_over_at_once();
    test_simultaneous_presses_go_up_down_left_right();
    test_clear_stops_the_repeat();
    test_the_directions_by_index_are_up_down_left_right();
    test_other_buttons_neither_jump_nor_repeat();
    test_a_swapped_table_moves_the_jump_directions();
    test_a_swapped_direction_repeats_on_its_own_button_only();
    test_a_mode_left_unbound_neither_jumps_nor_repeats();
    printf("test_pad_jump: ok\n");
    return 0;
}
