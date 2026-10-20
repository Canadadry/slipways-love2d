#include "image.h"

#include <math.h>
#include <stdio.h>

#include <lauxlib.h>

#define METATABLE "ImageData"

/* As Lua's tostring prints it, but "nan" whatever the sign bit. */
static const char *format_number(double value, char text[32]) {
    if (isnan(value))
        return "nan";
    snprintf(text, 32, "%.14g", value);
    return text;
}

static int new_image_data(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top >= 1 && lua_type(lua, 1) == LUA_TSTRING)
        return luaL_error(lua, "love.image.newImageData: argument 1 is a string, not a number (the file form is not "
                               "supported)");
    if (top != 2)
        return luaL_error(lua, "love.image.newImageData: takes 2 numbers, width and height, not %d arguments", top);
    for (int index = 1; index <= 2; index++)
        if (lua_type(lua, index) != LUA_TNUMBER)
            return luaL_error(lua, "love.image.newImageData: argument %d is a %s, not a number", index,
                              luaL_typename(lua, index));
    double width = lua_tonumber(lua, 1);
    double height = lua_tonumber(lua, 2);
    if (!image_data_accepts_side(width) || !image_data_accepts_side(height)) {
        char width_text[32], height_text[32];
        return luaL_error(lua, "love.image.newImageData: size %sx%s is not two integers from 1 to %d",
                          format_number(width, width_text), format_number(height, height_text), IMAGE_DATA_MAX_SIDE);
    }
    image_data_t *data = lua_newuserdata(lua, image_data_byte_count((int)width, (int)height));
    image_data_init(data, (int)width, (int)height);
    luaL_getmetatable(lua, METATABLE);
    lua_setmetatable(lua, -2);
    return 1;
}

/* luaL_error does not return: the helpers below raise or succeed. */
static image_data_t *check_self(lua_State *lua, const char *method) {
    image_data_t *data = luaL_testudata(lua, 1, METATABLE);
    if (data == NULL)
        luaL_error(lua, "ImageData:%s: argument self is a %s, not an ImageData (call it with a colon)", method,
                   luaL_typename(lua, 1));
    return data;
}

/* The `count` arguments after self must be numbers, those from 3 on finite,
 * and the first two must name a pixel inside. Argument 1 is x, as in LÖVE's
 * messages. */
static void check_pixel_arguments(lua_State *lua, const image_data_t *data, const char *method, int count, int *column,
                                  int *row) {
    for (int index = 1; index <= count; index++)
        if (lua_type(lua, 1 + index) != LUA_TNUMBER)
            luaL_error(lua, "ImageData:%s: argument %d is a %s, not a number", method, index,
                       luaL_typename(lua, 1 + index));
    for (int index = 3; index <= count; index++)
        if (!isfinite(lua_tonumber(lua, 1 + index)))
            luaL_error(lua, "ImageData:%s: argument %d is not finite", method, index);
    double x = lua_tonumber(lua, 2);
    double y = lua_tonumber(lua, 3);
    if (!image_data_find_pixel(data, x, y, column, row)) {
        char x_text[32], y_text[32];
        luaL_error(lua, "ImageData:%s: out-of-range pixel (%s, %s) on a %dx%d ImageData", method,
                   format_number(x, x_text), format_number(y, y_text), data->width, data->height);
    }
}

static int set_pixel(lua_State *lua) {
    image_data_t *data = check_self(lua, "setPixel");
    int count = lua_gettop(lua) - 1;
    if (count >= 3 && lua_type(lua, 4) == LUA_TTABLE)
        return luaL_error(lua, "ImageData:setPixel: argument 3 is a table, not a number (the table form is not "
                               "supported)");
    if (count != 5 && count != 6)
        return luaL_error(lua, "ImageData:setPixel: takes x, y and 3 or 4 components, not %d arguments", count);
    int column, row;
    check_pixel_arguments(lua, data, "setPixel", count, &column, &row);
    unsigned char rgba[4] = {0, 0, 0, 255};
    for (int channel = 0; channel < count - 2; channel++)
        rgba[channel] = image_data_convert_component(lua_tonumber(lua, 4 + channel));
    image_data_write_pixel(data, column, row, rgba);
    return 0;
}

static int get_pixel(lua_State *lua) {
    const image_data_t *data = check_self(lua, "getPixel");
    int count = lua_gettop(lua) - 1;
    if (count != 2)
        return luaL_error(lua, "ImageData:getPixel: takes x and y, not %d arguments", count);
    int column, row;
    check_pixel_arguments(lua, data, "getPixel", count, &column, &row);
    unsigned char rgba[4];
    image_data_read_pixel(data, column, row, rgba);
    for (int channel = 0; channel < 4; channel++)
        lua_pushnumber(lua, image_data_read_component(rgba[channel]));
    return 4;
}

const image_data_t *image_find_data(lua_State *lua, int index) {
    return luaL_testudata(lua, index, METATABLE);
}

void image_install(lua_State *lua) {
    luaL_newmetatable(lua, METATABLE);
    lua_createtable(lua, 0, 2);
    lua_pushcfunction(lua, set_pixel);
    lua_setfield(lua, -2, "setPixel");
    lua_pushcfunction(lua, get_pixel);
    lua_setfield(lua, -2, "getPixel");
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "love");
    lua_createtable(lua, 0, 1);
    lua_pushcfunction(lua, new_image_data);
    lua_setfield(lua, -2, "newImageData");
    lua_setfield(lua, -2, "image");
    lua_pop(lua, 1);
}
