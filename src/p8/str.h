#ifndef P8_STR_H
#define P8_STR_H

#include "p8/fixed.h"
#include "engine/allocator.h"

char *p8_sub(const allocator_t *zone, const char *s, int i, int j);

char *p8_tostr_fixed(const allocator_t *zone, p8_fixed_t x);

char *p8_concat2(const allocator_t *zone, const char *a, const char *b);

#endif
