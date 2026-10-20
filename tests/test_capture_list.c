#include "devtools/capture_list.h"

#include "devtools/draw_filter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum { FRAMES = 401 };

static capture_list_t list;
static char error[CAPTURE_LIST_ERROR_CAP];

static int parse(const char *text) {
    memset(&list, 0xA5, sizeof list);
    error[0] = '\0';
    return capture_list_parse(text, "pad_slipway", FRAMES, &list, error);
}

static void assert_refused(const char *text, const char *needle) {
    assert(!parse(text));
    if (strstr(error, needle) == NULL) {
        fprintf(stderr, "error '%s' lacks '%s'\n", error, needle);
        assert(0);
    }
}

static void test_a_valid_list_gives_its_captures_in_order(void) {
    assert(parse("# the check replay\n"
                 "replay pad_slipway\n"
                 "\n"
                 "cls 1 cls\n"
                 "  # a comment between captures\n"
                 "rectfill 11 cls,rectfill\n"
                 "pattern 190 cls,rectfill,circfill,pattern\n"
                 "all 400 all\n"));
    assert(strcmp(list.replay, "pad_slipway") == 0);
    assert(list.count == 4);
    assert(strcmp(list.entries[0].label, "cls") == 0 && list.entries[0].frame == 1);
    assert(list.entries[0].mask == DRAW_FILTER_BIT(DRAW_FILTER_CLS));
    assert(list.entries[1].mask == (DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL)));
    assert(list.entries[2].mask
           == (DRAW_FILTER_BIT(DRAW_FILTER_CLS) | DRAW_FILTER_BIT(DRAW_FILTER_RECTFILL)
               | DRAW_FILTER_BIT(DRAW_FILTER_CIRCFILL) | DRAW_FILTER_BIT(DRAW_FILTER_PATTERN)));
    assert(strcmp(list.entries[3].label, "all") == 0 && list.entries[3].mask == DRAW_FILTER_ALL);
}

static void test_a_list_with_no_capture_is_valid(void) {
    assert(parse("replay pad_slipway\n"));
    assert(list.count == 0);
}

static void test_each_refusal_names_its_line(void) {
    assert_refused("replay pad_slipway\ncls 1 cls\nblob 5 cls\n", "line 3: unknown family blob");
    assert_refused("replay pad_slipway\ncls 1 cls,blob\n", "line 2: unknown family blob");
    assert_refused("replay pad_slipway\nline 4 line\n", "line 2: mask without cls");
    assert_refused("replay pad_slipway\ncls 4 cls\nline 4 cls,line\n", "line 3: frame 4 does not increase");
    assert_refused("replay pad_slipway\ncls 9 cls\nline 4 cls,line\n", "line 3: frame 4 does not increase");
    assert_refused("replay pad_slipway\ncls 401 cls\n", "line 2: frame 401 past the replay's 401 frames");
    assert_refused("replay pad_slipway\ncls -1 cls\n", "line 2: frame is not an integer");
    assert_refused("replay pad_slipway\ncls 1\n", "line 2: expected <label> <frame> <mask>");
    assert_refused("cls 1 cls\n", "line 1: expected replay <name>");
    assert_refused("\n# nothing\n", "no replay line");
    assert_refused("# made for another\nreplay pad_title\ncls 1 cls\n", "line 2: replay pad_title, expected pad_slipway");
}

static void test_mask_names_follow_the_bit_order(void) {
    char names[CAPTURE_LIST_NAMES_CAP];
    capture_list_mask_names(DRAW_FILTER_ALL, names);
    assert(strcmp(names, "all") == 0);
    capture_list_mask_names(DRAW_FILTER_BIT(DRAW_FILTER_PATTERN) | DRAW_FILTER_BIT(DRAW_FILTER_CLS)
                                | DRAW_FILTER_BIT(DRAW_FILTER_CIRCFILL),
                            names);
    assert(strcmp(names, "cls,circfill,pattern") == 0);
    assert(strcmp(capture_list_family_name(DRAW_FILTER_PRINT), "print") == 0);
}

static void test_the_script_line_is_the_frame_and_the_mask_in_hex(void) {
    assert(parse("replay pad_slipway\ncls 1 cls\nprint 299 cls,print\nall 391 all\n"));
    char line[CAPTURE_LIST_SCRIPT_LINE_CAP];
    capture_list_script_line(&list.entries[0], line);
    assert(strcmp(line, "1 0001\n") == 0);
    capture_list_script_line(&list.entries[1], line);
    assert(strcmp(line, "299 0101\n") == 0);
    capture_list_script_line(&list.entries[2], line);
    assert(strcmp(line, "391 03ff\n") == 0);
}

static void test_a_display_entry_has_the_display_mask(void) {
    assert(parse("replay pad_slipway\nall 12 all\ndisplay 14 display\n"));
    assert(list.count == 2);
    assert(strcmp(list.entries[1].label, "display") == 0 && list.entries[1].frame == 14);
    assert(list.entries[1].mask == CAPTURE_LIST_DISPLAY_MASK);
    char names[CAPTURE_LIST_NAMES_CAP];
    capture_list_mask_names(CAPTURE_LIST_DISPLAY_MASK, names);
    assert(strcmp(names, "display") == 0);
    char line[CAPTURE_LIST_SCRIPT_LINE_CAP];
    capture_list_script_line(&list.entries[1], line);
    assert(strcmp(line, "14 8000\n") == 0);
}

static void test_a_display_entry_is_refused_where_the_display_is_not_the_unfiltered_frame_minus_two(void) {
    assert_refused("replay pad_slipway\ndisplay 14 all\n", "line 2: display takes the mask display, not all");
    assert_refused("replay pad_slipway\ndisplay 14 cls,display\n", "line 2: display takes the mask display");
    assert_refused("replay pad_slipway\ncls 14 display\n", "line 2: the mask display needs the label display");
    assert_refused("replay pad_slipway\nall 14 cls,display\n", "line 2: unknown family display");
    assert_refused("replay pad_slipway\ndisplay 1 display\n", "line 2: display at frame 1, before frame 2");
    assert_refused("replay pad_slipway\nall 14 all\ndisplay 14 display\n", "line 3: frame 14 does not increase");
    assert_refused("replay pad_slipway\nall 13 all\ndisplay 14 display\n",
                   "line 3: display at frame 14 after a capture at frame 13");
    assert_refused("replay pad_slipway\nprint 12 cls,print\ndisplay 14 display\n",
                   "line 3: display at frame 14 shows the masked capture at frame 12");
    assert(parse("replay pad_slipway\ndisplay 2 display\n"));
}

int main(void) {
    test_a_valid_list_gives_its_captures_in_order();
    test_a_list_with_no_capture_is_valid();
    test_each_refusal_names_its_line();
    test_mask_names_follow_the_bit_order();
    test_the_script_line_is_the_frame_and_the_mask_in_hex();
    test_a_display_entry_has_the_display_mask();
    test_a_display_entry_is_refused_where_the_display_is_not_the_unfiltered_frame_minus_two();
    printf("test_capture_list: ok\n");
    return 0;
}
