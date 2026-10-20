#include "p8/vram.h"
#include "p8/sprite_sheet.h"
#include "p8/gfx.h"
#include "p8/draw_meter.h"
#include "p8/input.h"
#include "game/input_step.h"
#include "devtools/capture_list.h"
#include "devtools/draw_filter.h"
#include "devtools/dump.h"
#include "devtools/pad_metrics.h"
#include "devtools/record_start.h"
#include "devtools/replay_source.h"
#include "devtools/screencast.h"
#include "devtools/state.h"
#include "devtools/vram_digest.h"
#include "engine/dispatch_context.h"
#include "game/cart.h"
#include "game/demo_data.h"
#include "game/save_state.h"
#include "game/save_storage.h"
#include "game/ui_zone.h"
#include "game/particle_zone.h"
#include "engine/game_zone.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const SLOT_KIND_NAMES[SLOT_KIND_COUNT] = {"button", "modal_backdrop", "message_box", "toast", "row", "ephemerals"};
static const char *const PARTICLE_NAMES[PARTICLE_SLOT_COUNT] = {"particle", "particles", "particles_help_screen", "burst", "transit_dots"};

#define PAD_METRICS_SCRATCH_BYTES (1 << 16)
/* Candidate lists of the pad metrics: never frame_zone(), or make state would see the hook in its peaks. */
BUMP_ZONE_STORAGE(pad_metrics_cells, PAD_METRICS_SCRATCH_BYTES);
static bump_zone_t pad_metrics_scratch;
static pad_metrics_t pad_metrics;

static int write_pad_metrics(const char *path) {
    pad_metrics_finish(&pad_metrics);
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "replay: cannot open the pad metrics file '%s'\n", path);
        return 0;
    }
    pad_metrics_write_report(&pad_metrics, out);
    if (fclose(out) != 0) {
        fprintf(stderr, "replay: cannot write the pad metrics file '%s'\n", path);
        return 0;
    }
    return 1;
}

static void write_fill(const char *path) {
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "replay: cannot open the fill file '%s'\n", path);
        return;
    }
    cart_flush_exit_queue();
    for (int s = 0; s < SLOT_KIND_COUNT; s++) {
        fprintf(out, "%s %d\n", SLOT_KIND_NAMES[s], ui_zone_live((slot_kind_t)s));
    }
    for (int s = 0; s < PARTICLE_SLOT_COUNT; s++) {
        fprintf(out, "%s %d\n", PARTICLE_NAMES[s], particle_zone_live((particle_slot_kind_t)s));
    }
    for (int s = 0; s < PARTICLE_SLOT_COUNT; s++) {
        fprintf(out, "max_live %s %d\n", PARTICLE_NAMES[s],
                particle_zone_max_live((particle_slot_kind_t)s));
    }
    for (int s = PARTICLE_SLOT_LIST; s < PARTICLE_SLOT_COUNT; s++) {
        fprintf(out, "max_bytes %s %lu\n", PARTICLE_NAMES[s],
                (unsigned long)particle_zone_max_bytes((particle_slot_kind_t)s));
    }
    for (int s = PARTICLE_SLOT_LIST; s < PARTICLE_SLOT_COUNT; s++) {
        if (s == PARTICLE_SLOT_BURST) continue;
        fprintf(out, "max_elements %s %d\n", PARTICLE_NAMES[s],
                particle_zone_max_elements((particle_slot_kind_t)s));
    }
    fprintf(out, "game %lu\n", (unsigned long)game_zone_used());
    fprintf(out, "max_bytes game %lu\n", (unsigned long)game_zone_max());
    fprintf(out, "load %lu\n", (unsigned long)load_zone_used());
    fprintf(out, "max_bytes load %lu\n", (unsigned long)game_zone_load_zone_max());
    fclose(out);
}

static void write_digest_line(void *context, const char *line) {
    fputs(line, (FILE *)context);
}

static int write_dependencies(const char *path, const char *capture_path, const char *fixture_path) {
    if (fixture_path[0] == '\0') {
        remove(path);
        return 1;
    }
    FILE *out = fopen(path, "w");
    int written = out != NULL && fprintf(out, "%s: %s\n%s:\n", capture_path, fixture_path, fixture_path) > 0;
    if (out != NULL && fclose(out) != 0) written = 0;
    if (!written) fprintf(stderr, "replay: cannot write the dependency file '%s'\n", path);
    return written;
}

/* Returns the replay's exit status. */
static int write_save_hex(const char *path) {
    static uint8_t bytes[SAVE_STATE_MAX_BYTES];
    static save_state_t decoded;
    int count = save_storage_read(bytes, (int)sizeof bytes);
    if (count == 0) {
        remove(path);
        fprintf(stderr, "replay: no stored record at the end of the run, '%s' not written\n", path);
        return 1;
    }
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "replay: cannot open the save file '%s'\n", path);
        return 1;
    }
    static const char hex_digits[] = "0123456789abcdef";
    for (int i = 0; i < count; i++) {
        fputc(hex_digits[bytes[i] >> 4], out);
        fputc(hex_digits[bytes[i] & 0x0f], out);
    }
    fputc('\n', out);
    if (fclose(out) != 0) {
        fprintf(stderr, "replay: cannot write the save file '%s'\n", path);
        return 1;
    }
    int decodes = save_state_decode(bytes, count, &decoded);
    printf("save: bytes %d\n", count);
    printf("save: version %d\n", count >= 6 ? bytes[4] | bytes[5] << 8 : -1);
    printf("save: decodes %d\n", decodes);
    if (!decodes) return 1;
    state_write_save_summary(stdout, &decoded, "save: ");
    return 0;
}

typedef struct {
    capture_list_t list;
    int next;
    screencast_writer_t frames;
    FILE *counts;
    /* Frame F-2 of the next display entry, kept until F. */
    p8_vram_t displayed;
    int displayed_counts[DRAW_FILTER_FAMILY_COUNT];
} captures_t;

/* Returns the length read, or -1. */
static long read_text(const char *path, char *text, size_t cap) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return -1;
    size_t length = fread(text, 1, cap - 1, file);
    int whole = feof(file);
    fclose(file);
    if (!whole) return -1;
    text[length] = '\0';
    return (long)length;
}

static void recording_name(const char *path, char name[CAPTURE_LIST_REPLAY_CAP]) {
    const char *base = strrchr(path, '/');
    base = base != NULL ? base + 1 : path;
    const char *dot = strrchr(base, '.');
    size_t length = dot != NULL && strcmp(dot, ".json") == 0 ? (size_t)(dot - base) : strlen(base);
    snprintf(name, CAPTURE_LIST_REPLAY_CAP, "%.*s", (int)length, base);
}

static FILE *open_in(const char *dir, const char *file_name, const char *mode) {
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, file_name);
    FILE *file = fopen(path, mode);
    if (file == NULL) fprintf(stderr, "replay: cannot open '%s'\n", path);
    return file;
}

/* Returns 0 after naming the failure; installs the draw filter on success. */
static int captures_open(captures_t *captures, const char *list_path, const char *dir, const char *replay_path,
                         int frame_count) {
    static char text[1 << 14];
    if (dir == NULL) {
        fprintf(stderr, "replay: CAPTURE_LIST needs CAPTURE_DIR\n");
        return 0;
    }
    if (read_text(list_path, text, sizeof text) < 0) {
        fprintf(stderr, "replay: cannot read the capture list '%s'\n", list_path);
        return 0;
    }
    char name[CAPTURE_LIST_REPLAY_CAP];
    recording_name(replay_path, name);
    char error[CAPTURE_LIST_ERROR_CAP];
    if (!capture_list_parse(text, name, frame_count, &captures->list, error)) {
        fprintf(stderr, "replay: %s: %s\n", list_path, error);
        return 0;
    }
    FILE *script = open_in(dir, "capture_script.txt", "w");
    if (script == NULL) return 0;
    for (int c = 0; c < captures->list.count; c++) {
        char line[CAPTURE_LIST_SCRIPT_LINE_CAP];
        capture_list_script_line(&captures->list.entries[c], line);
        fputs(line, script);
    }
    if (fclose(script) != 0) return 0;
    captures->counts = open_in(dir, "counts.txt", "w");
    if (captures->counts == NULL) return 0;
    char frames_path[1024];
    snprintf(frames_path, sizeof frames_path, "%s/pc_frames.bin", dir);
    if (!screencast_writer_open(&captures->frames, frames_path)) {
        fprintf(stderr, "replay: cannot open '%s'\n", frames_path);
        return 0;
    }
    captures->next = 0;
    draw_filter_install();
    return 1;
}

static const capture_list_entry_t *captures_at(const captures_t *captures, int frame) {
    if (captures->next >= captures->list.count) return NULL;
    const capture_list_entry_t *entry = &captures->list.entries[captures->next];
    return entry->frame == frame ? entry : NULL;
}

/* The mask to draw frame with: a display entry draws its own frame unfiltered. */
static unsigned captures_mask(const captures_t *captures, int frame) {
    const capture_list_entry_t *entry = captures_at(captures, frame);
    return entry != NULL && entry->mask != CAPTURE_LIST_DISPLAY_MASK ? entry->mask : DRAW_FILTER_ALL;
}

static int captures_display_two_frames_later(const captures_t *captures, int frame) {
    for (int c = captures->next; c < captures->list.count; c++) {
        const capture_list_entry_t *entry = &captures->list.entries[c];
        if (entry->frame == frame + 2) return entry->mask == CAPTURE_LIST_DISPLAY_MASK;
        if (entry->frame > frame + 2) return 0;
    }
    return 0;
}

static int captures_write(captures_t *captures, int frame, const p8_vram_t *mem, const int counts[]) {
    int ok = screencast_write_frame(&captures->frames, mem);
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        fprintf(captures->counts, "%d %s %d\n", frame, capture_list_family_name((draw_filter_family_t)family),
                counts[family]);
    }
    captures->next++;
    return ok;
}

/* Returns 0 on a write failure. A display entry at F writes frame F-2 and its counts, under F. */
static int captures_frame_drawn(captures_t *captures, int frame, const p8_vram_t *mem) {
    int counts[DRAW_FILTER_FAMILY_COUNT];
    for (int family = 0; family < DRAW_FILTER_FAMILY_COUNT; family++) {
        counts[family] = draw_filter_count((draw_filter_family_t)family);
    }
    if (captures_display_two_frames_later(captures, frame)) {
        captures->displayed = *mem;
        memcpy(captures->displayed_counts, counts, sizeof counts);
    }
    const capture_list_entry_t *entry = captures_at(captures, frame);
    int ok = 1;
    if (entry != NULL && entry->mask == CAPTURE_LIST_DISPLAY_MASK) {
        ok = captures_write(captures, frame, &captures->displayed, captures->displayed_counts);
    } else if (entry != NULL) {
        ok = captures_write(captures, frame, mem, counts);
    }
    draw_filter_frame_end();
    return ok;
}

static int captures_close(captures_t *captures) {
    int counts_ok = fclose(captures->counts) == 0;
    return screencast_writer_close(&captures->frames) && counts_ok;
}

static int write_state(const char *path, int cheat_activations) {
    FILE *out = fopen(path, "w");
    if (out == NULL) {
        fprintf(stderr, "replay: cannot open the state file '%s'\n", path);
        return 0;
    }
    state_write(out);
    state_write_cheat_activations(out, cheat_activations);
    fclose(out);
    return 1;
}

typedef struct {
    const char *path;
    int playbacks;
    int cheat_activations;
    int return_pending;
} attract_state_t;

static int write_attract_return(const attract_state_t *attract) {
    char path[1024];
    snprintf(path, sizeof path, "%s.%d.return", attract->path, attract->playbacks);
    if (!write_state(path, attract->cheat_activations)) return 0;
    snprintf(path, sizeof path, "%s.%d.return.hex", attract->path, attract->playbacks);
    return write_save_hex(path) == 0;
}

static int write_attract_state_after_frame(attract_state_t *attract, const input_step_t *step, int cheat) {
    if (attract->return_pending) {
        attract->return_pending = 0;
        if (!write_attract_return(attract)) return 0;
    }
    if (input_step_attract_phase(step) != ATTRACT_PHASE_PLAYING) return 1;
    int demo_frame = input_step_attract_demo_frame(step);
    if (demo_frame == 1) attract->cheat_activations = 0;
    attract->cheat_activations += cheat;
    if (demo_frame != demo_data_frame_count) return 1;
    char path[1024];
    attract->playbacks++;
    attract->return_pending = 1;
    snprintf(path, sizeof path, "%s.%d", attract->path, attract->playbacks);
    return write_state(path, attract->cheat_activations);
}

enum { TITLE_FRAMES_BETWEEN_STARTS = 30 };

static void play_title_between_starts(input_step_t *step, int metered) {
    static const p8_input_frame_t empty_frame = {0, 0, 0, 0};
    record_start_return(step);
    for (int frame = 0; frame < TITLE_FRAMES_BETWEEN_STARTS; frame++) {
        input_step_run(step, &empty_frame);
        cart_update60();
        cart_draw();
        if (metered) p8_draw_meter_frame_end();
    }
}

static int read_starts(void) {
    const char *value = getenv("REPLAY_STARTS");
    if (value == NULL || strcmp(value, "1") == 0) return 1;
    if (strcmp(value, "2") == 0) return 2;
    fprintf(stderr, "replay: REPLAY_STARTS is 1 or 2, not '%s'\n", value);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2 || argv[1][0] == '-') {
        fprintf(stderr, "usage: replay <events.json> [<capture.bin> [<dump.txt>]]\n");
        return 2;
    }
    const char *replay_path = argv[1];
    const char *capture_path = (argc > 2) ? argv[2] : NULL;
    const char *dump_path = (argc > 3) ? argv[3] : "tmp/replay_dump.txt";
    const char *dependencies_path = getenv("REPLAY_DEPS");
    if (dependencies_path != NULL && capture_path == NULL) {
        fprintf(stderr, "usage: REPLAY_DEPS=<file> replay <events.json> <capture.bin> [<dump.txt>]\n");
        return 2;
    }
    int starts = read_starts();
    if (starts == 0) return 2;
    if (starts == 2 && capture_path != NULL && strcmp(capture_path, "/dev/null") == 0) capture_path = NULL;
    if (starts == 2 && capture_path != NULL) {
        fprintf(stderr, "replay: REPLAY_STARTS=2 checks states only, it takes no capture file\n");
        return 2;
    }
    if (starts == 2 && getenv("PAD_METRICS") != NULL) {
        fprintf(stderr, "replay: REPLAY_STARTS=2 checks states only, it takes no PAD_METRICS\n");
        return 2;
    }
    if (starts == 2 && (getenv("DIGEST") != NULL || getenv("CAPTURE_LIST") != NULL)) {
        fprintf(stderr, "replay: REPLAY_STARTS=2 checks states only, it takes no DIGEST or CAPTURE_LIST\n");
        return 2;
    }
    const char *attract_state_path = getenv("ATTRACT_STATE");
    if (starts == 2 && attract_state_path != NULL) {
        fprintf(stderr, "replay: ATTRACT_STATE cycles through the attract mode, it takes no REPLAY_STARTS=2\n");
        return 2;
    }

    p8_vram_t memory;
    p8_vram_init(&memory);

    p8_graphics_t graphics;
    p8_gfx_init(&graphics);

    p8_sprite_sheet_load(&memory);

    const char *digest_path = getenv("DIGEST");
    vram_digest_sink_t digest = { write_digest_line, NULL };
    if (digest_path != NULL) {
        digest.context = fopen(digest_path, "w");
        if (digest.context == NULL) {
            fprintf(stderr, "replay: cannot open the digest file '%s'\n", digest_path);
            return 1;
        }
        vram_digest_write_sprite_sheet(&digest, &memory);
    }

    static replay_source_t source;
    replay_source_status_t source_status = replay_source_open(&source, replay_path);
    assert(source_status == REPLAY_SOURCE_LOADED);
    int seeded = replay_source_seed_storage(&source);
    assert(seeded);
    int resume_start = replay_source_start(&source) == REPLAY_SOURCE_START_RESUME;
    if (resume_start && attract_state_path != NULL) {
        fprintf(stderr, "replay: %s: ATTRACT_STATE needs a title-start file, this one says \"start\": \"resume\"\n",
                replay_path);
        return 2;
    }
    if (starts == 2 && !resume_start) {
        fprintf(stderr, "replay: %s: REPLAY_STARTS=2 needs a \"start\": \"resume\" file, this one starts on the title\n",
                replay_path);
        return 1;
    }
    static bindings_t bindings;
    static replay_source_bindings_report_t bindings_report;
    replay_source_bindings_status_t bindings_status =
        replay_source_read_bindings(&source, &bindings_compiled, &bindings, &bindings_report);
    if (resume_start && bindings_status != REPLAY_SOURCE_BINDINGS_DEFAULT) {
        fprintf(stderr, "replay: %s: a \"start\": \"resume\" file plays on the default bindings, it takes no \"bindings\" member\n",
                replay_path);
        return 1;
    }
    if (bindings_status != REPLAY_SOURCE_BINDINGS_DEFAULT && bindings_status != REPLAY_SOURCE_BINDINGS_FIXTURE) {
        fprintf(stderr, "replay: %s\n", bindings_report.message);
        return 1;
    }
    if (dependencies_path != NULL &&
        !write_dependencies(dependencies_path, capture_path, bindings_report.fixture_path)) return 1;
    p8_input_t *input = &source.recording;

    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(input);

    const char *capture_list_path = getenv("CAPTURE_LIST");
    static captures_t captures;
    if (capture_list_path != NULL
        && !captures_open(&captures, capture_list_path, getenv("CAPTURE_DIR"), replay_path, input->frame_count)) {
        return 1;
    }

    const char *state_path = getenv("STATE");
    if (state_path != NULL) p8_draw_meter_install();

    cart_load();
    cart_init();

    /* Every capture failure exits 1: a truncated capture yields a wrong hash that reads as a regression. */
    screencast_writer_t capture = { NULL };
    if (capture_path && !screencast_writer_open(&capture, capture_path)) {
        fprintf(stderr, "replay: cannot open the capture file '%s'\n",
                capture_path);
        return 1;
    }

    int frame_count = input->frame_count;
    static int i;
    if (state_path != NULL) state_write_on_exit(state_path, &i);
    input_step_t step;
    input_step_init(&step, &bindings);
    input_step_set_attract_after(&step, replay_source_attract_after(&source));
    if (resume_start && !record_start_resume(&step, source.initial_save, source.initial_save_count)) {
        fprintf(stderr, "replay: %s: the initial_save does not decode\n", replay_path);
        return 1;
    }
    const char *pad_metrics_path = getenv("PAD_METRICS");
    if (pad_metrics_path != NULL) {
        bump_zone_init(&pad_metrics_scratch, "pad metrics scratch", pad_metrics_cells, PAD_METRICS_SCRATCH_BYTES);
        pad_metrics_init_judging(&pad_metrics, &pad_metrics_scratch);
        pad_metrics_observe(&pad_metrics, &step);
    }
    int cheat_activations = 0;
    attract_state_t attract_state = {attract_state_path, 0, 0, 0};
    for (int start = 1; start <= starts; start++) {
        if (start > 1) {
            play_title_between_starts(&step, state_path != NULL);
            input->current_frame = 0;
            int resumed = record_start_resume(&step, source.initial_save, source.initial_save_count);
            assert(resumed);
            (void)resumed;
            cheat_activations = 0;
        }
        for (i = 0; i < frame_count; i++) {
            const p8_input_frame_t *frame = &input->frames[input->current_frame];
            int cheat = input_step_run(&step, frame);
            cheat_activations += cheat;
            cart_update60();
            if (capture_list_path != NULL) draw_filter_set_mask(captures_mask(&captures, i));
            cart_draw();
            if (state_path != NULL) p8_draw_meter_frame_end();
            if (capture_list_path != NULL && !captures_frame_drawn(&captures, i, &memory)) {
                fprintf(stderr, "replay: cannot write the capture outputs of frame %d\n", i);
                return 1;
            }
            if (digest_path != NULL && vram_digest_samples_frame(i, frame_count)) {
                vram_digest_write_screen(&digest, &memory, i);
            }
            if (pad_metrics_path != NULL) pad_metrics_record(&pad_metrics, &step, frame->pad_buttons, cheat);
            if (attract_state_path != NULL && !write_attract_state_after_frame(&attract_state, &step, cheat)) return 1;
            /* After cart_draw(): at the top of the loop the capture would be shifted by one frame. */
            if (capture_path && !screencast_write_frame(&capture, &memory)) {
                fprintf(stderr, "replay: cannot write frame %d to '%s'\n",
                        i, capture_path);
                screencast_writer_close(&capture);
                return 1;
            }
            if (i < frame_count - 1) {
                p8_input_step(input);
            }
        }
        if (start < starts && state_path != NULL) {
            char first_state_path[1024];
            snprintf(first_state_path, sizeof first_state_path, "%s.start1", state_path);
            if (!write_state(first_state_path, cheat_activations)) return 1;
        }
    }

    /* Before the text dump: a fresh dump would make a run whose capture is lost look normal. */
    if (capture_path && !screencast_writer_close(&capture)) {
        fprintf(stderr, "replay: cannot close the capture file '%s'\n",
                capture_path);
        return 1;
    }

    if (capture_list_path != NULL && !captures_close(&captures)) {
        fprintf(stderr, "replay: cannot close the capture outputs in '%s'\n", getenv("CAPTURE_DIR"));
        return 1;
    }

    if (digest_path != NULL) {
        vram_digest_write_map(&digest, &memory);
        if (fclose(digest.context) != 0) {
            fprintf(stderr, "replay: cannot write the digest file '%s'\n", digest_path);
            return 1;
        }
    }

    FILE *dump_out = fopen(dump_path, "w");
    assert(dump_out != NULL);
    int dump_ok = dump_screen(&memory, dump_out);
    fclose(dump_out);
    assert(dump_ok);

    if (state_path != NULL) {
        state_release();
        if (!write_state(state_path, cheat_activations)) return 1;
    }

    if (pad_metrics_path != NULL && !write_pad_metrics(pad_metrics_path)) return 1;

    const char *fill_path = getenv("REPLAY_FILL");
    if (fill_path != NULL) {
        write_fill(fill_path);
    }

    const char *save_hex_path = getenv("SAVE_HEX");
    int status = save_hex_path != NULL ? write_save_hex(save_hex_path) : 0;

    replay_source_close(&source);

    printf("replay: %d frame(s) run against %s; framebuffer dumped to %s\n",
           frame_count, replay_path, dump_path);
    if (capture_path) {
        printf("replay: %d frame(s) captured to %s\n", frame_count, capture_path);
    }
    return status;
}
