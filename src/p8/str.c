#include "p8/str.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

char *p8_sub(const allocator_t *zone, const char *s, int i, int j) {
    int len = (int)strlen(s);

    if (i < 1) {
        i = 1;
    }
    if (j > len) {
        j = len;
    }

    if (j < i) {
        char *out = (char *)allocator_alloc(zone, 1);
        out[0] = '\0';
        return out;
    }

    int n = j - i + 1;
    char *out = (char *)allocator_alloc(zone, (size_t)n + 1);
    memcpy(out, s + (i - 1), (size_t)n);
    out[n] = '\0';
    return out;
}

char *p8_tostr_fixed(const allocator_t *zone, p8_fixed_t x) {
    char buf[64];

    int negative = x < 0;
    uint32_t mag = negative ? 0u - (uint32_t)x
                            : (uint32_t)x;
    uint32_t ip = mag >> 16;
    uint32_t p = (mag & 0xFFFFu) * 625u;
    uint32_t q = p >> 12;
    uint32_t r = p & 4095u;
    if (r > 2048u || (r == 2048u && (q & 1u))) {
        q++;
    }
    if (q == 10000u) {
        ip++;
        q = 0;
    }
    snprintf(buf, sizeof buf, "%s%lu.%04lu", negative ? "-" : "",
             (unsigned long)ip, (unsigned long)q);
    char *dot = strchr(buf, '.');
    if (dot) {
        char *end = buf + strlen(buf) - 1;
        while (end > dot && *end == '0') {
            *end-- = '\0';
        }
        if (end == dot) {
            *end = '\0';
        }
    }
    size_t n = strlen(buf);
    char *out = (char *)allocator_alloc(zone, n + 1);
    memcpy(out, buf, n + 1);
    return out;
}

char *p8_concat2(const allocator_t *zone, const char *a, const char *b) {
    size_t la = strlen(a);
    size_t lb = strlen(b);
    char *out = (char *)allocator_alloc(zone, la + lb + 1);
    memcpy(out, a, la);
    memcpy(out + la, b, lb);
    out[la + lb] = '\0';
    return out;
}
