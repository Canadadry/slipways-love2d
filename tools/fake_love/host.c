#include "host.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>

#include "filesystem.h"
#include "joystick.h"
#include "pad.h"

#define NO_FRAME ((size_t)-1)
#define FRAME_DT (1.0 / 60.0)

struct host {
    lua_State *lua;
    unsigned held;
    char *game_dir;
    char *state_text;
    char error[8192];
};

static void set_error(host_t *host, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(host->error, sizeof host->error, format, args);
    va_end(args);
}

/* `where` names the file or callback; `frame` is NO_FRAME outside the loop. */
static void set_lua_error(host_t *host, const char *where, size_t frame) {
    const char *message = lua_tostring(host->lua, -1);
    if (message == NULL)
        message = "(error object is not a string)";
    if (frame == NO_FRAME)
        set_error(host, "%s: %s", where, message);
    else
        set_error(host, "%s f%zu: %s", where, frame, message);
    lua_pop(host->lua, 1);
}

/* Calls the function lying under its `nargs` arguments, with debug.traceback
 * as the message handler. Leaves `nresults` results on success, nothing on
 * failure. */
static bool call(host_t *host, int nargs, int nresults, const char *where, size_t frame) {
    lua_State *lua = host->lua;
    int function_index = lua_gettop(lua) - nargs;
    lua_getglobal(lua, "debug");
    lua_getfield(lua, -1, "traceback");
    lua_remove(lua, -2);
    lua_insert(lua, function_index);
    int status = lua_pcall(lua, nargs, nresults, function_index);
    lua_remove(lua, function_index);
    if (status != 0) {
        set_lua_error(host, where, frame);
        return false;
    }
    return true;
}

host_t *host_new(void) {
    host_t *host = calloc(1, sizeof *host);
    if (host == NULL)
        return NULL;
    host->lua = luaL_newstate();
    if (host->lua == NULL) {
        free(host);
        return NULL;
    }
    luaL_openlibs(host->lua);
    return host;
}

void host_close(host_t *host) {
    if (host == NULL)
        return;
    lua_close(host->lua);
    free(host->game_dir);
    free(host->state_text);
    free(host);
}

const char *host_runtime_version(host_t *host) {
    lua_getglobal(host->lua, "jit");
    lua_getfield(host->lua, -1, "version");
    const char *version = lua_tostring(host->lua, -1);
    lua_pop(host->lua, 2);
    return version;
}

const char *host_error(const host_t *host) {
    return host->error;
}

static bool file_exists(const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL)
        return false;
    fclose(file);
    return true;
}

static bool run_file(host_t *host, const char *name) {
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", host->game_dir, name);
    if (luaL_loadfile(host->lua, path) != 0) {
        set_lua_error(host, name, NO_FRAME);
        return false;
    }
    return call(host, 0, 0, name, NO_FRAME);
}

/* False when love.<name> is set but is not a function. Otherwise pushes it
 * when it is set, and `*defined` tells the caller whether to call it. */
static bool push_callback(host_t *host, const char *name, size_t frame, bool *defined) {
    lua_State *lua = host->lua;
    lua_getglobal(lua, "love");
    lua_getfield(lua, -1, name);
    lua_remove(lua, -2);
    *defined = !lua_isnil(lua, -1);
    if (!*defined) {
        lua_pop(lua, 1);
        return true;
    }
    if (!lua_isfunction(lua, -1)) {
        if (frame == NO_FRAME)
            set_error(host, "love.%s is a %s, not a function", name, luaL_typename(lua, -1));
        else
            set_error(host, "love.%s is a %s, not a function (f%zu)", name, luaL_typename(lua, -1), frame);
        lua_pop(lua, 1);
        return false;
    }
    return true;
}

static void set_package_paths(host_t *host) {
    lua_State *lua = host->lua;
    lua_getglobal(lua, "package");
    lua_pushfstring(lua, "%s/?.lua;%s/?/init.lua", host->game_dir, host->game_dir);
    lua_setfield(lua, -2, "path");
    lua_pushliteral(lua, "");
    lua_setfield(lua, -2, "cpath");
    lua_pop(lua, 1);
}

static bool run_conf(host_t *host) {
    char path[4096];
    snprintf(path, sizeof path, "%s/conf.lua", host->game_dir);
    if (file_exists(path) && !run_file(host, "conf.lua"))
        return false;
    bool defined;
    if (!push_callback(host, "conf", NO_FRAME, &defined))
        return false;
    if (!defined)
        return true;
    lua_State *lua = host->lua;
    lua_newtable(lua);
    lua_newtable(lua);
    lua_setfield(lua, -2, "window");
    lua_newtable(lua);
    lua_setfield(lua, -2, "modules");
    lua_newtable(lua);
    lua_setfield(lua, -2, "audio");
    return call(host, 1, 0, "love.conf", NO_FRAME);
}

bool host_open(host_t *host, const char *game_dir, disk_t *disk) {
    assert(host->game_dir == NULL && "host_open runs once per host");
    assert(disk != NULL);
    host->game_dir = malloc(strlen(game_dir) + 1);
    if (host->game_dir == NULL) {
        set_error(host, "out of memory");
        return false;
    }
    strcpy(host->game_dir, game_dir);
    set_package_paths(host);
    lua_newtable(host->lua);
    lua_setglobal(host->lua, "love");
    if (!run_conf(host))
        return false;
    joystick_install(host->lua);
    filesystem_install(host->lua, disk);
    return run_file(host, "main.lua");
}

bool host_load(host_t *host) {
    bool defined;
    if (!push_callback(host, "load", NO_FRAME, &defined))
        return false;
    if (!defined)
        return true;
    lua_newtable(host->lua);
    return call(host, 1, 0, "love.load", NO_FRAME);
}

static bool call_pad_callback(host_t *host, const pad_change_t *change, size_t frame) {
    const char *callback = change->kind == PAD_PRESSED ? "love.gamepadpressed" : "love.gamepadreleased";
    bool defined;
    if (!push_callback(host, callback + strlen("love."), frame, &defined))
        return false;
    if (!defined)
        return true;
    joystick_push(host->lua);
    lua_pushstring(host->lua, pad_button_name(change->button));
    return call(host, 2, 0, callback, frame);
}

bool host_frame(host_t *host, size_t frame, unsigned held) {
    pad_change_t changes[PAD_MAX_CHANGES];
    size_t change_count = pad_list_changes(host->held, held, changes);
    host->held = held;
    joystick_set_held(host->lua, held);
    for (size_t i = 0; i < change_count; i++)
        if (!call_pad_callback(host, &changes[i], frame))
            return false;
    bool defined;
    if (!push_callback(host, "update", frame, &defined))
        return false;
    if (defined) {
        lua_pushnumber(host->lua, FRAME_DT);
        if (!call(host, 1, 0, "love.update", frame))
            return false;
    }
    if (!push_callback(host, "draw", frame, &defined))
        return false;
    return !defined || call(host, 0, 0, "love.draw", frame);
}

bool host_debug_state(host_t *host, const char **text, size_t *length) {
    lua_State *lua = host->lua;
    lua_getglobal(lua, "debug_state");
    if (!lua_isfunction(lua, -1)) {
        set_error(host, "debug_state is a %s, not a function", luaL_typename(lua, -1));
        lua_pop(lua, 1);
        return false;
    }
    if (!call(host, 0, 1, "debug_state", NO_FRAME))
        return false;
    if (lua_type(lua, -1) != LUA_TSTRING) {
        set_error(host, "debug_state returned a %s, not a string", luaL_typename(lua, -1));
        lua_pop(lua, 1);
        return false;
    }
    size_t size;
    const char *state = lua_tolstring(lua, -1, &size);
    free(host->state_text);
    host->state_text = malloc(size + 1);
    if (host->state_text == NULL) {
        lua_pop(lua, 1);
        set_error(host, "out of memory");
        return false;
    }
    memcpy(host->state_text, state, size + 1);
    lua_pop(lua, 1);
    *text = host->state_text;
    *length = size;
    return true;
}
