#include "devtools/png_writer.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

enum {
    SIGNATURE_SIZE = 8,
    CHUNK_OVERHEAD = 12,
    HEADER_DATA_SIZE = 13,
    ZLIB_HEADER_SIZE = 2,
    ADLER_SIZE = 4,
    BLOCK_HEADER_SIZE = 5,
    BLOCK_CAPACITY = 65535
};

static size_t raw_size(int width, int height) {
    return (size_t)height * (1 + 3 * (size_t)width);
}

static size_t block_count(size_t raw) {
    return raw == 0 ? 1 : (raw + BLOCK_CAPACITY - 1) / BLOCK_CAPACITY;
}

static size_t zlib_size(size_t raw) {
    return ZLIB_HEADER_SIZE + block_count(raw) * BLOCK_HEADER_SIZE + raw + ADLER_SIZE;
}

size_t png_writer_size(int width, int height) {
    if (width <= 0 || height <= 0) return 0;
    size_t raw = raw_size(width, height);
    return SIGNATURE_SIZE + (CHUNK_OVERHEAD + HEADER_DATA_SIZE) + (CHUNK_OVERHEAD + zlib_size(raw)) + CHUNK_OVERHEAD;
}

static uint32_t crc_table_entry(uint32_t n) {
    uint32_t c = n;
    for (int k = 0; k < 8; k++) c = (c & 1u) ? 0xedb88320u ^ (c >> 1) : c >> 1;
    return c;
}

static uint32_t crc_update(uint32_t crc, const unsigned char *bytes, size_t count) {
    static uint32_t table[256];
    static int table_ready = 0;
    if (!table_ready) {
        for (uint32_t n = 0; n < 256; n++) table[n] = crc_table_entry(n);
        table_ready = 1;
    }
    for (size_t i = 0; i < count; i++) crc = table[(crc ^ bytes[i]) & 0xffu] ^ (crc >> 8);
    return crc;
}

typedef struct {
    unsigned char *bytes;
    size_t length;
    uint32_t adler_low;
    uint32_t adler_high;
} writer_t;

static void put_byte(writer_t *writer, unsigned char value) {
    writer->bytes[writer->length++] = value;
}

static void put_u32(writer_t *writer, uint32_t value) {
    put_byte(writer, (unsigned char)(value >> 24));
    put_byte(writer, (unsigned char)(value >> 16));
    put_byte(writer, (unsigned char)(value >> 8));
    put_byte(writer, (unsigned char)value);
}

static void put_bytes(writer_t *writer, const void *bytes, size_t count) {
    memcpy(writer->bytes + writer->length, bytes, count);
    writer->length += count;
}

static size_t chunk_begin(writer_t *writer, uint32_t data_length, const char type[4]) {
    put_u32(writer, data_length);
    size_t type_offset = writer->length;
    put_bytes(writer, type, 4);
    return type_offset;
}

static void chunk_end(writer_t *writer, size_t type_offset) {
    uint32_t crc = crc_update(0xffffffffu, writer->bytes + type_offset, writer->length - type_offset);
    put_u32(writer, crc ^ 0xffffffffu);
}

static void put_raw_byte(writer_t *writer, unsigned char value) {
    put_byte(writer, value);
    writer->adler_low = (writer->adler_low + value) % 65521u;
    writer->adler_high = (writer->adler_high + writer->adler_low) % 65521u;
}

static void put_block_header(writer_t *writer, size_t remaining) {
    size_t length = remaining < BLOCK_CAPACITY ? remaining : BLOCK_CAPACITY;
    put_byte(writer, remaining <= BLOCK_CAPACITY ? 1 : 0);
    put_byte(writer, (unsigned char)length);
    put_byte(writer, (unsigned char)(length >> 8));
    put_byte(writer, (unsigned char)~length);
    put_byte(writer, (unsigned char)(~length >> 8));
}

static void put_image_data(writer_t *writer, const unsigned char *rgb, int width, int height) {
    size_t raw = raw_size(width, height);
    size_t row_size = 3 * (size_t)width;
    size_t written = 0;
    writer->adler_low = 1;
    writer->adler_high = 0;
    for (int y = 0; y < height; y++) {
        for (size_t x = 0; x <= row_size; x++) {
            if (written % BLOCK_CAPACITY == 0) put_block_header(writer, raw - written);
            put_raw_byte(writer, x == 0 ? 0 : rgb[(size_t)y * row_size + x - 1]);
            written++;
        }
    }
    assert(written == raw);
}

size_t png_writer_encode(const unsigned char *rgb, int width, int height, unsigned char *out, size_t capacity) {
    size_t size = png_writer_size(width, height);
    if (size == 0 || capacity < size) return 0;
    assert(rgb != NULL && out != NULL);
    static const unsigned char signature[SIGNATURE_SIZE] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    writer_t writer = { out, 0, 1, 0 };
    put_bytes(&writer, signature, sizeof signature);

    size_t type_offset = chunk_begin(&writer, HEADER_DATA_SIZE, "IHDR");
    put_u32(&writer, (uint32_t)width);
    put_u32(&writer, (uint32_t)height);
    static const unsigned char depth_type_compression_filter_interlace[5] = { 8, 2, 0, 0, 0 };
    put_bytes(&writer, depth_type_compression_filter_interlace, 5);
    chunk_end(&writer, type_offset);

    type_offset = chunk_begin(&writer, (uint32_t)zlib_size(raw_size(width, height)), "IDAT");
    put_byte(&writer, 0x78);
    put_byte(&writer, 0x01);
    put_image_data(&writer, rgb, width, height);
    put_u32(&writer, (writer.adler_high << 16) | writer.adler_low);
    chunk_end(&writer, type_offset);

    type_offset = chunk_begin(&writer, 0, "IEND");
    chunk_end(&writer, type_offset);

    assert(writer.length == size);
    return size;
}
