#ifndef DEVTOOLS_SAVE_STORAGE_MEMORY_H
#define DEVTOOLS_SAVE_STORAGE_MEMORY_H

/* Successful save_storage_write calls since the process started; read and
   erase leave it alone. */
int save_storage_memory_write_count(void);

#endif
