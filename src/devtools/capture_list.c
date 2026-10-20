#include "devtools/capture_list.h"

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *const FAMILY_NAMES[DRAW_FILTER_FAMILY_COUNT] = {
    "cls", "pset", "line", "rect", "rectfill", "circ", "circfill", "spr", "print", "pattern",
};

enum { LINE_CAP = 256, MAX_WORDS = 4 };

const char *capture_list_family_name(draw_filter_family_t family) {
    assert(family >= 0 && family < DRAW_FILTER_FAMILY_COUNT);
    return FAMILY_NAMES[family];
}

static int family_of(const char *name, size_t length) {
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        if (strlen(FAMILY_NAMES[family]) == length && strncmp(FAMILY_NAMES[family], name, length) == 0) {
            return family;
        }
    }
    return -1;
}

static int refuse(char *error, int line_number, const char *reason, const char *word) {
    snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: %s%s", line_number, reason, word);
    return 0;
}

/* Returns the word count, at most MAX_WORDS + 1; the words point into line, which is cut. */
static int split(char *line, char *words[MAX_WORDS + 1]) {
    int count = 0;
    char *at = line;
    while (count <= MAX_WORDS) {
        while (*at != '\0' && isspace((unsigned char)*at)) at++;
        if (*at == '\0') break;
        words[count++] = at;
        while (*at != '\0' && !isspace((unsigned char)*at)) at++;
        if (*at != '\0') *at++ = '\0';
    }
    return count;
}

static int parse_frame(const char *word, int *frame) {
    int value = 0;
    if (*word == '\0' || strlen(word) > 9) return 0;
    for (const char *c = word; *c != '\0'; c++) {
        if (!isdigit((unsigned char)*c)) return 0;
        value = value * 10 + (*c - '0');
    }
    *frame = value;
    return 1;
}

/* Returns 0 and names the bad family in *bad (a pointer into word) when one is unknown. */
static int parse_mask(const char *word, unsigned *mask, const char **bad, size_t *bad_length) {
    if (strcmp(word, "all") == 0) {
        *mask = DRAW_FILTER_ALL;
        return 1;
    }
    *mask = 0;
    const char *start = word;
    for (;;) {
        const char *end = strchr(start, ',');
        size_t length = end != NULL ? (size_t)(end - start) : strlen(start);
        int family = family_of(start, length);
        if (family < 0) {
            *bad = start;
            *bad_length = length;
            return 0;
        }
        *mask |= DRAW_FILTER_BIT(family);
        if (end == NULL) return 1;
        start = end + 1;
    }
}

static void add_entry(capture_list_t *list, const char *label, int frame, unsigned mask) {
    capture_list_entry_t *entry = &list->entries[list->count++];
    snprintf(entry->label, sizeof entry->label, "%s", label);
    entry->frame = frame;
    entry->mask = mask;
}

/* Returns 0 with error set when frame does not follow the last entry or is past the replay. */
static int check_frame(int frame, int line_number, int frame_count, const capture_list_t *list, char *error) {
    if (list->count > 0 && frame <= list->entries[list->count - 1].frame) {
        snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: frame %d does not increase", line_number, frame);
        return 0;
    }
    if (frame >= frame_count) {
        snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: frame %d past the replay's %d frames", line_number, frame,
                 frame_count);
        return 0;
    }
    return 1;
}

/* A frame-locked capture at F-1 shows F-1 itself, and one masked at F-2 draws a filtered picture:
 * either way the display at F is no longer the unfiltered F-2. */
static int parse_display(char *words[], int line_number, int frame_count, capture_list_t *list, char *error) {
    if (strcmp(words[2], "display") != 0) {
        return refuse(error, line_number, "display takes the mask display, not ", words[2]);
    }
    int frame;
    if (!parse_frame(words[1], &frame)) return refuse(error, line_number, "frame is not an integer: ", words[1]);
    if (!check_frame(frame, line_number, frame_count, list, error)) return 0;
    if (frame < 2) {
        snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: display at frame %d, before frame 2", line_number, frame);
        return 0;
    }
    for (int c = 0; c < list->count; c++) {
        const capture_list_entry_t *entry = &list->entries[c];
        if (entry->frame == frame - 1) {
            snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: display at frame %d after a capture at frame %d",
                     line_number, frame, entry->frame);
            return 0;
        }
        if (entry->frame == frame - 2 && entry->mask != DRAW_FILTER_ALL) {
            snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: display at frame %d shows the masked capture at frame %d",
                     line_number, frame, entry->frame);
            return 0;
        }
    }
    if (list->count == CAPTURE_LIST_MAX) return refuse(error, line_number, "too many captures", "");
    add_entry(list, "display", frame, CAPTURE_LIST_DISPLAY_MASK);
    return 1;
}

static int parse_capture(char *words[], int line_number, int frame_count, capture_list_t *list, char *error) {
    if (strcmp(words[0], "display") == 0) return parse_display(words, line_number, frame_count, list, error);
    if (strcmp(words[2], "display") == 0) {
        return refuse(error, line_number, "the mask display needs the label display, not ", words[0]);
    }
    if (strcmp(words[0], "all") != 0 && family_of(words[0], strlen(words[0])) < 0) {
        return refuse(error, line_number, "unknown family ", words[0]);
    }
    if (strlen(words[0]) >= CAPTURE_LIST_LABEL_CAP) return refuse(error, line_number, "label too long: ", words[0]);
    int frame;
    if (!parse_frame(words[1], &frame)) return refuse(error, line_number, "frame is not an integer: ", words[1]);
    if (!check_frame(frame, line_number, frame_count, list, error)) return 0;
    unsigned mask;
    const char *bad = NULL;
    size_t bad_length = 0;
    if (!parse_mask(words[2], &mask, &bad, &bad_length)) {
        snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: unknown family %.*s", line_number, (int)bad_length, bad);
        return 0;
    }
    if (!(mask & DRAW_FILTER_BIT(DRAW_FILTER_CLS))) return refuse(error, line_number, "mask without cls: ", words[2]);
    if (list->count == CAPTURE_LIST_MAX) return refuse(error, line_number, "too many captures", "");
    add_entry(list, words[0], frame, mask);
    return 1;
}

int capture_list_parse(const char *text, const char *replay, int frame_count, capture_list_t *list,
                       char error[CAPTURE_LIST_ERROR_CAP]) {
    list->replay[0] = '\0';
    list->count = 0;
    int seen_replay = 0;
    int line_number = 0;
    const char *at = text;
    while (*at != '\0') {
        line_number++;
        const char *end = strchr(at, '\n');
        size_t length = end != NULL ? (size_t)(end - at) : strlen(at);
        if (length >= LINE_CAP) return refuse(error, line_number, "line too long", "");
        char line[LINE_CAP];
        memcpy(line, at, length);
        line[length] = '\0';
        at = end != NULL ? end + 1 : at + length;

        char *words[MAX_WORDS + 1];
        int count = split(line, words);
        if (count == 0 || words[0][0] == '#') continue;
        if (!seen_replay) {
            if (count != 2 || strcmp(words[0], "replay") != 0) {
                return refuse(error, line_number, "expected replay <name>", "");
            }
            if (strcmp(words[1], replay) != 0) {
                snprintf(error, CAPTURE_LIST_ERROR_CAP, "line %d: replay %s, expected %s", line_number, words[1],
                         replay);
                return 0;
            }
            snprintf(list->replay, sizeof list->replay, "%s", words[1]);
            seen_replay = 1;
            continue;
        }
        if (count != 3) return refuse(error, line_number, "expected <label> <frame> <mask>", "");
        if (!parse_capture(words, line_number, frame_count, list, error)) return 0;
    }
    if (!seen_replay) {
        snprintf(error, CAPTURE_LIST_ERROR_CAP, "no replay line");
        return 0;
    }
    return 1;
}

void capture_list_mask_names(unsigned mask, char names[CAPTURE_LIST_NAMES_CAP]) {
    if (mask == DRAW_FILTER_ALL) {
        snprintf(names, CAPTURE_LIST_NAMES_CAP, "all");
        return;
    }
    if (mask == CAPTURE_LIST_DISPLAY_MASK) {
        snprintf(names, CAPTURE_LIST_NAMES_CAP, "display");
        return;
    }
    names[0] = '\0';
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        if (!(mask & DRAW_FILTER_BIT(family))) continue;
        if (names[0] != '\0') strcat(names, ",");
        strcat(names, FAMILY_NAMES[family]);
    }
}

void capture_list_script_line(const capture_list_entry_t *entry, char line[CAPTURE_LIST_SCRIPT_LINE_CAP]) {
    snprintf(line, CAPTURE_LIST_SCRIPT_LINE_CAP, "%d %04x\n", entry->frame, entry->mask);
}
