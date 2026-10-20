#include "png_reader.h"

#include <stdint.h>
#include <string.h>

static uint32_t read_u32(const unsigned char *bytes) {
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static uint32_t crc32_bitwise(const unsigned char *bytes, size_t count) {
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < count; i++) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = -(crc & 1u);
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

typedef struct {
    const unsigned char *type;
    const unsigned char *data;
    uint32_t length;
} chunk_t;

static const char *read_chunk(const unsigned char *bytes, size_t size, size_t *offset, const char *expected_type, chunk_t *chunk) {
    if (size - *offset < 12) return "truncated chunk";
    uint32_t length = read_u32(bytes + *offset);
    if (length > size - *offset - 12) return "chunk length past the end";
    chunk->type = bytes + *offset + 4;
    chunk->data = bytes + *offset + 8;
    chunk->length = length;
    if (memcmp(chunk->type, expected_type, 4) != 0) return "unexpected chunk or chunk order";
    if (crc32_bitwise(chunk->type, 4 + (size_t)length) != read_u32(chunk->data + length)) return "chunk CRC mismatch";
    *offset += 12 + (size_t)length;
    return NULL;
}

typedef struct {
    unsigned char *rgb;
    size_t row_size;
    size_t raw_expected;
    size_t raw_seen;
    uint32_t sum_low;
    uint32_t sum_high;
} image_sink_t;

static const char *sink_byte(image_sink_t *sink, unsigned char value) {
    if (sink->raw_seen == sink->raw_expected) return "decoded data longer than the image";
    size_t column = sink->raw_seen % (sink->row_size + 1);
    if (column == 0) {
        if (value != 0) return "row filter other than 0";
    } else {
        size_t row = sink->raw_seen / (sink->row_size + 1);
        sink->rgb[row * sink->row_size + column - 1] = value;
    }
    sink->raw_seen++;
    sink->sum_low = (sink->sum_low + value) % 65521u;
    sink->sum_high = (sink->sum_high + sink->sum_low) % 65521u;
    return NULL;
}

static const char *inflate_stored(const unsigned char *data, size_t length, image_sink_t *sink) {
    if (length < 2) return "zlib stream too short";
    if (data[0] != 0x78) return "zlib header is not 78 xx";
    if (((unsigned)data[0] * 256u + data[1]) % 31u != 0) return "zlib header check bits wrong";
    if (data[1] & 0x20) return "zlib preset dictionary";
    size_t at = 2;
    int final = 0;
    while (!final) {
        if (length - at < 5) return "deflate stream ends before BFINAL";
        unsigned char header = data[at];
        if (header & 0x06) return "deflate block is not stored";
        if (header & 0xf8) return "stored block padding bits not zero";
        final = header & 1;
        unsigned block_length = data[at + 1] | ((unsigned)data[at + 2] << 8);
        unsigned complement = data[at + 3] | ((unsigned)data[at + 4] << 8);
        if ((block_length ^ 0xffffu) != complement) return "stored block LEN is not ~NLEN";
        at += 5;
        if (length - at < block_length) return "stored block past the end of IDAT";
        for (unsigned i = 0; i < block_length; i++) {
            const char *error = sink_byte(sink, data[at + i]);
            if (error != NULL) return error;
        }
        at += block_length;
    }
    if (length - at != 4) return "IDAT length is not the stream plus Adler-32";
    if (read_u32(data + at) != ((sink->sum_high << 16) | sink->sum_low)) return "Adler-32 mismatch";
    if (sink->raw_seen != sink->raw_expected) return "decoded length is not height * (1 + 3 * width)";
    return NULL;
}

const char *png_reader_decode(const unsigned char *bytes, size_t size, unsigned char *rgb, size_t rgb_capacity, int *width, int *height) {
    static const unsigned char signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    if (size < sizeof signature || memcmp(bytes, signature, sizeof signature) != 0) return "bad signature";
    size_t offset = sizeof signature;
    chunk_t chunk;

    const char *error = read_chunk(bytes, size, &offset, "IHDR", &chunk);
    if (error != NULL) return error;
    if (chunk.length != 13) return "IHDR length is not 13";
    uint32_t image_width = read_u32(chunk.data);
    uint32_t image_height = read_u32(chunk.data + 4);
    if (image_width == 0 || image_height == 0 || image_width > 0x7fffffffu || image_height > 0x7fffffffu) return "IHDR dimension out of range";
    static const unsigned char fields[5] = { 8, 2, 0, 0, 0 };
    if (memcmp(chunk.data + 8, fields, 5) != 0) return "IHDR fields other than 8/2/0/0/0";
    size_t row_size = 3 * (size_t)image_width;
    if (row_size * image_height > rgb_capacity) return "image larger than the rgb buffer";

    error = read_chunk(bytes, size, &offset, "IDAT", &chunk);
    if (error != NULL) return error;
    image_sink_t sink = { rgb, row_size, (row_size + 1) * image_height, 0, 1, 0 };
    error = inflate_stored(chunk.data, chunk.length, &sink);
    if (error != NULL) return error;

    error = read_chunk(bytes, size, &offset, "IEND", &chunk);
    if (error != NULL) return error;
    if (chunk.length != 0) return "IEND not empty";
    if (offset != size) return "bytes after IEND";

    *width = (int)image_width;
    *height = (int)image_height;
    return NULL;
}
