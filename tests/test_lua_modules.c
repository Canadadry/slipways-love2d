#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_lua_modules_work"
#define ROOT DIR "/game"
#define REFERENCE DIR "/reference.txt"
#define OUT DIR "/out.txt"
#define SCRIPT "luajit tests/lua/test_modules.lua "

/* Static buffer, NUL-terminated, overwritten by each call. */
static const char *read_file(const char *path) {
    static char buf[1 << 16];
    FILE *file = fopen(path, "rb");
    assert(file != NULL);
    size_t len = fread(buf, 1, sizeof(buf) - 1, file);
    fclose(file);
    buf[len] = '\0';
    return buf;
}

static void write_file(const char *path, const char *text) {
    FILE *file = fopen(path, "w");
    assert(file != NULL);
    fputs(text, file);
    fclose(file);
}

/* Writes the lines in the reference format, frozen from the C (deleted in plan 4): a "= <module> <name> <bytes>"
 * header before each run of lines of one table. */
static void reset(const char *lines) {
    assert(system("rm -rf " ROOT " && mkdir -p " ROOT "/p8 " ROOT "/engine") == 0);
    FILE *file = fopen(REFERENCE, "w");
    assert(file != NULL);
    const char *line = lines;
    while (*line != '\0') {
        size_t id = strcspn(line, " ");
        id += 1 + strspn(line + id + 1, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_");
        const char *end = line;
        while (*end != '\0' && strncmp(end, line, id) == 0 && strchr("[. ", end[id]) != NULL) {
            end = strchr(end, '\n') + 1;
        }
        fprintf(file, "= %.*s %d\n%.*s", (int)id, line, (int)(end - line), (int)(end - line), line);
        line = end;
    }
    fclose(file);
}

/* The files are relative to ROOT, separated by spaces; the options go before
 * the arguments as given. The exit code is read back from a file: the raw
 * status of system() needs <sys/wait.h>. */
static int run_script_with(const char *options, const char *files) {
    char cmd[2048];
    char paths[1024] = "";
    char copy[1024];
    snprintf(copy, sizeof copy, "%s", files);
    for (char *name = strtok(copy, " "); name != NULL; name = strtok(NULL, " ")) {
        size_t used = strlen(paths);
        snprintf(paths + used, sizeof paths - used, " " ROOT "/%s", name);
    }
    snprintf(cmd, sizeof cmd, SCRIPT "%s " ROOT " " REFERENCE "%s > " OUT " 2>&1; echo $? > " DIR "/code", options,
             paths);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

static int run_script(const char *files) {
    return run_script_with("", files);
}

static int out_has(const char *text) {
    return strstr(read_file(OUT), text) != NULL;
}

static void write_palette(void) {
    write_file(ROOT "/p8/palette.lua", "local M = {}\n"
                                       "M.COLORS = 2\n"
                                       "M.RGB = { [0] = { red = 0, green = 1 }, { red = 255, green = 7 } }\n"
                                       "return M\n");
}

static const char *PALETTE_REFERENCE = "p8.palette COLORS 2\n"
                                       "p8.palette RGB[0].red 0\n"
                                       "p8.palette RGB[0].green 1\n"
                                       "p8.palette RGB[1].red 255\n"
                                       "p8.palette RGB[1].green 7\n";

static void test_a_clean_tree_passes_with_a_summary(void) {
    reset(PALETTE_REFERENCE);
    write_palette();
    write_file(ROOT "/main.lua", "love.load = nil\n");
    write_file(ROOT "/engine/zone.lua", "local palette = require('p8.palette')\nreturn { size = palette.COLORS }\n");
    assert(run_script("engine/zone.lua main.lua p8/palette.lua") == 0);
    assert(out_has("lua-test-modules: 2 modules loaded, 2 names and 5 lines equal to the reference\n"));
}

static void test_a_module_that_fails_to_load_or_requires_a_missing_one_fails(void) {
    reset(PALETTE_REFERENCE);
    write_palette();
    write_file(ROOT "/engine/zone.lua", "local M = {}\nerror('zone broke')\nreturn M\n");
    write_file(ROOT "/engine/vec.lua", "local fixed = require('p8.fixed')\nreturn {}\n");
    write_file(ROOT "/engine/geom.lua", "local M = {}\nM.width = love.graphics\nreturn M\n");
    assert(run_script("p8/palette.lua engine/zone.lua engine/vec.lua engine/geom.lua") == 1);
    assert(out_has("load engine.zone: " ROOT "/engine/zone.lua:2: zone broke\n"));
    assert(out_has("load engine.vec: " ROOT "/engine/vec.lua:1: module 'p8.fixed' not found"));
    assert(out_has("load engine.geom: " ROOT "/engine/geom.lua:2: attempt to index global 'love'"));
    assert(out_has("lua-test-modules: 3 modules fail to load, 0 of 2 names differ from the reference\n"));
}

static void test_a_differing_value_names_module_table_entry_and_both_values(void) {
    reset(PALETTE_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "local M = {}\n"
                                       "M.COLORS = 2\n"
                                       "M.RGB = { [0] = { red = 0, green = 1 }, { red = 254, green = 7 } }\n"
                                       "return M\n");
    assert(run_script("p8/palette.lua") == 1);
    assert(out_has("p8.palette RGB[1].red: reference 255, Lua 254\n"));
    assert(out_has("lua-test-modules: 0 modules fail to load, 1 of 2 names differ from the reference\n"));
}

static void test_a_table_absent_from_its_module_is_missing_in_lua(void) {
    reset(PALETTE_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "local M = {}\nM.COLORS = 2\nlocal RGB = {}\nreturn M\n");
    assert(run_script("p8/palette.lua") == 1);
    assert(out_has("p8.palette RGB: missing in Lua\n"));
    assert(!out_has("COLORS"));
}

static void test_an_entry_only_in_lua_is_extra_and_one_only_in_c_is_missing(void) {
    reset(PALETTE_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "local M = {}\n"
                                       "M.COLORS = 2\n"
                                       "M.RGB = { [0] = { red = 0, green = 1, blue = 3 }, { red = 255 } }\n"
                                       "return M\n");
    assert(run_script("p8/palette.lua") == 1);
    assert(out_has("p8.palette RGB[0].blue: extra in Lua, Lua 3\n"));
    assert(out_has("p8.palette RGB[1].green: missing in Lua, reference 7\n"));
}

static void test_at_most_five_differences_are_printed_per_table(void) {
    reset("p8.palette RGB[0] 1\np8.palette RGB[1] 1\np8.palette RGB[2] 1\np8.palette RGB[3] 1\n"
          "p8.palette RGB[4] 1\np8.palette RGB[5] 1\np8.palette RGB[6] 1\n");
    write_file(ROOT "/p8/palette.lua", "return { RGB = { [0] = 0, 0, 0, 0, 0, 0, 0 } }\n");
    assert(run_script("p8/palette.lua") == 1);
    assert(out_has("p8.palette RGB[0]: reference 1, Lua 0\n"));
    assert(!out_has("RGB[5]"));
    assert(out_has("p8.palette RGB: 2 more differences\n"));
}

static void test_a_function_field_compares_as_function_and_a_registered_table_by_name(void) {
    reset("p8.gfx SOFTWARE.cls function\n"
          "p8.gfx SOFTWARE.size 2\n"
          "p8.gfx BACKENDS[0] &SOFTWARE\n"
          "p8.gfx BACKENDS[1].cls function\n");
    write_file(ROOT "/p8/gfx.lua", "local M = {}\n"
                                   "M.SOFTWARE = { cls = function() end, size = 2 }\n"
                                   "M.BACKENDS = { [0] = M.SOFTWARE, { cls = print } }\n"
                                   "return M\n");
    assert(run_script("p8/gfx.lua") == 0);
    write_file(ROOT "/p8/gfx.lua", "local M = {}\n"
                                   "M.SOFTWARE = { cls = function() end, size = 2 }\n"
                                   "M.BACKENDS = { [0] = { cls = M.SOFTWARE.cls, size = 2 }, { cls = 1 } }\n"
                                   "return M\n");
    assert(run_script("p8/gfx.lua") == 1);
    assert(out_has("p8.gfx BACKENDS[0]: missing in Lua, reference &SOFTWARE\n"));
    assert(out_has("p8.gfx BACKENDS[1].cls: reference function, Lua 1\n"));
    assert(out_has("p8.gfx BACKENDS[0].size: extra in Lua, Lua 2\n"));
}

static void test_a_module_returning_its_table_is_compared_whole(void) {
    reset("p8.wave p8_wave[0] 0\np8.wave p8_wave[1] -6\n");
    write_file(ROOT "/p8/wave.lua", "local p8_wave = { [0] = 0, -6 }\nreturn p8_wave\n");
    assert(run_script("p8/wave.lua") == 0);
}

static void test_strings_and_keys_are_quoted_and_a_boolean_never_equals_c(void) {
    reset("p8.text NAMES[0] \"a b\\034\\010\"\n"
          "p8.text SIZEOF[\"void *\"] 8\n"
          "p8.text FLAG 1\n");
    write_file(ROOT "/p8/text.lua", "return { NAMES = { [0] = 'a b\"\\n' }, SIZEOF = { ['void *'] = 8 }, FLAG = true }\n");
    assert(run_script("p8/text.lua") == 1);
    assert(!out_has("NAMES"));
    assert(!out_has("SIZEOF"));
    assert(out_has("p8.text FLAG: reference 1, Lua true\n"));
}

static const char *DEFERRED_REFERENCE = "p8.palette COLORS 2\n"
                                        "engine.vec ORIGIN.x 0\n"
                                        "engine.vec UNIT 1\n";

static void test_a_deferred_module_requiring_a_module_not_yet_written_is_skipped_with_its_names(void) {
    reset(DEFERRED_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "return { COLORS = 2 }\n");
    write_file(ROOT "/engine/vec.lua", "local probe = require('game.probe')\nreturn { UNIT = 1 }\n");
    assert(run_script_with("--defer engine.vec", "p8/palette.lua engine/vec.lua") == 0);
    assert(out_has("lua-test-modules: 1 modules loaded, 1 deferred: engine.vec (2 names skipped), 1 names and 1 lines "
                   "equal to the reference\n"));
}

static void test_a_deferred_module_already_required_by_a_failed_deferred_one_is_skipped_too(void) {
    reset(DEFERRED_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "return { COLORS = 2 }\n");
    write_file(ROOT "/engine/vec.lua", "local zone = require('engine.zone')\nreturn { UNIT = 1 }\n");
    write_file(ROOT "/engine/zone.lua", "local particle = require('game.particle')\nreturn {}\n");
    assert(run_script_with("--defer engine.vec --defer engine.zone", "p8/palette.lua engine/vec.lua engine/zone.lua") ==
           0);
    assert(out_has("2 deferred: engine.vec, engine.zone (2 names skipped)"));
}

static void test_a_deferred_module_failing_for_another_reason_still_fails(void) {
    reset(DEFERRED_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "return { COLORS = 2 }\n");
    write_file(ROOT "/engine/vec.lua", "local M = {}\nerror('vec broke')\nreturn M\n");
    assert(run_script_with("--defer engine.vec", "p8/palette.lua engine/vec.lua") == 1);
    assert(out_has("load engine.vec: " ROOT "/engine/vec.lua:2: vec broke\n"));
    assert(out_has("engine.vec UNIT: missing in Lua\n"));
    write_file(ROOT "/engine/vec.lua", "error(\"module 'p8.palette' not found\")\n");
    assert(run_script_with("--defer engine.vec", "p8/palette.lua engine/vec.lua") == 1);
    assert(out_has("load engine.vec: " ROOT "/engine/vec.lua:1: module 'p8.palette' not found\n"));
}

static void test_a_deferred_module_that_loads_fails_until_dropped_from_the_list(void) {
    reset(DEFERRED_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "return { COLORS = 2 }\n");
    write_file(ROOT "/engine/vec.lua", "return { ORIGIN = { x = 0 }, UNIT = 1 }\n");
    assert(run_script_with("--defer engine.vec", "p8/palette.lua engine/vec.lua") == 1);
    assert(out_has("engine.vec loads now: drop it from the deferred modules\n"));
    assert(run_script("p8/palette.lua engine/vec.lua") == 0);
}

static void test_a_deferred_name_that_is_not_a_file_of_the_tree_fails(void) {
    reset(DEFERRED_REFERENCE);
    write_file(ROOT "/p8/palette.lua", "return { COLORS = 2 }\n");
    write_file(ROOT "/engine/vec.lua", "local probe = require('game.probe')\nreturn { UNIT = 1 }\n");
    assert(run_script_with("--defer engine.vec --defer engine.vex", "p8/palette.lua engine/vec.lua") == 1);
    assert(out_has("engine.vex is deferred but is not a file of the tree\n"));
}

int main(void) {
    assert(system("mkdir -p " DIR) == 0);
    test_a_clean_tree_passes_with_a_summary();
    test_a_module_that_fails_to_load_or_requires_a_missing_one_fails();
    test_a_differing_value_names_module_table_entry_and_both_values();
    test_a_table_absent_from_its_module_is_missing_in_lua();
    test_an_entry_only_in_lua_is_extra_and_one_only_in_c_is_missing();
    test_at_most_five_differences_are_printed_per_table();
    test_a_function_field_compares_as_function_and_a_registered_table_by_name();
    test_a_module_returning_its_table_is_compared_whole();
    test_strings_and_keys_are_quoted_and_a_boolean_never_equals_c();
    test_a_deferred_module_requiring_a_module_not_yet_written_is_skipped_with_its_names();
    test_a_deferred_module_already_required_by_a_failed_deferred_one_is_skipped_too();
    test_a_deferred_module_failing_for_another_reason_still_fails();
    test_a_deferred_module_that_loads_fails_until_dropped_from_the_list();
    test_a_deferred_name_that_is_not_a_file_of_the_tree_fails();
    puts("test_lua_modules: ok");
    return 0;
}
