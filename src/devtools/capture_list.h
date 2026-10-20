#ifndef DEVTOOLS_CAPTURE_LIST_H
#define DEVTOOLS_CAPTURE_LIST_H

#include "devtools/draw_filter.h"

/* A capture list: "replay <name>", then one "<label> <frame> <mask>" a capture. The label is
 * a family name or "all"; the mask is "all" or a comma list of family names holding cls. Frames
 * strictly increase. Blank and '#' lines are skipped.
 * "display <frame> display" captures what is displayed at the top of <frame>, before its update:
 * the picture drawn at <frame> - 2. */

/* Outside the family bits: in the capture script it means "capture at the top of the frame, display
 * untouched". */
#define CAPTURE_LIST_DISPLAY_MASK 0x8000u

enum {
    CAPTURE_LIST_MAX = 32,
    CAPTURE_LIST_LABEL_CAP = 16,
    CAPTURE_LIST_REPLAY_CAP = 64,
    CAPTURE_LIST_ERROR_CAP = 160,
    CAPTURE_LIST_NAMES_CAP = 96,
    CAPTURE_LIST_SCRIPT_LINE_CAP = 24
};

typedef struct {
    char label[CAPTURE_LIST_LABEL_CAP];
    int frame;
    unsigned mask;
} capture_list_entry_t;

typedef struct {
    char replay[CAPTURE_LIST_REPLAY_CAP];
    int count;
    capture_list_entry_t entries[CAPTURE_LIST_MAX];
} capture_list_t;

/* Returns 1 with *list filled, or 0 with error naming the line; a list for another replay than
 * `replay`, or a frame at or past frame_count, is refused. */
int capture_list_parse(const char *text, const char *replay, int frame_count, capture_list_t *list,
                       char error[CAPTURE_LIST_ERROR_CAP]);

const char *capture_list_family_name(draw_filter_family_t family);

/* "all", or the mask's family names in bit order, comma-separated. */
void capture_list_mask_names(unsigned mask, char names[CAPTURE_LIST_NAMES_CAP]);

/* "<frame> <mask as 4 hex digits>\n", the line of the emulator's capture script. */
void capture_list_script_line(const capture_list_entry_t *entry, char line[CAPTURE_LIST_SCRIPT_LINE_CAP]);

#endif
