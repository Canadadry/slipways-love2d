#ifndef FAKE_LOVE_PAD_H
#define FAKE_LOVE_PAD_H

#include <stdbool.h>
#include <stddef.h>

#define PAD_BUTTON_COUNT 12
#define PAD_MAX_CHANGES (2 * PAD_BUTTON_COUNT)

typedef enum { PAD_RELEASED, PAD_PRESSED } pad_change_kind_t;

typedef struct {
    pad_change_kind_t kind;
    int button;
} pad_change_t;

/* Asserts 1 <= button <= PAD_BUTTON_COUNT. */
const char *pad_button_name(int button);

/* True when `name` is one of LÖVE 11.5's 15 gamepad button names; then
 * `button` gets its number, or 0 for guide, leftstick and rightstick.
 * False leaves `button` unchanged. */
bool pad_find_name(const char *name, int *button);

/* Held sets are masks, bit n-1 for button n. Fills `changes` in callback
 * order (every release, then every press, each in ascending number) and
 * returns how many it wrote. */
size_t pad_list_changes(unsigned previous, unsigned current, pad_change_t changes[PAD_MAX_CHANGES]);

#endif
