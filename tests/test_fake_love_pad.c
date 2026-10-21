#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../tools/fake_love/pad.h"

static const char *const DESIGN_NAMES[PAD_BUTTON_COUNT] = {
    "dpup", "dpdown", "dpleft", "dpright", "a", "b",
    "x", "y", "leftshoulder", "rightshoulder", "start", "back",
};

static void test_the_twelve_buttons_have_the_design_names(void) {
    for (int button = 1; button <= PAD_BUTTON_COUNT; button++) {
        assert(strcmp(pad_button_name(button), DESIGN_NAMES[button - 1]) == 0);
    }
}

static void test_the_twelve_names_are_distinct(void) {
    for (int first = 1; first <= PAD_BUTTON_COUNT; first++) {
        for (int second = first + 1; second <= PAD_BUTTON_COUNT; second++) {
            assert(strcmp(pad_button_name(first), pad_button_name(second)) != 0);
        }
    }
}

static void test_every_table_name_gives_its_button_number(void) {
    for (int button = 1; button <= PAD_BUTTON_COUNT; button++) {
        int found = -1;
        assert(pad_find_name(DESIGN_NAMES[button - 1], &found));
        assert(found == button);
    }
}

static void test_the_love_names_outside_the_table_give_zero(void) {
    const char *outside[] = {"guide", "leftstick", "rightstick"};
    for (size_t i = 0; i < sizeof(outside) / sizeof(outside[0]); i++) {
        int found = -1;
        assert(pad_find_name(outside[i], &found));
        assert(found == 0);
    }
}

static void test_a_name_love_does_not_know_is_refused(void) {
    const char *unknown[] = {"", "A", "dpUp", "select", "dpup ", "leftstic"};
    for (size_t i = 0; i < sizeof(unknown) / sizeof(unknown[0]); i++) {
        int found = -1;
        assert(!pad_find_name(unknown[i], &found));
        assert(found == -1 && "a refused name leaves the button untouched");
    }
}

#define BIT(button) (1u << ((button) - 1))
#define ALL_TWELVE 0xfffu

static void assert_changes(unsigned previous, unsigned current, const char *expected) {
    pad_change_t changes[PAD_MAX_CHANGES];
    size_t count = pad_list_changes(previous, current, changes);
    assert(count <= PAD_MAX_CHANGES);
    char text[PAD_MAX_CHANGES * 4 + 1] = "";
    size_t length = 0;
    for (size_t i = 0; i < count; i++) {
        length += (size_t)snprintf(text + length, sizeof(text) - length, "%s%c%d", i == 0 ? "" : " ",
                                   changes[i].kind == PAD_PRESSED ? '+' : '-', changes[i].button);
    }
    if (strcmp(text, expected) != 0) {
        fprintf(stderr, "%#x -> %#x\n  expected: %s\n  got: %s\n", previous, current, expected, text);
    }
    assert(strcmp(text, expected) == 0);
}

static void test_buttons_going_down_are_pressed_in_ascending_order(void) {
    assert_changes(0, BIT(5) | BIT(1), "+1 +5");
    assert_changes(0, ALL_TWELVE, "+1 +2 +3 +4 +5 +6 +7 +8 +9 +10 +11 +12");
}

static void test_buttons_going_up_are_released_in_ascending_order(void) {
    assert_changes(BIT(1) | BIT(5), BIT(5), "-1");
    assert_changes(ALL_TWELVE, 0, "-1 -2 -3 -4 -5 -6 -7 -8 -9 -10 -11 -12");
}

static void test_every_release_comes_before_every_press(void) {
    assert_changes(BIT(2) | BIT(4), BIT(1) | BIT(3), "-2 -4 +1 +3");
    assert_changes(BIT(12), BIT(1), "-12 +1");
}

static void test_an_unchanged_held_set_gives_no_change(void) {
    assert_changes(0, 0, "");
    assert_changes(BIT(1) | BIT(5), BIT(5) | BIT(1), "");
    assert_changes(ALL_TWELVE, ALL_TWELVE, "");
}

int main(void) {
    test_the_twelve_buttons_have_the_design_names();
    test_the_twelve_names_are_distinct();
    test_every_table_name_gives_its_button_number();
    test_the_love_names_outside_the_table_give_zero();
    test_a_name_love_does_not_know_is_refused();
    test_buttons_going_down_are_pressed_in_ascending_order();
    test_buttons_going_up_are_released_in_ascending_order();
    test_every_release_comes_before_every_press();
    test_an_unchanged_held_set_gives_no_change();
    printf("test_fake_love_pad: ok\n");
    return 0;
}
