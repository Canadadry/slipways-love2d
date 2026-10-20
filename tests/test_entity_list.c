#include "engine/entity_list.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "heap_allocator.h"

static void test_add_grows_past_four_keeps_order_and_free_empties(void) {
    entity_t cells[5];
    int i;
    entity_list_t entity_list;
    entity_list_init(&entity_list, &heap_allocator);
    assert(entity_list.count == 0 && entity_list.items == NULL);

    for (i = 0; i < 5; i++) {
        entity_list_add(&entity_list, &cells[i]);
    }
    assert(entity_list.count == 5 && entity_list.capacity >= 5);
    for (i = 0; i < 5; i++) {
        assert(entity_list.items[i] == &cells[i]);
    }

    entity_list_free(&entity_list);
    assert(entity_list.items == NULL && entity_list.count == 0 && entity_list.capacity == 0);
}

static void test_del_first_identical_and_has(void) {
    entity_t a, b, c, absent;
    entity_list_t entity_list;
    entity_list_init(&entity_list, &heap_allocator);
    entity_list_add(&entity_list, &a);
    entity_list_add(&entity_list, &b);
    entity_list_add(&entity_list, &c);
    entity_list_add(&entity_list, &b);

    assert(entity_list_del(&entity_list, &b) == 1);
    assert(entity_list.count == 3);
    assert(entity_list.items[0] == &a && entity_list.items[1] == &c && entity_list.items[2] == &b);

    assert(entity_list_del(&entity_list, &absent) == 0);
    assert(entity_list.count == 3);

    assert(entity_list_has(&entity_list, &c) == 1);
    assert(entity_list_has(&entity_list, &absent) == 0);
    assert(entity_list_has(NULL, &a) == 0);
    assert(entity_list_has(&entity_list, NULL) == 0);
    entity_list_add(&entity_list, NULL);
    assert(entity_list_has(&entity_list, NULL) == 1);

    entity_list_free(&entity_list);
}

static void test_cat_copies_a_then_b_and_leaves_them(void) {
    entity_t a, b, c;
    entity_list_t l, r, t;
    entity_list_init(&l, &heap_allocator);
    entity_list_init(&r, &heap_allocator);
    entity_list_add(&l, &a);
    entity_list_add(&l, &b);
    entity_list_add(&r, &c);

    t = entity_list_concat(&heap_allocator, &l, &r);
    assert(t.count == 3);
    assert(t.items[0] == &a && t.items[1] == &b && t.items[2] == &c);
    assert(l.count == 2 && l.items[0] == &a && l.items[1] == &b);
    assert(r.count == 1 && r.items[0] == &c);

    entity_list_free(&t);
    entity_list_free(&l);
    entity_list_free(&r);
}

static void test_all_sees_each_when_the_returned_one_is_deleted(void) {
    entity_t a, b, c;
    entity_t *seen[3];
    entity_t *e;
    int n = 0;
    entity_list_t entity_list;
    entity_list_iter_t it;
    entity_list_init(&entity_list, &heap_allocator);
    entity_list_add(&entity_list, &a);
    entity_list_add(&entity_list, &b);
    entity_list_add(&entity_list, &c);

    it = entity_list_all(&entity_list);
    while (entity_list_iter_next(&it, &e)) {
        assert(n < 3);
        seen[n++] = e;
        entity_list_del(&entity_list, e);
    }
    assert(n == 3);
    assert(seen[0] == &a && seen[1] == &b && seen[2] == &c);
    assert(entity_list.count == 0);

    entity_list_free(&entity_list);
}

static int odd_rank(entity_t *e, void *ctx) {
    const entity_t *cells = ctx;
    return (e - cells) % 2 == 1;
}

static void test_filter_keeps_what_the_predicate_keeps_in_order(void) {
    entity_t cells[5];
    entity_list_t src;
    entity_list_t kept;
    int i;
    entity_list_init(&src, &heap_allocator);
    for (i = 0; i < 5; i++) {
        entity_list_add(&src, &cells[i]);
    }

    kept = entity_list_filter(&heap_allocator, &src, odd_rank, cells);
    assert(kept.count == 2 && kept.items[0] == &cells[1] && kept.items[1] == &cells[3]);
    assert(src.count == 5);
    for (i = 0; i < 5; i++) {
        assert(src.items[i] == &cells[i]);
    }
    entity_list_free(&kept);

    kept = entity_list_filter(&heap_allocator, NULL, odd_rank, cells);
    assert(kept.count == 0);
    entity_list_free(&kept);
    entity_list_free(&src);
}

typedef struct {
    const entity_t *cells;
    int wanted;
    int calls;
    int saw_null;
} find_first_ctx_t;

static int rank_is_wanted(entity_t *e, void *ctx_ptr) {
    find_first_ctx_t *ctx = ctx_ptr;
    ctx->calls++;
    if (e == NULL) {
        ctx->saw_null = 1;
        return 0;
    }
    return ((e - ctx->cells) % ctx->wanted) == ctx->wanted - 1;
}

static void test_find_first_returns_the_first_match_and_asks_nothing_after_it(void) {
    entity_t cells[6];
    entity_list_t src;
    find_first_ctx_t ctx = {cells, 3, 0, 0};
    int i;
    entity_list_init(&src, &heap_allocator);
    for (i = 0; i < 6; i++) {
        entity_list_add(&src, &cells[i]);
    }

    assert(entity_list_find_first(&src, rank_is_wanted, &ctx) == &cells[2]);
    assert(ctx.calls == 3 && "the predicate is not asked about any entity after the match");

    ctx.wanted = 7;
    ctx.calls = 0;
    assert(entity_list_find_first(&src, rank_is_wanted, &ctx) == NULL);
    assert(ctx.calls == 6);
    assert(src.count == 6);
    entity_list_free(&src);
}

static void test_find_first_is_null_on_an_empty_or_null_list(void) {
    entity_t cells[1];
    entity_list_t empty;
    find_first_ctx_t ctx = {cells, 1, 0, 0};
    entity_list_init(&empty, &heap_allocator);
    assert(entity_list_find_first(&empty, rank_is_wanted, &ctx) == NULL);
    assert(entity_list_find_first(NULL, rank_is_wanted, &ctx) == NULL);
    assert(ctx.calls == 0);
    entity_list_free(&empty);
}

static void test_find_first_hands_a_null_item_to_the_predicate(void) {
    entity_t cells[3];
    entity_list_t src;
    find_first_ctx_t ctx = {cells, 1, 0, 0};
    entity_list_init(&src, &heap_allocator);
    entity_list_add(&src, NULL);
    entity_list_add(&src, &cells[1]);
    assert(entity_list_find_first(&src, rank_is_wanted, &ctx) == &cells[1]);
    assert(ctx.saw_null && ctx.calls == 2);
    entity_list_free(&src);
}

static void test_find_first_is_the_filters_first_item(void) {
    entity_t cells[40];
    uint32_t seed = 0x5eed1234u;
    int round;
    for (round = 0; round < 500; round++) {
        entity_list_t src;
        entity_list_t kept;
        find_first_ctx_t ctx;
        entity_t *first;
        int count;
        int i;
        seed = seed * 1664525u + 1013904223u;
        count = (int)((seed >> 8) % 41u);
        entity_list_init(&src, &heap_allocator);
        for (i = 0; i < count; i++) {
            seed = seed * 1664525u + 1013904223u;
            entity_list_add(&src, (seed >> 24) % 9u == 0 ? NULL : &cells[(seed >> 8) % 40u]);
        }
        seed = seed * 1664525u + 1013904223u;
        ctx.cells = cells;
        ctx.wanted = 1 + (int)((seed >> 8) % 50u);
        ctx.calls = 0;
        ctx.saw_null = 0;
        kept = entity_list_filter(&heap_allocator, &src, rank_is_wanted, &ctx);
        first = entity_list_find_first(&src, rank_is_wanted, &ctx);
        assert(first == (kept.count > 0 ? kept.items[0] : NULL));
        entity_list_free(&kept);
        entity_list_free(&src);
    }
}

static void test_clear_keeps_items_and_capacity_and_takes_nothing_until_past_them(void) {
    entity_t cells[9];
    entity_list_t entity_list;
    const allocator_t *arena = arena_take(&heap_allocator, "test-entity-list", 1024);
    int i;
    entity_list_init(&entity_list, arena);
    for (i = 0; i < 5; i++) {
        entity_list_add(&entity_list, &cells[i]);
    }
    entity_t **items = entity_list.items;
    const int capacity = entity_list.capacity;
    const size_t grab_arena = arena_used(arena);

    entity_list_clear(&entity_list);
    assert(entity_list.count == 0 && entity_list.capacity == capacity && entity_list.items == items && entity_list.zone == arena);
    for (i = 0; i < capacity; i++) {
        entity_list_add(&entity_list, &cells[i]);
    }
    assert(arena_used(arena) == grab_arena && "re-adding up to the kept capacity takes nothing");
    assert(entity_list.items == items && entity_list.items[0] == &cells[0] && entity_list.count == capacity);

    entity_list_add(&entity_list, &cells[capacity]);
    assert(entity_list.capacity == 2 * capacity && arena_used(arena) > grab_arena && "past it, it grows as before");
    arena_release(arena);
}

static void test_reserved_list_fills_its_capacity_without_taking_then_grows(void) {
    entity_t cells[4];
    entity_list_t entity_list;
    const allocator_t *arena = arena_take(&heap_allocator, "test-entity-list", 1024);
    int i;
    entity_list_init_reserve(&entity_list, arena, 3);
    assert(entity_list.count == 0 && entity_list.capacity == 3 && entity_list.zone == arena);
    const size_t grab_arena = arena_used(arena);

    for (i = 0; i < 3; i++) {
        entity_list_add(&entity_list, &cells[i]);
    }
    assert(arena_used(arena) == grab_arena && "adding up to the reserved capacity takes nothing");
    assert(entity_list.count == 3 && entity_list.capacity == 3);
    for (i = 0; i < 3; i++) {
        assert(entity_list.items[i] == &cells[i]);
    }

    entity_list_add(&entity_list, &cells[3]);
    assert(entity_list.capacity > 3 && arena_used(arena) > grab_arena && "past the reserve, it grows in its zone");
    assert(entity_list.count == 4 && entity_list.items[0] == &cells[0] && entity_list.items[3] == &cells[3]);
    arena_release(arena);
}

static void test_reserving_zero_takes_nothing(void) {
    entity_list_t entity_list;
    const allocator_t *arena = arena_take(&heap_allocator, "test-entity-list", 1024);
    const size_t grab_arena = arena_used(arena);
    entity_list_init_reserve(&entity_list, arena, 0);
    assert(entity_list.count == 0 && entity_list.capacity == 0 && entity_list.items == NULL && entity_list.zone == arena);
    assert(arena_used(arena) == grab_arena && "a reserve of 0 takes nothing from its zone");
    arena_release(arena);
}

int main(void) {
    test_add_grows_past_four_keeps_order_and_free_empties();
    test_del_first_identical_and_has();
    test_cat_copies_a_then_b_and_leaves_them();
    test_all_sees_each_when_the_returned_one_is_deleted();
    test_filter_keeps_what_the_predicate_keeps_in_order();
    test_find_first_returns_the_first_match_and_asks_nothing_after_it();
    test_find_first_is_null_on_an_empty_or_null_list();
    test_find_first_hands_a_null_item_to_the_predicate();
    test_find_first_is_the_filters_first_item();
    test_clear_keeps_items_and_capacity_and_takes_nothing_until_past_them();
    test_reserved_list_fills_its_capacity_without_taking_then_grows();
    test_reserving_zero_takes_nothing();
    printf("test_entity_list: ok\n");
    return 0;
}
