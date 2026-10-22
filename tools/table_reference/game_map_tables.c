#include "game/map_tables.c"
#include "reference_data.h"

#define MODULE "game.map_tables"

/* Every table of map_tables.c in C order, with the function rendering it. */
#define MAP_TABLES(ARRAY, STRUCT) \
    ARRAY(resource_codes, render_resource_codes) \
    ARRAY(resource_names_table, render_resource_names) \
    ARRAY(production_tables_table, render_production_tables) \
    ARRAY(project_records_table, render_project_records) \
    ARRAY(icon_tooltips_1_1_items, reference_data_items) \
    STRUCT(icon_tooltips_1_1, reference_data_row) \
    ARRAY(icon_tooltips_1_2_items, reference_data_items) \
    STRUCT(icon_tooltips_1_2, reference_data_row) \
    ARRAY(icon_tooltips_1_3_items, reference_data_items) \
    STRUCT(icon_tooltips_1_3, reference_data_row) \
    ARRAY(icon_tooltips_1_4_items, reference_data_items) \
    STRUCT(icon_tooltips_1_4, reference_data_row) \
    ARRAY(icon_tooltips_1_5_items, reference_data_items) \
    STRUCT(icon_tooltips_1_5, reference_data_row) \
    ARRAY(icon_tooltips_1_items, reference_data_items) \
    ARRAY(icon_tooltips_2_1_items, reference_data_items) \
    STRUCT(icon_tooltips_2_1, reference_data_row) \
    ARRAY(icon_tooltips_2_2_items, reference_data_items) \
    STRUCT(icon_tooltips_2_2, reference_data_row) \
    ARRAY(icon_tooltips_2_items, reference_data_items) \
    ARRAY(icon_tooltips_3_1_items, reference_data_items) \
    STRUCT(icon_tooltips_3_1, reference_data_row) \
    ARRAY(icon_tooltips_3_2_items, reference_data_items) \
    STRUCT(icon_tooltips_3_2, reference_data_row) \
    ARRAY(icon_tooltips_3_items, reference_data_items) \
    ARRAY(icon_tooltips_4_1_items, reference_data_items) \
    STRUCT(icon_tooltips_4_1, reference_data_row) \
    ARRAY(icon_tooltips_4_2_items, reference_data_items) \
    STRUCT(icon_tooltips_4_2, reference_data_row) \
    ARRAY(icon_tooltips_4_3_items, reference_data_items) \
    STRUCT(icon_tooltips_4_3, reference_data_row) \
    ARRAY(icon_tooltips_4_4_items, reference_data_items) \
    STRUCT(icon_tooltips_4_4, reference_data_row) \
    ARRAY(icon_tooltips_4_items, reference_data_items) \
    ARRAY(icon_tooltips_5_1_items, reference_data_items) \
    STRUCT(icon_tooltips_5_1, reference_data_row) \
    ARRAY(icon_tooltips_5_2_items, reference_data_items) \
    STRUCT(icon_tooltips_5_2, reference_data_row) \
    ARRAY(icon_tooltips_5_3_items, reference_data_items) \
    STRUCT(icon_tooltips_5_3, reference_data_row) \
    ARRAY(icon_tooltips_5_items, reference_data_items) \
    ARRAY(icon_tooltips_6_1_items, reference_data_items) \
    STRUCT(icon_tooltips_6_1, reference_data_row) \
    ARRAY(icon_tooltips_6_2_items, reference_data_items) \
    STRUCT(icon_tooltips_6_2, reference_data_row) \
    ARRAY(icon_tooltips_6_3_items, reference_data_items) \
    STRUCT(icon_tooltips_6_3, reference_data_row) \
    ARRAY(icon_tooltips_6_items, reference_data_items) \
    ARRAY(icon_tooltips_7_1_items, reference_data_items) \
    STRUCT(icon_tooltips_7_1, reference_data_row) \
    ARRAY(icon_tooltips_7_2_items, reference_data_items) \
    STRUCT(icon_tooltips_7_2, reference_data_row) \
    ARRAY(icon_tooltips_7_3_items, reference_data_items) \
    STRUCT(icon_tooltips_7_3, reference_data_row) \
    ARRAY(icon_tooltips_7_items, reference_data_items) \
    ARRAY(icon_tooltips_table, reference_data_rows) \
    ARRAY(technologies_1_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_1_tooltip_rows, reference_data_row) \
    ARRAY(technologies_2_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_2_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_2_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_2_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_2_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_2_tooltip_rows, reference_data_row) \
    ARRAY(technologies_3_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_3_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_3_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_3_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_3_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_3_tooltip_rows, reference_data_row) \
    ARRAY(technologies_4_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_4_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_4_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_4_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_4_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_4_tooltip_rows, reference_data_row) \
    ARRAY(technologies_5_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_5_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_5_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_5_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_5_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_5_tooltip_rows, reference_data_row) \
    ARRAY(technologies_6_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_6_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_6_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_6_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_6_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_6_tooltip_rows, reference_data_row) \
    ARRAY(technologies_7_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_7_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_7_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_7_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_7_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_7_tooltip_rows, reference_data_row) \
    ARRAY(technologies_8_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_8_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_8_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_8_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_8_tooltip_rows_3_items, reference_data_items) \
    STRUCT(technologies_8_tooltip_rows_3, reference_data_row) \
    ARRAY(technologies_8_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_8_tooltip_rows, reference_data_row) \
    ARRAY(technologies_9_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_9_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_9_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_9_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_9_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_9_tooltip_rows, reference_data_row) \
    ARRAY(technologies_10_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_10_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_10_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_10_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_10_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_10_tooltip_rows, reference_data_row) \
    ARRAY(technologies_11_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_11_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_11_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_11_tooltip_rows, reference_data_row) \
    ARRAY(technologies_12_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_12_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_12_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_12_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_12_tooltip_rows_3_items, reference_data_items) \
    STRUCT(technologies_12_tooltip_rows_3, reference_data_row) \
    ARRAY(technologies_12_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_12_tooltip_rows, reference_data_row) \
    ARRAY(technologies_13_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_13_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_13_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_13_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_13_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_13_tooltip_rows, reference_data_row) \
    ARRAY(technologies_14_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_14_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_14_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_14_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_14_tooltip_rows_3_items, reference_data_items) \
    STRUCT(technologies_14_tooltip_rows_3, reference_data_row) \
    ARRAY(technologies_14_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_14_tooltip_rows, reference_data_row) \
    ARRAY(technologies_15_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_15_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_15_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_15_tooltip_rows, reference_data_row) \
    ARRAY(technologies_16_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_16_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_16_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_16_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_16_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_16_tooltip_rows, reference_data_row) \
    ARRAY(technologies_17_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_17_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_17_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_17_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_17_tooltip_rows_3_items, reference_data_items) \
    STRUCT(technologies_17_tooltip_rows_3, reference_data_row) \
    ARRAY(technologies_17_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_17_tooltip_rows, reference_data_row) \
    ARRAY(technologies_18_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_18_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_18_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_18_tooltip_rows, reference_data_row) \
    ARRAY(technologies_19_tooltip_rows_1_items, reference_data_items) \
    STRUCT(technologies_19_tooltip_rows_1, reference_data_row) \
    ARRAY(technologies_19_tooltip_rows_2_items, reference_data_items) \
    STRUCT(technologies_19_tooltip_rows_2, reference_data_row) \
    ARRAY(technologies_19_tooltip_rows_items, reference_data_items) \
    STRUCT(technologies_19_tooltip_rows, reference_data_row) \
    ARRAY(technologies_table, render_technologies)

#define NAME_ARRAY(name, render) {name, #name},
#define NAME_STRUCT(name, render) {&name, #name},
static const reference_data_name_t NAMES[] = {MAP_TABLES(NAME_ARRAY, NAME_STRUCT)};
#undef NAME_ARRAY
#undef NAME_STRUCT

static const reference_data_t DATA = {MODULE, NAMES, (int)(sizeof NAMES / sizeof NAMES[0])};

static void render_resource_codes(const reference_data_t *data, const char *name, const resource_code_t *table,
                                  int count) {
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i].key, "%s[%d].key", name, i);
        table_reference_int(data->module, table[i].resource, "%s[%d].resource", name, i);
    }
}

static void render_resource_names(const reference_data_t *data, const char *name, const resource_names_entry_t *table,
                                  int count) {
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i].key, "%s[%d].key", name, i);
        table_reference_string(data->module, table[i].name, "%s[%d].name", name, i);
    }
}

static void render_production_tables(const reference_data_t *data, const char *name,
                                     const production_tables_entry_t *table, int count) {
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i].key, "%s[%d].key", name, i);
        table_reference_int(data->module, table[i].count, "%s[%d].count", name, i);
        for (int v = 0; v < 4; v++) {
            table_reference_int(data->module, table[i].values[v], "%s[%d].values[%d]", name, i, v);
        }
        if (table[i].upgraded_entry != NULL) {
            table_reference_string(data->module, table[i].upgraded_entry, "%s[%d].upgraded_entry", name, i);
        }
    }
}

static void render_project_records(const reference_data_t *data, const char *name,
                                   const project_records_entry_t *table, int count) {
    for (int i = 0; i < count; i++) {
        const project_records_entry_t *entry = &table[i];
#define STRING(field)                                                                              \
    if (entry->field != NULL) {                                                                    \
        table_reference_string(data->module, entry->field, "%s[%d]." #field, name, i);             \
    }
#define INT(field) table_reference_int(data->module, entry->field, "%s[%d]." #field, name, i);
        STRING(header)
        INT(npos)
        STRING(name)
        STRING(planet_codes)
        INT(base_cost)
        STRING(production_key)
        STRING(inputs)
        STRING(output)
        STRING(upgrade_tech)
        STRING(byproduct)
        INT(upgrade_tech_false)
        INT(has_icon_sprite)
        INT(icon_sprite)
        STRING(next_type)
        STRING(class_name)
#undef STRING
#undef INT
    }
}

static void render_technologies(const reference_data_t *data, const char *name, const technologies_entry_t *table,
                                int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i].name, "%s[%d].name", name, i);
        table_reference_int(data->module, table[i].era, "%s[%d].era", name, i);
        table_reference_string(data->module, table[i].icon_key, "%s[%d].icon_key", name, i);
        table_reference_int(data->module, table[i].icon, "%s[%d].icon", name, i);
        table_reference_int(data->module, table[i].cost, "%s[%d].cost", name, i);
        snprintf(path, sizeof path, "%s[%d].tooltip_rows", name, i);
        reference_data_pointer(data, table[i].tooltip_rows, path);
    }
}

void table_reference_game_map_tables(void) {
    table_reference_int(MODULE, RESOURCE_CODE_COUNT, "RESOURCE_CODE_COUNT");
    table_reference_int(MODULE, RESOURCE_NAMES_COUNT, "RESOURCE_NAMES_COUNT");
    table_reference_int(MODULE, PRODUCTION_TABLES_COUNT, "PRODUCTION_TABLES_COUNT");
    table_reference_int(MODULE, PROJECT_RECORDS_COUNT, "PROJECT_RECORDS_COUNT");
    table_reference_int(MODULE, ICON_TOOLTIPS_COUNT, "ICON_TOOLTIPS_COUNT");
    table_reference_int(MODULE, TECHNOLOGIES_COUNT, "TECHNOLOGIES_COUNT");
    table_reference_int(MODULE, DRAW_FUNCTION_NONE, "DRAW_FUNCTION_NONE");
    table_reference_int(MODULE, DRAW_FUNCTION_DRAW_BAR, "DRAW_FUNCTION_DRAW_BAR");
    table_reference_int(MODULE, DRAW_FUNCTION_PRINT_OUTLINED, "DRAW_FUNCTION_PRINT_OUTLINED");
    table_reference_int(MODULE, DRAW_FUNCTION_DRAW_HELP_FRAME, "DRAW_FUNCTION_DRAW_HELP_FRAME");
    table_reference_int(MODULE, DRAW_FUNCTION_SPR, "DRAW_FUNCTION_SPR");
    table_reference_int(MODULE, DRAW_FUNCTION_DRAW_ROW, "DRAW_FUNCTION_DRAW_ROW");
    table_reference_int(MODULE, DRAW_FUNCTION_COUNT, "DRAW_FUNCTION_COUNT");
#define RENDER_ARRAY(name, render) render(&DATA, #name, name, (int)(sizeof name / sizeof name[0]));
#define RENDER_STRUCT(name, render) render(&DATA, &name, #name);
    MAP_TABLES(RENDER_ARRAY, RENDER_STRUCT)
#undef RENDER_ARRAY
#undef RENDER_STRUCT
}
