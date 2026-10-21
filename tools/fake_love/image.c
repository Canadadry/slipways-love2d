#include "image.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>

#include "filesystem.h"
#include "function_list.h"
#include "png_file.h"

#define METATABLE "ImageData"

/* As Lua's tostring prints it, but "nan" whatever the sign bit. */
static const char *format_number(double value, char text[32]) {
    if (isnan(value))
        return "nan";
    snprintf(text, 32, "%.14g", value);
    return text;
}

static image_data_t *push_image_data(lua_State *lua, int width, int height) {
    image_data_t *data = lua_newuserdata(lua, image_data_byte_count(width, height));
    image_data_init(data, width, height);
    luaL_getmetatable(lua, METATABLE);
    lua_setmetatable(lua, -2);
    return data;
}

static int load_image_data(lua_State *lua) {
    const char *name = filesystem_check_name(lua, "love.image.newImageData", 1);
    size_t length;
    unsigned char *bytes = disk_read(lua_touserdata(lua, lua_upvalueindex(1)), name, &length);
    if (bytes == NULL)
        return luaL_error(lua, "love.image.newImageData: Could not open file %s. Does not exist.", name);
    unsigned char *pixels;
    int width, height;
    const char *reason = png_file_decode(bytes, length, IMAGE_DATA_MAX_SIDE, &pixels, &width, &height);
    free(bytes);
    if (reason != NULL)
        return luaL_error(lua, "love.image.newImageData: \"%s\" is not a PNG the fake decodes as LÖVE 11.5 does: %s",
                          name, reason);
    image_data_t *data = push_image_data(lua, width, height);
    memcpy(data->pixels, pixels, (size_t)width * height * 4);
    free(pixels);
    return 1;
}

static int new_image_data(lua_State *lua) {
    int top = lua_gettop(lua);
    if (top >= 1 && lua_type(lua, 1) == LUA_TSTRING) {
        if (top != 1)
            return luaL_error(lua, "love.image.newImageData: takes 2 numbers, width and height, or 1 file name, not "
                                   "%d arguments",
                              top);
        return load_image_data(lua);
    }
    if (top == 1 && lua_type(lua, 1) != LUA_TNUMBER)
        return luaL_error(lua, "love.image.newImageData: argument 1 is a %s, not a file name", luaL_typename(lua, 1));
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
    push_image_data(lua, (int)width, (int)height);
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

void image_install(lua_State *lua, disk_t *disk) {
    luaL_newmetatable(lua, METATABLE);
    lua_createtable(lua, 0, 2);
    lua_pushcfunction(lua, set_pixel);
    lua_setfield(lua, -2, "setPixel");
    lua_pushcfunction(lua, get_pixel);
    lua_setfield(lua, -2, "getPixel");
    function_list_protect_methods(lua, -1, METATABLE);
    lua_setfield(lua, -2, "__index");
    lua_pop(lua, 1);

    lua_getglobal(lua, "love");
    lua_pushliteral(lua, "image");
    lua_createtable(lua, 0, 1);
    lua_pushlightuserdata(lua, disk);
    lua_pushcclosure(lua, new_image_data, 1);
    lua_setfield(lua, -2, "newImageData");
    function_list_protect_module(lua, -1, "love.image");
    lua_rawset(lua, -3);
    lua_pop(lua, 1);
}
