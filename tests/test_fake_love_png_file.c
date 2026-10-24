#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tools/fake_love/png_file.h"

#define MAX_SIDE 1024
#define BUFFER_SIZE (1 << 18)

typedef struct {
    unsigned char bytes[BUFFER_SIZE];
    size_t length;
} buffer_t;

static void put_byte(buffer_t *buffer, unsigned value) {
    assert(buffer->length < BUFFER_SIZE);
    buffer->bytes[buffer->length++] = (unsigned char)value;
}

static void put_bytes(buffer_t *buffer, const void *bytes, size_t length) {
    assert(buffer->length + length <= BUFFER_SIZE);
    memcpy(buffer->bytes + buffer->length, bytes, length);
    buffer->length += length;
}

static void put_big_endian(buffer_t *buffer, unsigned long value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        put_byte(buffer, (unsigned)(value >> shift) & 0xFF);
}

static unsigned long crc32_of(const unsigned char *bytes, size_t length) {
    unsigned long crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320UL & (0UL - (crc & 1)));
    }
    return crc ^ 0xFFFFFFFFUL;
}

static unsigned long adler32_of(const unsigned char *bytes, size_t length) {
    unsigned long a = 1, b = 0;
    for (size_t i = 0; i < length; i++) {
        a = (a + bytes[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

static void put_chunk(buffer_t *png, const char type[4], const unsigned char *data, size_t length) {
    put_big_endian(png, length);
    size_t start = png->length;
    put_bytes(png, type, 4);
    put_bytes(png, data, length);
    put_big_endian(png, crc32_of(png->bytes + start, 4 + length));
}

static void put_signature(buffer_t *png) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    put_bytes(png, signature, sizeof signature);
}

static void put_header(buffer_t *png, unsigned long width, unsigned long height, int depth, int colour_type, int interlace) {
    buffer_t *header = calloc(1, sizeof *header);
    assert(header != NULL);
    put_big_endian(header, width);
    put_big_endian(header, height);
    put_byte(header, (unsigned)depth);
    put_byte(header, (unsigned)colour_type);
    put_byte(header, 0);
    put_byte(header, 0);
    put_byte(header, (unsigned)interlace);
    put_chunk(png, "IHDR", header->bytes, header->length);
    free(header);
}

static void put_zlib_stored(buffer_t *zlib, const unsigned char *raw, size_t length) {
    put_byte(zlib, 0x78);
    put_byte(zlib, 0x01);
    size_t offset = 0;
    do {
        size_t block = length - offset < 65535 ? length - offset : 65535;
        put_byte(zlib, offset + block == length ? 1 : 0);
        put_byte(zlib, block & 0xFF);
        put_byte(zlib, block >> 8);
        put_byte(zlib, ~block & 0xFF);
        put_byte(zlib, (~block >> 8) & 0xFF);
        put_bytes(zlib, raw + offset, block);
        offset += block;
    } while (offset < length);
    put_big_endian(zlib, adler32_of(raw, length));
}

typedef struct {
    buffer_t *buffer;
    unsigned long bits;
    int count;
} bit_writer_t;

static void put_bits(bit_writer_t *writer, unsigned value, int width) {
    writer->bits |= (unsigned long)value << writer->count;
    writer->count += width;
    for (; writer->count >= 8; writer->count -= 8) {
        put_byte(writer->buffer, writer->bits & 0xFF);
        writer->bits >>= 8;
    }
}

static void put_huffman_code(bit_writer_t *writer, unsigned code, int width) {
    unsigned reversed = 0;
    for (int bit = 0; bit < width; bit++)
        reversed |= ((code >> bit) & 1) << (width - 1 - bit);
    put_bits(writer, reversed, width);
}

/* One fixed-Huffman block of literals only: real deflate, no matches. */
static void put_zlib_fixed(buffer_t *zlib, const unsigned char *raw, size_t length) {
    put_byte(zlib, 0x78);
    put_byte(zlib, 0x01);
    bit_writer_t writer = {zlib, 0, 0};
    put_bits(&writer, 1, 1);
    put_bits(&writer, 1, 2);
    for (size_t i = 0; i < length; i++) {
        if (raw[i] < 144)
            put_huffman_code(&writer, 0x30 + raw[i], 8);
        else
            put_huffman_code(&writer, 0x190 + raw[i] - 144, 9);
    }
    put_huffman_code(&writer, 0, 7);
    if (writer.count > 0)
        put_bits(&writer, 0, 8 - writer.count);
    put_big_endian(zlib, adler32_of(raw, length));
}

typedef struct {
    int width, height, channels;
    const unsigned char *samples;
} image_t;

static size_t filter_rows(const image_t *image, unsigned char *raw) {
    size_t row = (size_t)image->width * image->channels;
    size_t length = 0;
    for (int y = 0; y < image->height; y++) {
        raw[length++] = 0;
        memcpy(raw + length, image->samples + y * row, row);
        length += row;
    }
    return length;
}

static size_t filter_adam7_passes(const image_t *image, unsigned char *raw) {
    static const int passes[7][4] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
    size_t length = 0;
    for (int pass = 0; pass < 7; pass++) {
        const int *p = passes[pass];
        if (p[0] >= image->width || p[1] >= image->height)
            continue;
        for (int y = p[1]; y < image->height; y += p[3]) {
            raw[length++] = 0;
            for (int x = p[0]; x < image->width; x += p[2]) {
                memcpy(raw + length, image->samples + ((size_t)y * image->width + x) * image->channels, (size_t)image->channels);
                length += (size_t)image->channels;
            }
        }
    }
    return length;
}

typedef enum { STORED, FIXED_HUFFMAN } deflate_t;

static void put_image_data(buffer_t *png, const unsigned char *raw, size_t length, deflate_t deflate, int pieces) {
    static buffer_t zlib;
    zlib.length = 0;
    if (deflate == STORED)
        put_zlib_stored(&zlib, raw, length);
    else
        put_zlib_fixed(&zlib, raw, length);
    size_t offset = 0;
    for (int piece = 1; piece <= pieces; piece++) {
        size_t end = zlib.length * piece / pieces;
        put_chunk(png, "IDAT", zlib.bytes + offset, end - offset);
        offset = end;
    }
}

static int colour_type_of(const image_t *image) {
    return image->channels == 4 ? 6 : 2;
}

/* A plain PNG of `image`, 8-bit, RGB or RGBA by its channels, filter 0. */
static void build_png(buffer_t *png, const image_t *image) {
    static unsigned char raw[BUFFER_SIZE];
    png->length = 0;
    put_signature(png);
    put_header(png, (unsigned long)image->width, (unsigned long)image->height, 8, colour_type_of(image), 0);
    put_image_data(png, raw, filter_rows(image, raw), STORED, 1);
    put_chunk(png, "IEND", NULL, 0);
}

static unsigned char *find_chunk(buffer_t *png, const char type[4]) {
    for (size_t at = 8; at + 12 <= png->length;) {
        unsigned char *chunk = png->bytes + at;
        size_t length = (size_t)chunk[0] << 24 | (size_t)chunk[1] << 16 | (size_t)chunk[2] << 8 | chunk[3];
        if (memcmp(chunk + 4, type, 4) == 0)
            return chunk;
        at += 12 + length;
    }
    assert(!"the chunk to change must exist");
    return NULL;
}

static void corrupt_crc(buffer_t *png, const char type[4]) {
    unsigned char *chunk = find_chunk(png, type);
    size_t length = (size_t)chunk[0] << 24 | (size_t)chunk[1] << 16 | (size_t)chunk[2] << 8 | chunk[3];
    chunk[8 + length + 3] ^= 1;
}

typedef struct {
    const char *reason;
    unsigned char *pixels;
    int width, height;
} decoded_t;

static decoded_t decode(const buffer_t *png) {
    decoded_t decoded = {0};
    decoded.reason = png_file_decode(png->bytes, png->length, MAX_SIDE, &decoded.pixels, &decoded.width, &decoded.height);
    return decoded;
}

static void assert_pixels(const buffer_t *png, int width, int height, const unsigned char *expected) {
    decoded_t decoded = decode(png);
    if (decoded.reason != NULL)
        fprintf(stderr, "refused: %s\n", decoded.reason);
    assert(decoded.reason == NULL);
    assert(decoded.width == width && decoded.height == height);
    for (int i = 0; i < width * height * 4; i++) {
        if (decoded.pixels[i] != expected[i])
            fprintf(stderr, "byte %d: expected %d, got %d\n", i, expected[i], decoded.pixels[i]);
        assert(decoded.pixels[i] == expected[i]);
    }
    free(decoded.pixels);
}

static const unsigned char RGBA_3X2[] = {
    10, 20, 30, 255, 40, 50, 60, 0, 70, 80, 90, 128,
    255, 0, 1, 1, 2, 3, 4, 254, 0, 0, 0, 0,
};

static void test_an_rgba_png_gives_its_bytes_alphas_included(void) {
    static buffer_t png;
    build_png(&png, &(image_t){3, 2, 4, RGBA_3X2});
    assert_pixels(&png, 3, 2, RGBA_3X2);
}

static const unsigned char RGB_3X2[] = {
    10, 20, 30, 200, 150, 100, 0, 0, 0,
    255, 255, 255, 1, 2, 3, 144, 143, 255,
};

static const unsigned char RGB_3X2_AS_RGBA[] = {
    10, 20, 30, 255, 200, 150, 100, 255, 0, 0, 0, 255,
    255, 255, 255, 255, 1, 2, 3, 255, 144, 143, 255, 255,
};

static void test_an_rgb_png_gives_its_bytes_with_opaque_alpha(void) {
    static buffer_t png;
    build_png(&png, &(image_t){3, 2, 3, RGB_3X2});
    assert_pixels(&png, 3, 2, RGB_3X2_AS_RGBA);
}

static void test_real_deflate_gives_the_same_bytes_as_stored(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_image_data(&png, raw, filter_rows(&image, raw), FIXED_HUFFMAN, 1);
    put_chunk(&png, "IEND", NULL, 0);
    assert_pixels(&png, 3, 2, RGB_3X2_AS_RGBA);
}

static void test_a_colour_key_gives_alpha_0_and_keeps_the_rgb(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    static const unsigned char key[6] = {0, 200, 0, 150, 0, 100};
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_chunk(&png, "tRNS", key, sizeof key);
    put_image_data(&png, raw, filter_rows(&image, raw), STORED, 1);
    put_chunk(&png, "IEND", NULL, 0);
    unsigned char expected[sizeof RGB_3X2_AS_RGBA];
    memcpy(expected, RGB_3X2_AS_RGBA, sizeof expected);
    expected[7] = 0;
    assert_pixels(&png, 3, 2, expected);
}

static void test_ancillary_chunks_split_data_and_trailing_bytes_change_nothing(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    static const unsigned char gamma_1[4] = {0, 1, 0x86, 0xA0};
    static const unsigned char palette[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
    static const unsigned char private_data[3] = {1, 2, 3};
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_chunk(&png, "gAMA", gamma_1, sizeof gamma_1);
    put_chunk(&png, "sRGB", (const unsigned char[1]){0}, 1);
    put_chunk(&png, "prVt", private_data, sizeof private_data);
    put_chunk(&png, "PLTE", palette, sizeof palette);
    put_image_data(&png, raw, filter_rows(&image, raw), FIXED_HUFFMAN, 3);
    put_chunk(&png, "tEXt", (const unsigned char *)"a\0b", 3);
    put_chunk(&png, "IEND", NULL, 0);
    put_bytes(&png, "garbage", 7);
    assert_pixels(&png, 3, 2, RGB_3X2_AS_RGBA);
}

static void test_an_interlaced_png_gives_the_same_bytes(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    static unsigned char samples[11 * 9 * 4];
    for (size_t i = 0; i < sizeof samples; i++)
        samples[i] = (unsigned char)(i * 37 + 11);
    const image_t image = {11, 9, 4, samples};
    put_signature(&png);
    put_header(&png, 11, 9, 8, 6, 1);
    put_image_data(&png, raw, filter_adam7_passes(&image, raw), FIXED_HUFFMAN, 1);
    put_chunk(&png, "IEND", NULL, 0);
    assert_pixels(&png, 11, 9, samples);
}

static void test_a_side_of_the_maximum_is_accepted(void) {
    static buffer_t png;
    static unsigned char samples[MAX_SIDE * 4];
    for (size_t i = 0; i < sizeof samples; i++)
        samples[i] = (unsigned char)i;
    build_png(&png, &(image_t){MAX_SIDE, 1, 4, samples});
    assert_pixels(&png, MAX_SIDE, 1, samples);
}

static void read_file(const char *path, buffer_t *buffer) {
    FILE *file = fopen(path, "rb");
    assert(file != NULL && "run from the repository root");
    buffer->length = fread(buffer->bytes, 1, BUFFER_SIZE, file);
    assert(feof(file) && "the file must fit the buffer");
    fclose(file);
}

static void assert_asset_decodes_opaque(const char *path, int width, int height) {
    static buffer_t png;
    read_file(path, &png);
    decoded_t decoded = decode(&png);
    if (decoded.reason != NULL)
        fprintf(stderr, "%s refused: %s; did the committed PNG change format?\n", path, decoded.reason);
    assert(decoded.reason == NULL);
    assert(decoded.width == width && decoded.height == height);
    for (int i = 3; i < width * height * 4; i += 4)
        assert(decoded.pixels[i] == 0xFF);
    free(decoded.pixels);
}

static void test_the_game_assets_decode_opaque(void) {
    assert_asset_decodes_opaque("game/sprites.png", 128, 128);
    assert_asset_decodes_opaque("game/font.png", 48, 30);
}

static void assert_refused(const buffer_t *png, const char *key_word, const char *suspect) {
    decoded_t decoded = decode(png);
    if (decoded.reason == NULL || strstr(decoded.reason, key_word) == NULL)
        fprintf(stderr, "expected a reason with \"%s\", got %s; %s\n", key_word, decoded.reason ? decoded.reason : "an accepted file", suspect);
    assert(decoded.reason != NULL && strstr(decoded.reason, key_word) != NULL);
}

static void build_plain_rgb(buffer_t *png) {
    build_png(png, &(image_t){3, 2, 3, RGB_3X2});
}

static void test_a_file_that_is_not_a_whole_png_is_refused(void) {
    static buffer_t png;
    assert_refused(&png, "signature", "an empty buffer must be refused");
    put_bytes(&png, "GIF89a\0\0", 8);
    assert_refused(&png, "signature", "the signature is probably not checked");
    build_plain_rgb(&png);
    png.length -= 20;
    assert_refused(&png, "truncated", "the chunk bounds are probably not checked");
}

static const char *const SKIPPED_CRC = "the gate probably skips CRCs, which stb_image never checks";

static void test_a_bad_crc_is_refused_on_any_chunk(void) {
    static buffer_t png;
    build_plain_rgb(&png);
    corrupt_crc(&png, "IHDR");
    assert_refused(&png, "CRC in chunk IHDR", SKIPPED_CRC);
    build_plain_rgb(&png);
    corrupt_crc(&png, "IEND");
    assert_refused(&png, "CRC in chunk IEND", SKIPPED_CRC);

    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    png.length = 0;
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_chunk(&png, "tEXt", (const unsigned char *)"a\0b", 3);
    put_image_data(&png, raw, filter_rows(&image, raw), STORED, 1);
    put_chunk(&png, "IEND", NULL, 0);
    corrupt_crc(&png, "tEXt");
    assert_refused(&png, "CRC in chunk tEXt", SKIPPED_CRC);
}

static void build_with_raw(buffer_t *png, int width, int height, int colour_type, int interlace, const unsigned char *raw, size_t length) {
    png->length = 0;
    put_signature(png);
    put_header(png, (unsigned long)width, (unsigned long)height, 8, colour_type, interlace);
    put_image_data(png, raw, length, STORED, 1);
    put_chunk(png, "IEND", NULL, 0);
}

static void test_a_bad_adler_32_is_refused(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    size_t length = filter_rows(&(image_t){3, 2, 3, RGB_3X2}, raw);
    static buffer_t zlib;
    zlib.length = 0;
    put_zlib_stored(&zlib, raw, length);
    zlib.bytes[zlib.length - 1] ^= 1;
    png.length = 0;
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_chunk(&png, "IDAT", zlib.bytes, zlib.length);
    put_chunk(&png, "IEND", NULL, 0);
    assert_refused(&png, "Adler-32", "the gate probably skips the Adler-32, which stb_image never checks");
}

static void test_image_data_of_the_wrong_length_is_refused(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    size_t length = filter_rows(&image, raw);
    memcpy(raw + length, raw, length / 2);
    build_with_raw(&png, 3, 2, 2, 0, raw, length + length / 2);
    assert_refused(&png, "image data is 30 bytes, 20 expected", "stb_image ignores extra image data");
    build_with_raw(&png, 3, 2, 2, 0, raw, length - 1);
    assert_refused(&png, "image data is 19 bytes, 20 expected", "the length is probably not checked");

    static unsigned char samples[11 * 9 * 3];
    const image_t interlaced = {11, 9, 3, samples};
    build_with_raw(&png, 11, 9, 2, 1, raw, filter_rows(&interlaced, raw));
    assert_refused(&png, "expected", "the interlaced length is probably not counted per Adam7 pass");
}

static void build_header_only(buffer_t *png, unsigned long width, unsigned long height, int depth, int colour_type) {
    png->length = 0;
    put_signature(png);
    put_header(png, width, height, depth, colour_type, 0);
    put_chunk(png, "IEND", NULL, 0);
}

static void test_other_depths_and_colour_types_are_refused(void) {
    static buffer_t png;
    build_header_only(&png, 3, 2, 16, 2);
    assert_refused(&png, "rgba16", "16-bit RGB must be refused");
    build_header_only(&png, 3, 2, 16, 6);
    assert_refused(&png, "rgba16", "16-bit RGBA must be refused");
    build_header_only(&png, 3, 2, 8, 0);
    assert_refused(&png, "colour type 0 (grey)", "grey must be refused");
    build_header_only(&png, 3, 2, 8, 3);
    assert_refused(&png, "colour type 3 (palette)", "palette must be refused");
    build_header_only(&png, 3, 2, 8, 4);
    assert_refused(&png, "colour type 4 (grey-alpha)", "grey-alpha must be refused");
}

static void test_sides_outside_1_to_the_maximum_are_refused(void) {
    static buffer_t png;
    build_header_only(&png, 0, 2, 8, 2);
    assert_refused(&png, "size 0x2", "a zero width must be refused");
    build_header_only(&png, MAX_SIDE + 1, 1, 8, 6);
    assert_refused(&png, "size 1025x1 is above 1024", "the maximum side is probably not checked");
}

static void test_a_bad_chunk_order_is_refused(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    const image_t image = {3, 2, 3, RGB_3X2};
    size_t length = filter_rows(&image, raw);

    png.length = 0;
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_chunk(&png, "CRIT", raw, 4);
    put_image_data(&png, raw, length, STORED, 1);
    put_chunk(&png, "IEND", NULL, 0);
    assert_refused(&png, "unknown critical chunk CRIT", "unknown critical chunks are probably skipped");

    png.length = 0;
    put_signature(&png);
    put_header(&png, 3, 2, 8, 2, 0);
    put_image_data(&png, raw, length, STORED, 1);
    assert_refused(&png, "no IEND", "the walk probably stops without IEND");

    png.length = 0;
    put_signature(&png);
    put_chunk(&png, "tEXt", (const unsigned char *)"a\0b", 3);
    put_header(&png, 3, 2, 8, 2, 0);
    put_image_data(&png, raw, length, STORED, 1);
    put_chunk(&png, "IEND", NULL, 0);
    assert_refused(&png, "IHDR", "IHDR must come first");
}

static void assert_round_trip(const char *path, int width, int height) {
    size_t length = (size_t)width * height * 4;
    unsigned char *pixels = malloc(length);
    assert(pixels != NULL);
    for (size_t i = 0; i < length; i++)
        pixels[i] = (unsigned char)(i * 7 + (i >> 12));
    remove(path);
    bool written = png_file_write(path, pixels, width, height);
    assert(written);
    unsigned char *loaded;
    int loaded_width, loaded_height;
    const char *reason = png_file_load(path, &loaded, &loaded_width, &loaded_height);
    if (reason != NULL)
        fprintf(stderr, "%s refused: %s\n", path, reason);
    assert(reason == NULL);
    assert(loaded_width == width && loaded_height == height);
    assert(memcmp(loaded, pixels, length) == 0);
    free(loaded);
    free(pixels);
    remove(path);
}

static void test_a_written_png_loads_to_the_same_bytes(void) {
    assert_round_trip("tmp/test_fake_love_png_file_1x1.png", 1, 1);
    assert_round_trip("tmp/test_fake_love_png_file_390x844.png", 390, 844);
}

static void test_a_png_that_cannot_be_created_is_reported(void) {
    static const unsigned char pixel[4] = {1, 2, 3, 4};
    bool written = png_file_write("tmp/no_such_directory/test_fake_love_png_file.png", pixel, 1, 1);
    assert(!written);
}

static void test_stb_image_reasons_pass_through(void) {
    static buffer_t png;
    static unsigned char raw[BUFFER_SIZE];
    size_t length = filter_rows(&(image_t){3, 2, 3, RGB_3X2}, raw);
    raw[0] = 7;
    build_with_raw(&png, 3, 2, 2, 0, raw, length);
    assert_refused(&png, "filter", "a filter byte of 7 must be refused");
}

int main(void) {
    test_an_rgba_png_gives_its_bytes_alphas_included();
    test_an_rgb_png_gives_its_bytes_with_opaque_alpha();
    test_real_deflate_gives_the_same_bytes_as_stored();
    test_a_colour_key_gives_alpha_0_and_keeps_the_rgb();
    test_ancillary_chunks_split_data_and_trailing_bytes_change_nothing();
    test_an_interlaced_png_gives_the_same_bytes();
    test_a_side_of_the_maximum_is_accepted();
    test_the_game_assets_decode_opaque();
    test_a_file_that_is_not_a_whole_png_is_refused();
    test_a_bad_crc_is_refused_on_any_chunk();
    test_a_bad_adler_32_is_refused();
    test_image_data_of_the_wrong_length_is_refused();
    test_other_depths_and_colour_types_are_refused();
    test_sides_outside_1_to_the_maximum_are_refused();
    test_a_bad_chunk_order_is_refused();
    test_stb_image_reasons_pass_through();
    test_a_written_png_loads_to_the_same_bytes();
    test_a_png_that_cannot_be_created_is_reported();
    puts("test_fake_love_png_file: ok");
    return 0;
}
