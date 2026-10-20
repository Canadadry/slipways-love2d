#ifndef P8_FORMAT_H
#define P8_FORMAT_H

#include <stdarg.h>
#include <stddef.h>

/* The C99 snprintf contract (returns the untruncated length, NUL-terminates
   when size > 0) for the conversions d i u x X c s % with the flags '-' and
   '0', a width, a string precision and the 'l' length; any other conversion
   is copied as is. */
int p8_vsnprintf(char *buffer, size_t size, const char *format, va_list args);
int p8_snprintf(char *buffer, size_t size, const char *format, ...);

#endif
