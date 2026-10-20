#include "devtools/demo_source.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_whole_file(const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }
    char *text = NULL;
    long size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    if (size >= 0 && fseek(file, 0, SEEK_SET) == 0) {
        text = malloc((size_t)size + 1);
    }
    if (text) {
        text[fread(text, 1, (size_t)size, file)] = '\0';
    }
    fclose(file);
    return text;
}

static int check_header(const demo_source_t *source, char *reason, size_t reason_size) {
    if (replay_source_start(&source->replay) != REPLAY_SOURCE_START_RESUME) {
        snprintf(reason, reason_size, "not a from-record demo");
        return 0;
    }
    bindings_t fallback = {{{0}}};
    bindings_t bindings;
    replay_source_bindings_report_t report;
    if (replay_source_read_bindings(&source->replay, &fallback, &bindings, &report) != REPLAY_SOURCE_BINDINGS_DEFAULT) {
        snprintf(reason, reason_size, "has a \"bindings\" member: the demo plays on the default bindings");
        return 0;
    }
    int frame_count = replay_source_frame_count(&source->replay);
    if (frame_count > DEMO_DATA_FRAME_LIMIT) {
        snprintf(reason, reason_size, "%d frames, at most %d", frame_count, DEMO_DATA_FRAME_LIMIT);
        return 0;
    }
    return 1;
}

static int read_captions(demo_source_t *source, const char *json, char *reason, size_t reason_size) {
    source->caption_count =
        pad_caption_read_member(json, source->captions, DEMO_SOURCE_CAPTIONS_MAX, reason, reason_size);
    if (source->caption_count < 0) {
        return 0;
    }
    if (source->caption_count == 0) {
        snprintf(reason, reason_size, "no caption");
        return 0;
    }
    pad_caption_track_t written = {
        .captions = source->captions, .capacity = DEMO_SOURCE_CAPTIONS_MAX, .count = source->caption_count};
    static char member[DEMO_SOURCE_CAPTIONS_MAX * (64 + 2 * PAD_CAPTION_TEXT_SIZE) + 16];
    int length = pad_caption_write_member(&written, member, sizeof member);
    if (strncmp(strstr(json, "\"captions\":"), member, (size_t)length) != 0) {
        snprintf(reason, reason_size, "\"captions\" is not as pad_author writes it");
        return 0;
    }
    return 1;
}

static int check_ranges(const demo_source_t *source, char *reason, size_t reason_size) {
    int frame_count = replay_source_frame_count(&source->replay);
    for (int i = 0; i < source->caption_count; i++) {
        const pad_caption_t *caption = &source->captions[i];
        if (caption->to <= caption->from) {
            snprintf(reason, reason_size, "caption %d: frames %d-%d are empty", i + 1, caption->from, caption->to);
            return 0;
        }
        if (caption->to > frame_count) {
            snprintf(reason, reason_size, "caption %d: frames %d-%d end past the demo's %d frames", i + 1,
                     caption->from, caption->to, frame_count);
            return 0;
        }
        if (i > 0 && caption->from < source->captions[i - 1].to) {
            snprintf(reason, reason_size, "caption %d: frames %d-%d start before caption %d ends, at frame %d", i + 1,
                     caption->from, caption->to, i, source->captions[i - 1].to);
            return 0;
        }
    }
    return 1;
}

static int check_texts(const demo_source_t *source, char *reason, size_t reason_size) {
    for (int i = 0; i < source->caption_count; i++) {
        char script_text[PAD_CAPTION_TEXT_SIZE];
        snprintf(script_text, sizeof script_text, "%s", source->captions[i].text);
        for (char *p = script_text; *p != '\0'; p++) {
            if (*p == '\n') {
                *p = '/';
            }
        }
        char parsed[PAD_CAPTION_TEXT_SIZE];
        char rule[128];
        if (!pad_caption_parse(script_text, parsed, sizeof parsed, rule, sizeof rule)) {
            snprintf(reason, reason_size, "caption %d: %s", i + 1, rule);
            return 0;
        }
        if (strcmp(parsed, source->captions[i].text) != 0) {
            snprintf(reason, reason_size, "caption %d: text is not as pad_author writes it", i + 1);
            return 0;
        }
    }
    return 1;
}

static int refuse_close(const pad_caption_track_t *track, const char *rule, char *reason, size_t reason_size) {
    assert(track->shown && "a failed close leaves the caption shown");
    snprintf(reason, reason_size, "frame %d: %s", track->captions[track->count - 1].from, rule);
    return 0;
}

/* The authoring order: a caption line runs before the frames it emits. */
static int check_track(const demo_source_t *source, char *reason, size_t reason_size) {
    pad_caption_t storage[DEMO_SOURCE_CAPTIONS_MAX];
    pad_caption_track_t track;
    pad_caption_track_init(&track, storage, DEMO_SOURCE_CAPTIONS_MAX);
    char rule[128];
    int frame_count = replay_source_frame_count(&source->replay);
    for (int frame = 0; frame < frame_count; frame++) {
        const pad_caption_t *next = track.count < source->caption_count ? &source->captions[track.count] : NULL;
        if (next && next->from == frame) {
            if (!pad_caption_track_show(&track, frame, next->text, rule, sizeof rule)) {
                return refuse_close(&track, rule, reason, reason_size);
            }
        } else if (track.shown && source->captions[track.count - 1].to == frame) {
            if (!pad_caption_track_clear(&track, frame, rule, sizeof rule)) {
                return refuse_close(&track, rule, reason, reason_size);
            }
        }
        int buttons = source->replay.recording.frames[frame].pad_buttons;
        if (!pad_caption_track_frame(&track, frame, buttons, rule, sizeof rule)) {
            snprintf(reason, reason_size, "frame %d: %s", frame, rule);
            return 0;
        }
    }
    if (!pad_caption_track_end(&track, frame_count, 1, rule, sizeof rule)) {
        return refuse_close(&track, rule, reason, reason_size);
    }
    return 1;
}

int demo_source_load(demo_source_t *source, const char *path, char *error, size_t error_size) {
    assert(source && path && error && error_size > 0);
    if (replay_source_open(&source->replay, path) != REPLAY_SOURCE_LOADED) {
        snprintf(error, error_size, "%s: cannot load as a replay", path);
        return 0;
    }
    char *json = read_whole_file(path);
    char reason[256] = "cannot re-read the file";
    int ok = json && check_header(source, reason, sizeof reason) && read_captions(source, json, reason, sizeof reason) &&
             check_ranges(source, reason, sizeof reason) && check_texts(source, reason, sizeof reason) &&
             check_track(source, reason, sizeof reason);
    free(json);
    if (!ok) {
        snprintf(error, error_size, "%s: %s", path, reason);
        demo_source_close(source);
        return 0;
    }
    return 1;
}

void demo_source_close(demo_source_t *source) {
    replay_source_close(&source->replay);
}
