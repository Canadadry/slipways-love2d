#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/screen.h"

static void test_an_unrecorded_frame_reads_null(void) {
    screen_t *screen = screen_create();
    assert(screen != NULL);
    assert(screen_read_pixels(screen, NULL, NULL) == NULL);
    assert(screen_begin_frame(screen, 390, 844, false));
    int width = -1;
    int height = -1;
    assert(screen_read_pixels(screen, &width, &height) == NULL);
    assert(width == -1 && height == -1);
    screen_free(screen);
}

#define SOURCE_MAX_TEXELS (CANVAS_WIDTH * CANVAS_HEIGHT)

typedef struct source {
    unsigned char pixels[4 * SOURCE_MAX_TEXELS];
    canvas_source_t view;
} source_t;

/* Texel (i, j) is (i, j, 0x80, FF): every drawn pixel names its texel. */
static void make_named_source(source_t *source, int width, int height) {
    assert(width * height <= SOURCE_MAX_TEXELS && width <= 256 && height <= 256);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            unsigned char *texel = source->pixels + 4 * (j * width + i);
            texel[0] = (unsigned char)i;
            texel[1] = (unsigned char)j;
            texel[2] = 0x80;
            texel[3] = 0xFF;
        }
    }
    source->view = (canvas_source_t){source->pixels, width, height};
}

static unsigned char *make_black_frame(int width, int height) {
    unsigned char *frame = malloc(4 * (size_t)width * (size_t)height);
    assert(frame != NULL);
    static const unsigned char black[4] = {0x00, 0x00, 0x00, 0xFF};
    size_t size = 4 * (size_t)width * (size_t)height;
    memcpy(frame, black, 4);
    for (size_t done = 4; done < size; done *= 2) {
        memcpy(frame + done, frame, done < size - done ? done : size - done);
    }
    return frame;
}

static const unsigned char white[4] = {0xFF, 0xFF, 0xFF, 0xFF};

/* design.md's 8-bit blender multiply, written out from the formula. */
static int blend_multiply(int p, int q) {
    return (p * q + ((p * q) >> 8) + 128) >> 8;
}

/* Independent of the module's tables. Alpha 00 leaves and an untinted FF
 * writes, as the formula gives, without its cost. */
static void draw_expected_quad(unsigned char *frame, int width, int height, const canvas_source_t *source,
                               const canvas_area_t *quad, double x, double y, double scale_x, double scale_y,
                               const unsigned char tint[4]) {
    double x1 = x + quad->width * scale_x;
    double y1 = y + quad->height * scale_y;
    for (int j = 0; j < height; j++) {
        double cy = j + 0.5;
        if (!(y <= cy && cy < y1)) {
            continue;
        }
        for (int i = 0; i < width; i++) {
            double cx = i + 0.5;
            if (!(x <= cx && cx < x1)) {
                continue;
            }
            double column = floor((cx - x) / scale_x);
            double row = floor((cy - y) / scale_y);
            int u = column < 0 ? 0 : column > quad->width - 1 ? quad->width - 1 : (int)column;
            int v = row < 0 ? 0 : row > quad->height - 1 ? quad->height - 1 : (int)row;
            const unsigned char *texel =
                source->pixels + 4 * ((size_t)(quad->row + v) * source->width + quad->column + u);
            unsigned char *pixel = frame + 4 * ((size_t)j * width + i);
            if (texel[3] == 0x00 || (texel[3] == 0xFF && memcmp(tint, white, 4) == 0)) {
                if (texel[3] == 0xFF) {
                    memcpy(pixel, texel, 4);
                }
                continue;
            }
            int tinted[4];
            for (int k = 0; k < 4; k++) {
                tinted[k] = (int)floor(tint[k] * texel[k] / 255.0 + 0.5);
            }
            for (int k = 0; k < 3; k++) {
                int sum = blend_multiply(tinted[k], tinted[3]) + blend_multiply(pixel[k], 255 - tinted[3]);
                pixel[k] = (unsigned char)(sum > 255 ? 255 : sum);
            }
            pixel[3] = 0xFF;
        }
    }
}

static void draw_expected(unsigned char *frame, int width, int height, const canvas_source_t *source, double x,
                          double y, double scale) {
    canvas_area_t whole = {0, 0, source->width, source->height};
    draw_expected_quad(frame, width, height, source, &whole, x, y, scale, scale, white);
}

static void assert_frame(const screen_t *screen, const unsigned char *expected, int width, int height) {
    int screen_width = 0;
    int screen_height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &screen_width, &screen_height);
    assert(pixels != NULL && screen_width == width && screen_height == height);
    if (memcmp(pixels, expected, 4 * (size_t)width * (size_t)height) == 0) {
        return;
    }
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            const unsigned char *got = pixels + 4 * ((size_t)j * width + i);
            const unsigned char *want = expected + 4 * ((size_t)j * width + i);
            if (memcmp(got, want, 4) != 0) {
                fprintf(stderr, "screen pixel (%d, %d) is %02X%02X%02X%02X, expected %02X%02X%02X%02X\n", i, j, got[0],
                        got[1], got[2], got[3], want[0], want[1], want[2], want[3]);
                assert(!"screen pixel differs from the rule");
            }
        }
    }
}

/* Asserts screen pixel (i, j) shows texel (u, v) of a named source, or is
 * black for u < 0. */
static void assert_landmark(const screen_t *screen, int i, int j, int u, int v) {
    int width = 0;
    int height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    const unsigned char *pixel = pixels + 4 * ((size_t)j * width + i);
    unsigned char want[4] = {0x00, 0x00, 0x00, 0xFF};
    if (u >= 0) {
        want[0] = (unsigned char)u;
        want[1] = (unsigned char)v;
        want[2] = 0x80;
    }
    if (memcmp(pixel, want, 4) != 0) {
        fprintf(stderr, "screen pixel (%d, %d) is %02X%02X%02X%02X, landmark %02X%02X%02X%02X\n", i, j, pixel[0],
                pixel[1], pixel[2], pixel[3], want[0], want[1], want[2], want[3]);
    }
    assert(memcmp(pixel, want, 4) == 0);
}

#define BLACK (-1)

/* The present: the whole source, one scale, no tint. */
static void draw_whole(screen_t *screen, const canvas_source_t *source, double x, double y, double scale) {
    canvas_area_t whole = {0, 0, source->width, source->height};
    screen_draw(screen, source, &whole, x, y, scale, scale, white);
}

static source_t named;

/* Draws `named` on a new recorded frame and checks the whole frame; the
 * caller frees the returned screen. */
static screen_t *draw_named_and_check(int width, int height, double x, double y, double scale) {
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, width, height, true));
    draw_whole(screen, &named.view, x, y, scale);
    unsigned char *expected = make_black_frame(width, height);
    draw_expected(expected, width, height, &named.view, x, y, scale);
    assert_frame(screen, expected, width, height);
    free(expected);
    return screen;
}

static void check_one_draw(int width, int height, double x, double y, double scale) {
    screen_free(draw_named_and_check(width, height, x, y, scale));
}

static void test_scale_one_at_an_integer_position_copies_the_canvas(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = draw_named_and_check(200, 150, 3.0, 5.0, 1.0);
    assert_landmark(screen, 3, 5, 0, 0);
    assert_landmark(screen, 2, 5, BLACK, 0);
    assert_landmark(screen, 3, 4, BLACK, 0);
    assert_landmark(screen, 100, 70, 97, 65);
    assert_landmark(screen, 162, 124, 159, 119);
    assert_landmark(screen, 163, 124, BLACK, 0);
    assert_landmark(screen, 162, 125, BLACK, 0);
    screen_free(screen);
}

static void test_the_tall_present_takes_the_texel_right_of_and_below_a_tie(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = draw_named_and_check(390, 844, 0.0, 64.75, 2.4375);
    assert_landmark(screen, 18, 65, 7, 0);
    assert_landmark(screen, 19, 65, 8, 0);
    assert_landmark(screen, 0, 64, BLACK, 0);
    assert_landmark(screen, 0, 74, 0, 4);
    assert_landmark(screen, 389, 356, 159, 119);
    assert_landmark(screen, 0, 357, BLACK, 0);
    screen_free(screen);
}

static void test_a_centre_on_the_left_or_top_edge_is_in_and_on_the_right_or_bottom_edge_out(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = draw_named_and_check(170, 130, 2.5, 3.5, 1.0);
    assert_landmark(screen, 1, 3, BLACK, 0);
    assert_landmark(screen, 2, 3, 0, 0);
    assert_landmark(screen, 2, 2, BLACK, 0);
    assert_landmark(screen, 161, 122, 159, 119);
    assert_landmark(screen, 162, 122, BLACK, 0);
    assert_landmark(screen, 161, 123, BLACK, 0);
    screen_free(screen);
}

static void test_a_present_off_the_window_is_clipped(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = draw_named_and_check(640, 480, -10.5, 7.25, 3.1);
    assert_landmark(screen, 0, 7, 3, 0);
    assert_landmark(screen, 0, 6, BLACK, 0);
    assert_landmark(screen, 484, 378, 159, 119);
    assert_landmark(screen, 485, 378, BLACK, 0);
    assert_landmark(screen, 484, 379, BLACK, 0);
    screen_free(screen);
    check_one_draw(640, 480, 500.25, -100.5, 3.1);
    check_one_draw(640, 480, -1048575.5, 1048575.5, 1024.0);
}

static void test_a_scale_below_one_skips_texels(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = draw_named_and_check(390, 844, 100.5, 50.5, 0.75);
    assert_landmark(screen, 100, 50, 0, 0);
    assert_landmark(screen, 102, 50, 2, 0);
    assert_landmark(screen, 103, 50, 4, 0);
    assert_landmark(screen, 219, 50, 158, 0);
    assert_landmark(screen, 220, 50, BLACK, 0);
    assert_landmark(screen, 100, 139, 0, 118);
    assert_landmark(screen, 100, 140, BLACK, 0);
    screen_free(screen);
    check_one_draw(7, 5, 0.25, 0.25, 0.0390625);
}

/* Draws `quad` of `named` on a new recorded frame and checks the whole
 * frame; the caller frees the returned screen. */
static screen_t *draw_quad_and_check(int width, int height, const canvas_area_t *quad, double x, double y,
                                     double scale_x, double scale_y) {
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, width, height, true));
    screen_draw(screen, &named.view, quad, x, y, scale_x, scale_y, white);
    unsigned char *expected = make_black_frame(width, height);
    draw_expected_quad(expected, width, height, &named.view, quad, x, y, scale_x, scale_y, white);
    assert_frame(screen, expected, width, height);
    free(expected);
    return screen;
}

static void test_a_quad_stretched_by_unequal_fractional_scales_shows_only_its_texels(void) {
    make_named_source(&named, 20, 16);
    canvas_area_t quad = {3, 2, 5, 4};
    screen_t *screen = draw_quad_and_check(60, 40, &quad, 10.0625, 7.6, 2.4375, 1.7);
    assert_landmark(screen, 9, 8, BLACK, 0);
    assert_landmark(screen, 10, 8, 3, 2);
    assert_landmark(screen, 11, 8, 3, 2);
    assert_landmark(screen, 12, 8, 4, 2);
    assert_landmark(screen, 21, 8, 7, 2);
    assert_landmark(screen, 22, 8, BLACK, 0);
    assert_landmark(screen, 10, 7, BLACK, 0);
    assert_landmark(screen, 10, 9, 3, 3);
    assert_landmark(screen, 10, 13, 3, 5);
    assert_landmark(screen, 10, 14, BLACK, 0);
    screen_free(screen);
}

static void test_a_quad_shrunk_on_one_axis_is_clipped_on_the_left_and_the_bottom(void) {
    make_named_source(&named, 20, 16);
    canvas_area_t whole = {0, 0, 20, 16};
    screen_t *screen = draw_quad_and_check(60, 40, &whole, -3.5, 20.5, 0.75, 3.0);
    assert_landmark(screen, 0, 19, BLACK, 0);
    assert_landmark(screen, 0, 20, 5, 0);
    assert_landmark(screen, 1, 20, 6, 0);
    assert_landmark(screen, 10, 20, 18, 0);
    assert_landmark(screen, 11, 20, BLACK, 0);
    assert_landmark(screen, 0, 23, 5, 1);
    assert_landmark(screen, 0, 39, 5, 6);
    screen_free(screen);
}

static void assert_pixel(const screen_t *screen, int i, int j, unsigned int rgba) {
    int width = 0;
    int height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    const unsigned char *pixel = pixels + 4 * ((size_t)j * width + i);
    unsigned int got = (unsigned int)pixel[0] << 24 | (unsigned int)pixel[1] << 16 | (unsigned int)pixel[2] << 8 | pixel[3];
    if (got != rgba) {
        fprintf(stderr, "screen pixel (%d, %d) is %08X, expected %08X\n", i, j, got, rgba);
    }
    assert(got == rgba);
}

static void assert_opaque(const screen_t *screen) {
    int width = 0;
    int height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    for (size_t p = 0; p < (size_t)width * (size_t)height; p++) {
        assert(pixels[4 * p + 3] == 0xFF);
    }
}

static source_t translucent;

static void test_translucent_texels_blend_over_the_earlier_draw(void) {
    static const unsigned char alphas[8] = {0x01, 0x40, 0x7F, 0x80, 0xC0, 0xFE, 0x00, 0xFF};
    for (int k = 0; k < 8; k++) {
        unsigned char *texel = translucent.pixels + 4 * k;
        texel[0] = 0xC8;
        texel[1] = 0x37;
        texel[2] = 0x9A;
        texel[3] = alphas[k];
    }
    translucent.view = (canvas_source_t){translucent.pixels, 8, 1};
    make_named_source(&named, 40, 8);
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, 40, 8, true));
    draw_whole(screen, &named.view, 0.0, 0.0, 1.0);
    draw_whole(screen, &translucent.view, 4.0, 2.0, 3.0);
    unsigned char *expected = make_black_frame(40, 8);
    draw_expected(expected, 40, 8, &named.view, 0.0, 0.0, 1.0);
    draw_expected(expected, 40, 8, &translucent.view, 4.0, 2.0, 3.0);
    assert_frame(screen, expected, 40, 8);
    free(expected);
    assert_pixel(screen, 5, 3, 0x060380FF);
    assert_pixel(screen, 14, 3, 0x6B1D8DFF);
    assert_pixel(screen, 20, 3, 0xC7379AFF);
    assert_landmark(screen, 23, 3, 23, 3);
    assert_pixel(screen, 26, 3, 0xC8379AFF);
    assert_opaque(screen);
    screen_free(screen);
}

static void test_a_translucent_tint_blends_an_opaque_texel(void) {
    make_named_source(&named, 40, 8);
    canvas_area_t quad = {30, 5, 1, 1};
    static const unsigned char tint[4] = {0x7F, 0xFF, 0x3F, 0x80};
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, 10, 10, true));
    screen_draw(screen, &named.view, &quad, 2.0, 2.0, 3.0, 3.0, tint);
    unsigned char *expected = make_black_frame(10, 10);
    draw_expected_quad(expected, 10, 10, &named.view, &quad, 2.0, 2.0, 3.0, 3.0, tint);
    assert_frame(screen, expected, 10, 10);
    free(expected);
    assert_pixel(screen, 3, 3, 0x080310FF);
    assert_pixel(screen, 5, 3, 0x000000FF);
    assert_opaque(screen);
    screen_free(screen);
}

/* Transparent but for texel (i, j) with (i + j) % 7 == 0, which is
 * (i, j, 0x40, FF). */
static void make_sparse_source(source_t *source, int width, int height) {
    make_named_source(source, width, height);
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            unsigned char *texel = source->pixels + 4 * (j * width + i);
            static const unsigned char clear[4] = {0x00, 0x00, 0x00, 0x00};
            if ((i + j) % 7 == 0) {
                texel[2] = 0x40;
            } else {
                memcpy(texel, clear, 4);
            }
        }
    }
}

static source_t sparse;

static void test_alpha_00_leaves_the_earlier_draw_and_a_later_draw_lies_over_it(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    make_sparse_source(&sparse, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, 390, 844, true));
    draw_whole(screen, &named.view, 0.0, 0.0, 1.0);
    draw_whole(screen, &sparse.view, 100.5, 50.5, 0.75);
    unsigned char *expected = make_black_frame(390, 844);
    draw_expected(expected, 390, 844, &named.view, 0.0, 0.0, 1.0);
    draw_expected(expected, 390, 844, &sparse.view, 100.5, 50.5, 0.75);
    assert_frame(screen, expected, 390, 844);
    free(expected);
    int width = 0;
    int height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    static const unsigned char sparse_texel_0_0[4] = {0x00, 0x00, 0x40, 0xFF};
    assert(memcmp(pixels + 4 * (50 * width + 100), sparse_texel_0_0, 4) == 0);
    assert_landmark(screen, 101, 50, 101, 50);
    screen_free(screen);
}

static void assert_all_black(const unsigned char *pixels, int width, int height) {
    unsigned char *black = make_black_frame(width, height);
    assert(memcmp(pixels, black, 4 * (size_t)width * (size_t)height) == 0);
    free(black);
}

static void test_each_recorded_frame_starts_black_at_its_own_size(void) {
    make_named_source(&named, CANVAS_WIDTH, CANVAS_HEIGHT);
    screen_t *screen = screen_create();
    int width = 0;
    int height = 0;
    assert(screen_begin_frame(screen, 390, 844, true));
    draw_whole(screen, &named.view, 0.0, 64.75, 2.4375);
    assert(screen_begin_frame(screen, 640, 480, false));
    draw_whole(screen, &named.view, 0.0, 0.0, 4.0);
    assert(screen_read_pixels(screen, &width, &height) == NULL);
    assert(screen_begin_frame(screen, 640, 480, true));
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    assert(pixels != NULL && width == 640 && height == 480);
    assert_all_black(pixels, width, height);
    draw_whole(screen, &named.view, -10.5, 7.25, 3.1);
    assert(screen_begin_frame(screen, 640, 480, true));
    pixels = screen_read_pixels(screen, &width, &height);
    assert(pixels != NULL && width == 640 && height == 480);
    assert_all_black(pixels, width, height);
    assert(screen_begin_frame(screen, 1, 4096, true));
    pixels = screen_read_pixels(screen, &width, &height);
    assert(pixels != NULL && width == 1 && height == 4096);
    assert_all_black(pixels, width, height);
    screen_free(screen);
}

static void test_a_recorded_frame_starts_opaque_black_at_its_size(void) {
    screen_t *screen = screen_create();
    assert(screen_begin_frame(screen, 390, 844, true));
    int width = 0;
    int height = 0;
    const unsigned char *pixels = screen_read_pixels(screen, &width, &height);
    assert(pixels != NULL && width == 390 && height == 844);
    assert_all_black(pixels, width, height);
    screen_free(screen);
}

int main(void) {
    test_an_unrecorded_frame_reads_null();
    test_a_recorded_frame_starts_opaque_black_at_its_size();
    test_scale_one_at_an_integer_position_copies_the_canvas();
    test_the_tall_present_takes_the_texel_right_of_and_below_a_tie();
    test_a_centre_on_the_left_or_top_edge_is_in_and_on_the_right_or_bottom_edge_out();
    test_a_present_off_the_window_is_clipped();
    test_a_scale_below_one_skips_texels();
    test_alpha_00_leaves_the_earlier_draw_and_a_later_draw_lies_over_it();
    test_a_quad_stretched_by_unequal_fractional_scales_shows_only_its_texels();
    test_a_quad_shrunk_on_one_axis_is_clipped_on_the_left_and_the_bottom();
    test_translucent_texels_blend_over_the_earlier_draw();
    test_a_translucent_tint_blends_an_opaque_texel();
    test_each_recorded_frame_starts_black_at_its_own_size();
    printf("test_fake_love_screen: all tests passed\n");
    return 0;
}
