#ifndef FAKE_LOVE_FILESYSTEM_H
#define FAKE_LOVE_FILESYSTEM_H

#include <lua.h>

#include "disk.h"

/* Sets love.filesystem, holding only read, write and getInfo, on the global
 * `love` table. The disk is not owned and must outlive the Lua state. */
void filesystem_install(lua_State *lua, disk_t *disk);

/* Raises "<function>: ..." unless argument `index` is a string the disk
 * accepts as a name; else returns it, valid while it stays on the stack. */
const char *filesystem_check_name(lua_State *lua, const char *function, int index);

#endif
