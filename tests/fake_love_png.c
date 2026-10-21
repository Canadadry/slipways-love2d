#include "fake_love_png.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STORED_BLOCK_MAX 65535

typedef struct {
    unsigned char *bytes;
    size_t length;
} buffer_t;

static void put_byte(buffer_t *buffer, unsigned value) {
    buffer->bytes[buffer->length++] = (unsigned char)value;
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

static void put_chunk(buffer_t *buffer, const char type[4], const unsigned char *data, size_t length) {
    put_big_endian(buffer, length);
    size_t start = buffer->length;
    memcpy(buffer->bytes + buffer->length, type, 4);
    if (length > 0)
        memcpy(buffer->bytes + buffer->length + 4, data, length);
    buffer->length += 4 + length;
    put_big_endian(buffer, crc32_of(buffer->bytes + start, 4 + length));
}

void fake_love_png_write(const char *path, const unsigned char *rgba, int width, int height) {
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
    put_byte(&zlib, 0x78);
    put_byte(&zlib, 0x01);
    for (size_t offset = 0; offset < raw_length;) {
        size_t length = raw_length - offset < STORED_BLOCK_MAX ? raw_length - offset : STORED_BLOCK_MAX;
        put_byte(&zlib, offset + length == raw_length ? 1 : 0);
        put_byte(&zlib, length & 0xFF);
        put_byte(&zlib, length >> 8);
        put_byte(&zlib, ~length & 0xFF);
        put_byte(&zlib, (~length >> 8) & 0xFF);
        memcpy(zlib.bytes + zlib.length, raw + offset, length);
        zlib.length += length;
        offset += length;
    }
    put_big_endian(&zlib, adler32_of(raw, raw_length));
    free(raw);

    buffer_t png = {malloc(8 + 25 + 12 + zlib.length + 12), 0};
    assert(png.bytes != NULL);
    static const unsigned char signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    memcpy(png.bytes, signature, sizeof signature);
    png.length = sizeof signature;
    buffer_t header = {(unsigned char[13]){0}, 0};
    put_big_endian(&header, (unsigned long)width);
    put_big_endian(&header, (unsigned long)height);
    put_byte(&header, 8);
    put_byte(&header, 6);
    put_byte(&header, 0);
    put_byte(&header, 0);
    put_byte(&header, 0);
    put_chunk(&png, "IHDR", header.bytes, header.length);
    put_chunk(&png, "IDAT", zlib.bytes, zlib.length);
    put_chunk(&png, "IEND", NULL, 0);
    free(zlib.bytes);

    FILE *file = fopen(path, "wb");
    assert(file != NULL && "the PNG must be created");
    assert(fwrite(png.bytes, 1, png.length, file) == png.length);
    assert(fclose(file) == 0);
    free(png.bytes);
}
