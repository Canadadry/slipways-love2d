#ifndef DEVTOOLS_RECORD_START_H
#define DEVTOOLS_RECORD_START_H

#include "game/input_step.h"

#include <stddef.h>
#include <stdint.h>

int record_start_read_hex(const char *path, uint8_t *bytes, int capacity, char *error, size_t error_size);

int record_start_resume(input_step_t *step, const uint8_t *bytes, int count);

void record_start_return(input_step_t *step);

#endif
