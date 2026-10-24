#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>
#include <lualib.h>

#include "../tools/fake_love/disk.h"
#include "../tools/fake_love/function_list.h"
#include "../tools/fake_love/host.h"

static lua_State *new_state(void) {
    lua_State *lua = luaL_newstate();
    assert(lua != NULL);
    luaL_openlibs(lua);
    return lua;
}

/* Runs `chunk` and leaves on the stack what it returns through tostring, or
 * its error; the text is owned by the state. */
static const char *run(lua_State *lua, const char *chunk) {
    if (luaL_loadbuffer(lua, chunk, strlen(chunk), "=game.lua") != 0 || lua_pcall(lua, 0, 1, 0) != 0)
        return lua_tostring(lua, -1);
    lua_getglobal(lua, "tostring");
    lua_insert(lua, -2);
    lua_call(lua, 1, 1);
    return lua_tostring(lua, -1);
}

static void assert_runs_to(lua_State *lua, const char *chunk, const char *expected) {
    const char *result = run(lua, chunk);
    if (strcmp(result, expected) != 0)
        fprintf(stderr, "%s\n  gave:     %s\n  expected: %s\n", chunk, result, expected);
    assert(strcmp(result, expected) == 0);
    lua_pop(lua, 1);
}

static lua_State *new_state_with_module(void) {
    lua_State *lua = new_state();
    lua_newtable(lua);
    lua_createtable(lua, 0, 1);
    lua_pushliteral(lua, "listed");
    lua_setfield(lua, -2, "setColor");
    function_list_protect_module(lua, -1, "love.graphics");
    lua_setfield(lua, -2, "graphics");
    lua_setglobal(lua, "love");
    return lua;
}

static void test_a_module_table_refuses_an_absent_key_naming_it_in_full(void) {
    lua_State *lua = new_state_with_module();
    assert_runs_to(lua, "return love.graphics.getWidth",
                   "game.lua:1: love.graphics.getWidth is not in the function list of docs/love-port/design.md");
    lua_close(lua);
}

static void test_a_module_table_reads_its_listed_fields_and_lists_only_them(void) {
    lua_State *lua = new_state_with_module();
    assert_runs_to(lua, "return love.graphics.setColor", "listed");
    assert_runs_to(lua,
                   "local names = {}\n"
                   "for name in pairs(love.graphics) do names[#names + 1] = name end\n"
                   "return table.concat(names, ' ')",
                   "setColor");
    lua_close(lua);
}

static void test_a_module_table_refuses_a_new_key_naming_it_in_full(void) {
    lua_State *lua = new_state_with_module();
    assert_runs_to(lua, "love.graphics.extra = 1",
                   "game.lua:1: love.graphics.extra is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "return rawget(love.graphics, 'extra')", "nil");
    lua_close(lua);
}

static void test_a_non_string_key_is_shown_in_brackets(void) {
    lua_State *lua = new_state_with_module();
    assert_runs_to(lua, "return love.graphics[1]",
                   "game.lua:1: love.graphics[1] is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "love.graphics[true] = 1",
                   "game.lua:1: love.graphics[true] is not in the function list of docs/love-port/design.md");
    lua_close(lua);
}

static lua_State *new_state_with_love(void) {
    lua_State *lua = new_state();
    lua_newtable(lua);
    lua_setglobal(lua, "love");
    function_list_protect_love(lua);
    return lua;
}

static void test_love_reads_nil_for_an_absent_callback_or_module(void) {
    lua_State *lua = new_state_with_love();
    static const char *const names[] = {"conf",           "load",     "update", "draw",
                                        "gamepadpressed", "gamepadreleased", "resize",
                                        "graphics",       "image",    "joystick", "filesystem",
                                        "touch",          "mouse"};
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
        char chunk[64];
        snprintf(chunk, sizeof chunk, "return love.%s", names[i]);
        assert_runs_to(lua, chunk, "nil");
    }
    assert_runs_to(lua, "return next(love) == nil and 'empty' or 'not empty'", "empty");
    lua_close(lua);
}

static void test_love_refuses_any_other_absent_key(void) {
    lua_State *lua = new_state_with_love();
    assert_runs_to(lua, "if love.timer then return 'yes' end return 'no'",
                   "game.lua:1: love.timer is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "return love.keypressed",
                   "game.lua:1: love.keypressed is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "return love[1]",
                   "game.lua:1: love[1] is not in the function list of docs/love-port/design.md");
    lua_close(lua);
}

static void test_love_takes_the_seven_callbacks_and_nothing_else(void) {
    lua_State *lua = new_state_with_love();
    assert_runs_to(lua,
                   "function love.conf() end\n"
                   "function love.load() end\n"
                   "function love.update() end\n"
                   "function love.draw() end\n"
                   "function love.gamepadpressed() end\n"
                   "love.gamepadreleased = function() return 'released' end\n"
                   "function love.resize() end\n"
                   "return love.gamepadreleased()",
                   "released");
    assert_runs_to(lua, "love.load = nil; love.load = function() return 'again' end; return love.load()", "again");
    assert_runs_to(lua, "function love.keypressed() end",
                   "game.lua:1: love.keypressed is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "function love.touchpressed() end",
                   "game.lua:1: love.touchpressed is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "love.mousepressed = print",
                   "game.lua:1: love.mousepressed is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "love.errorhandler = print",
                   "game.lua:1: love.errorhandler is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "love.graphics = {}",
                   "game.lua:1: love.graphics is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "return rawget(love, 'keypressed') == nil and rawget(love, 'graphics') == nil", "true");
    lua_close(lua);
}

static void test_a_method_table_names_the_kind_and_method_with_a_colon(void) {
    lua_State *lua = new_state();
    lua_newtable(lua);
    lua_createtable(lua, 0, 1);
    lua_pushliteral(lua, "pixel");
    lua_setfield(lua, -2, "getPixel");
    function_list_protect_methods(lua, -1, "ImageData");
    lua_setfield(lua, -2, "__index");
    lua_newuserdata(lua, 1);
    lua_insert(lua, -2);
    lua_setmetatable(lua, -2);
    lua_setglobal(lua, "data");
    assert_runs_to(lua, "return data.getPixel", "pixel");
    assert_runs_to(lua, "return data:mapPixel(print)",
                   "game.lua:1: ImageData:mapPixel is not in the function list of docs/love-port/design.md");
    assert_runs_to(lua, "return data[2]",
                   "game.lua:1: ImageData[2] is not in the function list of docs/love-port/design.md");
    lua_close(lua);
}

#define LIST_FILE "tools/love_function_list.lua"
#define LIST_FIXTURE "tests/fixtures/fake_love/function_list"
#define MAX_LINES 256

/* Returns the lines the fake installs, as the fixture's debug_state renders them. */
static char *fake_installed_lines(void) {
    disk_t *disk = disk_create(LIST_FIXTURE);
    host_t *host = host_new();
    assert(disk != NULL && host != NULL);
    const char *text;
    size_t length;
    bool ok = host_open(host, LIST_FIXTURE, disk) && host_debug_state(host, &text, &length);
    if (!ok)
        fprintf(stderr, "%s: %s\n", LIST_FIXTURE, host_error(host));
    assert(ok);
    char *lines = strdup(text);
    assert(lines != NULL);
    host_close(host);
    disk_free(disk);
    return lines;
}

static char *fake_guard_lines(void) {
    size_t capacity = 1;
    for (size_t i = 0; i < function_list_callback_count; i++)
        capacity += strlen("callback \n") + strlen(function_list_callbacks[i]);
    for (size_t i = 0; i < function_list_module_count; i++)
        capacity += strlen("module \n") + strlen(function_list_modules[i]);
    char *lines = malloc(capacity);
    assert(lines != NULL);
    size_t used = 0;
    for (size_t i = 0; i < function_list_callback_count; i++)
        used += (size_t)snprintf(lines + used, capacity - used, "callback %s\n", function_list_callbacks[i]);
    for (size_t i = 0; i < function_list_module_count; i++)
        used += (size_t)snprintf(lines + used, capacity - used, "module %s\n", function_list_modules[i]);
    return lines;
}

static const char RENDER_LIST[] =
    "local list = ...\n"
    "local installed, guard = {}, {}\n"
    "for _, name in ipairs(list.callbacks) do table.insert(guard, 'callback ' .. name) end\n"
    "for module_name, names in pairs(list.modules) do\n"
    "    table.insert(guard, 'module ' .. module_name)\n"
    "    table.insert(installed, 'module ' .. module_name)\n"
    "    for _, name in ipairs(names) do\n"
    "        table.insert(installed, 'function love.' .. module_name .. '.' .. name)\n"
    "    end\n"
    "end\n"
    "for kind, names in pairs(list.methods) do\n"
    "    table.insert(installed, 'kind ' .. kind)\n"
    "    for _, name in ipairs(names) do table.insert(installed, 'method ' .. kind .. ':' .. name) end\n"
    "end\n"
    "return table.concat(installed, '\\n') .. '\\n', table.concat(guard, '\\n') .. '\\n'\n";

/* Sets `*installed` and `*guard` to the file's lines in the forms of
 * fake_installed_lines and fake_guard_lines; the caller frees both. */
static void list_file_lines(char **installed, char **guard) {
    lua_State *lua = new_state();
    if (luaL_dofile(lua, LIST_FILE) != 0 || luaL_loadbuffer(lua, RENDER_LIST, strlen(RENDER_LIST), "=render") != 0) {
        fprintf(stderr, "%s\n", lua_tostring(lua, -1));
        assert(!"tools/love_function_list.lua does not load");
    }
    lua_insert(lua, -2);
    if (lua_pcall(lua, 1, 2, 0) != 0) {
        fprintf(stderr, "%s: %s\n", LIST_FILE, lua_tostring(lua, -1));
        assert(!"tools/love_function_list.lua is not a table of callbacks, modules and methods");
    }
    *installed = strdup(lua_tostring(lua, -2));
    *guard = strdup(lua_tostring(lua, -1));
    assert(*installed != NULL && *guard != NULL);
    lua_close(lua);
}

static int compare_text(const void *left, const void *right) {
    return strcmp(*(const char *const *)left, *(const char *const *)right);
}

/* Cuts `text` into its lines in place, sorted; returns their count. */
static size_t split_sorted(char *text, char **lines) {
    size_t count = 0;
    for (char *line = strtok(text, "\n"); line != NULL; line = strtok(NULL, "\n")) {
        assert(count < MAX_LINES);
        lines[count++] = line;
    }
    qsort(lines, count, sizeof lines[0], compare_text);
    return count;
}

static size_t report_missing(char **lines, size_t count, char **others, size_t other_count, const char *label) {
    size_t missing = 0;
    for (size_t i = 0; i < count; i++)
        if (bsearch(&lines[i], others, other_count, sizeof others[0], compare_text) == NULL) {
            fprintf(stderr, "%s: %s\n", label, lines[i]);
            missing++;
        }
    return missing;
}

/* Takes ownership of both texts. */
static size_t report_difference(char *fake, char *list) {
    char *fake_lines[MAX_LINES];
    char *list_lines[MAX_LINES];
    size_t fake_count = split_sorted(fake, fake_lines);
    size_t list_count = split_sorted(list, list_lines);
    size_t missing = report_missing(fake_lines, fake_count, list_lines, list_count,
                                    "in the fake, not in " LIST_FILE) +
                     report_missing(list_lines, list_count, fake_lines, fake_count,
                                    "in " LIST_FILE ", not in the fake");
    free(fake);
    free(list);
    return missing;
}

static void test_the_fake_installs_exactly_the_names_of_the_list_file(void) {
    char *list_installed;
    char *list_guard;
    list_file_lines(&list_installed, &list_guard);
    size_t missing = report_difference(fake_installed_lines(), list_installed) +
                     report_difference(fake_guard_lines(), list_guard);
    assert(missing == 0 && "the fake and tools/love_function_list.lua differ");
}

int main(void) {
    test_a_module_table_refuses_an_absent_key_naming_it_in_full();
    test_a_module_table_reads_its_listed_fields_and_lists_only_them();
    test_a_module_table_refuses_a_new_key_naming_it_in_full();
    test_a_non_string_key_is_shown_in_brackets();
    test_love_reads_nil_for_an_absent_callback_or_module();
    test_love_refuses_any_other_absent_key();
    test_love_takes_the_seven_callbacks_and_nothing_else();
    test_a_method_table_names_the_kind_and_method_with_a_colon();
    test_the_fake_installs_exactly_the_names_of_the_list_file();
    printf("test_fake_love_function_list: all tests passed\n");
    return 0;
}
