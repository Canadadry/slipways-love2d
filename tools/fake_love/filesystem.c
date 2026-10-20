#include "filesystem.h"

#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>

static disk_t *upvalue_disk(lua_State *lua) {
    return lua_touserdata(lua, lua_upvalueindex(1));
}

const char *filesystem_check_name(lua_State *lua, const char *function, int index) {
    if (lua_type(lua, index) != LUA_TSTRING)
        luaL_error(lua, "%s: argument %d is a %s, not a file name", function, index, luaL_typename(lua, index));
    size_t length;
    const char *name = lua_tolstring(lua, index, &length);
    const char *reason = strlen(name) == length ? disk_check_name(name) : "NUL byte in path";
    if (reason != NULL)
        luaL_error(lua, "%s: invalid path \"%s\": %s", function, name, reason);
    return name;
}

/* `form` is the supported call, as "read(name)". */
static void check_argument_count(lua_State *lua, const char *form, int minimum, int maximum) {
    int count = lua_gettop(lua);
    if (count < minimum || count > maximum)
        luaL_error(lua, "love.filesystem.%s is the only supported form, got %d arguments", form, count);
}

static int push_missing(lua_State *lua, const char *name) {
    lua_pushnil(lua);
    lua_pushfstring(lua, "Could not open file %s. Does not exist.", name);
    return 2;
}

static int read_file(lua_State *lua) {
    check_argument_count(lua, "read(name)", 1, 1);
    const char *name = filesystem_check_name(lua, "love.filesystem.read", 1);
    size_t length;
    unsigned char *bytes = disk_read(upvalue_disk(lua), name, &length);
    if (bytes == NULL)
        return push_missing(lua, name);
    lua_pushlstring(lua, (const char *)bytes, length);
    free(bytes);
    lua_pushnumber(lua, (lua_Number)length);
    return 2;
}

static int write_file(lua_State *lua) {
    check_argument_count(lua, "write(name, data)", 2, 2);
    const char *name = filesystem_check_name(lua, "love.filesystem.write", 1);
    if (lua_type(lua, 2) != LUA_TSTRING)
        return luaL_error(lua, "love.filesystem.write: the data is a %s, not a string", luaL_typename(lua, 2));
    size_t length;
    const char *data = lua_tolstring(lua, 2, &length);
    if (disk_put(upvalue_disk(lua), name, data, length) != NULL) {
        lua_pushboolean(lua, 0);
        lua_pushfstring(lua, "Could not open file %s. Does not exist.", name);
        return 2;
    }
    lua_pushboolean(lua, 1);
    return 1;
}

static const char *const FILTER_TYPES[] = {"file", "directory", "symlink", "other"};

static int get_info(lua_State *lua) {
    check_argument_count(lua, "getInfo(name [, filtertype])", 1, 2);
    const char *name = filesystem_check_name(lua, "love.filesystem.getInfo", 1);
    const char *filter = NULL;
    if (!lua_isnoneornil(lua, 2)) {
        if (lua_type(lua, 2) != LUA_TSTRING)
            return luaL_error(lua, "love.filesystem.getInfo: the filter is a %s, not a file type",
                              luaL_typename(lua, 2));
        filter = lua_tostring(lua, 2);
        size_t known = 0;
        while (known < sizeof FILTER_TYPES / sizeof FILTER_TYPES[0] && strcmp(filter, FILTER_TYPES[known]) != 0)
            known++;
        if (known == sizeof FILTER_TYPES / sizeof FILTER_TYPES[0])
            return luaL_error(lua, "love.filesystem.getInfo: invalid file type \"%s\"", filter);
    }
    size_t size = 0;
    disk_kind_t kind = disk_info(upvalue_disk(lua), name, &size);
    const char *type = kind == DISK_FILE ? "file" : "directory";
    if (kind == DISK_MISSING || (filter != NULL && strcmp(filter, type) != 0)) {
        lua_pushnil(lua);
        return 1;
    }
    lua_createtable(lua, 0, 2);
    lua_pushstring(lua, type);
    lua_setfield(lua, -2, "type");
    if (kind == DISK_FILE) {
        lua_pushnumber(lua, (lua_Number)size);
        lua_setfield(lua, -2, "size");
    }
    return 1;
}

static void set_closure(lua_State *lua, disk_t *disk, lua_CFunction function, const char *name) {
    lua_pushlightuserdata(lua, disk);
    lua_pushcclosure(lua, function, 1);
    lua_setfield(lua, -2, name);
}

void filesystem_install(lua_State *lua, disk_t *disk) {
    lua_getglobal(lua, "love");
    lua_createtable(lua, 0, 3);
    set_closure(lua, disk, read_file, "read");
    set_closure(lua, disk, write_file, "write");
    set_closure(lua, disk, get_info, "getInfo");
    lua_setfield(lua, -2, "filesystem");
    lua_pop(lua, 1);
}
