#include "game/gen.h"
#include "p8/fixed.h"
#include "heap_allocator.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

static void test_random_walk_draw_matches_random_walk_next(void) {
    random_walk_t walk = {P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00004000, 0.25)};
    random_walk_t twin = walk;
    uint32_t saved = p8_fixed_rnd_state();
    for (int i = 0; i < 16; i++) {
        uint32_t before = p8_fixed_rnd_state();
        p8_fixed_t drawn = random_walk_draw(&walk);
        p8_fixed_set_rnd_state(before);
        assert(drawn == random_walk_next(&twin));
        assert(walk.step == twin.step);
        assert(walk.bias == twin.bias);
    }
    p8_fixed_set_rnd_state(saved);
}

static p8_fixed_t constant_half(void *ctx) {
    (void)ctx;
    return P8_FIXED_LIT(0x00008000, 0.5);
}

static void test_picker_hands_back_its_walk(void) {
    random_walk_t *walk = random_walk_new(&heap_allocator, P8_FIXED_LIT(0x00008000, 0.5), 0);
    weighted_picker_t *picker = weighted_picker_new(&heap_allocator, p8_fixed_from_int(2), random_walk_draw, walk);
    assert(weighted_picker_walk(picker) == walk);
}

static void test_picker_with_another_source_has_no_walk(void) {
    random_walk_t walk = {0, 0};
    weighted_picker_t *picker = weighted_picker_new(&heap_allocator, p8_fixed_from_int(2), constant_half, &walk);
    assert(weighted_picker_walk(picker) == NULL);
}

static void test_restored_picker_draws_the_same_types(void) {
    enum { DRAWS = 40 };
    const planet_types_entry_t *first[DRAWS];
    uint32_t saved = p8_fixed_rnd_state();
    random_walk_t *walk = random_walk_new(&heap_allocator, P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00004000, 0.25));
    weighted_picker_t *original = weighted_picker_new(&heap_allocator, p8_fixed_from_int(2), random_walk_draw, walk);
    for (int i = 0; i < 7; i++) {
        weighted_picker_next(original);
    }
    weighted_picker_t snapshot = *original;
    random_walk_t walk_snapshot = *weighted_picker_walk(original);
    uint32_t rnd_snapshot = p8_fixed_rnd_state();
    for (int i = 0; i < DRAWS; i++) {
        first[i] = weighted_picker_next(original);
    }

    random_walk_t *fresh_walk = random_walk_new(&heap_allocator, 0, 0);
    weighted_picker_t *restored = weighted_picker_new(&heap_allocator, snapshot.multiplier, random_walk_draw, fresh_walk);
    restored->tickets_count = snapshot.tickets_count;
    for (int i = 0; i < snapshot.tickets_count; i++) {
        restored->tickets[i] = snapshot.tickets[i];
    }
    for (int i = 0; i < 3; i++) {
        restored->recent[i] = snapshot.recent[i];
        restored->recent_set[i] = snapshot.recent_set[i];
    }
    weighted_picker_walk(restored)->step = walk_snapshot.step;
    weighted_picker_walk(restored)->bias = walk_snapshot.bias;
    assert(fresh_walk->step == walk_snapshot.step);
    assert(fresh_walk->bias == walk_snapshot.bias);
    p8_fixed_set_rnd_state(rnd_snapshot);
    for (int i = 0; i < DRAWS; i++) {
        assert(weighted_picker_next(restored) == first[i]);
    }
    p8_fixed_set_rnd_state(saved);
}

int main(void) {
    test_random_walk_draw_matches_random_walk_next();
    test_picker_hands_back_its_walk();
    test_picker_with_another_source_has_no_walk();
    test_restored_picker_draws_the_same_types();
    printf("test_gen: all tests passed\n");
    return 0;
}
