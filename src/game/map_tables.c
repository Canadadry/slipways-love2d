#include "game/map_tables.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

const resource_code_t resource_codes[RESOURCE_CODE_COUNT] = {
    {"f", 28}, {"o", 21}, {"b", 18}, {"p", 19}, {"g", 48}, {"t", 22}, {"l", 20}, {"w", 12}, {"k", 24}, {"\?", 36},
    {"a", 33}, {"e", 54}, {"h", 25},
};

const resource_names_entry_t resource_names_table[RESOURCE_NAMES_COUNT] = {
    {"28", "food"}, {"21", "ore"}, {"18", "bots"}, {"19", "people"}, {"48", "goods"}, {"22", "tech"},
    {"20", "organics"}, {"12", "luxury"}, {"24", "science"}, {"54", "energy"}, {"25", "joy"},
};

const production_tables_entry_t production_tables_table[PRODUCTION_TABLES_COUNT] = {
    {"f-", 4, {0, 1, 2, 3}, "f+"},
    {"f+", 4, {0, 2, 3, 4}, NULL},
    {"-", 3, {1, 2, 3}, "+"},
    {"+", 3, {2, 3, 4}, NULL},
    {"structure", 4, {0, 1, 3, 5}, "structure+"},
    {"structure+", 4, {0, 2, 5, 8}, NULL},
};

const project_records_entry_t project_records_table[PROJECT_RECORDS_COUNT] = {
    {.npos = 7, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "structure", .inputs = "p\?", .output = "k", .upgrade_tech = "superhuman ai"},
    {.npos = 6, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "f+", .inputs = "p", .output = "h"},
    {.npos = 6, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "-", .inputs = "", .output = "e"},
    {.npos = 7, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "f-", .inputs = "l", .output = "f", .upgrade_tech = "xenofoods"},
    {.npos = 6, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "f-", .inputs = "", .output = "\?"},
    {.npos = 8, .name = "", .planet_codes = "", .base_cost = 0, .production_key = "f-", .inputs = "\?\?", .output = "w", .upgrade_tech_false = 1, .byproduct = "\?"},
    {.npos = 6, .name = "nanotech", .planet_codes = "f", .base_cost = 8, .production_key = "-", .inputs = "o", .output = "t"},
    {.npos = 6, .name = "bots", .planet_codes = "f", .base_cost = 8, .production_key = "+", .inputs = "ot", .output = "b"},
    {.npos = 6, .name = "goods", .planet_codes = "f", .base_cost = 10, .production_key = "+", .inputs = "ob", .output = "g"},
    {.npos = 6, .name = "gadgets", .planet_codes = "f", .base_cost = 10, .production_key = "+", .inputs = "ot", .output = "g"},
    {.npos = 8, .name = "hiveworld", .planet_codes = "e", .base_cost = 5, .production_key = "+", .inputs = "f", .output = "p", .upgrade_tech_false = 1, .byproduct = "g"},
    {.npos = 8, .name = "colony", .planet_codes = "o", .base_cost = 10, .production_key = "-", .inputs = "of", .output = "p", .upgrade_tech = "gene rewriting", .byproduct = "g"},
    {.npos = 8, .name = "colony", .planet_codes = "x", .base_cost = 10, .production_key = "-", .inputs = "tf", .output = "p", .upgrade_tech = "gene rewriting", .byproduct = "g"},
    {.npos = 6, .name = "mine", .planet_codes = "m", .base_cost = 5, .production_key = "-", .inputs = "p", .output = "o"},
    {.npos = 6, .name = "strip", .planet_codes = "r", .base_cost = 5, .production_key = "-", .inputs = "pf", .output = "o"},
    {.npos = 8, .name = "agroworld", .planet_codes = "e", .base_cost = 5, .production_key = "-", .inputs = "b", .output = "f", .upgrade_tech = "xenofoods", .byproduct = "l"},
    {.npos = 8, .name = "algae", .planet_codes = "o", .base_cost = 8, .production_key = "-", .inputs = "b", .output = "f", .upgrade_tech = "xenofoods", .byproduct = "l"},
    {.npos = 6, .name = "breed", .planet_codes = "o", .base_cost = 5, .production_key = "+", .inputs = "p", .output = "l"},
    {.npos = 6, .name = "breed", .planet_codes = "x", .base_cost = 5, .production_key = "-", .inputs = "b", .output = "l"},
    {.npos = 6, .name = "hunt", .planet_codes = "j", .base_cost = 5, .production_key = "-", .inputs = "p", .output = "l"},
    {.npos = 6, .name = "dismantle", .planet_codes = "r", .base_cost = 5, .production_key = "-", .inputs = "pf", .output = "t"},
    {.npos = 6, .name = "scavenge", .planet_codes = "r", .base_cost = 5, .production_key = "-", .inputs = "pf", .output = "b"},
    {.npos = 6, .name = "tourism", .planet_codes = "j", .base_cost = 10, .production_key = "f-", .inputs = "pg", .output = "w"},
    {.npos = 6, .name = "luxury", .planet_codes = "e", .base_cost = 10, .production_key = "f-", .inputs = "tg", .output = "w"},
    {.header = "xenofoods"},
    {.npos = 8, .name = "farm", .planet_codes = "xj", .base_cost = 10, .production_key = "-", .inputs = "b", .output = "f", .upgrade_tech_false = 1, .byproduct = "l"},
    {.header = "geneseeds"},
    {.npos = 8, .name = "habitat", .planet_codes = "si", .base_cost = 12, .production_key = "-", .inputs = "lf", .output = "p", .upgrade_tech_false = 1, .byproduct = "g"},
    {.npos = 6, .name = "genefarm", .planet_codes = "si", .base_cost = 12, .production_key = "-", .inputs = "lb", .output = "f"},
    {.header = "drillbots"},
    {.npos = 6, .name = "excavate", .planet_codes = "m", .base_cost = 12, .production_key = "+", .inputs = "b", .output = "o"},
    {.npos = 6, .name = "excavate", .planet_codes = "ig", .base_cost = 12, .production_key = "-", .inputs = "b", .output = "o"},
    {.header = "gene rewriting"},
    {.npos = 6, .name = "uplift", .planet_codes = "f", .base_cost = 20, .production_key = "-", .inputs = "l", .output = "p"},
    {.header = "replication"},
    {.npos = 3, .name = "mechanize", .planet_codes = "mrsoijx", .base_cost = 20, .has_icon_sprite = 1, .icon_sprite = 38, .next_type = "f"},
    {.header = "biome hacking"},
    {.npos = 3, .name = "terraform", .planet_codes = "xisoj", .base_cost = 20, .has_icon_sprite = 1, .icon_sprite = 39, .next_type = "e"},
    {.header = "starbirth"},
    {.npos = 3, .name = "collapse", .planet_codes = "gm", .base_cost = 30, .has_icon_sprite = 1, .icon_sprite = 41, .class_name = "protostar"},
    {.header = "geoharvesting"},
    {.npos = 3, .name = "harvest", .planet_codes = "efmorxjisg", .base_cost = -20, .has_icon_sprite = 1, .icon_sprite = 23, .class_name = "burst"},
};

static const data_item_t icon_tooltips_1_1_items[] = {{.text = "-lab-"}};
static const data_row_t icon_tooltips_1_1 = {.count = 1, .items = icon_tooltips_1_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_1_2_items[] = {{.text = "makes"}, {.number = 24}, {.text = "science when supplied"}};
static const data_row_t icon_tooltips_1_2 = {.count = 3, .items = icon_tooltips_1_2_items};
static const data_item_t icon_tooltips_1_3_items[] = {{.text = "with"}, {.number = 19}, {.text = "and any resource"}};
static const data_row_t icon_tooltips_1_3 = {.count = 3, .items = icon_tooltips_1_3_items};
static const data_item_t icon_tooltips_1_4_items[] = {{.text = "adding more of that resource"}};
static const data_row_t icon_tooltips_1_4 = {.count = 1, .items = icon_tooltips_1_4_items};
static const data_item_t icon_tooltips_1_5_items[] = {{.text = "increases output"}};
static const data_row_t icon_tooltips_1_5 = {.count = 1, .items = icon_tooltips_1_5_items};
static const data_item_t icon_tooltips_1_items[] = {{.row = &icon_tooltips_1_1}, {.row = &icon_tooltips_1_2}, {.row = &icon_tooltips_1_3}, {.row = &icon_tooltips_1_4},
    {.row = &icon_tooltips_1_5}};
static const data_item_t icon_tooltips_2_1_items[] = {{.text = "-food processor-"}};
static const data_row_t icon_tooltips_2_1 = {.count = 1, .items = icon_tooltips_2_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_2_2_items[] = {{.text = "converts"}, {.number = 20}, {.text = "into"}, {.number = 28}};
static const data_row_t icon_tooltips_2_2 = {.count = 4, .items = icon_tooltips_2_2_items};
static const data_item_t icon_tooltips_2_items[] = {{.row = &icon_tooltips_2_1}, {.row = &icon_tooltips_2_2}};
static const data_item_t icon_tooltips_3_1_items[] = {{.text = "-slipgate-"}};
static const data_row_t icon_tooltips_3_1 = {.count = 1, .items = icon_tooltips_3_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_3_2_items[] = {{.text = "connects up to 3 slipways"}};
static const data_row_t icon_tooltips_3_2 = {.count = 1, .items = icon_tooltips_3_2_items};
static const data_item_t icon_tooltips_3_items[] = {{.row = &icon_tooltips_3_1}, {.row = &icon_tooltips_3_2}};
static const data_item_t icon_tooltips_4_1_items[] = {{.text = "-infragate-"}};
static const data_row_t icon_tooltips_4_1 = {.count = 1, .items = icon_tooltips_4_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_4_2_items[] = {{.text = "connects up to 3 slipways"}};
static const data_row_t icon_tooltips_4_2 = {.count = 1, .items = icon_tooltips_4_2_items};
static const data_item_t icon_tooltips_4_3_items[] = {{.text = "routes through infragates"}};
static const data_row_t icon_tooltips_4_3 = {.count = 1, .items = icon_tooltips_4_3_items};
static const data_item_t icon_tooltips_4_4_items[] = {{.text = "can cross other slipways"}};
static const data_row_t icon_tooltips_4_4 = {.count = 1, .items = icon_tooltips_4_4_items};
static const data_item_t icon_tooltips_4_items[] = {{.row = &icon_tooltips_4_1}, {.row = &icon_tooltips_4_2}, {.row = &icon_tooltips_4_3}, {.row = &icon_tooltips_4_4}};
static const data_item_t icon_tooltips_5_1_items[] = {{.text = "-trading hub-"}};
static const data_row_t icon_tooltips_5_1 = {.count = 1, .items = icon_tooltips_5_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_5_2_items[] = {{.text = "generates"}, {.number = 25}, {.number = 23}, {.text = "once you connect"}};
static const data_row_t icon_tooltips_5_2 = {.count = 4, .items = icon_tooltips_5_2_items};
static const data_item_t icon_tooltips_5_3_items[] = {{.text = "2 or 3 different resources"}};
static const data_row_t icon_tooltips_5_3 = {.count = 1, .items = icon_tooltips_5_3_items};
static const data_item_t icon_tooltips_5_items[] = {{.row = &icon_tooltips_5_1}, {.row = &icon_tooltips_5_2}, {.row = &icon_tooltips_5_3}};
static const data_item_t icon_tooltips_6_1_items[] = {{.text = "-synthesizer-"}};
static const data_row_t icon_tooltips_6_1 = {.count = 1, .items = icon_tooltips_6_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_6_2_items[] = {{.text = "can provide"}, {.number = 28}, {.number = 20}, {.number = 21}, {.number = 22}};
static const data_row_t icon_tooltips_6_2 = {.count = 5, .items = icon_tooltips_6_2_items};
static const data_item_t icon_tooltips_6_3_items[] = {{.text = "to a planet that needs it"}};
static const data_row_t icon_tooltips_6_3 = {.count = 1, .items = icon_tooltips_6_3_items};
static const data_item_t icon_tooltips_6_items[] = {{.row = &icon_tooltips_6_1}, {.row = &icon_tooltips_6_2}, {.row = &icon_tooltips_6_3}};
static const data_item_t icon_tooltips_7_1_items[] = {{.text = "-ascension gate-"}};
static const data_row_t icon_tooltips_7_1 = {.count = 1, .items = icon_tooltips_7_1_items, .has_color = 1, .color = 9};
static const data_item_t icon_tooltips_7_2_items[] = {{.text = "accepts"}, {.number = 19}, {.text = "and turns them"}};
static const data_row_t icon_tooltips_7_2 = {.count = 3, .items = icon_tooltips_7_2_items};
static const data_item_t icon_tooltips_7_3_items[] = {{.text = "into"}, {.number = 25}, {.text = "happiness"}};
static const data_row_t icon_tooltips_7_3 = {.count = 3, .items = icon_tooltips_7_3_items};
static const data_item_t icon_tooltips_7_items[] = {{.row = &icon_tooltips_7_1}, {.row = &icon_tooltips_7_2}, {.row = &icon_tooltips_7_3}};

const data_row_t icon_tooltips_table[ICON_TOOLTIPS_COUNT] = {
    {.count = 5, .items = icon_tooltips_1_items},
    {.count = 2, .items = icon_tooltips_2_items},
    {.count = 2, .items = icon_tooltips_3_items},
    {.count = 4, .items = icon_tooltips_4_items},
    {.count = 3, .items = icon_tooltips_5_items},
    {.count = 3, .items = icon_tooltips_6_items},
    {.count = 3, .items = icon_tooltips_7_items},
};

static const data_item_t technologies_1_tooltip_rows_items[] = {{.text = "longer range"}, {.text = "on slipways and probes"}};
static const data_row_t technologies_1_tooltip_rows = {.count = 2, .items = technologies_1_tooltip_rows_items};
static const data_item_t technologies_2_tooltip_rows_1_items[] = {{.text = "use"}, {.number = 102}, {.number = 105}, {.text = "planets"}};
static const data_row_t technologies_2_tooltip_rows_1 = {.count = 4, .items = technologies_2_tooltip_rows_1_items};
static const data_item_t technologies_2_tooltip_rows_2_items[] = {{.text = "to settle"}, {.number = 19}, {.text = "and farm"}, {.number = 28}};
static const data_row_t technologies_2_tooltip_rows_2 = {.count = 4, .items = technologies_2_tooltip_rows_2_items};
static const data_item_t technologies_2_tooltip_rows_items[] = {{.row = &technologies_2_tooltip_rows_1}, {.row = &technologies_2_tooltip_rows_2}};
static const data_row_t technologies_2_tooltip_rows = {.count = 2, .items = technologies_2_tooltip_rows_items};
static const data_item_t technologies_3_tooltip_rows_1_items[] = {{.text = "colonizing planets"}};
static const data_row_t technologies_3_tooltip_rows_1 = {.count = 1, .items = technologies_3_tooltip_rows_1_items};
static const data_item_t technologies_3_tooltip_rows_2_items[] = {{.text = "becomes 15% cheaper"}};
static const data_row_t technologies_3_tooltip_rows_2 = {.count = 1, .items = technologies_3_tooltip_rows_2_items};
static const data_item_t technologies_3_tooltip_rows_items[] = {{.row = &technologies_3_tooltip_rows_1}, {.row = &technologies_3_tooltip_rows_2}};
static const data_row_t technologies_3_tooltip_rows = {.count = 2, .items = technologies_3_tooltip_rows_items};
static const data_item_t technologies_4_tooltip_rows_1_items[] = {{.text = "build mines using"}, {.number = 18}};
static const data_row_t technologies_4_tooltip_rows_1 = {.count = 2, .items = technologies_4_tooltip_rows_1_items};
static const data_item_t technologies_4_tooltip_rows_2_items[] = {{.text = "on"}, {.number = 106}, {.number = 105}, {.number = 110}, {.text = "planets"}};
static const data_row_t technologies_4_tooltip_rows_2 = {.count = 5, .items = technologies_4_tooltip_rows_2_items};
static const data_item_t technologies_4_tooltip_rows_items[] = {{.row = &technologies_4_tooltip_rows_1}, {.row = &technologies_4_tooltip_rows_2}};
static const data_row_t technologies_4_tooltip_rows = {.count = 2, .items = technologies_4_tooltip_rows_items};
static const data_item_t technologies_5_tooltip_rows_1_items[] = {{.text = "allow farming"}, {.number = 28}, {.text = "on"}, {.number = 107},
    {.number = 108}};
static const data_row_t technologies_5_tooltip_rows_1 = {.count = 5, .items = technologies_5_tooltip_rows_1_items};
static const data_item_t technologies_5_tooltip_rows_2_items[] = {{.text = "improve"}, {.number = 28}, {.text = "output of"}, {.number = 103},
    {.number = 104}, {.number = 119}};
static const data_row_t technologies_5_tooltip_rows_2 = {.count = 6, .items = technologies_5_tooltip_rows_2_items};
static const data_item_t technologies_5_tooltip_rows_items[] = {{.row = &technologies_5_tooltip_rows_1}, {.row = &technologies_5_tooltip_rows_2}};
static const data_row_t technologies_5_tooltip_rows = {.count = 2, .items = technologies_5_tooltip_rows_items};
static const data_item_t technologies_6_tooltip_rows_1_items[] = {{.text = "build"}, {.number = 109}, {.text = "slipgates"}};
static const data_row_t technologies_6_tooltip_rows_1 = {.count = 3, .items = technologies_6_tooltip_rows_1_items};
static const data_item_t technologies_6_tooltip_rows_2_items[] = {{.text = "to extend your slipways"}};
static const data_row_t technologies_6_tooltip_rows_2 = {.count = 1, .items = technologies_6_tooltip_rows_2_items};
static const data_item_t technologies_6_tooltip_rows_items[] = {{.row = &technologies_6_tooltip_rows_1}, {.row = &technologies_6_tooltip_rows_2}};
static const data_row_t technologies_6_tooltip_rows = {.count = 2, .items = technologies_6_tooltip_rows_items};
static const data_item_t technologies_7_tooltip_rows_1_items[] = {{.text = "destroy empty planets"}};
static const data_row_t technologies_7_tooltip_rows_1 = {.count = 1, .items = technologies_7_tooltip_rows_1_items};
static const data_item_t technologies_7_tooltip_rows_2_items[] = {{.text = "for an instant 20"}, {.number = 23}, {.text = "boost"}};
static const data_row_t technologies_7_tooltip_rows_2 = {.count = 3, .items = technologies_7_tooltip_rows_2_items};
static const data_item_t technologies_7_tooltip_rows_items[] = {{.row = &technologies_7_tooltip_rows_1}, {.row = &technologies_7_tooltip_rows_2}};
static const data_row_t technologies_7_tooltip_rows = {.count = 2, .items = technologies_7_tooltip_rows_items};
static const data_item_t technologies_8_tooltip_rows_1_items[] = {{.text = "build"}, {.number = 120}, {.text = "trading hubs to"}};
static const data_row_t technologies_8_tooltip_rows_1 = {.count = 3, .items = technologies_8_tooltip_rows_1_items};
static const data_item_t technologies_8_tooltip_rows_2_items[] = {{.text = "turn 2-3 different resources"}};
static const data_row_t technologies_8_tooltip_rows_2 = {.count = 1, .items = technologies_8_tooltip_rows_2_items};
static const data_item_t technologies_8_tooltip_rows_3_items[] = {{.text = "into"}, {.number = 25}, {.text = "happiness and"}, {.number = 23},
    {.text = "money"}};
static const data_row_t technologies_8_tooltip_rows_3 = {.count = 5, .items = technologies_8_tooltip_rows_3_items};
static const data_item_t technologies_8_tooltip_rows_items[] = {{.row = &technologies_8_tooltip_rows_1}, {.row = &technologies_8_tooltip_rows_2}, {.row = &technologies_8_tooltip_rows_3}};
static const data_row_t technologies_8_tooltip_rows = {.count = 3, .items = technologies_8_tooltip_rows_items};
static const data_item_t technologies_9_tooltip_rows_1_items[] = {{.text = "turn any of"}, {.number = 102}, {.number = 104}, {.number = 105}, {.number = 106},
    {.number = 107}, {.number = 108}};
static const data_row_t technologies_9_tooltip_rows_1 = {.count = 7, .items = technologies_9_tooltip_rows_1_items};
static const data_item_t technologies_9_tooltip_rows_2_items[] = {{.text = "into"}, {.number = 118}, {.text = "forgeworlds"}};
static const data_row_t technologies_9_tooltip_rows_2 = {.count = 3, .items = technologies_9_tooltip_rows_2_items};
static const data_item_t technologies_9_tooltip_rows_items[] = {{.row = &technologies_9_tooltip_rows_1}, {.row = &technologies_9_tooltip_rows_2}};
static const data_row_t technologies_9_tooltip_rows = {.count = 2, .items = technologies_9_tooltip_rows_items};
static const data_item_t technologies_10_tooltip_rows_1_items[] = {{.text = "colonizing planets"}};
static const data_row_t technologies_10_tooltip_rows_1 = {.count = 1, .items = technologies_10_tooltip_rows_1_items};
static const data_item_t technologies_10_tooltip_rows_2_items[] = {{.text = "takes 2 months instead of 3"}};
static const data_row_t technologies_10_tooltip_rows_2 = {.count = 1, .items = technologies_10_tooltip_rows_2_items};
static const data_item_t technologies_10_tooltip_rows_items[] = {{.row = &technologies_10_tooltip_rows_1}, {.row = &technologies_10_tooltip_rows_2}};
static const data_row_t technologies_10_tooltip_rows = {.count = 2, .items = technologies_10_tooltip_rows_items};
static const data_item_t technologies_11_tooltip_rows_1_items[] = {{.text = "earn 15% more"}, {.number = 23}, {.text = "income"}};
static const data_row_t technologies_11_tooltip_rows_1 = {.count = 3, .items = technologies_11_tooltip_rows_1_items};
static const data_item_t technologies_11_tooltip_rows_items[] = {{.row = &technologies_11_tooltip_rows_1}, {.text = "from trade"}};
static const data_row_t technologies_11_tooltip_rows = {.count = 2, .items = technologies_11_tooltip_rows_items};
static const data_item_t technologies_12_tooltip_rows_1_items[] = {{.text = "collapse"}, {.number = 110}, {.number = 106}, {.text = "into stars"}};
static const data_row_t technologies_12_tooltip_rows_1 = {.count = 4, .items = technologies_12_tooltip_rows_1_items};
static const data_item_t technologies_12_tooltip_rows_2_items[] = {{.text = "to make"}, {.number = 54}, {.text = "that fills any need"}};
static const data_row_t technologies_12_tooltip_rows_2 = {.count = 3, .items = technologies_12_tooltip_rows_2_items};
static const data_item_t technologies_12_tooltip_rows_3_items[] = {{.text = "and increases production"}};
static const data_row_t technologies_12_tooltip_rows_3 = {.count = 1, .items = technologies_12_tooltip_rows_3_items};
static const data_item_t technologies_12_tooltip_rows_items[] = {{.row = &technologies_12_tooltip_rows_1}, {.row = &technologies_12_tooltip_rows_2}, {.row = &technologies_12_tooltip_rows_3}};
static const data_row_t technologies_12_tooltip_rows = {.count = 3, .items = technologies_12_tooltip_rows_items};
static const data_item_t technologies_13_tooltip_rows_1_items[] = {{.text = "terraform"}, {.number = 102}, {.number = 104}, {.number = 105}, {.number = 107},
    {.number = 108}};
static const data_row_t technologies_13_tooltip_rows_1 = {.count = 6, .items = technologies_13_tooltip_rows_1_items};
static const data_item_t technologies_13_tooltip_rows_2_items[] = {{.text = "into"}, {.number = 103}, {.text = "earth-like worlds"}};
static const data_row_t technologies_13_tooltip_rows_2 = {.count = 3, .items = technologies_13_tooltip_rows_2_items};
static const data_item_t technologies_13_tooltip_rows_items[] = {{.row = &technologies_13_tooltip_rows_1}, {.row = &technologies_13_tooltip_rows_2}};
static const data_row_t technologies_13_tooltip_rows = {.count = 2, .items = technologies_13_tooltip_rows_items};
static const data_item_t technologies_14_tooltip_rows_1_items[] = {{.text = "build"}, {.number = 121}, {.text = "infragates"}};
static const data_row_t technologies_14_tooltip_rows_1 = {.count = 3, .items = technologies_14_tooltip_rows_1_items};
static const data_item_t technologies_14_tooltip_rows_2_items[] = {{.text = "that let slipways pass"}};
static const data_row_t technologies_14_tooltip_rows_2 = {.count = 1, .items = technologies_14_tooltip_rows_2_items};
static const data_item_t technologies_14_tooltip_rows_3_items[] = {{.text = "under other slipways"}};
static const data_row_t technologies_14_tooltip_rows_3 = {.count = 1, .items = technologies_14_tooltip_rows_3_items};
static const data_item_t technologies_14_tooltip_rows_items[] = {{.row = &technologies_14_tooltip_rows_1}, {.row = &technologies_14_tooltip_rows_2}, {.row = &technologies_14_tooltip_rows_3}};
static const data_row_t technologies_14_tooltip_rows = {.count = 3, .items = technologies_14_tooltip_rows_items};
static const data_item_t technologies_15_tooltip_rows_2_items[] = {{.text = "2/5/8"}, {.number = 24}, {.text = "based on level"}};
static const data_row_t technologies_15_tooltip_rows_2 = {.count = 3, .items = technologies_15_tooltip_rows_2_items};
static const data_item_t technologies_15_tooltip_rows_items[] = {{.text = "labs now generate"}, {.row = &technologies_15_tooltip_rows_2}};
static const data_row_t technologies_15_tooltip_rows = {.count = 2, .items = technologies_15_tooltip_rows_items};
static const data_item_t technologies_16_tooltip_rows_1_items[] = {{.text = "build synthesizers"}};
static const data_row_t technologies_16_tooltip_rows_1 = {.count = 1, .items = technologies_16_tooltip_rows_1_items};
static const data_item_t technologies_16_tooltip_rows_2_items[] = {{.text = "that create"}, {.number = 28}, {.number = 20}, {.number = 21}, {.number = 22}};
static const data_row_t technologies_16_tooltip_rows_2 = {.count = 5, .items = technologies_16_tooltip_rows_2_items};
static const data_item_t technologies_16_tooltip_rows_items[] = {{.row = &technologies_16_tooltip_rows_1}, {.row = &technologies_16_tooltip_rows_2}};
static const data_row_t technologies_16_tooltip_rows = {.count = 2, .items = technologies_16_tooltip_rows_items};
static const data_item_t technologies_17_tooltip_rows_1_items[] = {{.text = "colonies yield more"}, {.number = 19}};
static const data_row_t technologies_17_tooltip_rows_1 = {.count = 2, .items = technologies_17_tooltip_rows_1_items};
static const data_item_t technologies_17_tooltip_rows_2_items[] = {{.text = "you can uplift"}, {.number = 20}, {.number = 13}, {.number = 19}};
static const data_row_t technologies_17_tooltip_rows_2 = {.count = 4, .items = technologies_17_tooltip_rows_2_items};
static const data_item_t technologies_17_tooltip_rows_3_items[] = {{.text = "using"}, {.number = 118}, {.text = "forgeworlds"}};
static const data_row_t technologies_17_tooltip_rows_3 = {.count = 3, .items = technologies_17_tooltip_rows_3_items};
static const data_item_t technologies_17_tooltip_rows_items[] = {{.row = &technologies_17_tooltip_rows_1}, {.row = &technologies_17_tooltip_rows_2}, {.row = &technologies_17_tooltip_rows_3}};
static const data_row_t technologies_17_tooltip_rows = {.count = 3, .items = technologies_17_tooltip_rows_items};
static const data_item_t technologies_18_tooltip_rows_1_items[] = {{.text = "adds 3 months to each"}, {.number = 15}, {.text = "year"}};
static const data_row_t technologies_18_tooltip_rows_1 = {.count = 3, .items = technologies_18_tooltip_rows_1_items};
static const data_item_t technologies_18_tooltip_rows_items[] = {{.row = &technologies_18_tooltip_rows_1}};
static const data_row_t technologies_18_tooltip_rows = {.count = 1, .items = technologies_18_tooltip_rows_items};
static const data_item_t technologies_19_tooltip_rows_1_items[] = {{.text = "build"}, {.number = 127}, {.text = "ascension gates"}};
static const data_row_t technologies_19_tooltip_rows_1 = {.count = 3, .items = technologies_19_tooltip_rows_1_items};
static const data_item_t technologies_19_tooltip_rows_2_items[] = {{.text = "to turn"}, {.number = 19}, {.text = "into"}, {.number = 25},
    {.text = "happiness"}};
static const data_row_t technologies_19_tooltip_rows_2 = {.count = 5, .items = technologies_19_tooltip_rows_2_items};
static const data_item_t technologies_19_tooltip_rows_items[] = {{.row = &technologies_19_tooltip_rows_1}, {.row = &technologies_19_tooltip_rows_2}};
static const data_row_t technologies_19_tooltip_rows = {.count = 2, .items = technologies_19_tooltip_rows_items};

const technologies_entry_t technologies_table[TECHNOLOGIES_COUNT] = {
    {"space folding", 1, "t", 22, 4, &technologies_1_tooltip_rows},
    {"geneseeds", 1, "l", 20, 5, &technologies_2_tooltip_rows},
    {"nanomaterials", 1, "o", 21, 6, &technologies_3_tooltip_rows},
    {"drillbots", 1, "b", 18, 7, &technologies_4_tooltip_rows},
    {"xenofoods", 2, "l", 20, 10, &technologies_5_tooltip_rows},
    {"slipgates", 2, "t", 22, 12, &technologies_6_tooltip_rows},
    {"geoharvesting", 2, "o", 21, 14, &technologies_7_tooltip_rows},
    {"trade league", 2, "t", 22, 16, &technologies_8_tooltip_rows},
    {"replication", 2, "b", 18, 18, &technologies_9_tooltip_rows},
    {"autoassemblers", 3, "b", 18, 19, &technologies_10_tooltip_rows},
    {"skill implants", 3, "b", 18, 22, &technologies_11_tooltip_rows},
    {"starbirth", 3, "o", 21, 25, &technologies_12_tooltip_rows},
    {"biome hacking", 3, "l", 20, 28, &technologies_13_tooltip_rows},
    {"infraspace", 3, "t", 22, 31, &technologies_14_tooltip_rows},
    {"superhuman ai", 4, "t", 22, 34, &technologies_15_tooltip_rows},
    {"void synthesis", 4, "o", 21, 38, &technologies_16_tooltip_rows},
    {"gene rewriting", 4, "l", 20, 42, &technologies_17_tooltip_rows},
    {"time compression", 4, "b", 18, 46, &technologies_18_tooltip_rows},
    {"ascension", 4, "a", 33, 50, &technologies_19_tooltip_rows},
};

int resource_code_lookup(const char *key, int *out_resource) {
    int i;

    assert(key != NULL && "resource_code_lookup: key must not be NULL");
    for (i = 0; i < RESOURCE_CODE_COUNT; i++) {
        if (strcmp(resource_codes[i].key, key) == 0) {
            *out_resource = resource_codes[i].resource;
            return 1;
        }
    }
    return 0;
}

const char *resource_names_lookup(const char *key) {
    int i;

    assert(key != NULL && "resource_names_lookup: key must not be NULL");
    for (i = 0; i < RESOURCE_NAMES_COUNT; i++) {
        if (strcmp(resource_names_table[i].key, key) == 0) {
            return resource_names_table[i].name;
        }
    }
    return NULL;
}

const production_tables_entry_t *production_tables_lookup(const char *key) {
    int i;

    assert(key != NULL && "production_tables_lookup: key must not be NULL");
    for (i = 0; i < PRODUCTION_TABLES_COUNT; i++) {
        if (strcmp(production_tables_table[i].key, key) == 0) {
            return &production_tables_table[i];
        }
    }
    return NULL;
}

const technologies_entry_t *map_tables_technologies_lookup(const char *px) {
    int i;

    for (i = 0; px != NULL && i < TECHNOLOGIES_COUNT; i++) {
        if (strcmp(technologies_table[i].name, px) == 0) {
            return &technologies_table[i];
        }
    }
    return NULL;
}
