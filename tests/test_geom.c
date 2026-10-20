#include "engine/geom.h"
#include "devtools/fixed_test_double.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_side_positive_side(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(1), p8_fixed_from_int(0));
    vec_t c = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(-1));
    assert(geom_side_of_line(a, b, c) == SIDE_POSITIVE);
}

static void test_side_negative_side(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(1), p8_fixed_from_int(0));
    vec_t c = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(1));
    assert(geom_side_of_line(a, b, c) == SIDE_NEGATIVE);
}

static void test_side_on_line_is_degenerate_collinear(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(1), p8_fixed_from_int(0));
    vec_t c = vec_make(p8_fixed_from_int(2), p8_fixed_from_int(0));
    assert(geom_side_of_line(a, b, c) == SIDE_ON_LINE);
}

static void test_segments_cross_clear_crossing_case(void) {
    vec_t p1 = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t p2 = vec_make(p8_fixed_from_int(2), p8_fixed_from_int(2));
    vec_t q1 = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(2));
    vec_t q2 = vec_make(p8_fixed_from_int(2), p8_fixed_from_int(0));
    assert(geom_segments_cross(p1, p2, q1, q2));
}

static void test_segments_cross_clear_non_crossing_case(void) {
    vec_t p1 = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t p2 = vec_make(p8_fixed_from_int(1), p8_fixed_from_int(0));
    vec_t q1 = vec_make(p8_fixed_from_int(5), p8_fixed_from_int(5));
    vec_t q2 = vec_make(p8_fixed_from_int(6), p8_fixed_from_int(6));
    assert(!geom_segments_cross(p1, p2, q1, q2));
}

static void test_near_segment_within_margin_and_within_distance_is_true(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(10), p8_fixed_from_int(0));
    vec_t c = vec_make(fixed_test_double_from_double(0.5), p8_fixed_from_int(0));
    p8_fixed_t fh = fixed_test_double_from_double(1.0);
    assert(geom_near_segment(a, b, c, fh));
}

static void test_near_segment_outside_margin_near_start_is_false(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(10), p8_fixed_from_int(0));
    vec_t c = vec_make(fixed_test_double_from_double(0.05), p8_fixed_from_int(0));
    p8_fixed_t fh = fixed_test_double_from_double(1.0);
    assert(!geom_near_segment(a, b, c, fh));
}

static void test_near_segment_outside_margin_near_end_is_false(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(10), p8_fixed_from_int(0));
    vec_t c = vec_make(fixed_test_double_from_double(9.95), p8_fixed_from_int(0));
    p8_fixed_t fh = fixed_test_double_from_double(1.0);
    assert(!geom_near_segment(a, b, c, fh));
}

static void test_near_segment_within_margin_but_outside_distance_is_false(void) {
    vec_t a = vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
    vec_t b = vec_make(p8_fixed_from_int(10), p8_fixed_from_int(0));
    vec_t c = vec_make(p8_fixed_from_int(5), p8_fixed_from_int(3));
    p8_fixed_t fh = fixed_test_double_from_double(4.0);
    assert(!geom_near_segment(a, b, c, fh));
}

static int reference_near_segment(vec_t a, vec_t b, vec_t c, p8_fixed_t fh) {
    vec_t d = vec_sub(b, a);
    p8_fixed_t l = p8_fixed_div(vec_dot(d, vec_sub(c, a)), vec_length_squared(d));
    vec_t projected = vec_sub(vec_add(a, vec_mul(d, l)), c);
    p8_fixed_t distance_squared = vec_length_squared(projected);
    return l > 0x0000028f && l < 0x0000fd70 && distance_squared < fh;
}

static uint32_t g_near_seed = 0x6e36u;

static uint32_t near_random(uint32_t below) {
    g_near_seed = g_near_seed * 1664525u + 1013904223u;
    return (g_near_seed >> 8) % below;
}

static p8_fixed_t near_offset(uint32_t range) {
    return (p8_fixed_t)near_random(2u * range + 1u) - (p8_fixed_t)range;
}

/* c at a box edge, plus or minus 1/8 map unit, plus a few ulps either way. */
static p8_fixed_t near_box_edge(p8_fixed_t a, p8_fixed_t b) {
    p8_fixed_t edge = near_random(2u) ? (a < b ? a : b) : (a < b ? b : a);
    p8_fixed_t margin = (p8_fixed_t)near_random(3u) * 0x2000 - 0x2000;
    return edge + margin + near_offset(8u);
}

static void near_random_case(vec_t *a, vec_t *b, vec_t *c) {
    switch (near_random(6u)) {
    case 0: /* game-like: 0..8 map units, c near the segment */
        *a = vec_make((p8_fixed_t)near_random(0x80000u), (p8_fixed_t)near_random(0x80000u));
        *b = vec_make((p8_fixed_t)near_random(0x80000u), (p8_fixed_t)near_random(0x80000u));
        *c = vec_make(a->x + p8_fixed_mul(b->x - a->x, (p8_fixed_t)near_random(0x10000u)) + near_offset(0x3000u),
                      a->y + p8_fixed_mul(b->y - a->y, (p8_fixed_t)near_random(0x10000u)) + near_offset(0x3000u));
        break;
    case 1: /* game-like, c at the box's edges */
        *a = vec_make((p8_fixed_t)near_random(0x80000u), (p8_fixed_t)near_random(0x80000u));
        *b = vec_make((p8_fixed_t)near_random(0x80000u), (p8_fixed_t)near_random(0x80000u));
        *c = vec_make(near_box_edge(a->x, b->x), near_box_edge(a->y, b->y));
        if (near_random(2u)) {
            c->y = a->y + p8_fixed_mul(b->y - a->y, (p8_fixed_t)near_random(0x10000u));
        }
        break;
    case 2: /* degenerate: a == b, a few ulps long, horizontal or vertical */
        *a = vec_make((p8_fixed_t)near_random(0x80000u), (p8_fixed_t)near_random(0x80000u));
        *b = *a;
        switch (near_random(3u)) {
        case 0: b->x += near_offset(4u); b->y += near_offset(4u); break;
        case 1: b->x += near_offset(0x40000u); break;
        default: b->y += near_offset(0x40000u); break;
        }
        *c = vec_make(near_box_edge(a->x, b->x), a->y + near_offset(0x3000u));
        if (near_random(2u)) {
            *c = vec_make(a->x + near_offset(0x3000u), near_box_edge(a->y, b->y));
        }
        break;
    case 3: /* just inside and just outside 32 map units */
        *a = vec_make(0x200000 - (p8_fixed_t)near_random(0x40u) + 0x20, (p8_fixed_t)near_random(0x80000u));
        *b = vec_make(-0x200000 + (p8_fixed_t)near_random(0x40u) - 0x20, a->y + near_offset(0x2000u));
        if (near_random(2u)) {
            p8_fixed_t t = a->x;
            a->x = a->y;
            a->y = t;
            t = b->x;
            b->x = b->y;
            b->y = t;
        }
        *c = vec_make(near_box_edge(a->x, b->x), near_box_edge(a->y, b->y));
        break;
    case 4: /* raw values near the int32 edges */
        *a = vec_make((p8_fixed_t)(0x7fff0000u + near_random(0x20000u)), (p8_fixed_t)(0x7fff0000u + near_random(0x20000u)));
        *b = vec_make((p8_fixed_t)(0x7fff0000u + near_random(0x20000u)), (p8_fixed_t)(0x7fff0000u + near_random(0x20000u)));
        *c = vec_make((p8_fixed_t)(0x7fff0000u + near_random(0x20000u)), (p8_fixed_t)(0x7fff0000u + near_random(0x20000u)));
        break;
    default: /* anywhere, c near a */
        *a = vec_make((p8_fixed_t)(near_random(0x1000000u) << 8), (p8_fixed_t)(near_random(0x1000000u) << 8));
        *b = vec_make((p8_fixed_t)(near_random(0x1000000u) << 8), (p8_fixed_t)(near_random(0x1000000u) << 8));
        *c = vec_make((p8_fixed_t)((uint32_t)a->x + (uint32_t)near_offset(0x100000u)),
                      (p8_fixed_t)((uint32_t)a->y + (uint32_t)near_offset(0x100000u)));
        break;
    }
}

static void test_near_segment_answers_what_the_full_projection_answers(void) {
    static const p8_fixed_t thresholds[] = {0xd1, 0x100, 0x101, 0x10000, 0x40000};
    int i;
    int near = 0;
    for (i = 0; i < 6000; i++) {
        vec_t a;
        vec_t b;
        vec_t c;
        p8_fixed_t fh = thresholds[near_random(5u)];
        int expected;
        near_random_case(&a, &b, &c);
        expected = reference_near_segment(a, b, c, fh);
        near += expected;
        assert(geom_near_segment(a, b, c, fh) == expected);
    }
    assert(near > 300 && "the random cases include matches");
}

static void test_near_segment_keeps_a_match_at_the_farthest_point_from_the_box(void) {
    vec_t a = vec_make(0x10000, 0x10000);
    vec_t b = vec_make(0x10000, 0x30000);
    vec_t right = vec_make(0x10000 + 0x0fff, 0x20000);
    vec_t left = vec_make(0x10000 - 0x0fff, 0x20000);
    vec_t beyond = vec_make(0x10000 + 0x1001, 0x20000);
    assert(geom_near_segment(a, b, right, 0x100));
    assert(geom_near_segment(a, b, left, 0x100));
    assert(geom_near_segment(vec_make(a.y, a.x), vec_make(b.y, b.x), vec_make(right.y, right.x), 0x100));
    assert(!geom_near_segment(a, b, beyond, 0x100));
}

static void test_near_segment_keeps_a_wrapped_match_outside_32_map_units(void) {
    vec_t a = vec_make((p8_fixed_t)0xf5a32fe3u, (p8_fixed_t)0x67f7d3e6u);
    vec_t b = vec_make((p8_fixed_t)0xf3e85a0du, (p8_fixed_t)0xfe61b408u);
    vec_t c = vec_make((p8_fixed_t)0xf5ae3f74u, (p8_fixed_t)0x680357a2u);
    assert(c.x - a.x > 0x2000 && "c lies more than 1/8 map unit right of the segment's box");
    assert(reference_near_segment(a, b, c, 0xd1) && "a product wraps and the full projection answers near");
    assert(geom_near_segment(a, b, c, 0xd1));
}

static void test_box_contains_includes_its_edges_and_excludes_outside_points(void) {
    box_t box = {p8_fixed_from_int(0), p8_fixed_from_int(-1), p8_fixed_from_int(20), p8_fixed_from_int(11)};

    assert(box_contains(box, vec_make(p8_fixed_from_int(5), p8_fixed_from_int(5))));
    assert(box_contains(box, vec_make(p8_fixed_from_int(20), p8_fixed_from_int(-1))));
    assert(!box_contains(box, vec_make(p8_fixed_from_int(21), p8_fixed_from_int(5))));
    assert(!box_contains(box, vec_make(p8_fixed_from_int(5), p8_fixed_from_int(12))));
}

int main(void) {
    test_side_positive_side();
    test_side_negative_side();
    test_side_on_line_is_degenerate_collinear();
    test_segments_cross_clear_crossing_case();
    test_segments_cross_clear_non_crossing_case();
    test_near_segment_within_margin_and_within_distance_is_true();
    test_near_segment_outside_margin_near_start_is_false();
    test_near_segment_outside_margin_near_end_is_false();
    test_near_segment_within_margin_but_outside_distance_is_false();
    test_near_segment_answers_what_the_full_projection_answers();
    test_near_segment_keeps_a_match_at_the_farthest_point_from_the_box();
    test_near_segment_keeps_a_wrapped_match_outside_32_map_units();
    test_box_contains_includes_its_edges_and_excludes_outside_points();

    printf("test_geom: all tests passed\n");
    return 0;
}
