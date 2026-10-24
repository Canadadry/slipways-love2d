#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DIR "tmp/test_lua_guard_work"
#define ROOT DIR "/game"
#define OUT DIR "/out.txt"
#define GUARD "luajit tools/lua_guard.lua " ROOT

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
    assert(system("rm -rf " ROOT " && mkdir -p " ROOT "/p8 " ROOT "/love " ROOT "/game/lovers") == 0);
}

/* The files are relative to ROOT, separated by spaces. The exit code is read
 * back from a file: the raw status of system() needs <sys/wait.h>. */
static int run_guard(const char *files) {
    char cmd[2048];
    char paths[1024] = "";
    char copy[1024];
    snprintf(copy, sizeof copy, "%s", files);
    for (char *name = strtok(copy, " "); name != NULL; name = strtok(NULL, " ")) {
        size_t used = strlen(paths);
        snprintf(paths + used, sizeof paths - used, " " ROOT "/%s", name);
    }
    snprintf(cmd, sizeof cmd, GUARD "%s > " OUT " 2>&1; echo $? > " DIR "/code", paths);
    assert(system(cmd) == 0);
    return atoi(read_file(DIR "/code"));
}

static int out_has(const char *text) {
    return strstr(read_file(OUT), text) != NULL;
}

static int out_lines(void) {
    int lines = 0;
    for (const char *c = read_file(OUT); *c != '\0'; c++) {
        lines += *c == '\n';
    }
    return lines;
}

static void test_a_file_that_does_not_compile_fails_with_its_line(void) {
    reset();
    write_file(ROOT "/p8/good.lua", "local M = {}\nreturn M\n");
    write_file(ROOT "/p8/bad.lua", "local M = {}\n\nfunction M.f(\nreturn M\n");
    assert(run_guard("p8/good.lua p8/bad.lua") == 1);
    assert(out_has(ROOT "/p8/bad.lua:4:"));
    assert(!out_has("good.lua"));
    assert(out_lines() == 1);
}

static void test_a_love_name_off_the_list_fails_but_not_in_strings_or_comments(void) {
    reset();
    write_file(ROOT "/love/screen.lua", "local M = {}\n"
                                        "function M.f()\n"
                                        "  love.graphics.print('x', 0, 0)\n"
                                        "  love.audio.play(nil)\n"
                                        "  love.timer.getTime()\n"
                                        "  love.filesystem.remove('a')\n"
                                        "  love.joystick.getJoystickCount()\n"
                                        "  love.image.newImageData(1, 1):encode('png')\n"
                                        "end\n"
                                        "local s = 'love.audio.play()' .. \"love.timer\\\".x\"\n"
                                        "local t = [==[\nlove.timer.sleep(1)\n]] ]==]\n"
                                        "-- love.timer.sleep(1)\n"
                                        "--[[ love.timer.sleep(1)\n]]\n"
                                        "--[=[ love.timer.sleep(1) ]]\n]=]\n"
                                        "return M\n");
    assert(run_guard("love/screen.lua") == 1);
    assert(out_has(ROOT "/love/screen.lua:3: love.graphics.print is not on the design's list\n"));
    assert(out_has(ROOT "/love/screen.lua:4: love.audio is not on the design's list\n"));
    assert(out_has(ROOT "/love/screen.lua:5: love.timer is not on the design's list\n"));
    assert(out_has(ROOT "/love/screen.lua:6: love.filesystem.remove is not on the design's list\n"));
    assert(out_has(ROOT "/love/screen.lua:7: love.joystick.getJoystickCount is not on the design's list\n"));
    assert(out_lines() == 5);
}

static void test_every_listed_use_passes(void) {
    reset();
    write_file(ROOT "/conf.lua", "function love.conf(t)\n  t.window.width = 128\nend\n");
    write_file(ROOT "/main.lua", "function love.load()\nend\n"
                                 "love.update = function(dt)\nend\n"
                                 "function love.draw()\nend\n"
                                 "function love.gamepadpressed(j, b)\nend\n"
                                 "love .gamepadreleased=function(j, b)\nend\n"
                                 "function love.resize(w, h)\nend\n"
                                 "local pads = love.joystick.getJoysticks()\n"
                                 "local data = love.filesystem.read('a')\n"
                                 "love.filesystem.write('a', data)\n"
                                 "local info = love . filesystem\n  .getInfo ('a')\n");
    write_file(ROOT "/love/screen.lua",
               "local M = {}\n"
               "function M.f(image, quad)\n"
               "  love.graphics.setDefaultFilter('nearest', 'nearest')\n"
               "  local canvas = love.graphics.newCanvas(128, 128)\n"
               "  love.graphics.setCanvas(canvas)\n"
               "  love.graphics.clear(0, 0, 0, 1)\n"
               "  love.graphics.setColor(1, 1, 1, 1)\n"
               "  love.graphics.points({ 1, 1 })\n"
               "  love.graphics.rectangle('fill', 0, 0, 1, 1)\n"
               "  love.graphics.push()\n"
               "  love.graphics.translate(1, 1)\n"
               "  love.graphics.pop()\n"
               "  local w, h = love.graphics.getDimensions()\n"
               "  local data = love.image.newImageData(1, 1)\n"
               "  local img = love.graphics.newImage(data)\n"
               "  local q = love.graphics.newQuad(0, 0, 1, 1, 1, 1)\n"
               "  love.graphics.draw(img, q, 0, 0)\n"
               "  data:setPixel(0, 0, 1, 1, 1, 1)\n"
               "end\n"
               "M.lovely = 1\n"
               "M.love = { graphics = 2 }\n"
               "return M\n");
    assert(run_guard("conf.lua main.lua love/screen.lua") == 0);
    assert(out_has("lua-guard: 3 files clean\n"));
}

static void test_a_callback_used_without_being_defined_fails(void) {
    reset();
    write_file(ROOT "/main.lua", "function love.load()\nend\n"
                                 "love.load()\n"
                                 "local draw = love.draw\n"
                                 "if love.update == function() end then end\n"
                                 "local function_ = 1 love.conf(nil)\n");
    assert(run_guard("main.lua") == 1);
    assert(out_has(ROOT "/main.lua:3: love.load is only allowed where it is defined\n"));
    assert(out_has(ROOT "/main.lua:4: love.draw is only allowed where it is defined\n"));
    assert(out_has(ROOT "/main.lua:5: love.update is only allowed where it is defined\n"));
    assert(out_has(ROOT "/main.lua:6: love.conf is only allowed where it is defined\n"));
    assert(out_lines() == 4);
}

static void test_an_alias_a_bracket_index_or_an_indirect_call_fails(void) {
    reset();
    write_file(ROOT "/love/screen.lua", "local l = love\n"
                                        "local g = love.graphics\n"
                                        "local h = love[\"graphics\"]\n"
                                        "love.graphics[\"points\"]({})\n"
                                        "local read = love.filesystem.read\n"
                                        "local x = 1 .. love\n");
    assert(run_guard("love/screen.lua") == 1);
    assert(out_has(ROOT "/love/screen.lua:1: love is aliased\n"));
    assert(out_has(ROOT "/love/screen.lua:2: love.graphics is aliased\n"));
    assert(out_has(ROOT "/love/screen.lua:3: love is indexed with brackets\n"));
    assert(out_has(ROOT "/love/screen.lua:4: love.graphics is indexed with brackets\n"));
    assert(out_has(ROOT "/love/screen.lua:5: love.filesystem.read is not called directly\n"));
    assert(out_has(ROOT "/love/screen.lua:6: love is aliased\n"));
    assert(out_lines() == 6);
}

static void test_graphics_and_image_outside_the_glue_directory_fail(void) {
    reset();
    write_file(ROOT "/p8/gfx.lua", "love.graphics.points({})\nlocal d = love.image.newImageData(1, 1)\n"
                                   "love.filesystem.read('a')\n");
    write_file(ROOT "/game/lovers/x.lua", "love.graphics.clear()\n");
    write_file(ROOT "/game/lovers/y.lua", "local w, h = love.graphics.getDimensions()\n");
    write_file(ROOT "/main.lua", "function love.draw()\n  love.graphics.clear()\nend\n");
    assert(run_guard("p8/gfx.lua game/lovers/x.lua game/lovers/y.lua main.lua") == 1);
    assert(out_has(ROOT "/p8/gfx.lua:1: love.graphics is only allowed in game/love/\n"));
    assert(out_has(ROOT "/p8/gfx.lua:2: love.image is only allowed in game/love/\n"));
    assert(out_has(ROOT "/game/lovers/x.lua:1: love.graphics is only allowed in game/love/\n"));
    assert(out_has(ROOT "/game/lovers/y.lua:1: love.graphics is only allowed in game/love/\n"));
    assert(out_has(ROOT "/main.lua:2: love.graphics is only allowed in game/love/\n"));
    assert(out_lines() == 5);
}

static void test_an_undeclared_global_read_fails_with_file_line_and_name(void) {
    reset();
    write_file(ROOT "/p8/typo.lua", "local M = {}\n"
                                    "local fixed = require('p8.fixed')\n"
                                    "M.x = fixd\n"
                                    "print(M.x)\n"
                                    "return M\n");
    assert(run_guard("p8/typo.lua") == 1);
    assert(out_has(ROOT "/p8/typo.lua:3: global fixd is read but not declared\n"));
    assert(out_has(ROOT "/p8/typo.lua:4: global print is read but not declared\n"));
    assert(out_lines() == 2);
}

static void test_a_global_write_fails_except_debug_state_in_main(void) {
    reset();
    write_file(ROOT "/main.lua", "function debug_state()\n  return ''\nend\n"
                                 "function love.load()\nend\n");
    write_file(ROOT "/p8/leak.lua", "local M = {}\n"
                                    "function helper()\nend\n"
                                    "debug_state = nil\n"
                                    "return M\n");
    write_file(ROOT "/game/lovers/main.lua", "counter = 1\n");
    assert(run_guard("main.lua p8/leak.lua game/lovers/main.lua") == 1);
    assert(out_has(ROOT "/p8/leak.lua:2: global helper is written\n"));
    assert(out_has(ROOT "/p8/leak.lua:4: global debug_state is written\n"));
    assert(out_has(ROOT "/game/lovers/main.lua:1: global counter is written\n"));
    assert(out_lines() == 3);
}

static void test_reads_are_found_in_nested_functions_and_locals_shadow_globals(void) {
    reset();
    write_file(ROOT "/p8/deep.lua", "local M = {}\n"
                                    "local bit = require('bit')\n"
                                    "local print = function() end\n"
                                    "function M.f(io)\n"
                                    "  return function()\n"
                                    "    print(io, bit.band(1, 1), os)\n"
                                    "  end\n"
                                    "end\n"
                                    "return M\n");
    assert(run_guard("p8/deep.lua") == 1);
    assert(out_has(ROOT "/p8/deep.lua:6: global os is read but not declared\n"));
    assert(out_lines() == 1);
}

static void test_every_listed_global_read_passes(void) {
    reset();
    write_file(ROOT "/p8/ok.lua",
               "local M = {}\n"
               "local x = { assert, error, getmetatable, ipairs, next, pairs, pcall, rawequal, rawget, rawset,\n"
               "  require, select, setmetatable, tonumber, tostring, type, unpack, xpcall, math, string, table }\n"
               "function M.f()\n  return love.joystick.getJoysticks()\nend\n"
               "return M\n");
    assert(run_guard("p8/ok.lua") == 0);
    assert(out_has("lua-guard: 1 files clean\n"));
}

static void test_touch_and_mouse_polling_passes_outside_the_glue_directory(void) {
    reset();
    write_file(ROOT "/main.lua", "function love.update(dt)\n"
                                 "  for _, id in ipairs(love.touch.getTouches()) do\n"
                                 "    local x, y = love.touch.getPosition(id)\n"
                                 "  end\n"
                                 "  local mx, my = love.mouse.getPosition()\n"
                                 "  local down = love.mouse.isDown(1)\n"
                                 "end\n");
    write_file(ROOT "/game/lovers/pad.lua", "local M = {}\n"
                                            "function M.f()\n"
                                            "  return love.touch.getTouches(), love.mouse.isDown(1)\n"
                                            "end\n"
                                            "return M\n");
    assert(run_guard("main.lua game/lovers/pad.lua") == 0);
    assert(out_has("lua-guard: 2 files clean\n"));
}

static void test_another_touch_or_mouse_name_fails(void) {
    reset();
    write_file(ROOT "/main.lua", "local p = love.touch.getPressure(nil)\n"
                                 "love.mouse.setVisible(false)\n"
                                 "function love.touchpressed(id, x, y)\nend\n"
                                 "function love.mousepressed(x, y, b)\nend\n"
                                 "local get = love.touch.getTouches\n");
    assert(run_guard("main.lua") == 1);
    assert(out_has(ROOT "/main.lua:1: love.touch.getPressure is not on the design's list\n"));
    assert(out_has(ROOT "/main.lua:2: love.mouse.setVisible is not on the design's list\n"));
    assert(out_has(ROOT "/main.lua:3: love.touchpressed is not on the design's list\n"));
    assert(out_has(ROOT "/main.lua:5: love.mousepressed is not on the design's list\n"));
    assert(out_has(ROOT "/main.lua:7: love.touch.getTouches is not called directly\n"));
    assert(out_lines() == 5);
}

int main(void) {
    assert(system("mkdir -p " DIR) == 0);
    test_a_file_that_does_not_compile_fails_with_its_line();
    test_a_love_name_off_the_list_fails_but_not_in_strings_or_comments();
    test_every_listed_use_passes();
    test_a_callback_used_without_being_defined_fails();
    test_an_alias_a_bracket_index_or_an_indirect_call_fails();
    test_graphics_and_image_outside_the_glue_directory_fail();
    test_an_undeclared_global_read_fails_with_file_line_and_name();
    test_a_global_write_fails_except_debug_state_in_main();
    test_reads_are_found_in_nested_functions_and_locals_shadow_globals();
    test_every_listed_global_read_passes();
    test_touch_and_mouse_polling_passes_outside_the_glue_directory();
    test_another_touch_or_mouse_name_fails();
    puts("test_lua_guard: ok");
    return 0;
}
