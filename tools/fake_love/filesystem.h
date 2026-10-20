#ifndef FAKE_LOVE_FILESYSTEM_H
#define FAKE_LOVE_FILESYSTEM_H

#include <lua.h>

#include "disk.h"

/* Sets love.filesystem, holding only read, write and getInfo, on the global
 * `love` table. The disk is not owned and must outlive the Lua state. */
void filesystem_install(lua_State *lua, disk_t *disk);

#endif
