#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../tools/fake_love/point.h"

static void test_a_pixel_centre_lights_its_pixel(void) {
    assert(point_find_pixel(0.5, 160) == 0);
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
        int pixel = point_find_pixel(cases[i].coordinate, cases[i].size);
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
        int pixel = point_find_pixel(cases[i].column + 1.0 / 512, cases[i].size);
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

static void test_an_integer_span_fills_from_its_start_to_before_its_end(void) {
    int first = -1;
    int last = -1;
    assert(point_find_span(10.0, 5.0, 160, &first, &last));
    assert(first == 10 && last == 14);
}

#define SPAN_NONE (-1)

struct span_case {
    double start;
    double extent;
    int size;
    int first;
    int last;
};

static bool span_matches(struct span_case c, const char *hint) {
    int first = SPAN_NONE;
    int last = SPAN_NONE;
    if (!point_find_span(c.start, c.extent, c.size, &first, &last)) {
        first = SPAN_NONE;
        last = SPAN_NONE;
    }
    if (first == c.first && last == c.last) {
        return true;
    }
    fprintf(stderr, "point_find_span(%.17g, %.17g, %d) = %d ... %d, real LÖVE fills %d ... %d%s\n",
            c.start, c.extent, c.size, first, last, c.first, c.last, hint);
    return false;
}

static void test_a_span_fills_what_the_captures_show(void) {
    struct span_case cases[] = {
        {10.0, 5.0, 160, 10, 14},
        {25.0, -5.0, 160, 20, 24},
        {10.0, 0.0, 160, SPAN_NONE, SPAN_NONE},
        {20.5, 5.0, 160, 20, 24},
        {30.25, 5.0, 160, 30, 34},
        {40.75, 5.0, 160, 41, 45},
        {70.0, 0.4, 160, SPAN_NONE, SPAN_NONE},
        {75.3, 0.4, 160, 75, 75},
        {0.0, 1.0, 160, 0, 0},
        {159.0, 1.0, 160, 159, 159},
        {119.0, 1.0, 120, 119, 119},
        {119.5, 1.0, 120, 119, 119},
        {119.51, 1.0, 120, SPAN_NONE, SPAN_NONE},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ""));
    }
}

static void test_a_span_is_clipped_to_the_canvas(void) {
    const double far = 1048575.0;
    struct span_case cases[] = {
        {-5.0, 10.0, 160, 0, 4},
        {155.0, 10.0, 160, 155, 159},
        {-0.5, 1.0, 160, SPAN_NONE, SPAN_NONE},
        {-0.49, 1.0, 160, 0, 0},
        {159.5, 1.0, 160, 159, 159},
        {159.51, 1.0, 160, SPAN_NONE, SPAN_NONE},
        {-far, far + 10.0, 160, 0, 9},
        {150.0, far, 160, 150, 159},
        {far, -far - 5.0, 160, 0, 159},
        {1e6, 5.0, 160, SPAN_NONE, SPAN_NONE},
        {-1.6, 1.0, 160, SPAN_NONE, SPAN_NONE},
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
        struct span_case c = {cases[i].column + 0.5 + 1.0 / 512, 1.75, cases[i].size,
                              cases[i].first, cases[i].column + 1};
        assert(span_matches(c, ": the compiler probably fused the projection's product and sum, "
                               "or split the viewport's fmaf"));
    }
}

static void test_the_far_edge_is_a_single_precision_sum(void) {
    const double extent = 0.2 + 1.0 / 512 + 1e-6;
    struct span_case cases[] = {
        {16.3, extent, 160, SPAN_NONE, SPAN_NONE},
        {1.3, extent, 160, 1, 1},
        {17.3, extent, 120, SPAN_NONE, SPAN_NONE},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        assert(span_matches(cases[i], ": the far edge was probably added in double precision"));
    }
}

int main(void) {
    test_a_pixel_centre_lights_its_pixel();
    test_a_coordinate_lands_where_the_captures_show();
    test_the_half_step_tie_depends_on_the_position();
    test_finite_coordinates_below_two_to_the_twenty_are_accepted();
    test_coordinates_llvmpipe_could_wrap_are_refused();
    test_an_integer_span_fills_from_its_start_to_before_its_end();
    test_a_span_fills_what_the_captures_show();
    test_a_span_is_clipped_to_the_canvas();
    test_the_low_edge_tie_depends_on_the_position();
    test_the_far_edge_is_a_single_precision_sum();
    printf("test_fake_love_point: all tests passed\n");
    return 0;
}
