#ifndef P8_FATAL_H
#define P8_FATAL_H

#if defined(__GNUC__)
#define P8_NORETURN __attribute__((noreturn))
#else
#define P8_NORETURN
#endif

void p8_fatal(const char *message) P8_NORETURN;

#endif
