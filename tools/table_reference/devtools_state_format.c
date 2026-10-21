#include "devtools/state_format.c"
#include "table_reference.h"

#define STATE_FORMAT_DATA(TEXT) \
    TEXT(SLOT_KIND_NAMES) \
    TEXT(PARTICLE_NAMES)

#define RENDER_TEXT(name)                                                    \
    for (int i = 0; i < (int)(sizeof name / sizeof name[0]); i++) {          \
        table_reference_string("devtools.state_format", name[i], #name "[%d]", i); \
    }

void table_reference_devtools_state_format(void) {
    STATE_FORMAT_DATA(RENDER_TEXT)
}
