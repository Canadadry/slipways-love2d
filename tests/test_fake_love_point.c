#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../tools/fake_love/point.h"

static void test_a_pixel_centre_lights_its_pixel(void) {
    assert(point_find_pixel(0.5, 0, 160) == 0);
}

static void test_a_coordinate_lands_where_the_captures_show(void) {
    struct {
        double coordinate;
        int size;
        int pixel;
    } cases[] = {
        {159.5, 160, 159},
        {10.0, 160, 9},
        {160.0, 160, 159},
        {0.0, 160, POINT_OFF},
        {1.0 / 1024, 160, POINT_OFF},
        {3.0 / 1024, 160, 0},
        {159.99, 160, 159},
        {160.0 + 1.0 / 1024, 160, 159},
        {160.0 + 3.0 / 1024, 160, POINT_OFF},
        {160.4, 160, POINT_OFF},
        {-0.25, 160, POINT_OFF},
        {1e6, 160, POINT_OFF},
        {-1e6, 160, POINT_OFF},
        {119.5, 120, 119},
        {120.0, 120, 119},
        {120.0 + 1.0 / 1024, 120, 119},
        {120.0 + 3.0 / 1024, 120, POINT_OFF},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int pixel = point_find_pixel(cases[i].coordinate, 0, cases[i].size);
        if (pixel != cases[i].pixel) {
            fprintf(stderr, "point_find_pixel(%.17g, %d) = %d, real LÖVE lights %d\n",
                    cases[i].coordinate, cases[i].size, pixel, cases[i].pixel);
        }
        assert(pixel == cases[i].pixel);
    }
}

static void test_the_half_step_tie_depends_on_the_position(void) {
    struct {
        int column;
        int size;
        int pixel;
    } cases[] = {
        {0, 160, 0},
        {1, 160, 1},
        {2, 160, 1},
        {5, 160, 5},
        {0, 120, POINT_OFF},
        {1, 120, 0},
        {2, 120, 2},
        {70, 120, 70},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int pixel = point_find_pixel(cases[i].column + 1.0 / 512, 0, cases[i].size);
        if (pixel != cases[i].pixel) {
            fprintf(stderr,
                    "point_find_pixel(%d + 1/512, %d) = %d, real LÖVE lights %d: "
                    "the compiler probably fused the projection's product and sum, "
                    "or split the viewport's fmaf\n",
                    cases[i].column, cases[i].size, pixel, cases[i].pixel);
        }
        assert(pixel == cases[i].pixel);
    }
}

static void test_a_translated_coordinate_is_a_single_precision_sum(void) {
    struct {
        int offset;
        double coordinate;
        int pixel;
    } cases[] = {
        {37, -33.998045563697815, 2},
        {-91, 95.001953840255737, 3},
        {3, 0.0019544362994565745, 3},
        {100, -95.998046159744263, 3},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        int pixel = point_find_pixel(cases[i].coordinate, cases[i].offset, 160);
        if (pixel != cases[i].pixel) {
            fprintf(stderr,
                    "point_find_pixel(%.17g, %d, 160) = %d, real LÖVE lights %d: "
                    "the offset was probably added in double precision\n",
                    cases[i].coordinate, cases[i].offset, pixel, cases[i].pixel);
        }
        assert(pixel == cases[i].pixel);
    }
}

static void test_finite_coordinates_below_two_to_the_twenty_are_accepted(void) {
    assert(point_accepts_coordinate(0.0));
    assert(point_accepts_coordinate(1048575.0));
    assert(point_accepts_coordinate(-1048575.0));
    assert(point_accepts_coordinate(1048575.999));
}

static void test_coordinates_llvmpipe_could_wrap_are_refused(void) {
    assert(!point_accepts_coordinate(1048576.0));
    assert(!point_accepts_coordinate(-1048576.0));
    assert(!point_accepts_coordinate(1e300));
    assert(!point_accepts_coordinate(NAN));
    assert(!point_accepts_coordinate(INFINITY));
    assert(!point_accepts_coordinate(-INFINITY));
}

static void test_a_translated_coordinate_stays_below_two_to_the_twenty(void) {
    assert(point_accepts_translated(10.5, 0));
    assert(point_accepts_translated(-1048575.0, 1048575));
    assert(point_accepts_translated(1048575.0, -1048575));
    assert(point_accepts_translated(1048574.5, 1));
    assert(point_accepts_translated(-1048574.5, -1));
    assert(!point_accepts_translated(1048575.0, 1));
    assert(!point_accepts_translated(-1048575.0, -1));
    assert(!point_accepts_translated(0.0, 1048576));
    assert(!point_accepts_translated(1048575.5, 1));
    assert(!point_accepts_translated(1048576.0, -10));
    assert(!point_accepts_translated(NAN, 0));
    assert(!point_accepts_translated(INFINITY, -5));
}

static void test_an_integer_span_fills_from_its_start_to_before_its_end(void) {
    int first = -1;
    int last = -1;
    assert(point_find_span(10.0, 5.0, 0, 160, &first, &last));
    assert(first == 10 && last == 14);
}

#define SPAN_NONE (-1)

struct span_case {
    double start;
    double extent;
    int offset;
    int size;
    int first;
    int last;
};

static bool span_matches(struct span_case c, const char *hint) {
    int first = SPAN_NONE;
    int last = SPAN_NONE;
    if (!point_find_span(c.start, c.extent, c.offset, c.size, &first, &last)) {
        first = SPAN_NONE;
        last = SPAN_NONE;
    }
    if (first == c.first && last == c.last) {
        return true;
    }
    fprintf(stderr, "point_find_span(%.17g, %.17g, %d, %d) = %d ... %d, real LÖVE fills %d ... %d%s\n",
            c.start, c.extent, c.offset, c.size, first, last, c.first, c.last, hint);
    return false;
}

static void test_a_span_fills_what_the_captures_show(void) {
    struct span_case cases[] = {
        {10.0, 5.0, 0, 160, 10, 14},
        {25.0, -5.0, 0, 160, 20, 24},
        {10.0, 0.0, 0, 160, SPAN_NONE, SPAN_NONE},
        {20.5, 5.0, 0, 160, 20, 24},
        {30.25, 5.0, 0, 160, 30, 34},
        {40.75, 5.0, 0, 160, 41, 45},
        {70.0, 0.4, 0, 160, SPAN_NONE, SPAN_NONE},
        {75.3, 0.4, 0, 160, 75, 75},
        {0.0, 1.0, 0, 160, 0, 0},
        {159.0, 1.0, 0, 160, 159, 159},
        {119.0, 1.0, 0, 120, 119, 119},
        {119.5, 1.0, 0, 120, 119, 119},
        {119.51, 1.0, 0, 120, SPAN_NONE, SPAN_NONE},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ""));
    }
}

static void test_a_span_is_clipped_to_the_canvas(void) {
    const double far = 1048575.0;
    struct span_case cases[] = {
        {-5.0, 10.0, 0, 160, 0, 4},
        {155.0, 10.0, 0, 160, 155, 159},
        {-0.5, 1.0, 0, 160, SPAN_NONE, SPAN_NONE},
        {-0.49, 1.0, 0, 160, 0, 0},
        {159.5, 1.0, 0, 160, 159, 159},
        {159.51, 1.0, 0, 160, SPAN_NONE, SPAN_NONE},
        {-far, far + 10.0, 0, 160, 0, 9},
        {150.0, far, 0, 160, 150, 159},
        {far, -far - 5.0, 0, 160, 0, 159},
        {1e6, 5.0, 0, 160, SPAN_NONE, SPAN_NONE},
        {-1.6, 1.0, 0, 160, SPAN_NONE, SPAN_NONE},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ""));
    }
}

static void test_the_low_edge_tie_depends_on_the_position(void) {
    struct {
        int column;
        int size;
        int first;
    } cases[] = {
        {0, 160, 0},
        {2, 160, 3},
        {5, 160, 5},
        {7, 160, 8},
        {0, 120, 0},
        {1, 120, 2},
        {3, 120, 3},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        struct span_case c = {cases[i].column + 0.5 + 1.0 / 512, 1.75, 0, cases[i].size,
                              cases[i].first, cases[i].column + 1};
        assert(span_matches(c, ": the compiler probably fused the projection's product and sum, "
                               "or split the viewport's fmaf"));
    }
}

static void test_the_far_edge_is_a_single_precision_sum(void) {
    const double extent = 0.2 + 1.0 / 512 + 1e-6;
    struct span_case cases[] = {
        {16.3, extent, 0, 160, SPAN_NONE, SPAN_NONE},
        {1.3, extent, 0, 160, 1, 1},
        {17.3, extent, 0, 120, SPAN_NONE, SPAN_NONE},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ": the far edge was probably added in double precision"));
    }
}

static void test_a_translated_far_edge_adds_the_extent_before_the_offset(void) {
    struct span_case cases[] = {
        {32.157488867645952, 81.344477964683847, 37, 160, 69, 150},
        {9.4861900185299888, -29.984235350377077, 37, 160, 16, 45},
        {15.092026511109584, -11.590072818471622, 3, 160, 7, 17},
        {20.130568780484534, 119.37138868644311, -91, 160, 0, 47},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ": the far edge was probably (x + offset) + extent, or a double sum"));
    }
}

static const int INTEGER_OFFSETS[] = {0, 1, -1, 3, -40, 37, 1048400, -1048400};

static void test_an_integer_offset_moves_a_pixel_centre_by_that_many_pixels(void) {
    const int sizes[] = {160, 120};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        for (size_t o = 0; o < sizeof(INTEGER_OFFSETS) / sizeof(INTEGER_OFFSETS[0]); o++) {
            int offset = INTEGER_OFFSETS[o];
            for (int target = -3; target < sizes[s] + 3; target++) {
                int pixel = point_find_pixel(target - offset + 0.5, offset, sizes[s]);
                int expected = target >= 0 && target < sizes[s] ? target : POINT_OFF;
                if (pixel != expected) {
                    fprintf(stderr, "point_find_pixel(%d + 0.5, %d, %d) = %d, expected %d\n", target - offset, offset,
                            sizes[s], pixel, expected);
                }
                assert(pixel == expected);
            }
        }
    }
}

static void test_an_integer_offset_moves_an_integer_span_by_that_many_pixels(void) {
    const int sizes[] = {160, 120};
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        for (size_t o = 0; o < sizeof(INTEGER_OFFSETS) / sizeof(INTEGER_OFFSETS[0]); o++) {
            int offset = INTEGER_OFFSETS[o];
            for (int target = -6; target < sizes[s] + 3; target++) {
                int low = target < 0 ? 0 : target;
                int high = target + 4 > sizes[s] ? sizes[s] - 1 : target + 3;
                struct span_case forward = {target - offset, 4.0, offset, sizes[s], low, high};
                struct span_case backward = {target - offset + 4, -4.0, offset, sizes[s], low, high};
                if (low > high) {
                    forward.first = forward.last = backward.first = backward.last = SPAN_NONE;
                }
                assert(span_matches(forward, ""));
                assert(span_matches(backward, ""));
            }
        }
    }
}

int main(void) {
    test_a_pixel_centre_lights_its_pixel();
    test_a_coordinate_lands_where_the_captures_show();
    test_the_half_step_tie_depends_on_the_position();
    test_a_translated_coordinate_is_a_single_precision_sum();
    test_finite_coordinates_below_two_to_the_twenty_are_accepted();
    test_coordinates_llvmpipe_could_wrap_are_refused();
    test_a_translated_coordinate_stays_below_two_to_the_twenty();
    test_an_integer_span_fills_from_its_start_to_before_its_end();
    test_a_span_fills_what_the_captures_show();
    test_a_span_is_clipped_to_the_canvas();
    test_the_low_edge_tie_depends_on_the_position();
    test_the_far_edge_is_a_single_precision_sum();
    test_a_translated_far_edge_adds_the_extent_before_the_offset();
    test_an_integer_offset_moves_a_pixel_centre_by_that_many_pixels();
    test_an_integer_offset_moves_an_integer_span_by_that_many_pixels();
    printf("test_fake_love_point: all tests passed\n");
    return 0;
}
