#include "graphics.h"

#include <math.h>

#include <lauxlib.h>

#include "point.h"

#define METATABLE "Canvas"

typedef struct {
    int handle;
} canvas_object_t;

static canvas_store_t *upvalue_store(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

static int new_canvas(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top != 2)
        return luaL_error(lua, "love.graphics.newCanvas: takes 2 arguments, 160 and 120, not %d", top);
    for (int index = 1; index <= 2; index++)
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.newCanvas: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
    if (lua_tonumber(lua, 1) != CANVAS_WIDTH || lua_tonumber(lua, 2) != CANVAS_HEIGHT)
        return luaL_error(lua, "love.graphics.newCanvas: the canvas is %dx%d, not %fx%f", CANVAS_WIDTH, CANVAS_HEIGHT,
                          lua_tonumber(lua, 1), lua_tonumber(lua, 2));
    int handle = canvas_create(upvalue_store(lua));
    if (handle == CANVAS_NONE)
        return luaL_error(lua, "love.graphics.newCanvas: more than %d canvases", CANVAS_MAX_COUNT);
    canvas_object_t *object = lua_newuserdata(lua, sizeof *object);
    object->handle = handle;
    luaL_getmetatable(lua, METATABLE);
    lua_setmetatable(lua, -2);
    return 1;
}

static int set_canvas(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top > 1)
        return luaL_error(lua, "love.graphics.setCanvas: takes a canvas or nothing, not %d arguments", top);
    if (top == 0 || lua_isnil(lua, 1)) {
        canvas_select(upvalue_store(lua), CANVAS_NONE);
        return 0;
    }
    canvas_object_t *object = luaL_testudata(lua, 1, METATABLE);
    if (object == NULL)
        return luaL_error(lua, "love.graphics.setCanvas: the argument is a %s, not a canvas", luaL_typename(lua, 1));
    canvas_select(upvalue_store(lua), object->handle);
    return 0;
}

static int clear(lua_State *lua) {
    int top = lua_gettop(lua);
    for (int index = 1; index <= top; index++) {
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.clear: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
        if (!isfinite(lua_tonumber(lua, index)))
            return luaL_error(lua, "love.graphics.clear: argument %d is not a finite number", index);
    }
    if (top != 0 && top != 3 && top != 4)
        return luaL_error(lua, "love.graphics.clear: takes 0, 3 or 4 numbers, not %d arguments", top);
    unsigned char rgba[4] = {0, 0, 0, top == 3 ? 255 : 0};
    for (int index = 1; index <= top; index++)
        rgba[index - 1] = canvas_convert_component(lua_tonumber(lua, index));
    canvas_clear(upvalue_store(lua), rgba);
    return 0;
}

static void push_coordinate(lua_State *lua, bool table_form, int index) {
    if (table_form)
        lua_rawgeti(lua, 1, index);
    else
        lua_pushvalue(lua, index);
}

static int points(lua_State *lua) {
    int top = lua_gettop(lua);
    bool table_form = top >= 1 && lua_type(lua, 1) == LUA_TTABLE;
    if (table_form && top > 1)
        return luaL_error(lua, "love.graphics.points: a table must be the only argument, not one of %d", top);
    int count = table_form ? (int)lua_objlen(lua, 1) : top;
    const char *kind = table_form ? "element" : "argument";
    for (int index = 1; index <= count; index++) {
        push_coordinate(lua, table_form, index);
        if (lua_type(lua, -1) != LUA_TNUMBER)
            return luaL_error(lua, "love.graphics.points: %s %d is a %s, not a number", kind, index,
                              luaL_typename(lua, -1));
        double coordinate = lua_tonumber(lua, -1);
        lua_pop(lua, 1);
        if (!isfinite(coordinate))
            return luaL_error(lua, "love.graphics.points: %s %d is not finite", kind, index);
        if (!point_accepts_coordinate(coordinate))
            return luaL_error(lua, "love.graphics.points: %s %d is out of range (|v| >= 2^20)", kind, index);
    }
    if (count % 2 != 0)
        return luaL_error(lua, "love.graphics.points: odd number of coordinates (%d)", count);
    canvas_store_t *store = upvalue_store(lua);
    for (int index = 1; index <= count; index += 2) {
        push_coordinate(lua, table_form, index);
        push_coordinate(lua, table_form, index + 1);
        int x = point_find_pixel(lua_tonumber(lua, -2), CANVAS_WIDTH);
        int y = point_find_pixel(lua_tonumber(lua, -1), CANVAS_HEIGHT);
        lua_pop(lua, 2);
        if (x != POINT_OFF && y != POINT_OFF)
            canvas_plot(store, x, y);
    }
    return 0;
}

static void set_function(lua_State *lua, canvas_store_t *store, lua_CFunction function, const char *name) {
    lua_pushlightuserdata(lua, store);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

void graphics_install(lua_State *lua, canvas_store_t *store) {
    luaL_newmetatable(lua, METATABLE);
    lua_pop(lua, 1);
    lua_getglobal(lua, "love");
    lua_createtable(lua, 0, 4);
    set_function(lua, store, new_canvas, "newCanvas");
    set_function(lua, store, set_canvas, "setCanvas");
    set_function(lua, store, clear, "clear");
    set_function(lua, store, points, "points");
    lua_setfield(lua, -2, "graphics");
    lua_pop(lua, 1);
}
