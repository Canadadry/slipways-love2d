/* Host tool: writes to argv[1] the inputs and C results of src/p8/fixed.c,
 * the oracle of tests/lua/test_fixed.lua. Layout, every number a
 * little-endian uint32: "P8FIXREF", version; per section: name length, name,
 * input count, output count, record count, records; then 0 and the total
 * record count. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "p8/fixed.h"

#define FORMAT_VERSION 1u
#define SAMPLE_PER_SECTION 256

typedef struct {
    FILE *file;
    uint32_t total_records;
    uint32_t *section;
    uint32_t section_values;
    uint32_t section_capacity;
    const char *name;
    uint32_t inputs;
    uint32_t outputs;
} writer_t;

static void write_u32(writer_t *w, uint32_t v) {
    unsigned char bytes[4] = {(unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16),
                              (unsigned char)(v >> 24)};
    if (fwrite(bytes, 1, 4, w->file) != 4) {
        fprintf(stderr, "fixed_reference: write failed\n");
        exit(1);
    }
}

static void section_begin(writer_t *w, const char *name, uint32_t inputs, uint32_t outputs) {
    w->name = name;
    w->inputs = inputs;
    w->outputs = outputs;
    w->section_values = 0;
}

static void section_push(writer_t *w, uint32_t v) {
    if (w->section_values == w->section_capacity) {
        w->section_capacity = w->section_capacity ? w->section_capacity * 2 : 4096;
        w->section = realloc(w->section, w->section_capacity * sizeof *w->section);
        if (!w->section) {
            fprintf(stderr, "fixed_reference: out of memory\n");
            exit(1);
        }
    }
    w->section[w->section_values++] = v;
}

static void section_end(writer_t *w) {
    uint32_t width = w->inputs + w->outputs;
    uint32_t records = w->section_values / width;
    uint32_t length = 0;
    uint32_t i;
    while (w->name[length]) {
        length++;
    }
    write_u32(w, length);
    if (fwrite(w->name, 1, length, w->file) != length) {
        fprintf(stderr, "fixed_reference: write failed\n");
        exit(1);
    }
    write_u32(w, w->inputs);
    write_u32(w, w->outputs);
    write_u32(w, records);
    for (i = 0; i < w->section_values; i++) {
        write_u32(w, w->section[i]);
    }
    w->total_records += records;
}

static const int32_t edge_values[] = {
    0,           1,           -1,         2,           -2,          0x8000,      -0x8000,
    0xFFFF,      -0xFFFF,     0x10000,    -0x10000,    0x10001,     -0x10001,    0x18000,
    -0x18000,    0x1FFFF,     -0x1FFFF,   0x20000,     0x30000,     -0x30000,    0x00B504F3,
    0x00FF0000,  0x01000000,  0x40000000, -0x40000000, 0x7FFEFFFF,  0x7FFF0000,  -0x7FFF0000,
    0x7FFF8000,  -0x7FFF8000, 0x7FFFFFFF, -0x7FFFFFFF, INT32_MIN,
};
#define EDGE_COUNT ((int)(sizeof edge_values / sizeof edge_values[0]))

static uint32_t sample_state = 0x9E3779B9u;

static uint32_t sample_next(void) {
    sample_state ^= sample_state << 13;
    sample_state ^= sample_state >> 17;
    sample_state ^= sample_state << 5;
    return sample_state;
}

static int32_t sample_value(void) {
    uint32_t kind = sample_next() % 3u;
    if (kind == 0) {
        return (int32_t)sample_next();
    }
    if (kind == 1) {
        return (int32_t)sample_next() / 4096;
    }
    return (int32_t)((sample_next() & 0xFFFF0000u) + sample_next() % 7u - 3u);
}

static const int32_t edge_ints[] = {
    0, 1, -1, 2, 127, 128, -128, 32767, 32768, -32768, -32769, 65535, 65536, 65537, -65536, -65537,
    0x12345678, INT32_MAX, INT32_MIN,
};
#define EDGE_INT_COUNT ((int)(sizeof edge_ints / sizeof edge_ints[0]))

static const int32_t edge_shifts[] = {
    INT32_MIN, -100, -33, -32, -31, -1, 0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12,
    13,        14,   15,  16,  17,  18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,  30,  31,
    32,        33,   64,  100, INT32_MAX,
};
#define EDGE_SHIFT_COUNT ((int)(sizeof edge_shifts / sizeof edge_shifts[0]))

typedef int32_t (*unary_fn)(int32_t);
typedef int32_t (*binary_fn)(int32_t, int32_t);
typedef int32_t (*ternary_fn)(int32_t, int32_t, int32_t);

static int32_t call_from_int(int32_t i) { return p8_fixed_from_int(i); }
static int32_t call_int_macro(int32_t i) { return P8_FIXED_INT(i); }
static int32_t call_neg(int32_t x) { return p8_fixed_neg(x); }
static int32_t call_flr(int32_t x) { return p8_fixed_flr(x); }
static int32_t call_flr_int(int32_t x) { return p8_fixed_flr_int(x); }
static int32_t call_abs(int32_t x) { return p8_fixed_abs(x); }
static int32_t call_sgn(int32_t x) { return p8_fixed_sgn(x); }
static int32_t call_add(int32_t a, int32_t b) { return p8_fixed_add(a, b); }
static int32_t call_sub(int32_t a, int32_t b) { return p8_fixed_sub(a, b); }
static int32_t call_mul(int32_t a, int32_t b) { return p8_fixed_mul(a, b); }
static int32_t call_div(int32_t a, int32_t b) { return p8_fixed_div(a, b); }
static int32_t call_band(int32_t a, int32_t b) { return p8_fixed_band(a, b); }
static int32_t call_bor(int32_t a, int32_t b) { return p8_fixed_bor(a, b); }
static int32_t call_mod(int32_t a, int32_t b) { return p8_fixed_mod(a, b); }
static int32_t call_sqrt(int32_t x) { return p8_fixed_sqrt(x); }
static int32_t call_pow075(int32_t x) { return p8_fixed_pow075(x); }
static int32_t call_cos(int32_t x) { return p8_fixed_cos(x); }
static int32_t call_sin(int32_t x) { return p8_fixed_sin(x); }

static void write_constant(writer_t *w, const char *name, int32_t value) {
    section_begin(w, name, 0, 1);
    section_push(w, (uint32_t)value);
    section_end(w);
}

static void push_unary(writer_t *w, unary_fn fn, int32_t x) {
    section_push(w, (uint32_t)x);
    section_push(w, (uint32_t)fn(x));
}

static void write_unary(writer_t *w, const char *name, unary_fn fn, const int32_t *extra, int extra_count) {
    int i;
    section_begin(w, name, 1, 1);
    for (i = 0; i < EDGE_COUNT; i++) {
        push_unary(w, fn, edge_values[i]);
    }
    for (i = 0; i < extra_count; i++) {
        push_unary(w, fn, extra[i]);
    }
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        push_unary(w, fn, sample_value());
    }
    section_end(w);
}

/* P8_FIXED_LIT pastes a literal: only literals reach it, so the sample is
 * spelled out rather than drawn. */
static void write_literal(writer_t *w) {
    section_begin(w, "P8_FIXED_LIT", 2, 1);
#define PUSH_LITERAL(raw) \
    section_push(w, (uint32_t)(raw##u)); \
    section_push(w, 0); \
    section_push(w, (uint32_t)P8_FIXED_LIT(raw, 0))
    PUSH_LITERAL(0x00000000);
    PUSH_LITERAL(0x00000001);
    PUSH_LITERAL(0x00008000);
    PUSH_LITERAL(0x00010000);
    PUSH_LITERAL(0x7FFFFFFF);
    PUSH_LITERAL(0x80000000);
    PUSH_LITERAL(0x80000001);
    PUSH_LITERAL(0xFFFF0000);
    PUSH_LITERAL(0xFFFF8000);
    PUSH_LITERAL(0xFFFFFFFF);
#undef PUSH_LITERAL
    section_end(w);
}

static void write_binary(writer_t *w, const char *name, binary_fn fn) {
    int i;
    int j;
    section_begin(w, name, 2, 1);
    for (i = 0; i < EDGE_COUNT; i++) {
        for (j = 0; j < EDGE_COUNT; j++) {
            section_push(w, (uint32_t)edge_values[i]);
            section_push(w, (uint32_t)edge_values[j]);
            section_push(w, (uint32_t)fn(edge_values[i], edge_values[j]));
        }
    }
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        int32_t a = sample_value();
        int32_t b = sample_value();
        section_push(w, (uint32_t)a);
        section_push(w, (uint32_t)b);
        section_push(w, (uint32_t)fn(a, b));
    }
    section_end(w);
}

static void push_shift(writer_t *w, int32_t x, int32_t n) {
    section_push(w, (uint32_t)x);
    section_push(w, (uint32_t)n);
    section_push(w, (uint32_t)p8_fixed_shl(x, n));
}

static void write_shift(writer_t *w) {
    int i;
    int j;
    section_begin(w, "p8_fixed_shl", 2, 1);
    for (i = 0; i < EDGE_COUNT; i++) {
        for (j = 0; j < EDGE_SHIFT_COUNT; j++) {
            push_shift(w, edge_values[i], edge_shifts[j]);
        }
    }
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        int32_t x = sample_value();
        push_shift(w, x, (int32_t)(sample_next() % 80u) - 24);
    }
    section_end(w);
}

static const int32_t edge_roots[] = {
    3,          4,          0x3FFF,     0x4000,     0x4001,     0xFFFE,     0x24000,    0x8FFFF,
    0x90000,    0x90001,    0x640000,   0xFFFFFF,   0x1000000,  0x3FFFFFFF, 0x7FFE0001, 0x7FFFFFFE,
};
#define EDGE_ROOT_COUNT ((int)(sizeof edge_roots / sizeof edge_roots[0]))

/* Every 16-bit angle once, each under drawn high bits: only the low 16 bits
 * may select the table entry. */
static void write_angle(writer_t *w, const char *name, unary_fn fn) {
    int i;
    uint32_t angle;
    section_begin(w, name, 1, 1);
    for (i = 0; i < EDGE_COUNT; i++) {
        push_unary(w, fn, edge_values[i]);
    }
    for (angle = 0; angle <= 0xFFFFu; angle++) {
        push_unary(w, fn, (int32_t)((sample_next() & 0xFFFF0000u) | angle));
    }
    section_end(w);
}

static void push_ternary(writer_t *w, ternary_fn fn, int32_t a, int32_t b, int32_t c) {
    section_push(w, (uint32_t)a);
    section_push(w, (uint32_t)b);
    section_push(w, (uint32_t)c);
    section_push(w, (uint32_t)fn(a, b, c));
}

static void write_ternary(writer_t *w, const char *name, ternary_fn fn) {
    int i;
    int j;
    int k;
    section_begin(w, name, 3, 1);
    for (i = 0; i < EDGE_COUNT; i++) {
        for (j = 0; j < EDGE_COUNT; j++) {
            for (k = 0; k < EDGE_COUNT; k++) {
                push_ternary(w, fn, edge_values[i], edge_values[j], edge_values[k]);
            }
        }
    }
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        int32_t a = sample_value();
        int32_t b = sample_value();
        push_ternary(w, fn, a, b, sample_value());
    }
    section_end(w);
}

/* Every rnd section draws on the library's global state in file order: the
 * first draw is the default seed's, so no earlier section may draw. */
static void push_rnd(writer_t *w, int32_t x) {
    section_push(w, (uint32_t)x);
    section_push(w, (uint32_t)p8_fixed_rnd(x));
    section_push(w, p8_fixed_rnd_state());
}

static void write_rnd_stream(writer_t *w) {
    int i;
    section_begin(w, "p8_fixed_rnd", 1, 2);
    for (i = 0; i < 8; i++) {
        push_rnd(w, p8_fixed_from_int(1));
    }
    for (i = 0; i < EDGE_COUNT; i++) {
        push_rnd(w, edge_values[i]);
    }
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        push_rnd(w, sample_value());
    }
    section_end(w);
}

static const uint32_t edge_states[] = {
    0u, 1u, 0x7FFFFFFFu, 0x80000000u, 0xFFFFFFFFu, 0x2545F491u, 0xC5E1D7B3u,
};
#define EDGE_STATE_COUNT ((int)(sizeof edge_states / sizeof edge_states[0]))

static void write_set_rnd_streams(writer_t *w) {
    int i;
    int j;
    for (i = 0; i < EDGE_STATE_COUNT; i++) {
        section_begin(w, "p8_fixed_set_rnd_state", 1, 1);
        p8_fixed_set_rnd_state(edge_states[i]);
        section_push(w, edge_states[i]);
        section_push(w, p8_fixed_rnd_state());
        section_end(w);
        section_begin(w, "p8_fixed_rnd", 1, 2);
        for (j = 0; j < 4; j++) {
            push_rnd(w, p8_fixed_from_int(1));
        }
        push_rnd(w, 0);
        push_rnd(w, INT32_MIN);
        push_rnd(w, 0x7FFFFFFF);
        push_rnd(w, -0x10000);
        section_end(w);
    }
}

static void write_rnd_default_stream(writer_t *w) {
    int i;
    p8_fixed_t result;
    section_begin(w, "p8_fixed_rnd_default", 0, 2);
    for (i = 0; i < 32; i++) {
        result = p8_fixed_rnd_default();
        section_push(w, (uint32_t)result);
        section_push(w, p8_fixed_rnd_state());
    }
    section_end(w);
}

static void push_rnd_from_state(writer_t *w, uint32_t *state, int32_t x) {
    section_push(w, *state);
    section_push(w, (uint32_t)x);
    section_push(w, (uint32_t)p8_fixed_rnd_from_state(state, x));
    section_push(w, *state);
}

/* Chains: each record's input state is the previous record's new state.
 * The trailing rnd_state section proves the global state was left alone. */
static void write_rnd_from_state_chains(writer_t *w) {
    int i;
    int j;
    uint32_t state;
    section_begin(w, "p8_fixed_rnd_from_state", 2, 2);
    for (i = 0; i < EDGE_STATE_COUNT; i++) {
        state = edge_states[i];
        for (j = 0; j < EDGE_COUNT; j++) {
            push_rnd_from_state(w, &state, edge_values[j]);
        }
    }
    state = sample_next();
    for (i = 0; i < SAMPLE_PER_SECTION; i++) {
        push_rnd_from_state(w, &state, sample_value());
    }
    /* 0xA4B668AA advances to 0x80000001, and 0x7FFFFFFF * 0x80000001 is
     * 2^32 * 0x3FFFFFFF + 0xFFFFFFFF: a double product rounds it up to the
     * next multiple of 2^32, so a Lua copy multiplying in doubles fails here. */
    state = 0xA4B668AAu;
    push_rnd_from_state(w, &state, 0x7FFFFFFF);
    state = 0xA4B668AAu;
    push_rnd_from_state(w, &state, -0x7FFFFFFF);
    section_end(w);
    section_begin(w, "p8_fixed_rnd_state", 0, 1);
    section_push(w, p8_fixed_rnd_state());
    section_end(w);
}

int main(int argc, char **argv) {
    writer_t w = {0};
    if (argc != 2) {
        fprintf(stderr, "usage: fixed_reference <output file>\n");
        return 2;
    }
    w.file = fopen(argv[1], "wb");
    if (!w.file) {
        perror(argv[1]);
        return 1;
    }
    if (fwrite("P8FIXREF", 1, 8, w.file) != 8) {
        fprintf(stderr, "fixed_reference: write failed\n");
        return 1;
    }
    write_u32(&w, FORMAT_VERSION);
    write_constant(&w, "P8_FIXED_SHIFT", P8_FIXED_SHIFT);
    write_constant(&w, "P8_FIXED_DIV0_POS", P8_FIXED_DIV0_POS);
    write_constant(&w, "P8_FIXED_DIV0_NEG", P8_FIXED_DIV0_NEG);
    write_literal(&w);
    write_unary(&w, "p8_fixed_from_int", call_from_int, edge_ints, EDGE_INT_COUNT);
    write_unary(&w, "P8_FIXED_INT", call_int_macro, edge_ints, EDGE_INT_COUNT);
    write_binary(&w, "p8_fixed_add", call_add);
    write_binary(&w, "p8_fixed_sub", call_sub);
    write_binary(&w, "p8_fixed_mul", call_mul);
    write_binary(&w, "p8_fixed_div", call_div);
    write_unary(&w, "p8_fixed_neg", call_neg, NULL, 0);
    write_unary(&w, "p8_fixed_flr", call_flr, NULL, 0);
    write_unary(&w, "p8_fixed_flr_int", call_flr_int, NULL, 0);
    write_unary(&w, "p8_fixed_abs", call_abs, NULL, 0);
    write_unary(&w, "p8_fixed_sgn", call_sgn, NULL, 0);
    write_ternary(&w, "p8_fixed_mid", p8_fixed_mid);
    write_binary(&w, "p8_fixed_band", call_band);
    write_binary(&w, "p8_fixed_bor", call_bor);
    write_binary(&w, "p8_fixed_mod", call_mod);
    write_shift(&w);
    write_unary(&w, "p8_fixed_sqrt", call_sqrt, edge_roots, EDGE_ROOT_COUNT);
    write_unary(&w, "p8_fixed_pow075", call_pow075, edge_roots, EDGE_ROOT_COUNT);
    write_angle(&w, "p8_fixed_cos", call_cos);
    write_angle(&w, "p8_fixed_sin", call_sin);
    write_rnd_stream(&w);
    write_set_rnd_streams(&w);
    write_rnd_default_stream(&w);
    write_rnd_from_state_chains(&w);
    write_u32(&w, 0);
    write_u32(&w, w.total_records);
    free(w.section);
    if (fclose(w.file) != 0) {
        perror(argv[1]);
        return 1;
    }
    return 0;
}
