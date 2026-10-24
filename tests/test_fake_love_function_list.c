#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <lauxlib.h>
#include <lualib.h>

#include "../tools/fake_love/function_list.h"

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
                                        "graphics",       "image",    "joystick", "filesystem"};
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

int main(void) {
    test_a_module_table_refuses_an_absent_key_naming_it_in_full();
    test_a_module_table_reads_its_listed_fields_and_lists_only_them();
    test_a_module_table_refuses_a_new_key_naming_it_in_full();
    test_a_non_string_key_is_shown_in_brackets();
    test_love_reads_nil_for_an_absent_callback_or_module();
    test_love_refuses_any_other_absent_key();
    test_love_takes_the_seven_callbacks_and_nothing_else();
    test_a_method_table_names_the_kind_and_method_with_a_colon();
    printf("test_fake_love_function_list: all tests passed\n");
    return 0;
}
