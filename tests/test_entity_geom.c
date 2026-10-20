#include "engine/entity_geom.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "heap_allocator.h"

enum { POINT_COUNT = 24, ROUNDS = 400 };

static uint32_t lcg_state = 12345u;

static uint32_t lcg_next(void) {
    lcg_state = lcg_state * 1103515245u + 12345u;
    return lcg_state >> 8;
}

static p8_fixed_t grid_coordinate(void) {
    return (p8_fixed_t)((lcg_next() % 32u) * 0x0800);
}

static int nearest_within(vec_t p, const entity_list_t *list, p8_fixed_t threshold) {
    entity_t *nearest;
    return entity_geom_nearest(p, list, &nearest) <= threshold;
}

static void test_null_and_empty_lists_have_nothing_within(void) {
    entity_list_t empty;
    vec_t p = vec_make(P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00008000, 0.5));

    entity_list_init(&empty, &heap_allocator);
    assert(entity_geom_any_within(p, NULL, p8_fixed_from_int(1000)) == 0);
    assert(entity_geom_any_within(p, &empty, p8_fixed_from_int(1000)) == 0);
    entity_list_free(&empty);
}

static void test_an_entity_at_exactly_p_is_skipped(void) {
    entity_t at_p;
    entity_t away;
    entity_list_t list;
    vec_t p = vec_make(P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00004000, 0.25));

    entity_init_header(&at_p, NULL);
    entity_set_map_pos(&at_p, p);
    entity_init_header(&away, NULL);
    entity_set_map_pos(&away, vec_make(P8_FIXED_LIT(0x00008000, 0.5), P8_FIXED_LIT(0x00006000, 0.375)));
    entity_list_init(&list, &heap_allocator);
    entity_list_add(&list, &at_p);
    assert(entity_geom_any_within(p, &list, p8_fixed_from_int(0)) == 0);
    assert(entity_geom_any_within(p, &list, p8_fixed_from_int(1000)) == 0);

    entity_list_add(&list, &away);
    assert(entity_geom_any_within(p, &list, P8_FIXED_LIT(0x00000400, 0.015625)) == 1);
    assert(entity_geom_any_within(p, &list, P8_FIXED_LIT(0x000003ff, 0.0156)) == 0);
    entity_list_free(&list);
}

static void test_answers_what_the_nearest_distance_answers(void) {
    static const p8_fixed_t thresholds[] = {
        0, P8_FIXED_LIT(0x00000ccc, 0.05), P8_FIXED_LIT(0x00004000, 0.25), P8_FIXED_LIT(0x7ffeffff, 32766.99),
    };
    entity_t points[POINT_COUNT];
    int round;

    for (round = 0; round < ROUNDS; round++) {
        entity_list_t list;
        int count = (int)(lcg_next() % (POINT_COUNT + 1));
        vec_t p = vec_make(grid_coordinate(), grid_coordinate());
        size_t t;
        int i;

        entity_list_init(&list, &heap_allocator);
        for (i = 0; i < count; i++) {
            entity_init_header(&points[i], NULL);
            if (lcg_next() % 8u != 0) {
                entity_set_map_pos(&points[i], lcg_next() % 4u == 0 ? p : vec_make(grid_coordinate(), grid_coordinate()));
            }
            entity_list_add(&list, &points[i]);
        }
        for (t = 0; t < sizeof thresholds / sizeof thresholds[0]; t++) {
            assert(entity_geom_any_within(p, &list, thresholds[t]) == nearest_within(p, &list, thresholds[t]));
        }
        entity_list_free(&list);
    }
}

int main(void) {
    test_null_and_empty_lists_have_nothing_within();
    test_an_entity_at_exactly_p_is_skipped();
    test_answers_what_the_nearest_distance_answers();
    printf("test_entity_geom: ok\n");
    return 0;
}
