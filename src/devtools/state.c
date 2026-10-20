#include "devtools/state.h"

#include <assert.h>
#include <stdlib.h>

#include "devtools/state_format.h"

static void write_file_line(void *context, const char *line) {
    fputs(line, (FILE *)context);
}

static state_sink_t file_sink(FILE *out) {
    assert(out != NULL);
    state_sink_t sink = { write_file_line, out };
    return sink;
}

void state_write(FILE *out) {
    state_sink_t sink = file_sink(out);
    state_format(&sink);
}

void state_write_save_summary(FILE *out, const save_state_t *state, const char *prefix) {
    state_sink_t sink = file_sink(out);
    state_format_save_summary(&sink, state, prefix);
}

void state_write_cheat_activations(FILE *out, int count) {
    state_sink_t sink = file_sink(out);
    state_format_cheat_activations(&sink, count);
}

static const char *g_exit_path;
static const int *g_exit_frame;
static int g_exit_registered;

static void write_on_exit(void) {
    if (g_exit_path == NULL) return;
    FILE *out = fopen(g_exit_path, "w");
    if (out == NULL) return;
    state_sink_t sink = file_sink(out);
    state_format_stopped(&sink, *g_exit_frame);
    state_format(&sink);
    fclose(out);
    fprintf(stderr, "state: stopped at frame %d, state written to %s\n", *g_exit_frame, g_exit_path);
}

void state_write_on_exit(const char *path, const int *frame) {
    assert(path != NULL && frame != NULL);
    g_exit_path = path;
    g_exit_frame = frame;
    if (!g_exit_registered) {
        int registered = atexit(write_on_exit) == 0;
        assert(registered);
        (void)registered;
        g_exit_registered = 1;
    }
}

void state_release(void) {
    g_exit_path = NULL;
}
