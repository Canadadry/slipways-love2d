#include <assert.h>
#include <stdio.h>

#include "../tools/fake_love/frame_capture.h"

static const unsigned char palette[16][3] = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
};

static void test_each_opaque_palette_colour_is_its_number(void) {
    for (int n = 0; n < 16; n++) {
        unsigned char rgba[4] = {palette[n][0], palette[n][1], palette[n][2], 0xFF};
        int number = frame_capture_palette_number(rgba);
        if (number != n) {
            fprintf(stderr, "%02X%02X%02XFF converts to %d, expected %d\n", rgba[0], rgba[1], rgba[2], number, n);
        }
        assert(number == n);
    }
}

static void assert_foreign(const unsigned char rgba[4]) {
    int number = frame_capture_palette_number(rgba);
    if (number != FRAME_CAPTURE_FOREIGN) {
        fprintf(stderr, "%02X%02X%02X%02X converts to %d, expected foreign\n", rgba[0], rgba[1], rgba[2], rgba[3], number);
    }
    assert(number == FRAME_CAPTURE_FOREIGN);
}

static void test_a_colour_one_byte_off_a_palette_colour_is_foreign(void) {
    for (int n = 0; n < 16; n++) {
        for (int channel = 0; channel < 3; channel++) {
            for (int step = -1; step <= 1; step += 2) {
                int byte = palette[n][channel] + step;
                if (byte < 0 || byte > 0xFF) {
                    continue;
                }
                unsigned char rgba[4] = {palette[n][0], palette[n][1], palette[n][2], 0xFF};
                rgba[channel] = (unsigned char)byte;
                assert_foreign(rgba);
            }
        }
    }
}

static void test_a_palette_colour_with_alpha_00_is_foreign(void) {
    for (int n = 0; n < 16; n++) {
        unsigned char rgba[4] = {palette[n][0], palette[n][1], palette[n][2], 0x00};
        assert_foreign(rgba);
    }
}

static unsigned char pixels[FRAME_CAPTURE_HEIGHT][FRAME_CAPTURE_WIDTH][4];
static unsigned char frame[FRAME_CAPTURE_FRAME_SIZE];

static void fill_pixels(int n) {
    for (int y = 0; y < FRAME_CAPTURE_HEIGHT; y++) {
        for (int x = 0; x < FRAME_CAPTURE_WIDTH; x++) {
            pixels[y][x][0] = palette[n][0];
            pixels[y][x][1] = palette[n][1];
            pixels[y][x][2] = palette[n][2];
            pixels[y][x][3] = 0xFF;
        }
    }
}

static void set_pixel(int x, int y, const unsigned char rgba[4]) {
    for (int channel = 0; channel < 4; channel++) {
        pixels[y][x][channel] = rgba[channel];
    }
}

static void set_palette_pixel(int x, int y, int n) {
    unsigned char rgba[4] = {palette[n][0], palette[n][1], palette[n][2], 0xFF};
    set_pixel(x, y, rgba);
}

static void assert_byte(int index, unsigned char expected) {
    if (frame[index] != expected) {
        fprintf(stderr, "byte %d is %02X, expected %02X\n", index, frame[index], expected);
    }
    assert(frame[index] == expected);
}

static void test_a_frame_packs_two_pixels_per_byte_even_x_low_row_by_row(void) {
    assert(FRAME_CAPTURE_FRAME_SIZE == 9600);
    fill_pixels(0);
    set_palette_pixel(0, 0, 1);
    set_palette_pixel(1, 0, 2);
    set_palette_pixel(158, 0, 3);
    set_palette_pixel(159, 0, 4);
    set_palette_pixel(0, 1, 5);
    set_palette_pixel(1, 1, 6);
    set_palette_pixel(158, 119, 14);
    set_palette_pixel(159, 119, 15);
    frame_capture_foreign_t first = {0};
    int foreign = frame_capture_pack(&pixels[0][0][0], frame, &first);
    assert(foreign == 0);
    assert(!first.found);
    assert_byte(0, 0x21);
    assert_byte(79, 0x43);
    assert_byte(80, 0x65);
    assert_byte(9599, 0xFE);
    int nonzero = 0;
    for (int i = 0; i < FRAME_CAPTURE_FRAME_SIZE; i++) {
        nonzero += frame[i] != 0;
    }
    assert(nonzero == 4);
}

static void test_a_frame_of_each_colour_repeats_its_number_in_both_nibbles(void) {
    for (int n = 0; n < 16; n++) {
        fill_pixels(n);
        frame_capture_foreign_t first = {0};
        assert(frame_capture_pack(&pixels[0][0][0], frame, &first) == 0);
        for (int i = 0; i < FRAME_CAPTURE_FRAME_SIZE; i++) {
            assert_byte(i, (unsigned char)(n | n << 4));
        }
    }
}

static void test_foreign_pixels_are_counted_written_as_0_and_the_first_recorded(void) {
    fill_pixels(7);
    unsigned char dark_red[4] = {0x4C, 0x00, 0x00, 0xFF};
    unsigned char transparent[4] = {0x1D, 0x2B, 0x53, 0x00};
    set_pixel(3, 2, dark_red);
    set_pixel(4, 2, transparent);
    set_pixel(159, 119, dark_red);
    frame_capture_foreign_t first = {0};
    assert(frame_capture_pack(&pixels[0][0][0], frame, &first) == 3);
    assert_byte(2 * 80 + 1, 0x07);
    assert_byte(2 * 80 + 2, 0x70);
    assert_byte(9599, 0x07);
    assert(first.found && !first.no_canvas);
    assert(first.x == 3 && first.y == 2);
    assert(first.rgba[0] == 0x4C && first.rgba[1] == 0x00 && first.rgba[2] == 0x00 && first.rgba[3] == 0xFF);
}

static void test_a_recorded_foreign_pixel_is_kept_by_later_frames(void) {
    fill_pixels(0);
    unsigned char transparent[4] = {0, 0, 0, 0};
    set_pixel(10, 20, transparent);
    frame_capture_foreign_t first = {0};
    assert(frame_capture_pack(&pixels[0][0][0], frame, &first) == 1);
    set_pixel(5, 6, transparent);
    assert(frame_capture_pack(&pixels[0][0][0], frame, &first) == 2);
    assert(first.found && first.x == 10 && first.y == 20);
    assert(frame_capture_pack(NULL, frame, &first) == FRAME_CAPTURE_WIDTH * FRAME_CAPTURE_HEIGHT);
    assert(first.found && !first.no_canvas && first.x == 10 && first.y == 20);
}

static void test_no_canvas_is_a_frame_of_foreign_pixels_written_as_0(void) {
    for (int i = 0; i < FRAME_CAPTURE_FRAME_SIZE; i++) {
        frame[i] = 0xAA;
    }
    frame_capture_foreign_t first = {0};
    assert(frame_capture_pack(NULL, frame, &first) == 19200);
    for (int i = 0; i < FRAME_CAPTURE_FRAME_SIZE; i++) {
        assert_byte(i, 0x00);
    }
    assert(first.found && first.no_canvas);
}

int main(void) {
    test_each_opaque_palette_colour_is_its_number();
    test_a_colour_one_byte_off_a_palette_colour_is_foreign();
    test_a_palette_colour_with_alpha_00_is_foreign();
    test_a_frame_packs_two_pixels_per_byte_even_x_low_row_by_row();
    test_a_frame_of_each_colour_repeats_its_number_in_both_nibbles();
    test_foreign_pixels_are_counted_written_as_0_and_the_first_recorded();
    test_a_recorded_foreign_pixel_is_kept_by_later_frames();
    test_no_canvas_is_a_frame_of_foreign_pixels_written_as_0();
    printf("test_fake_love_frame_capture: all tests passed\n");
    return 0;
}
