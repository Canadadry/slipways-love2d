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

int main(void) {
    test_a_pixel_centre_lights_its_pixel();
    test_a_coordinate_lands_where_the_captures_show();
    test_the_half_step_tie_depends_on_the_position();
    test_finite_coordinates_below_two_to_the_twenty_are_accepted();
    test_coordinates_llvmpipe_could_wrap_are_refused();
    printf("test_fake_love_point: all tests passed\n");
    return 0;
}
