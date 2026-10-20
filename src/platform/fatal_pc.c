#include "p8/fatal.h"

#include <stdio.h>
#include <stdlib.h>

void p8_fatal(const char *message) {
    fprintf(stderr, "p8_fatal: %s\n", message);
    exit(3);
}
