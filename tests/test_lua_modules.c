#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_lua_modules_work"
#define ROOT DIR "/game"
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

static void reset(void) {
    assert(system("rm -rf " ROOT " && mkdir -p " ROOT "/p8 " ROOT "/engine") == 0);
}

/* The arguments are given as is after the script. The exit code is read back
 * from a file: the raw status of system() needs <sys/wait.h>. */
static int run_script_args(const char *args) {
    char cmd[2048];
    snprintf(cmd, sizeof cmd, SCRIPT "%s > " OUT " 2>&1; echo $? > " DIR "/code", args);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

/* The files are relative to ROOT, separated by spaces. */
static int run_script(const char *files) {
    char args[1024] = ROOT;
    char copy[1024];
    snprintf(copy, sizeof copy, "%s", files);
    for (char *name = strtok(copy, " "); name != NULL; name = strtok(NULL, " ")) {
        size_t used = strlen(args);
        snprintf(args + used, sizeof args - used, " " ROOT "/%s", name);
    }
    return run_script_args(args);
}

static int out_has(const char *text) {
    return strstr(read_file(OUT), text) != NULL;
}

static void write_palette(void) {
    write_file(ROOT "/p8/palette.lua", "local M = {}\nM.COLORS = 2\nreturn M\n");
}

static void test_a_clean_tree_passes_with_a_summary(void) {
    reset();
    write_palette();
    write_file(ROOT "/main.lua", "love.load = nil\n");
    write_file(ROOT "/engine/zone.lua", "local palette = require('p8.palette')\nreturn { size = palette.COLORS }\n");
    assert(run_script("engine/zone.lua main.lua p8/palette.lua") == 0);
    assert(strcmp(read_file(OUT), "lua-test-modules: 2 modules loaded\n") == 0);
}

static void test_each_module_that_fails_to_load_reports_its_own_error(void) {
    reset();
    write_palette();
    write_file(ROOT "/engine/zone.lua", "local M = {}\nerror('zone broke')\nreturn M\n");
    write_file(ROOT "/engine/vec.lua", "local fixed = require('p8.fixed')\nreturn {}\n");
    write_file(ROOT "/engine/geom.lua", "local M = {}\nM.width = love.graphics\nreturn M\n");
    assert(run_script("p8/palette.lua engine/zone.lua engine/vec.lua engine/geom.lua") == 1);
    assert(out_has("load engine.zone: " ROOT "/engine/zone.lua:2: zone broke\n"));
    assert(out_has("load engine.vec: " ROOT "/engine/vec.lua:1: module 'p8.fixed' not found"));
    assert(out_has("load engine.geom: " ROOT "/engine/geom.lua:2: attempt to index global 'love'"));
    assert(out_has("lua-test-modules: 3 modules fail to load\n"));
}

static void test_a_module_requiring_one_that_failed_reports_the_inner_missing_module(void) {
    reset();
    write_file(ROOT "/engine/vec.lua", "local zone = require('engine.zone')\nreturn {}\n");
    write_file(ROOT "/engine/zone.lua", "local particle = require('game.particle')\nreturn {}\n");
    assert(run_script("engine/zone.lua engine/vec.lua") == 1);
    assert(out_has("load engine.zone: " ROOT "/engine/zone.lua:1: module 'game.particle' not found"));
    assert(out_has("load engine.vec: " ROOT "/engine/zone.lua:1: module 'game.particle' not found"));
    assert(!out_has("loop or previous error"));
    assert(out_has("lua-test-modules: 2 modules fail to load\n"));
}

static void test_a_file_outside_the_game_directory_or_not_lua_is_a_usage_error(void) {
    reset();
    write_palette();
    write_file(DIR "/stray.lua", "return {}\n");
    write_file(ROOT "/p8/notes.txt", "notes\n");
    assert(run_script_args(ROOT " " DIR "/stray.lua") == 2);
    assert(out_has("usage: "));
    assert(run_script("p8/palette.lua p8/notes.txt") == 2);
    assert(out_has("usage: "));
    assert(run_script_args("") == 2);
    assert(out_has("usage: "));
}

int main(void) {
    assert(system("mkdir -p " DIR) == 0);
    test_a_clean_tree_passes_with_a_summary();
    test_each_module_that_fails_to_load_reports_its_own_error();
    test_a_module_requiring_one_that_failed_reports_the_inner_missing_module();
    test_a_file_outside_the_game_directory_or_not_lua_is_a_usage_error();
    puts("test_lua_modules: ok");
    return 0;
}
