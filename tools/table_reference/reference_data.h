#ifndef TABLE_REFERENCE_REFERENCE_DATA_H
#define TABLE_REFERENCE_REFERENCE_DATA_H

#include <stdio.h>
#include <stdlib.h>

#include "game/map_tables.h"
#include "table_reference.h"

typedef struct {
    const void *address;
    const char *name;
} reference_data_name_t;

/* The tables of one unit, listed once in C order: a pointer renders as the
 * name of the listed table it points at. */
typedef struct {
    const char *module;
    const reference_data_name_t *names;
    int count;
} reference_data_t;

/* NULL for a NULL address; exits when the address starts no listed table, so
 * an unlisted table cannot hide behind a pointer. */
static const char *reference_data_name_of(const reference_data_t *data, const void *address, const char *path) {
    if (address == NULL) {
        return NULL;
    }
    for (int i = 0; i < data->count; i++) {
        if (data->names[i].address == address) {
            return data->names[i].name;
        }
    }
    fprintf(stderr, "table_reference: %s %s points at no listed table\n", data->module, path);
    exit(1);
}

static void reference_data_pointer(const reference_data_t *data, const void *address, const char *path) {
    table_reference_pointer(data->module, reference_data_name_of(data, address, path), "%s", path);
}

static void reference_data_item(const reference_data_t *data, const data_item_t *item, const char *path) {
    char field[256];
#define FIELD(name) (snprintf(field, sizeof field, "%s." #name, path), field)
    if (item->text != NULL) {
        table_reference_string(data->module, item->text, "%s", FIELD(text));
    }
    table_reference_int(data->module, item->number, "%s", FIELD(number));
    reference_data_pointer(data, item->row, FIELD(row));
    table_reference_int(data->module, item->is_false, "%s", FIELD(is_false));
    table_reference_int(data->module, item->is_fixed, "%s", FIELD(is_fixed));
    table_reference_int(data->module, item->fixed, "%s", FIELD(fixed));
    table_reference_int(data->module, item->is_nil, "%s", FIELD(is_nil));
#undef FIELD
}

static void reference_data_row(const reference_data_t *data, const data_row_t *row, const char *path) {
    char field[256];
#define FIELD(name) (snprintf(field, sizeof field, "%s." #name, path), field)
    table_reference_int(data->module, row->count, "%s", FIELD(count));
    reference_data_pointer(data, row->items, FIELD(items));
    table_reference_int(data->module, row->has_color, "%s", FIELD(has_color));
    table_reference_int(data->module, row->color, "%s", FIELD(color));
    table_reference_int(data->module, row->draw_function, "%s", FIELD(draw_function));
    table_reference_int(data->module, row->has_resource, "%s", FIELD(has_resource));
    table_reference_int(data->module, row->resource, "%s", FIELD(resource));
#undef FIELD
}

static void reference_data_items(const reference_data_t *data, const char *name, const data_item_t *items, int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s[%d]", name, i);
        reference_data_item(data, &items[i], path);
    }
}

static void reference_data_rows(const reference_data_t *data, const char *name, const data_row_t *rows, int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s[%d]", name, i);
        reference_data_row(data, &rows[i], path);
    }
}

#endif
