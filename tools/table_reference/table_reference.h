#ifndef TABLE_REFERENCE_H
#define TABLE_REFERENCE_H

/* Each call adds one line "<module> <path> <value>" to the reference read by
 * tests/lua/test_modules.lua; the path is a printf format, written
 * "NAME[3].field" as the Lua rendering spells it. */
void table_reference_int(const char *module, long long value, const char *path_format, ...);
/* Writes nothing for a NULL function pointer (present 0), as an absent Lua field renders nothing. */
void table_reference_function(const char *module, int present, const char *path_format, ...);
/* `&<name>` for a pointer to another registered table; nothing for NULL. */
void table_reference_pointer(const char *module, const char *name, const char *path_format, ...);
void table_reference_string(const char *module, const char *text, const char *path_format, ...);

#endif
