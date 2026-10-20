#include "pad.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

static const char *const BUTTON_NAMES[PAD_BUTTON_COUNT] = {
    "dpup", "dpdown", "dpleft", "dpright", "a", "b",
    "x", "y", "leftshoulder", "rightshoulder", "start", "back",
};

static const char *const NAMES_OUTSIDE_TABLE[] = {"guide", "leftstick", "rightstick"};

const char *pad_button_name(int button) {
    assert(button >= 1 && button <= PAD_BUTTON_COUNT && "pad buttons are numbered 1-12");
    return BUTTON_NAMES[button - 1];
}

bool pad_find_name(const char *name, int *button) {
    for (int candidate = 1; candidate <= PAD_BUTTON_COUNT; candidate++) {
        if (strcmp(name, BUTTON_NAMES[candidate - 1]) == 0) {
            *button = candidate;
            return true;
        }
    }
    for (size_t i = 0; i < sizeof(NAMES_OUTSIDE_TABLE) / sizeof(NAMES_OUTSIDE_TABLE[0]); i++) {
        if (strcmp(name, NAMES_OUTSIDE_TABLE[i]) == 0) {
            *button = 0;
            return true;
        }
    }
    return false;
}

static size_t append_changes(unsigned went, pad_change_kind_t kind, pad_change_t *changes) {
    size_t count = 0;
    for (int button = 1; button <= PAD_BUTTON_COUNT; button++) {
        if (went & (1u << (button - 1))) {
            changes[count++] = (pad_change_t){kind, button};
        }
    }
    return count;
}

size_t pad_list_changes(unsigned previous, unsigned current, pad_change_t changes[PAD_MAX_CHANGES]) {
    assert((previous | current) >> PAD_BUTTON_COUNT == 0 && "a held set has 12 bits, one per button");
    size_t count = append_changes(previous & ~current, PAD_RELEASED, changes);
    count += append_changes(current & ~previous, PAD_PRESSED, changes + count);
    return count;
}
