#ifndef GAME_MAP_TABLES_H
#define GAME_MAP_TABLES_H

#include "p8/fixed.h"

enum {
    RESOURCE_CODE_COUNT = 13,
    RESOURCE_NAMES_COUNT = 11,
    PRODUCTION_TABLES_COUNT = 6,
    PROJECT_RECORDS_COUNT = 42,
    ICON_TOOLTIPS_COUNT = 7,
    TECHNOLOGIES_COUNT = 19
};

typedef struct {
    const char *key;
    int resource;
} resource_code_t;

typedef struct {
    const char *key;
    const char *name;
} resource_names_entry_t;

typedef struct {
    const char *key;
    int count;
    int values[4];
    const char *upgraded_entry;
} production_tables_entry_t;

typedef struct data_row data_row_t;
typedef struct {
    const char *text;
    int number;
    const data_row_t *row;
    int is_false;
    int is_fixed;
    p8_fixed_t fixed;
    int is_nil;
} data_item_t;

typedef enum {
    DRAW_FUNCTION_NONE,
    DRAW_FUNCTION_DRAW_BAR,
    DRAW_FUNCTION_PRINT_OUTLINED,
    DRAW_FUNCTION_DRAW_HELP_FRAME,
    DRAW_FUNCTION_SPR,
    DRAW_FUNCTION_DRAW_ROW,
    DRAW_FUNCTION_COUNT
} draw_function_t;

struct data_row {
    int count;
    const data_item_t *items;
    int has_color;
    int color;
    draw_function_t draw_function;
    int has_resource;
    p8_fixed_t resource;
};

typedef struct {
    const char *header;
    int npos;
    const char *name;
    const char *planet_codes;
    int base_cost;
    const char *production_key;
    const char *inputs;
    const char *output;
    const char *upgrade_tech;
    const char *byproduct;
    int upgrade_tech_false;
    int has_icon_sprite;
    int icon_sprite;
    const char *next_type;
    const char *class_name;
} project_records_entry_t;

typedef struct {
    const char *name;
    int era;
    const char *icon_key;
    int icon;
    int cost;
    const data_row_t *tooltip_rows;
} technologies_entry_t;

extern const resource_code_t resource_codes[RESOURCE_CODE_COUNT];
extern const resource_names_entry_t resource_names_table[RESOURCE_NAMES_COUNT];
extern const production_tables_entry_t production_tables_table[PRODUCTION_TABLES_COUNT];
extern const project_records_entry_t project_records_table[PROJECT_RECORDS_COUNT];
extern const data_row_t icon_tooltips_table[ICON_TOOLTIPS_COUNT];
extern const technologies_entry_t technologies_table[TECHNOLOGIES_COUNT];

int resource_code_lookup(const char *key, int *out_resource);
const char *resource_names_lookup(const char *key);
const production_tables_entry_t *production_tables_lookup(const char *key);
const technologies_entry_t *map_tables_technologies_lookup(const char *px);

#endif
