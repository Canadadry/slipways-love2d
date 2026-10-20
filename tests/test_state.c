#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "devtools/state.h"
#include "engine/zone_sizes.h"
#include "p8/gpu_sizes.h"
#include "game/save_state.h"
#include "p8/input.h"
#include "p8/fatal.h"

#define STATE "tmp/test_state.txt"
#define REPLAY_TO(events, out) "STATE=" STATE " ./tmp/replay " events " tmp/test_state.bin tmp/test_state_dump.txt > " out
#define REPLAY(events) REPLAY_TO(events, "/dev/null")
#define SAVE_HEX "tmp/test_state_save.hex"
#define SAVE_OUT "tmp/test_state_save.out"

static char text[1 << 16];

static void replay(const char *command) {
    remove(STATE);
    assert(system(command) == 0);
    FILE *f = fopen(STATE, "r");
    assert(f != NULL && "state file missing");
    size_t n = fread(text, 1, sizeof text - 1, f);
    text[n] = '\0';
    fclose(f);
}

static char save_out[1 << 12];

static void read_file(const char *path, char *buffer, size_t cap) {
    FILE *f = fopen(path, "r");
    assert(f != NULL);
    size_t n = fread(buffer, 1, cap - 1, f);
    buffer[n] = '\0';
    fclose(f);
}

static int has(const char *line) {
    char pattern[128];
    snprintf(pattern, sizeof pattern, "\n%s\n", line);
    return strstr(text, pattern) != NULL;
}

static const char *const MEMORY_LINES[] = {
    "zone frame", "zone ui", "zone particles", "zone game", "zone load",
    "cap button", "cap modal_backdrop", "cap message_box", "cap toast", "cap row", "cap ephemerals",
    "cap bytes_button", "cap bytes_modal_backdrop", "cap bytes_message_box", "cap bytes_toast", "cap bytes_row",
    "cap bytes_ephemerals", "cap particle", "cap particles", "cap particles_help_screen", "cap burst",
    "cap transit_dots", "cap exit_queue", "cap entity_table", "cap elements_particles", "cap elements_particles_help_screen",
    "cap elements_transit_dots", "cap elements_ephemerals", "cap bytes_particles", "cap bytes_particles_help_screen", "cap bytes_burst",
    "zone gpu_frame", "zone gpu_build", "cap gpu_cluts", "cap gpu_patterns",
};

static long memory_value(const char *line, int column) {
    char prefix[16], name[64], v[3][32];
    assert(sscanf(line, "%15s %63s %31s %31s %31s", prefix, name, v[0], v[1], v[2]) == 5);
    return strcmp(v[column], "-") == 0 ? -1 : atol(v[column]);
}

/* The line of that name, without its newline, in a buffer the next call
 * overwrites; NULL if absent. */
static const char *memory_line(const char *name) {
    static char line[160];
    char pattern[96];
    snprintf(pattern, sizeof pattern, "\n%s ", name);
    const char *at = strstr(text, pattern);
    if (at == NULL) return NULL;
    at++;
    snprintf(line, sizeof line, "%.*s", (int)strcspn(at, "\n"), at);
    return line;
}

static void memory_lines_have_peak_and_limit(void) {
    for (size_t i = 0; i < sizeof MEMORY_LINES / sizeof MEMORY_LINES[0]; i++) {
        const char *line = memory_line(MEMORY_LINES[i]);
        assert(line != NULL && "a zone or cap line is missing");
        char copy[160];
        snprintf(copy, sizeof copy, "%s", line);
        char extra[8];
        char prefix[16], name[64], v[3][32];
        assert(sscanf(copy, "%15s %63s %31s %31s %31s %7s", prefix, name, v[0], v[1], v[2], extra) == 5 &&
               "five columns, prefix included");
        long current = memory_value(copy, 0), peak = memory_value(copy, 1), limit = memory_value(copy, 2);
        assert(peak >= 0 && limit > 0);
        assert(peak >= current && "the peak is never under the current");
        assert(peak <= limit && "a clean replay stays within its limits");
    }
    const char *frame = memory_line("zone frame");
    assert(memory_value(frame, 1) > 0);
    assert(memory_value(frame, 2) == (long)FRAME_ZONE_BYTES);
    assert(memory_value(memory_line("cap entity_table"), 1) > 0 && "every constructed entity takes a slot");
    char budget[64];
    snprintf(budget, sizeof budget, "budget zones %lu %lu", (unsigned long)ZONE_BUDGET_USED_BYTES,
             (unsigned long)ZONE_BUDGET_BYTES);
    assert(has(budget) && "the state carries the zone budget of the build");
}

static int hex_value(char digit) {
    if (digit >= '0' && digit <= '9') return digit - '0';
    if (digit >= 'a' && digit <= 'f') return digit - 'a' + 10;
    return -1;
}

static void save_line_matches_state(const char *name) {
    size_t length = strlen(name);
    const char *at = text;
    while (at != NULL && !(strncmp(at, name, length) == 0 && at[length] == ' ')) {
        at = strchr(at, '\n');
        if (at != NULL) at++;
    }
    assert(at != NULL && "the state dump has the line");
    char pattern[192];
    snprintf(pattern, sizeof pattern, "save: %.*s\n", (int)strcspn(at, "\n"), at);
    assert(strstr(save_out, pattern) != NULL && "the decoded record matches the state dump");
}

/* pad_tech_costs ends after its last action: the stored record is that game. */
static void save_hex_writes_the_stored_record(void) {
    static char hex[2 * SAVE_STATE_MAX_BYTES + 2];
    static uint8_t bytes[SAVE_STATE_MAX_BYTES];
    static save_state_t decoded;
    read_file(SAVE_HEX, hex, sizeof hex);
    size_t digits = strcspn(hex, "\n");
    assert(hex[digits] == '\n' && hex[digits + 1] == '\0' && "one line");
    assert(digits > 0 && digits % 2 == 0 && digits <= 2 * SAVE_STATE_MAX_BYTES);
    for (size_t i = 0; i < digits / 2; i++) {
        int high = hex_value(hex[2 * i]), low = hex_value(hex[2 * i + 1]);
        assert(high >= 0 && low >= 0 && "lowercase hex digits only");
        bytes[i] = (uint8_t)(high * 16 + low);
    }
    assert(save_state_decode(bytes, (int)(digits / 2), &decoded) == 1);
    read_file(SAVE_OUT, save_out, sizeof save_out);
    char count_line[64];
    snprintf(count_line, sizeof count_line, "save: bytes %d\n", (int)(digits / 2));
    assert(strstr(save_out, count_line) != NULL);
    snprintf(count_line, sizeof count_line, "save: version %d\n", SAVE_STATE_VERSION);
    assert(strstr(save_out, count_line) != NULL);
    assert(strstr(save_out, "save: decodes 1\n") != NULL);
    save_line_matches_state("difficulty");
    save_line_matches_state("clock year");
    save_line_matches_state("resources credits");
    save_line_matches_state("count nodes");
}

static void pad_tech_costs_rich_mine(void) {
    remove(SAVE_HEX);
    replay("SAVE_HEX=" SAVE_HEX " " REPLAY_TO("tests/e2e/pad_tech_costs.json", SAVE_OUT));
    assert(has("difficulty reasonable") && "the saved game is reasonable");
    assert(has("count nodes 90") && "90 planets, no structure, no protostar");
    save_hex_writes_the_stored_record();
    assert(has("count planets 90"));
    assert(has("count slipways 6"));
    assert(has("count colonies 6") && "p1, p31, p37, p54 and the two labs");
    assert(has("count structures 0"));
    assert(has("planet p31 colonized planet e world 223.89 276.36 screen 122.89 34.36 visible 1"));
    assert(has("planet p2 unknown planet - world 159.28 30.36 screen 58.28 -211.63 visible 0"));
    assert(has("slipway p54 p31 normal"));
    assert(has("slipway s2 p31 normal"));
    assert(has("camera world 101 242"));
    assert(has("techs space folding"));
    assert(has("techs xenofoods"));
    assert(has("cheat activations 1"));
    assert(strstr(text, "stopped") == NULL && "a replay that runs to the end writes no stop line");
    memory_lines_have_peak_and_limit();
    assert(memory_value(memory_line("cap message_box"), 1) >= 1 && "the replay opens a message box");
    assert(memory_value(memory_line("cap bytes_message_box"), 1) > 0);
    const char *gpu_frame = memory_line("zone gpu_frame");
    assert(memory_value(gpu_frame, 0) > 0 && memory_value(gpu_frame, 1) > 0 && "STATE installs the draw meter");
    assert(memory_value(gpu_frame, 2) == GPU_FRAME_BUFFER_BYTES);
    assert(memory_value(memory_line("zone gpu_build"), 2) == GPU_BUILD_BUFFER_BYTES);
    assert(memory_value(memory_line("cap gpu_cluts"), 1) > 0 && "the background map draws sprites");
}

#define CHEAT_CUT "tmp/test_state_cheat.json"

/* pad_research cut after f46, the frame its cheat fires: 99 science, $200 at
 * the last frame. */
static void pad_research_cheat(void) {
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/e2e/pad_research.json"));
    assert(input.frame_count > 47);
    assert(p8_input_save(input.frames, 47, CHEAT_CUT, NULL));
    p8_input_free(&input);
    replay(REPLAY(CHEAT_CUT));
    assert(strstr(text, "\nresources credits 200\n") != NULL);
    assert(strstr(text, "\nresources science 99\n") != NULL);
    assert(has("cheat activations 1"));
    memory_lines_have_peak_and_limit();
}

#define IDLE_TAIL "tmp/test_state_idle_tail.json"

/* Returns the "planet s1 ... world <x> <y>" prefix of the state just read; static buffer. */
static const char *placed_structure_world(void) {
    static char line[128];
    const char *at = strstr(text, "\nplanet s1 ");
    assert(at != NULL && "pad_build places s1");
    const char *screen = strstr(at, " screen ");
    assert(screen != NULL);
    snprintf(line, sizeof line, "%.*s", (int)(screen - at - 1), at + 1);
    return line;
}

/* A settled structure bobs every frame: its world y is printed at rest, so
 * idle frames at the end leave it unchanged. */
static void bobbing_structure_world_ignores_idle_frames(void) {
    replay(REPLAY("tests/e2e/pad_build.json"));
    static char before[128];
    snprintf(before, sizeof before, "%s", placed_structure_world());
    assert(strstr(before, "colonized processor") != NULL && "s1 is settled and bobs");
    p8_input_t input;
    p8_input_init(&input);
    assert(p8_input_load(&input, "tests/e2e/pad_build.json"));
    static p8_input_frame_t frames[1024];
    int count = input.frame_count;
    assert(count + 60 <= (int)(sizeof frames / sizeof frames[0]));
    memcpy(frames, input.frames, (size_t)count * sizeof frames[0]);
    for (int i = 0; i < 60; i++) {
        memset(&frames[count + i], 0, sizeof frames[0]);
    }
    p8_input_free(&input);
    assert(p8_input_save(frames, count + 60, IDLE_TAIL, NULL));
    replay(REPLAY(IDLE_TAIL));
    assert(strcmp(placed_structure_world(), before) == 0);
}

static void fatal_writes_the_stop_frame(void) {
    remove(STATE);
    fflush(stdout);
    pid_t pid = fork();
    assert(pid >= 0);
    if (pid == 0) {
        static const int frame = 120;
        assert(freopen("/dev/null", "w", stderr) != NULL);
        state_write_on_exit(STATE, &frame);
        p8_fatal("test zone full");
    }
    int status;
    assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 3 && "p8_fatal exits with 3");
    FILE *f = fopen(STATE, "r");
    assert(f != NULL);
    size_t n = fread(text, 1, sizeof text - 1, f);
    text[n] = '\0';
    fclose(f);
    assert(strncmp(text, "stopped p8_fatal frame 120\n", 27) == 0);
    assert(memory_line("zone frame") != NULL && memory_line("cap exit_queue") != NULL &&
           "the memory section follows the stop line");
}

int main(void) {
    pad_tech_costs_rich_mine();
    pad_research_cheat();
    bobbing_structure_world_ignores_idle_frames();
    fatal_writes_the_stop_frame();
    remove(STATE);
    remove(CHEAT_CUT);
    remove(IDLE_TAIL);
    remove(SAVE_HEX);
    remove(SAVE_OUT);
    remove("tmp/test_state.bin");
    remove("tmp/test_state_dump.txt");
    printf("test_state: all tests passed\n");
    return 0;
}
