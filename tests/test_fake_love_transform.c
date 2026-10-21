#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include "../tools/fake_love/transform.h"

static void assert_offset(const transform_t *transform, int x, int y) {
    transform_offset_t offset = transform_get_offset(transform);
    if (offset.x != x || offset.y != y) {
        fprintf(stderr, "offset (%d, %d), expected (%d, %d)\n", offset.x, offset.y, x, y);
    }
    assert(offset.x == x && offset.y == y);
}

static void test_a_new_transform_has_no_offset_and_nothing_saved(void) {
    transform_t transform;
    transform_init(&transform);
    assert_offset(&transform, 0, 0);
    assert(transform_get_depth(&transform) == 0);
}

static void test_translations_add(void) {
    transform_t transform;
    transform_init(&transform);
    assert(transform_translate(&transform, 3, -4));
    assert(transform_translate(&transform, -10, 20));
    assert_offset(&transform, -7, 16);
}

static void test_a_pop_undoes_the_translations_since_its_push(void) {
    transform_t transform;
    transform_init(&transform);
    assert(transform_translate(&transform, 5, 6));
    assert(transform_push(&transform));
    assert_offset(&transform, 5, 6);
    assert(transform_get_depth(&transform) == 1);
    assert(transform_translate(&transform, 100, -200));
    assert(transform_pop(&transform));
    assert_offset(&transform, 5, 6);
    assert(transform_get_depth(&transform) == 0);
}

static void test_nested_pushes_restore_in_order(void) {
    transform_t transform;
    transform_init(&transform);
    assert(transform_push(&transform));
    assert(transform_translate(&transform, 20, 0));
    assert(transform_push(&transform));
    assert(transform_translate(&transform, -40, 10));
    assert_offset(&transform, -20, 10);
    assert(transform_pop(&transform));
    assert_offset(&transform, 20, 0);
    assert(transform_pop(&transform));
    assert_offset(&transform, 0, 0);
}

static void test_the_hundred_and_twenty_ninth_push_is_refused_and_changes_nothing(void) {
    transform_t transform;
    transform_init(&transform);
    for (int depth = 1; depth <= 128; depth++) {
        assert(transform_translate(&transform, 1, -1));
        assert(transform_push(&transform));
        assert(transform_get_depth(&transform) == depth);
    }
    assert(transform_translate(&transform, 1, -1));
    assert(!transform_push(&transform));
    assert(transform_get_depth(&transform) == 128);
    assert_offset(&transform, 129, -129);
    for (int depth = 128; depth >= 1; depth--) {
        assert(transform_pop(&transform));
        assert_offset(&transform, depth, -depth);
    }
}

static void test_a_pop_with_nothing_pushed_is_refused_and_changes_nothing(void) {
    transform_t transform;
    transform_init(&transform);
    assert(transform_translate(&transform, 7, 9));
    assert(!transform_pop(&transform));
    assert(transform_get_depth(&transform) == 0);
    assert_offset(&transform, 7, 9);
    assert(transform_push(&transform));
    assert(transform_pop(&transform));
    assert(!transform_pop(&transform));
    assert_offset(&transform, 7, 9);
}

static void test_an_offset_reaching_two_to_the_twenty_is_refused_and_changes_nothing(void) {
    const int limit = 1048576;
    struct {
        int start_x;
        int start_y;
        int dx;
        int dy;
        bool accepted;
    } cases[] = {
        {0, 0, limit - 1, -(limit - 1), true},
        {limit - 2, 0, 1, 0, true},
        {limit - 1, 0, 1, 0, false},
        {0, -(limit - 1), 0, -1, false},
        {5, 0, limit - 5, 0, false},
        {0, -5, 0, -(limit - 5), false},
        {limit - 1, limit - 1, -(limit - 1), 1, false},
        {limit - 1, -(limit - 1), -(limit - 1), limit - 1, true},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        transform_t transform;
        transform_init(&transform);
        assert(transform_translate(&transform, cases[i].start_x, cases[i].start_y));
        bool accepted = transform_translate(&transform, cases[i].dx, cases[i].dy);
        if (accepted != cases[i].accepted) {
            fprintf(stderr, "translate (%d, %d) by (%d, %d): %s, expected %s\n", cases[i].start_x, cases[i].start_y,
                    cases[i].dx, cases[i].dy, accepted ? "accepted" : "refused",
                    cases[i].accepted ? "accepted" : "refused");
        }
        assert(accepted == cases[i].accepted);
        if (accepted) {
            assert_offset(&transform, cases[i].start_x + cases[i].dx, cases[i].start_y + cases[i].dy);
        } else {
            assert_offset(&transform, cases[i].start_x, cases[i].start_y);
        }
    }
}

static void test_a_reset_zeroes_the_offset_and_keeps_the_stack(void) {
    transform_t transform;
    transform_init(&transform);
    assert(transform_translate(&transform, 7, 9));
    assert(transform_push(&transform));
    assert(transform_translate(&transform, 2, 3));
    transform_reset(&transform);
    assert_offset(&transform, 0, 0);
    assert(transform_get_depth(&transform) == 1);
    assert(transform_pop(&transform));
    assert_offset(&transform, 7, 9);
}

int main(void) {
    test_a_new_transform_has_no_offset_and_nothing_saved();
    test_translations_add();
    test_a_pop_undoes_the_translations_since_its_push();
    test_nested_pushes_restore_in_order();
    test_the_hundred_and_twenty_ninth_push_is_refused_and_changes_nothing();
    test_a_pop_with_nothing_pushed_is_refused_and_changes_nothing();
    test_an_offset_reaching_two_to_the_twenty_is_refused_and_changes_nothing();
    test_a_reset_zeroes_the_offset_and_keeps_the_stack();
    printf("test_fake_love_transform: all tests passed\n");
    return 0;
}
