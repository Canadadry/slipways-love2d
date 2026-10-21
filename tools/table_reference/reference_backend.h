#ifndef TABLE_REFERENCE_REFERENCE_BACKEND_H
#define TABLE_REFERENCE_REFERENCE_BACKEND_H

#include "p8/draw_backend.h"
#include "table_reference.h"

static void table_reference_draw_backend(const char *module, const char *name, const p8_draw_backend_t *backend) {
#define FIELD(field) table_reference_function(module, backend->field != NULL, "%s." #field, name)
    FIELD(cls);
    FIELD(pset);
    FIELD(line);
    FIELD(rect);
    FIELD(rectfill);
    FIELD(circ);
    FIELD(circfill);
    FIELD(spr);
    FIELD(print);
    FIELD(print_outlined);
    FIELD(image_begin);
    FIELD(image_window);
    FIELD(image_end);
    FIELD(image_draw);
    FIELD(lines);
    FIELD(psets);
#undef FIELD
}

#endif
