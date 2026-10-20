#include "engine/pointer_list.h"

#include <assert.h>
#include <stdio.h>

#include "heap_allocator.h"

static void test_add_grows_past_four_and_keeps_order(void) {
    int cells[5];
    int i;
    pointer_list_t pointer_list;
    pointer_list_init(&pointer_list, &heap_allocator);
    assert(pointer_list.count == 0 && pointer_list.items == NULL);

    for (i = 0; i < 5; i++) {
        pointer_list_add(&pointer_list, &cells[i]);
    }

    assert(pointer_list.count == 5);
    assert(pointer_list.capacity >= 5);
    for (i = 0; i < 5; i++) {
        assert(pointer_list.items[i] == &cells[i]);
    }

    pointer_list_free(&pointer_list);
    assert(pointer_list.items == NULL && pointer_list.count == 0 && pointer_list.capacity == 0);
}

static void test_del_removes_only_the_first_identical_pointer(void) {
    int a, b, c;
    pointer_list_t pointer_list;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);
    pointer_list_add(&pointer_list, &c);
    pointer_list_add(&pointer_list, &b);

    assert(pointer_list_del(&pointer_list, &b) == 1);

    assert(pointer_list.count == 3);
    assert(pointer_list.items[0] == &a);
    assert(pointer_list.items[1] == &c);
    assert(pointer_list.items[2] == &b);

    pointer_list_free(&pointer_list);
}

static void test_del_absent_is_a_no_op(void) {
    int a, b, absent;
    pointer_list_t pointer_list;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);

    assert(pointer_list_del(&pointer_list, &absent) == 0);

    assert(pointer_list.count == 2);
    assert(pointer_list.items[0] == &a);
    assert(pointer_list.items[1] == &b);

    pointer_list_free(&pointer_list);
}

static void test_null_is_stored_and_del_null_removes_the_first(void) {
    int a;
    pointer_list_t pointer_list;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, NULL);
    pointer_list_add(&pointer_list, NULL);

    assert(pointer_list.count == 3);
    assert(pointer_list_del(&pointer_list, NULL) == 1);

    assert(pointer_list.count == 2);
    assert(pointer_list.items[0] == &a);
    assert(pointer_list.items[1] == NULL);

    pointer_list_free(&pointer_list);
}

static void test_all_walks_in_order(void) {
    int a, b, c;
    int *expected[3];
    void *e;
    int seen = 0;
    pointer_list_t pointer_list;
    pointer_list_iter_t it;
    expected[0] = &a;
    expected[1] = &b;
    expected[2] = &c;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);
    pointer_list_add(&pointer_list, &c);

    it = pointer_list_all(&pointer_list);
    while (pointer_list_iter_next(&it, &e)) {
        assert(e == expected[seen]);
        seen++;
    }
    assert(seen == 3);

    pointer_list_free(&pointer_list);
}

static void test_all_tolerates_deleting_the_current_element(void) {
    int a, b, c, d;
    void *visited[4];
    int visited_count = 0;
    void *e;
    pointer_list_t pointer_list;
    pointer_list_iter_t it;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);
    pointer_list_add(&pointer_list, &c);
    pointer_list_add(&pointer_list, &d);

    it = pointer_list_all(&pointer_list);
    while (pointer_list_iter_next(&it, &e)) {
        visited[visited_count++] = e;
        if (e == &b) {
            pointer_list_del(&pointer_list, &b);
        }
    }

    assert(visited_count == 4);
    assert(visited[0] == &a);
    assert(visited[1] == &b);
    assert(visited[2] == &c);
    assert(visited[3] == &d);
    assert(pointer_list.count == 3);

    pointer_list_free(&pointer_list);
}

/* Tolerance only for the element just returned: deleting another one makes
 * the walk re-read the current slot, so the second element comes twice. */
static void test_all_revisits_when_another_element_is_deleted(void) {
    int a, b, c, d;
    void *visited[5];
    int visited_count = 0;
    void *e;
    pointer_list_t pointer_list;
    pointer_list_iter_t it;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);
    pointer_list_add(&pointer_list, &c);
    pointer_list_add(&pointer_list, &d);

    it = pointer_list_all(&pointer_list);
    while (pointer_list_iter_next(&it, &e)) {
        assert(visited_count < 5);
        visited[visited_count++] = e;
        if (e == &b) {
            pointer_list_del(&pointer_list, &c);
        }
    }

    assert(visited_count == 4);
    assert(visited[0] == &a);
    assert(visited[1] == &b);
    assert(visited[2] == &b);
    assert(visited[3] == &d);

    pointer_list_free(&pointer_list);
}

static void test_all_on_empty_yields_nothing(void) {
    void *e = &e;
    pointer_list_t pointer_list;
    pointer_list_iter_t it;
    pointer_list_init(&pointer_list, &heap_allocator);

    it = pointer_list_all(&pointer_list);
    assert(pointer_list_iter_next(&it, &e) == 0);
    assert(e == &e);

    pointer_list_free(&pointer_list);
}

typedef struct {
    void *seen[4];
    int count;
} collector_t;

static void collect(void *e, void *ctx) {
    collector_t *collector = ctx;
    collector->seen[collector->count++] = e;
}

static void test_foreach_visits_each_once_in_order_with_ctx(void) {
    int a, b, c;
    collector_t collector;
    pointer_list_t pointer_list;
    collector.count = 0;
    pointer_list_init(&pointer_list, &heap_allocator);
    pointer_list_add(&pointer_list, &a);
    pointer_list_add(&pointer_list, &b);
    pointer_list_add(&pointer_list, &c);

    pointer_list_foreach(&pointer_list, collect, &collector);

    assert(collector.count == 3);
    assert(collector.seen[0] == &a);
    assert(collector.seen[1] == &b);
    assert(collector.seen[2] == &c);

    pointer_list_free(&pointer_list);
}

static void test_reserved_list_holds_its_capacity_in_order(void) {
    int cells[6];
    int i;
    pointer_list_t pointer_list;
    pointer_list_init_reserve(&pointer_list, &heap_allocator, 6);
    assert(pointer_list.count == 0 && pointer_list.capacity == 6);

    for (i = 0; i < 6; i++) {
        pointer_list_add(&pointer_list, &cells[i]);
    }

    assert(pointer_list.count == 6 && pointer_list.capacity == 6 && "a reserved list fills to its capacity without growing");
    for (i = 0; i < 6; i++) {
        assert(pointer_list.items[i] == &cells[i]);
    }
    pointer_list_free(&pointer_list);
}

int main(void) {
    test_add_grows_past_four_and_keeps_order();
    test_del_removes_only_the_first_identical_pointer();
    test_del_absent_is_a_no_op();
    test_null_is_stored_and_del_null_removes_the_first();
    test_all_walks_in_order();
    test_all_tolerates_deleting_the_current_element();
    test_all_revisits_when_another_element_is_deleted();
    test_all_on_empty_yields_nothing();
    test_foreach_visits_each_once_in_order_with_ctx();
    test_reserved_list_holds_its_capacity_in_order();
    printf("test_pointer_list: all tests passed\n");
    return 0;
}
