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

static uint32_t crc32_of(const unsigned char *bytes, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}

static uint32_t adler32_of(const unsigned char *bytes, size_t length) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < length; i++) {
        a = (a + bytes[i]) % 65521;
        b = (b + a) % 65521;
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
