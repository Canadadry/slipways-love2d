#include "devtools/png_writer.h"
#include "png_reader.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_size_equals_the_bytes_written(void) {
    const unsigned char rgb[3] = { 0, 0, 0 };
    unsigned char out[128];
    size_t size = png_writer_size(1, 1);
    assert(size > 0 && size <= sizeof out);
    assert(png_writer_encode(rgb, 1, 1, out, sizeof out) == size);
}

/* Checked once outside the repository: Python's zlib.decompress and
   macOS sips decode it as one black RGB pixel. */
static const unsigned char ONE_BLACK_PIXEL[72] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x02, 0x00, 0x00, 0x00, 0x90, 0x77, 0x53, 0xde, 0x00, 0x00, 0x00,
    0x0f, 0x49, 0x44, 0x41, 0x54, 0x78, 0x01, 0x01, 0x04, 0x00, 0xfb, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x01, 0x65, 0x49, 0xc3, 0x60,
    0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82,
};

static void test_one_black_pixel_is_the_known_file(void) {
    const unsigned char rgb[3] = { 0, 0, 0 };
    unsigned char out[sizeof ONE_BLACK_PIXEL];
    assert(png_writer_encode(rgb, 1, 1, out, sizeof out) == sizeof ONE_BLACK_PIXEL);
    assert(memcmp(out, ONE_BLACK_PIXEL, sizeof ONE_BLACK_PIXEL) == 0);
}

static void test_writes_nothing_into_a_buffer_one_byte_short(void) {
    const unsigned char rgb[3] = { 0, 0, 0 };
    unsigned char out[sizeof ONE_BLACK_PIXEL];
    memset(out, 0xaa, sizeof out);
    assert(png_writer_encode(rgb, 1, 1, out, sizeof out - 1) == 0);
    for (size_t i = 0; i < sizeof out; i++) assert(out[i] == 0xaa);
}

static void test_refuses_a_dimension_that_is_not_positive(void) {
    const unsigned char rgb[3] = { 0, 0, 0 };
    unsigned char out[sizeof ONE_BLACK_PIXEL];
    assert(png_writer_size(0, 1) == 0);
    assert(png_writer_size(1, -1) == 0);
    assert(png_writer_encode(rgb, 0, 1, out, sizeof out) == 0);
    assert(png_writer_encode(rgb, 1, 0, out, sizeof out) == 0);
}

enum { WIDE_WIDTH = 200, WIDE_HEIGHT = 120 };

static unsigned char wide_rgb[WIDE_WIDTH * WIDE_HEIGHT * 3];
static unsigned char wide_png[WIDE_WIDTH * WIDE_HEIGHT * 3 + 1024];
static unsigned char wide_decoded[WIDE_WIDTH * WIDE_HEIGHT * 3];

static size_t encode_wide_image(void) {
    for (size_t i = 0; i < sizeof wide_rgb; i++) wide_rgb[i] = (unsigned char)(i * 7 + i / 600);
    size_t size = png_writer_encode(wide_rgb, WIDE_WIDTH, WIDE_HEIGHT, wide_png, sizeof wide_png);
    assert(size > 0);
    return size;
}

static void test_an_image_over_one_block_round_trips(void) {
    size_t raw = WIDE_HEIGHT * (1 + 3 * WIDE_WIDTH);
    assert(raw > 65535 && raw <= 2 * 65535);
    size_t two_blocks = 8 + 25 + 12 + 2 + 2 * 5 + raw + 4 + 12;
    assert(png_writer_size(WIDE_WIDTH, WIDE_HEIGHT) == two_blocks);
    size_t size = encode_wide_image();
    int width = 0;
    int height = 0;
    const char *error = png_reader_decode(wide_png, size, wide_decoded, sizeof wide_decoded, &width, &height);
    if (error != NULL) fprintf(stderr, "png_reader: %s\n", error);
    assert(error == NULL);
    assert(width == WIDE_WIDTH && height == WIDE_HEIGHT);
    assert(memcmp(wide_decoded, wide_rgb, sizeof wide_rgb) == 0);
}

/* Raw size exactly 65535: the only block must still carry BFINAL. */
static void test_an_image_filling_exactly_one_block_round_trips(void) {
    enum { WIDTH = 428, HEIGHT = 51 };
    assert(HEIGHT * (1 + 3 * WIDTH) == 65535);
    for (size_t i = 0; i < WIDTH * HEIGHT * 3; i++) wide_rgb[i] = (unsigned char)(i * 5 + i / 300);
    size_t size = png_writer_encode(wide_rgb, WIDTH, HEIGHT, wide_png, sizeof wide_png);
    assert(size == png_writer_size(WIDTH, HEIGHT));
    int width = 0;
    int height = 0;
    const char *error = png_reader_decode(wide_png, size, wide_decoded, sizeof wide_decoded, &width, &height);
    if (error != NULL) fprintf(stderr, "png_reader: %s\n", error);
    assert(error == NULL);
    assert(width == WIDTH && height == HEIGHT);
    assert(memcmp(wide_decoded, wide_rgb, WIDTH * HEIGHT * 3) == 0);
}

static const char *decode_one_pixel(const unsigned char *bytes, size_t size) {
    unsigned char rgb[3];
    int width = 0;
    int height = 0;
    return png_reader_decode(bytes, size, rgb, sizeof rgb, &width, &height);
}

static void test_the_reader_decodes_the_known_file(void) {
    unsigned char rgb[3] = { 1, 1, 1 };
    int width = 0;
    int height = 0;
    assert(png_reader_decode(ONE_BLACK_PIXEL, sizeof ONE_BLACK_PIXEL, rgb, sizeof rgb, &width, &height) == NULL);
    assert(width == 1 && height == 1);
    assert(rgb[0] == 0 && rgb[1] == 0 && rgb[2] == 0);
}

static void test_the_reader_refuses_a_flipped_crc_byte(void) {
    unsigned char bytes[sizeof ONE_BLACK_PIXEL];
    memcpy(bytes, ONE_BLACK_PIXEL, sizeof bytes);
    bytes[29] ^= 0x01;
    assert(strcmp(decode_one_pixel(bytes, sizeof bytes), "chunk CRC mismatch") == 0);
}

/* The IDAT CRC recomputed by Python's zlib.crc32 after the flip, so that
   only the Adler-32 is wrong; zlib.decompress refuses it. */
static void test_the_reader_refuses_a_flipped_adler_byte(void) {
    static const unsigned char adler_and_crc[8] = { 0x00, 0x04, 0x00, 0x00, 0x12, 0x4e, 0xf3, 0xf6 };
    unsigned char bytes[sizeof ONE_BLACK_PIXEL];
    memcpy(bytes, ONE_BLACK_PIXEL, sizeof bytes);
    memcpy(bytes + 52, adler_and_crc, sizeof adler_and_crc);
    assert(strcmp(decode_one_pixel(bytes, sizeof bytes), "Adler-32 mismatch") == 0);
}

static void test_the_reader_refuses_bytes_after_iend(void) {
    unsigned char bytes[sizeof ONE_BLACK_PIXEL + 1];
    memcpy(bytes, ONE_BLACK_PIXEL, sizeof ONE_BLACK_PIXEL);
    bytes[sizeof ONE_BLACK_PIXEL] = 0;
    assert(strcmp(decode_one_pixel(bytes, sizeof bytes), "bytes after IEND") == 0);
}

int main(void) {
    test_size_equals_the_bytes_written();
    test_one_black_pixel_is_the_known_file();
    test_writes_nothing_into_a_buffer_one_byte_short();
    test_refuses_a_dimension_that_is_not_positive();
    test_an_image_over_one_block_round_trips();
    test_an_image_filling_exactly_one_block_round_trips();
    test_the_reader_decodes_the_known_file();
    test_the_reader_refuses_a_flipped_crc_byte();
    test_the_reader_refuses_a_flipped_adler_byte();
    test_the_reader_refuses_bytes_after_iend();
    puts("test_png_writer: OK");
    return 0;
}
