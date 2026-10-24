#include "png_file.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_LINEAR
#define STBI_NO_HDR
#include "stb_image.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *png_file_load(const char *path, unsigned char **pixels, int *width, int *height) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return "cannot be opened";
    int channels;
    unsigned char *decoded = stbi_load_from_file(file, width, height, &channels, 4);
    fclose(file);
    if (decoded == NULL)
        return stbi_failure_reason();
    *pixels = decoded;
    return NULL;
}

static char reason_text[96];

static uint32_t big_endian_at(const unsigned char *bytes) {
    return (uint32_t)bytes[0] << 24 | (uint32_t)bytes[1] << 16 | (uint32_t)bytes[2] << 8 | bytes[3];
}

/* Slicing by 8: table[k][n] is the CRC of byte n followed by k zero bytes. */
static uint32_t crc32_of(const unsigned char *bytes, size_t length) {
    static uint32_t table[8][256];
    if (table[0][1] == 0) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t crc = n;
            for (int bit = 0; bit < 8; bit++)
                crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
            table[0][n] = crc;
        }
        for (int k = 1; k < 8; k++)
            for (int n = 0; n < 256; n++)
                table[k][n] = (table[k - 1][n] >> 8) ^ table[0][table[k - 1][n] & 0xFF];
    }
    uint32_t crc = 0xFFFFFFFFu;
    size_t i = 0;
    for (; i + 8 <= length; i += 8) {
        uint32_t low = crc ^ ((uint32_t)bytes[i] | (uint32_t)bytes[i + 1] << 8 | (uint32_t)bytes[i + 2] << 16 |
                              (uint32_t)bytes[i + 3] << 24);
        crc = table[7][low & 0xFF] ^ table[6][(low >> 8) & 0xFF] ^ table[5][(low >> 16) & 0xFF] ^
              table[4][low >> 24] ^ table[3][bytes[i + 4]] ^ table[2][bytes[i + 5]] ^ table[1][bytes[i + 6]] ^
              table[0][bytes[i + 7]];
    }
    for (; i < length; i++)
        crc = (crc >> 8) ^ table[0][(crc ^ bytes[i]) & 0xFF];
    return ~crc;
}

/* 5552 bytes is the longest run whose sums cannot overflow 32 bits. */
static uint32_t adler32_of(const unsigned char *bytes, size_t length) {
    uint32_t a = 1, b = 0;
    while (length > 0) {
        size_t run = length < 5552 ? length : 5552;
        size_t i = 0;
        for (; i + 4 <= run; i += 4) {
            b += 4 * a + 4 * bytes[i] + 3 * bytes[i + 1] + 2 * bytes[i + 2] + bytes[i + 3];
            a += bytes[i] + bytes[i + 1] + bytes[i + 2] + bytes[i + 3];
        }
        for (; i < run; i++) {
            a += bytes[i];
            b += a;
        }
        a %= 65521;
        b %= 65521;
        bytes += run;
        length -= run;
    }
    return b << 16 | a;
}

typedef struct {
    uint32_t width, height;
    int channels, interlaced;
} header_t;

static const char *check_header(const unsigned char *data, uint32_t length, int max_side, header_t *header) {
    if (length != 13)
        return "IHDR is not 13 bytes";
    header->width = big_endian_at(data);
    header->height = big_endian_at(data + 4);
    int depth = data[8], colour_type = data[9];
    header->interlaced = data[12];
    if (depth == 16)
        return "bit depth 16 (LÖVE makes an rgba16 ImageData)";
    if (colour_type != 2 && colour_type != 6) {
        const char *name = colour_type == 0 ? " (grey)" : colour_type == 3 ? " (palette)" : colour_type == 4 ? " (grey-alpha)" : "";
        snprintf(reason_text, sizeof reason_text, "colour type %d%s", colour_type, name);
        return reason_text;
    }
    if (depth != 8) {
        snprintf(reason_text, sizeof reason_text, "bit depth %d", depth);
        return reason_text;
    }
    header->channels = colour_type == 6 ? 4 : 3;
    if (header->width == 0 || header->height == 0) {
        snprintf(reason_text, sizeof reason_text, "size %lux%lu has a zero side", (unsigned long)header->width, (unsigned long)header->height);
        return reason_text;
    }
    if (header->width > (uint32_t)max_side || header->height > (uint32_t)max_side) {
        snprintf(reason_text, sizeof reason_text, "size %lux%lu is above %d", (unsigned long)header->width, (unsigned long)header->height, max_side);
        return reason_text;
    }
    return NULL;
}

static size_t filtered_length(const header_t *header) {
    if (!header->interlaced)
        return header->height * (1 + (size_t)header->width * header->channels);
    static const uint32_t passes[7][4] = {{0, 0, 8, 8}, {4, 0, 8, 8}, {0, 4, 4, 8}, {2, 0, 4, 4}, {0, 2, 2, 4}, {1, 0, 2, 2}, {0, 1, 1, 2}};
    size_t length = 0;
    for (int pass = 0; pass < 7; pass++) {
        const uint32_t *p = passes[pass];
        if (header->width <= p[0] || header->height <= p[1])
            continue;
        size_t width = (header->width - p[0] + p[2] - 1) / p[2];
        size_t height = (header->height - p[1] + p[3] - 1) / p[3];
        length += height * (1 + width * header->channels);
    }
    return length;
}

static const char *check_image_data(const unsigned char *zlib, size_t length, const header_t *header) {
    if (length == 0)
        return "no IDAT";
    int inflated_length;
    unsigned char *inflated = (unsigned char *)stbi_zlib_decode_malloc((const char *)zlib, (int)length, &inflated_length);
    if (inflated == NULL)
        return "image data does not inflate";
    uint32_t adler = adler32_of(inflated, (size_t)inflated_length);
    free(inflated);
    if (length < 4 || adler != big_endian_at(zlib + length - 4))
        return "zlib Adler-32 mismatch";
    size_t expected = filtered_length(header);
    if ((size_t)inflated_length != expected) {
        snprintf(reason_text, sizeof reason_text, "image data is %d bytes, %zu expected", inflated_length, expected);
        return reason_text;
    }
    return NULL;
}

/* What lodepng checks and stb_image does not, plus the kinds and sides the
 * fake accepts. */
static const char *check_png(const unsigned char *bytes, size_t length, int max_side) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (length < 8 || memcmp(bytes, signature, 8) != 0)
        return "not a PNG signature";
    header_t header = {0};
    unsigned char *image_data = NULL;
    size_t image_data_length = 0;
    const char *reason = NULL;
    size_t at = 8;
    for (int seen_end = 0; !seen_end && reason == NULL;) {
        if (length - at < 12) {
            reason = at == length ? "no IEND" : "truncated chunk";
            break;
        }
        uint32_t chunk_length = big_endian_at(bytes + at);
        if (chunk_length > length - at - 12) {
            reason = "truncated chunk";
            break;
        }
        const unsigned char *type = bytes + at + 4, *data = bytes + at + 8;
        if (crc32_of(type, chunk_length + 4) != big_endian_at(data + chunk_length)) {
            snprintf(reason_text, sizeof reason_text, "bad CRC in chunk %.4s", (const char *)type);
            reason = reason_text;
        } else if (at == 8) {
            reason = memcmp(type, "IHDR", 4) != 0 ? "first chunk is not IHDR" : check_header(data, chunk_length, max_side, &header);
        } else if (memcmp(type, "IDAT", 4) == 0) {
            unsigned char *grown = realloc(image_data, image_data_length + chunk_length + 1);
            assert(grown != NULL && "out of memory");
            image_data = grown;
            memcpy(image_data + image_data_length, data, chunk_length);
            image_data_length += chunk_length;
        } else if (memcmp(type, "IEND", 4) == 0) {
            seen_end = 1;
        } else if (!(type[0] & 0x20) && memcmp(type, "PLTE", 4) != 0) {
            snprintf(reason_text, sizeof reason_text, "unknown critical chunk %.4s", (const char *)type);
            reason = reason_text;
        }
        at += 12 + chunk_length;
    }
    if (reason == NULL)
        reason = check_image_data(image_data, image_data_length, &header);
    free(image_data);
    return reason;
}

const char *png_file_decode(const unsigned char *bytes, size_t length, int max_side, unsigned char **pixels, int *width, int *height) {
    assert(max_side > 0);
    if (length > INT_MAX)
        return "file above 2 GiB";
    const char *reason = check_png(bytes, length, max_side);
    if (reason != NULL)
        return reason;
    int channels;
    unsigned char *decoded = stbi_load_from_memory(bytes, (int)length, width, height, &channels, 4);
    if (decoded == NULL)
        return stbi_failure_reason();
    *pixels = decoded;
    return NULL;
}

#define STORED_BLOCK_MAX 65535

typedef struct {
    unsigned char *bytes;
    size_t length;
} buffer_t;

static void put_byte(buffer_t *buffer, unsigned value) {
    buffer->bytes[buffer->length++] = (unsigned char)value;
}

static void put_big_endian(buffer_t *buffer, uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8)
        put_byte(buffer, (unsigned)(value >> shift) & 0xFF);
}

static void put_chunk(buffer_t *buffer, const char type[4], const unsigned char *data, size_t length) {
    assert(length <= UINT32_MAX);
    put_big_endian(buffer, (uint32_t)length);
    size_t start = buffer->length;
    memcpy(buffer->bytes + buffer->length, type, 4);
    if (length > 0)
        memcpy(buffer->bytes + buffer->length + 4, data, length);
    buffer->length += 4 + length;
    put_big_endian(buffer, crc32_of(buffer->bytes + start, 4 + length));
}

static void put_zlib_stored(buffer_t *zlib, const unsigned char *raw, size_t raw_length) {
    put_byte(zlib, 0x78);
    put_byte(zlib, 0x01);
    for (size_t offset = 0; offset < raw_length;) {
        size_t length = raw_length - offset < STORED_BLOCK_MAX ? raw_length - offset : STORED_BLOCK_MAX;
        put_byte(zlib, offset + length == raw_length ? 1 : 0);
        put_byte(zlib, length & 0xFF);
        put_byte(zlib, length >> 8);
        put_byte(zlib, ~length & 0xFF);
        put_byte(zlib, (~length >> 8) & 0xFF);
        memcpy(zlib->bytes + zlib->length, raw + offset, length);
        zlib->length += length;
        offset += length;
    }
    put_big_endian(zlib, adler32_of(raw, raw_length));
}

static void put_png(buffer_t *png, const buffer_t *zlib, int width, int height) {
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    memcpy(png->bytes, signature, sizeof signature);
    png->length = sizeof signature;
    buffer_t header = {(unsigned char[13]){0}, 0};
    put_big_endian(&header, (uint32_t)width);
    put_big_endian(&header, (uint32_t)height);
    put_byte(&header, 8);
    put_byte(&header, 6);
    put_byte(&header, 0);
    put_byte(&header, 0);
    put_byte(&header, 0);
    put_chunk(png, "IHDR", header.bytes, header.length);
    put_chunk(png, "IDAT", zlib->bytes, zlib->length);
    put_chunk(png, "IEND", NULL, 0);
}

bool png_file_write(const char *path, const unsigned char *rgba, int width, int height) {
    assert(width > 0 && height > 0);
    size_t row = 1 + (size_t)width * 4;
    size_t raw_length = row * (size_t)height;
    unsigned char *raw = malloc(raw_length);
    assert(raw != NULL);
    for (int y = 0; y < height; y++) {
        raw[y * row] = 0;
        memcpy(raw + y * row + 1, rgba + (size_t)y * width * 4, (size_t)width * 4);
    }
    size_t blocks = raw_length / STORED_BLOCK_MAX + 1;
    buffer_t zlib = {malloc(2 + blocks * 5 + raw_length + 4), 0};
    assert(zlib.bytes != NULL);
    put_zlib_stored(&zlib, raw, raw_length);
    free(raw);
    buffer_t png = {malloc(8 + 25 + 12 + zlib.length + 12), 0};
    assert(png.bytes != NULL);
    put_png(&png, &zlib, width, height);
    free(zlib.bytes);

    FILE *file = fopen(path, "wb");
    bool written = file != NULL && fwrite(png.bytes, 1, png.length, file) == png.length;
    if (file != NULL && fclose(file) != 0)
        written = false;
    free(png.bytes);
    return written;
}
