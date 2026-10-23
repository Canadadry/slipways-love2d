-- Copy of src/game/map_tables.h then src/game/map_tables.c.
--
-- Shapes, with the C field names (a pointer is the table, NULL is nil; the
-- lookup tables and production values indexed from 1, data_row_t items from 0):
--   resource_code_t: { key, resource }.
--   resource_names_entry_t: { key, name }.
--   production_tables_entry_t: { key, count, values (4 entries), upgraded_entry }.
--   data_item_t: { text, number, row, is_false, is_fixed, fixed, is_nil }.
--   data_row_t: { count, items, color, draw_function, resource }, color and resource nil
--     when absent.
--   project_records_entry_t: { header, npos, name, planet_codes, base_cost,
--     production_key, inputs, output, upgrade_tech, byproduct,
--     upgrade_tech_false, icon_sprite, next_type, class_name }, base_cost and icon_sprite
--     nil when absent.
--   technologies_entry_t: { name, era, icon_key, icon, cost, tooltip_rows }.
-- Every data table is a module field, also the C statics: a local per table
-- would pass LuaJIT's 200 locals per function.
local M = {}

M.RESOURCE_CODE_COUNT = 13
M.RESOURCE_NAMES_COUNT = 11
M.PRODUCTION_TABLES_COUNT = 6
M.PROJECT_RECORDS_COUNT = 42
M.ICON_TOOLTIPS_COUNT = 7
M.TECHNOLOGIES_COUNT = 19

M.DRAW_FUNCTION_NONE = 0
M.DRAW_FUNCTION_DRAW_BAR = 1
M.DRAW_FUNCTION_PRINT_OUTLINED = 2
M.DRAW_FUNCTION_DRAW_HELP_FRAME = 3
M.DRAW_FUNCTION_SPR = 4
M.DRAW_FUNCTION_DRAW_ROW = 5
M.DRAW_FUNCTION_COUNT = 6

-- Lua only, like the two constructors below, here rather than after the C
-- functions because the tables below call them at load. Returns made; a
-- misspelt field would be dropped silently and read as its zero.
function M.map_tables_check_fields(fields, made, type_name)
  for key in pairs(fields) do
    if made[key] == nil then
      error(type_name .. ": unknown field " .. tostring(key))
    end
  end
  return made
end

local check_fields = M.map_tables_check_fields

-- Lua only: a new data_item_t, the fields not given at their C zero.
function M.map_tables_make_data_item(fields)
  return check_fields(fields, {
    text = fields.text,
    number = fields.number or 0,
    row = fields.row,
    is_false = fields.is_false or false,
    is_fixed = fields.is_fixed or false,
    fixed = fields.fixed or 0,
    is_nil = fields.is_nil or false,
  }, "data_item_t")
end

-- Lua only: a new data_row_t, the fields not given at their C zero.
function M.map_tables_make_data_row(fields)
  return check_fields(fields, {
    count = fields.count or 0,
    items = fields.items,
    color = fields.color,
    draw_function = fields.draw_function or M.DRAW_FUNCTION_NONE,
    resource = fields.resource,
  }, "data_row_t")
end

local data_item = M.map_tables_make_data_item
local data_row = M.map_tables_make_data_row

local function project_record(fields)
  return check_fields(fields, {
    header = fields.header,
    npos = fields.npos or 0,
    name = fields.name,
    planet_codes = fields.planet_codes,
    base_cost = fields.base_cost,
    production_key = fields.production_key,
    inputs = fields.inputs,
    output = fields.output,
    upgrade_tech = fields.upgrade_tech,
    byproduct = fields.byproduct,
    upgrade_tech_false = fields.upgrade_tech_false or false,
    icon_sprite = fields.icon_sprite,
    next_type = fields.next_type,
    class_name = fields.class_name,
  }, "project_records_entry_t")
end

M.resource_codes = {
  { key = "f", resource = 28 }, { key = "o", resource = 21 }, { key = "b", resource = 18 },
  { key = "p", resource = 19 }, { key = "g", resource = 48 }, { key = "t", resource = 22 },
  { key = "l", resource = 20 }, { key = "w", resource = 12 }, { key = "k", resource = 24 },
  { key = "?", resource = 36 }, { key = "a", resource = 33 }, { key = "e", resource = 54 },
  { key = "h", resource = 25 },
}

M.resource_names_table = {
  { key = "28", name = "food" }, { key = "21", name = "ore" }, { key = "18", name = "bots" },
  { key = "19", name = "people" }, { key = "48", name = "goods" }, { key = "22", name = "tech" },
  { key = "20", name = "organics" }, { key = "12", name = "luxury" }, { key = "24", name = "science" },
  { key = "54", name = "energy" }, { key = "25", name = "joy" },
}

M.production_tables_table = {
  { key = "f-", count = 4, values = { 0, 1, 2, 3 }, upgraded_entry = "f+" },
  { key = "f+", count = 4, values = { 0, 2, 3, 4 }, upgraded_entry = nil },
  { key = "-", count = 3, values = { 1, 2, 3, 0 }, upgraded_entry = "+" },
  { key = "+", count = 3, values = { 2, 3, 4, 0 }, upgraded_entry = nil },
  { key = "structure", count = 4, values = { 0, 1, 3, 5 }, upgraded_entry = "structure+" },
  { key = "structure+", count = 4, values = { 0, 2, 5, 8 }, upgraded_entry = nil },
}

M.project_records_table = {
  project_record({
    npos = 7, name = "", planet_codes = "", base_cost = 0, production_key = "structure", inputs = "p?", output = "k",
    upgrade_tech = "superhuman ai",
  }),
  project_record({
    npos = 6, name = "", planet_codes = "", base_cost = 0, production_key = "f+", inputs = "p", output = "h",
  }),
  project_record({
    npos = 6, name = "", planet_codes = "", base_cost = 0, production_key = "-", inputs = "", output = "e",
  }),
  project_record({
    npos = 7, name = "", planet_codes = "", base_cost = 0, production_key = "f-", inputs = "l", output = "f",
    upgrade_tech = "xenofoods",
  }),
  project_record({
    npos = 6, name = "", planet_codes = "", base_cost = 0, production_key = "f-", inputs = "", output = "?",
  }),
  project_record({
    npos = 8, name = "", planet_codes = "", base_cost = 0, production_key = "f-", inputs = "??", output = "w",
    upgrade_tech_false = true, byproduct = "?",
  }),
  project_record({
    npos = 6, name = "nanotech", planet_codes = "f", base_cost = 8, production_key = "-", inputs = "o", output = "t",
  }),
  project_record({
    npos = 6, name = "bots", planet_codes = "f", base_cost = 8, production_key = "+", inputs = "ot", output = "b",
  }),
  project_record({
    npos = 6, name = "goods", planet_codes = "f", base_cost = 10, production_key = "+", inputs = "ob", output = "g",
  }),
  project_record({
    npos = 6, name = "gadgets", planet_codes = "f", base_cost = 10, production_key = "+", inputs = "ot", output = "g",
  }),
  project_record({
    npos = 8, name = "hiveworld", planet_codes = "e", base_cost = 5, production_key = "+", inputs = "f", output = "p",
    upgrade_tech_false = true, byproduct = "g",
  }),
  project_record({
    npos = 8, name = "colony", planet_codes = "o", base_cost = 10, production_key = "-", inputs = "of", output = "p",
    upgrade_tech = "gene rewriting", byproduct = "g",
  }),
  project_record({
    npos = 8, name = "colony", planet_codes = "x", base_cost = 10, production_key = "-", inputs = "tf", output = "p",
    upgrade_tech = "gene rewriting", byproduct = "g",
  }),
  project_record({
    npos = 6, name = "mine", planet_codes = "m", base_cost = 5, production_key = "-", inputs = "p", output = "o",
  }),
  project_record({
    npos = 6, name = "strip", planet_codes = "r", base_cost = 5, production_key = "-", inputs = "pf", output = "o",
  }),
  project_record({
    npos = 8, name = "agroworld", planet_codes = "e", base_cost = 5, production_key = "-", inputs = "b", output = "f",
    upgrade_tech = "xenofoods", byproduct = "l",
  }),
  project_record({
    npos = 8, name = "algae", planet_codes = "o", base_cost = 8, production_key = "-", inputs = "b", output = "f",
    upgrade_tech = "xenofoods", byproduct = "l",
  }),
  project_record({
    npos = 6, name = "breed", planet_codes = "o", base_cost = 5, production_key = "+", inputs = "p", output = "l",
  }),
  project_record({
    npos = 6, name = "breed", planet_codes = "x", base_cost = 5, production_key = "-", inputs = "b", output = "l",
  }),
  project_record({
    npos = 6, name = "hunt", planet_codes = "j", base_cost = 5, production_key = "-", inputs = "p", output = "l",
  }),
  project_record({
    npos = 6, name = "dismantle", planet_codes = "r", base_cost = 5, production_key = "-", inputs = "pf", output = "t",
  }),
  project_record({
    npos = 6, name = "scavenge", planet_codes = "r", base_cost = 5, production_key = "-", inputs = "pf", output = "b",
  }),
  project_record({
    npos = 6, name = "tourism", planet_codes = "j", base_cost = 10, production_key = "f-", inputs = "pg", output = "w",
  }),
  project_record({
    npos = 6, name = "luxury", planet_codes = "e", base_cost = 10, production_key = "f-", inputs = "tg", output = "w",
  }),
  project_record({ header = "xenofoods" }),
  project_record({
    npos = 8, name = "farm", planet_codes = "xj", base_cost = 10, production_key = "-", inputs = "b", output = "f",
    upgrade_tech_false = true, byproduct = "l",
  }),
  project_record({ header = "geneseeds" }),
  project_record({
    npos = 8, name = "habitat", planet_codes = "si", base_cost = 12, production_key = "-", inputs = "lf", output = "p",
    upgrade_tech_false = true, byproduct = "g",
  }),
  project_record({
    npos = 6, name = "genefarm", planet_codes = "si", base_cost = 12, production_key = "-", inputs = "lb",
    output = "f",
  }),
  project_record({ header = "drillbots" }),
  project_record({
    npos = 6, name = "excavate", planet_codes = "m", base_cost = 12, production_key = "+", inputs = "b", output = "o",
  }),
  project_record({
    npos = 6, name = "excavate", planet_codes = "ig", base_cost = 12, production_key = "-", inputs = "b", output = "o",
  }),
  project_record({ header = "gene rewriting" }),
  project_record({
    npos = 6, name = "uplift", planet_codes = "f", base_cost = 20, production_key = "-", inputs = "l", output = "p",
  }),
  project_record({ header = "replication" }),
  project_record({
    npos = 3, name = "mechanize", planet_codes = "mrsoijx", base_cost = 20, icon_sprite = 38,
    next_type = "f",
  }),
  project_record({ header = "biome hacking" }),
  project_record({
    npos = 3, name = "terraform", planet_codes = "xisoj", base_cost = 20, icon_sprite = 39,
    next_type = "e",
  }),
  project_record({ header = "starbirth" }),
  project_record({
    npos = 3, name = "collapse", planet_codes = "gm", base_cost = 30, icon_sprite = 41,
    class_name = "protostar",
  }),
  project_record({ header = "geoharvesting" }),
  project_record({
    npos = 3, name = "harvest", planet_codes = "efmorxjisg", base_cost = -20, icon_sprite = 23,
    class_name = "burst",
  }),
}

M.icon_tooltips_1_1_items = { [0] = data_item({ text = "-lab-" }) }
M.icon_tooltips_1_1 = data_row({ count = 1, items = M.icon_tooltips_1_1_items, color = 9 })
M.icon_tooltips_1_2_items = {
  [0] = data_item({ text = "makes" }),
  data_item({ number = 24 }),
  data_item({ text = "science when supplied" }),
}
M.icon_tooltips_1_2 = data_row({ count = 3, items = M.icon_tooltips_1_2_items })
M.icon_tooltips_1_3_items = {
  [0] = data_item({ text = "with" }),
  data_item({ number = 19 }),
  data_item({ text = "and any resource" }),
}
M.icon_tooltips_1_3 = data_row({ count = 3, items = M.icon_tooltips_1_3_items })
M.icon_tooltips_1_4_items = { [0] = data_item({ text = "adding more of that resource" }) }
M.icon_tooltips_1_4 = data_row({ count = 1, items = M.icon_tooltips_1_4_items })
M.icon_tooltips_1_5_items = { [0] = data_item({ text = "increases output" }) }
M.icon_tooltips_1_5 = data_row({ count = 1, items = M.icon_tooltips_1_5_items })
M.icon_tooltips_1_items = {
  [0] = data_item({ row = M.icon_tooltips_1_1 }),
  data_item({ row = M.icon_tooltips_1_2 }),
  data_item({ row = M.icon_tooltips_1_3 }),
  data_item({ row = M.icon_tooltips_1_4 }),
  data_item({ row = M.icon_tooltips_1_5 }),
}
M.icon_tooltips_2_1_items = { [0] = data_item({ text = "-food processor-" }) }
M.icon_tooltips_2_1 = data_row({ count = 1, items = M.icon_tooltips_2_1_items, color = 9 })
M.icon_tooltips_2_2_items = {
  [0] = data_item({ text = "converts" }),
  data_item({ number = 20 }),
  data_item({ text = "into" }),
  data_item({ number = 28 }),
}
M.icon_tooltips_2_2 = data_row({ count = 4, items = M.icon_tooltips_2_2_items })
M.icon_tooltips_2_items = { [0] = data_item({ row = M.icon_tooltips_2_1 }), data_item({ row = M.icon_tooltips_2_2 }) }
M.icon_tooltips_3_1_items = { [0] = data_item({ text = "-slipgate-" }) }
M.icon_tooltips_3_1 = data_row({ count = 1, items = M.icon_tooltips_3_1_items, color = 9 })
M.icon_tooltips_3_2_items = { [0] = data_item({ text = "connects up to 3 slipways" }) }
M.icon_tooltips_3_2 = data_row({ count = 1, items = M.icon_tooltips_3_2_items })
M.icon_tooltips_3_items = { [0] = data_item({ row = M.icon_tooltips_3_1 }), data_item({ row = M.icon_tooltips_3_2 }) }
M.icon_tooltips_4_1_items = { [0] = data_item({ text = "-infragate-" }) }
M.icon_tooltips_4_1 = data_row({ count = 1, items = M.icon_tooltips_4_1_items, color = 9 })
M.icon_tooltips_4_2_items = { [0] = data_item({ text = "connects up to 3 slipways" }) }
M.icon_tooltips_4_2 = data_row({ count = 1, items = M.icon_tooltips_4_2_items })
M.icon_tooltips_4_3_items = { [0] = data_item({ text = "routes through infragates" }) }
M.icon_tooltips_4_3 = data_row({ count = 1, items = M.icon_tooltips_4_3_items })
M.icon_tooltips_4_4_items = { [0] = data_item({ text = "can cross other slipways" }) }
M.icon_tooltips_4_4 = data_row({ count = 1, items = M.icon_tooltips_4_4_items })
M.icon_tooltips_4_items = {
  [0] = data_item({ row = M.icon_tooltips_4_1 }),
  data_item({ row = M.icon_tooltips_4_2 }),
  data_item({ row = M.icon_tooltips_4_3 }),
  data_item({ row = M.icon_tooltips_4_4 }),
}
M.icon_tooltips_5_1_items = { [0] = data_item({ text = "-trading hub-" }) }
M.icon_tooltips_5_1 = data_row({ count = 1, items = M.icon_tooltips_5_1_items, color = 9 })
M.icon_tooltips_5_2_items = {
  [0] = data_item({ text = "generates" }),
  data_item({ number = 25 }),
  data_item({ number = 23 }),
  data_item({ text = "once you connect" }),
}
M.icon_tooltips_5_2 = data_row({ count = 4, items = M.icon_tooltips_5_2_items })
M.icon_tooltips_5_3_items = { [0] = data_item({ text = "2 or 3 different resources" }) }
M.icon_tooltips_5_3 = data_row({ count = 1, items = M.icon_tooltips_5_3_items })
M.icon_tooltips_5_items = {
  [0] = data_item({ row = M.icon_tooltips_5_1 }),
  data_item({ row = M.icon_tooltips_5_2 }),
  data_item({ row = M.icon_tooltips_5_3 }),
}
M.icon_tooltips_6_1_items = { [0] = data_item({ text = "-synthesizer-" }) }
M.icon_tooltips_6_1 = data_row({ count = 1, items = M.icon_tooltips_6_1_items, color = 9 })
M.icon_tooltips_6_2_items = {
  [0] = data_item({ text = "can provide" }),
  data_item({ number = 28 }),
  data_item({ number = 20 }),
  data_item({ number = 21 }),
  data_item({ number = 22 }),
}
M.icon_tooltips_6_2 = data_row({ count = 5, items = M.icon_tooltips_6_2_items })
M.icon_tooltips_6_3_items = { [0] = data_item({ text = "to a planet that needs it" }) }
M.icon_tooltips_6_3 = data_row({ count = 1, items = M.icon_tooltips_6_3_items })
M.icon_tooltips_6_items = {
  [0] = data_item({ row = M.icon_tooltips_6_1 }),
  data_item({ row = M.icon_tooltips_6_2 }),
  data_item({ row = M.icon_tooltips_6_3 }),
}
M.icon_tooltips_7_1_items = { [0] = data_item({ text = "-ascension gate-" }) }
M.icon_tooltips_7_1 = data_row({ count = 1, items = M.icon_tooltips_7_1_items, color = 9 })
M.icon_tooltips_7_2_items = {
  [0] = data_item({ text = "accepts" }),
  data_item({ number = 19 }),
  data_item({ text = "and turns them" }),
}
M.icon_tooltips_7_2 = data_row({ count = 3, items = M.icon_tooltips_7_2_items })
M.icon_tooltips_7_3_items = {
  [0] = data_item({ text = "into" }),
  data_item({ number = 25 }),
  data_item({ text = "happiness" }),
}
M.icon_tooltips_7_3 = data_row({ count = 3, items = M.icon_tooltips_7_3_items })
M.icon_tooltips_7_items = {
  [0] = data_item({ row = M.icon_tooltips_7_1 }),
  data_item({ row = M.icon_tooltips_7_2 }),
  data_item({ row = M.icon_tooltips_7_3 }),
}
M.icon_tooltips_table = {
  data_row({ count = 5, items = M.icon_tooltips_1_items }),
  data_row({ count = 2, items = M.icon_tooltips_2_items }),
  data_row({ count = 2, items = M.icon_tooltips_3_items }),
  data_row({ count = 4, items = M.icon_tooltips_4_items }),
  data_row({ count = 3, items = M.icon_tooltips_5_items }),
  data_row({ count = 3, items = M.icon_tooltips_6_items }),
  data_row({ count = 3, items = M.icon_tooltips_7_items }),
}

M.technologies_1_tooltip_rows_items = {
  [0] = data_item({ text = "longer range" }),
  data_item({ text = "on slipways and probes" }),
}
M.technologies_1_tooltip_rows = data_row({ count = 2, items = M.technologies_1_tooltip_rows_items })
M.technologies_2_tooltip_rows_1_items = {
  [0] = data_item({ text = "use" }),
  data_item({ number = 102 }),
  data_item({ number = 105 }),
  data_item({ text = "planets" }),
}
M.technologies_2_tooltip_rows_1 = data_row({ count = 4, items = M.technologies_2_tooltip_rows_1_items })
M.technologies_2_tooltip_rows_2_items = {
  [0] = data_item({ text = "to settle" }),
  data_item({ number = 19 }),
  data_item({ text = "and farm" }),
  data_item({ number = 28 }),
}
M.technologies_2_tooltip_rows_2 = data_row({ count = 4, items = M.technologies_2_tooltip_rows_2_items })
M.technologies_2_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_2_tooltip_rows_1 }),
  data_item({ row = M.technologies_2_tooltip_rows_2 }),
}
M.technologies_2_tooltip_rows = data_row({ count = 2, items = M.technologies_2_tooltip_rows_items })
M.technologies_3_tooltip_rows_1_items = { [0] = data_item({ text = "colonizing planets" }) }
M.technologies_3_tooltip_rows_1 = data_row({ count = 1, items = M.technologies_3_tooltip_rows_1_items })
M.technologies_3_tooltip_rows_2_items = { [0] = data_item({ text = "becomes 15% cheaper" }) }
M.technologies_3_tooltip_rows_2 = data_row({ count = 1, items = M.technologies_3_tooltip_rows_2_items })
M.technologies_3_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_3_tooltip_rows_1 }),
  data_item({ row = M.technologies_3_tooltip_rows_2 }),
}
M.technologies_3_tooltip_rows = data_row({ count = 2, items = M.technologies_3_tooltip_rows_items })
M.technologies_4_tooltip_rows_1_items = { [0] = data_item({ text = "build mines using" }), data_item({ number = 18 }) }
M.technologies_4_tooltip_rows_1 = data_row({ count = 2, items = M.technologies_4_tooltip_rows_1_items })
M.technologies_4_tooltip_rows_2_items = {
  [0] = data_item({ text = "on" }),
  data_item({ number = 106 }),
  data_item({ number = 105 }),
  data_item({ number = 110 }),
  data_item({ text = "planets" }),
}
M.technologies_4_tooltip_rows_2 = data_row({ count = 5, items = M.technologies_4_tooltip_rows_2_items })
M.technologies_4_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_4_tooltip_rows_1 }),
  data_item({ row = M.technologies_4_tooltip_rows_2 }),
}
M.technologies_4_tooltip_rows = data_row({ count = 2, items = M.technologies_4_tooltip_rows_items })
M.technologies_5_tooltip_rows_1_items = {
  [0] = data_item({ text = "allow farming" }),
  data_item({ number = 28 }),
  data_item({ text = "on" }),
  data_item({ number = 107 }),
  data_item({ number = 108 }),
}
M.technologies_5_tooltip_rows_1 = data_row({ count = 5, items = M.technologies_5_tooltip_rows_1_items })
M.technologies_5_tooltip_rows_2_items = {
  [0] = data_item({ text = "improve" }),
  data_item({ number = 28 }),
  data_item({ text = "output of" }),
  data_item({ number = 103 }),
  data_item({ number = 104 }),
  data_item({ number = 119 }),
}
M.technologies_5_tooltip_rows_2 = data_row({ count = 6, items = M.technologies_5_tooltip_rows_2_items })
M.technologies_5_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_5_tooltip_rows_1 }),
  data_item({ row = M.technologies_5_tooltip_rows_2 }),
}
M.technologies_5_tooltip_rows = data_row({ count = 2, items = M.technologies_5_tooltip_rows_items })
M.technologies_6_tooltip_rows_1_items = {
  [0] = data_item({ text = "build" }),
  data_item({ number = 109 }),
  data_item({ text = "slipgates" }),
}
M.technologies_6_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_6_tooltip_rows_1_items })
M.technologies_6_tooltip_rows_2_items = { [0] = data_item({ text = "to extend your slipways" }) }
M.technologies_6_tooltip_rows_2 = data_row({ count = 1, items = M.technologies_6_tooltip_rows_2_items })
M.technologies_6_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_6_tooltip_rows_1 }),
  data_item({ row = M.technologies_6_tooltip_rows_2 }),
}
M.technologies_6_tooltip_rows = data_row({ count = 2, items = M.technologies_6_tooltip_rows_items })
M.technologies_7_tooltip_rows_1_items = { [0] = data_item({ text = "destroy empty planets" }) }
M.technologies_7_tooltip_rows_1 = data_row({ count = 1, items = M.technologies_7_tooltip_rows_1_items })
M.technologies_7_tooltip_rows_2_items = {
  [0] = data_item({ text = "for an instant 20" }),
  data_item({ number = 23 }),
  data_item({ text = "boost" }),
}
M.technologies_7_tooltip_rows_2 = data_row({ count = 3, items = M.technologies_7_tooltip_rows_2_items })
M.technologies_7_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_7_tooltip_rows_1 }),
  data_item({ row = M.technologies_7_tooltip_rows_2 }),
}
M.technologies_7_tooltip_rows = data_row({ count = 2, items = M.technologies_7_tooltip_rows_items })
M.technologies_8_tooltip_rows_1_items = {
  [0] = data_item({ text = "build" }),
  data_item({ number = 120 }),
  data_item({ text = "trading hubs to" }),
}
M.technologies_8_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_8_tooltip_rows_1_items })
M.technologies_8_tooltip_rows_2_items = { [0] = data_item({ text = "turn 2-3 different resources" }) }
M.technologies_8_tooltip_rows_2 = data_row({ count = 1, items = M.technologies_8_tooltip_rows_2_items })
M.technologies_8_tooltip_rows_3_items = {
  [0] = data_item({ text = "into" }),
  data_item({ number = 25 }),
  data_item({ text = "happiness and" }),
  data_item({ number = 23 }),
  data_item({ text = "money" }),
}
M.technologies_8_tooltip_rows_3 = data_row({ count = 5, items = M.technologies_8_tooltip_rows_3_items })
M.technologies_8_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_8_tooltip_rows_1 }),
  data_item({ row = M.technologies_8_tooltip_rows_2 }),
  data_item({ row = M.technologies_8_tooltip_rows_3 }),
}
M.technologies_8_tooltip_rows = data_row({ count = 3, items = M.technologies_8_tooltip_rows_items })
M.technologies_9_tooltip_rows_1_items = {
  [0] = data_item({ text = "turn any of" }),
  data_item({ number = 102 }),
  data_item({ number = 104 }),
  data_item({ number = 105 }),
  data_item({ number = 106 }),
  data_item({ number = 107 }),
  data_item({ number = 108 }),
}
M.technologies_9_tooltip_rows_1 = data_row({ count = 7, items = M.technologies_9_tooltip_rows_1_items })
M.technologies_9_tooltip_rows_2_items = {
  [0] = data_item({ text = "into" }),
  data_item({ number = 118 }),
  data_item({ text = "forgeworlds" }),
}
M.technologies_9_tooltip_rows_2 = data_row({ count = 3, items = M.technologies_9_tooltip_rows_2_items })
M.technologies_9_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_9_tooltip_rows_1 }),
  data_item({ row = M.technologies_9_tooltip_rows_2 }),
}
M.technologies_9_tooltip_rows = data_row({ count = 2, items = M.technologies_9_tooltip_rows_items })
M.technologies_10_tooltip_rows_1_items = { [0] = data_item({ text = "colonizing planets" }) }
M.technologies_10_tooltip_rows_1 = data_row({ count = 1, items = M.technologies_10_tooltip_rows_1_items })
M.technologies_10_tooltip_rows_2_items = { [0] = data_item({ text = "takes 2 months instead of 3" }) }
M.technologies_10_tooltip_rows_2 = data_row({ count = 1, items = M.technologies_10_tooltip_rows_2_items })
M.technologies_10_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_10_tooltip_rows_1 }),
  data_item({ row = M.technologies_10_tooltip_rows_2 }),
}
M.technologies_10_tooltip_rows = data_row({ count = 2, items = M.technologies_10_tooltip_rows_items })
M.technologies_11_tooltip_rows_1_items = {
  [0] = data_item({ text = "earn 15% more" }),
  data_item({ number = 23 }),
  data_item({ text = "income" }),
}
M.technologies_11_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_11_tooltip_rows_1_items })
M.technologies_11_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_11_tooltip_rows_1 }),
  data_item({ text = "from trade" }),
}
M.technologies_11_tooltip_rows = data_row({ count = 2, items = M.technologies_11_tooltip_rows_items })
M.technologies_12_tooltip_rows_1_items = {
  [0] = data_item({ text = "collapse" }),
  data_item({ number = 110 }),
  data_item({ number = 106 }),
  data_item({ text = "into stars" }),
}
M.technologies_12_tooltip_rows_1 = data_row({ count = 4, items = M.technologies_12_tooltip_rows_1_items })
M.technologies_12_tooltip_rows_2_items = {
  [0] = data_item({ text = "to make" }),
  data_item({ number = 54 }),
  data_item({ text = "that fills any need" }),
}
M.technologies_12_tooltip_rows_2 = data_row({ count = 3, items = M.technologies_12_tooltip_rows_2_items })
M.technologies_12_tooltip_rows_3_items = { [0] = data_item({ text = "and increases production" }) }
M.technologies_12_tooltip_rows_3 = data_row({ count = 1, items = M.technologies_12_tooltip_rows_3_items })
M.technologies_12_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_12_tooltip_rows_1 }),
  data_item({ row = M.technologies_12_tooltip_rows_2 }),
  data_item({ row = M.technologies_12_tooltip_rows_3 }),
}
M.technologies_12_tooltip_rows = data_row({ count = 3, items = M.technologies_12_tooltip_rows_items })
M.technologies_13_tooltip_rows_1_items = {
  [0] = data_item({ text = "terraform" }),
  data_item({ number = 102 }),
  data_item({ number = 104 }),
  data_item({ number = 105 }),
  data_item({ number = 107 }),
  data_item({ number = 108 }),
}
M.technologies_13_tooltip_rows_1 = data_row({ count = 6, items = M.technologies_13_tooltip_rows_1_items })
M.technologies_13_tooltip_rows_2_items = {
  [0] = data_item({ text = "into" }),
  data_item({ number = 103 }),
  data_item({ text = "earth-like worlds" }),
}
M.technologies_13_tooltip_rows_2 = data_row({ count = 3, items = M.technologies_13_tooltip_rows_2_items })
M.technologies_13_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_13_tooltip_rows_1 }),
  data_item({ row = M.technologies_13_tooltip_rows_2 }),
}
M.technologies_13_tooltip_rows = data_row({ count = 2, items = M.technologies_13_tooltip_rows_items })
M.technologies_14_tooltip_rows_1_items = {
  [0] = data_item({ text = "build" }),
  data_item({ number = 121 }),
  data_item({ text = "infragates" }),
}
M.technologies_14_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_14_tooltip_rows_1_items })
M.technologies_14_tooltip_rows_2_items = { [0] = data_item({ text = "that let slipways pass" }) }
M.technologies_14_tooltip_rows_2 = data_row({ count = 1, items = M.technologies_14_tooltip_rows_2_items })
M.technologies_14_tooltip_rows_3_items = { [0] = data_item({ text = "under other slipways" }) }
M.technologies_14_tooltip_rows_3 = data_row({ count = 1, items = M.technologies_14_tooltip_rows_3_items })
M.technologies_14_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_14_tooltip_rows_1 }),
  data_item({ row = M.technologies_14_tooltip_rows_2 }),
  data_item({ row = M.technologies_14_tooltip_rows_3 }),
}
M.technologies_14_tooltip_rows = data_row({ count = 3, items = M.technologies_14_tooltip_rows_items })
M.technologies_15_tooltip_rows_2_items = {
  [0] = data_item({ text = "2/5/8" }),
  data_item({ number = 24 }),
  data_item({ text = "based on level" }),
}
M.technologies_15_tooltip_rows_2 = data_row({ count = 3, items = M.technologies_15_tooltip_rows_2_items })
M.technologies_15_tooltip_rows_items = {
  [0] = data_item({ text = "labs now generate" }),
  data_item({ row = M.technologies_15_tooltip_rows_2 }),
}
M.technologies_15_tooltip_rows = data_row({ count = 2, items = M.technologies_15_tooltip_rows_items })
M.technologies_16_tooltip_rows_1_items = { [0] = data_item({ text = "build synthesizers" }) }
M.technologies_16_tooltip_rows_1 = data_row({ count = 1, items = M.technologies_16_tooltip_rows_1_items })
M.technologies_16_tooltip_rows_2_items = {
  [0] = data_item({ text = "that create" }),
  data_item({ number = 28 }),
  data_item({ number = 20 }),
  data_item({ number = 21 }),
  data_item({ number = 22 }),
}
M.technologies_16_tooltip_rows_2 = data_row({ count = 5, items = M.technologies_16_tooltip_rows_2_items })
M.technologies_16_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_16_tooltip_rows_1 }),
  data_item({ row = M.technologies_16_tooltip_rows_2 }),
}
M.technologies_16_tooltip_rows = data_row({ count = 2, items = M.technologies_16_tooltip_rows_items })
M.technologies_17_tooltip_rows_1_items = {
  [0] = data_item({ text = "colonies yield more" }),
  data_item({ number = 19 }),
}
M.technologies_17_tooltip_rows_1 = data_row({ count = 2, items = M.technologies_17_tooltip_rows_1_items })
M.technologies_17_tooltip_rows_2_items = {
  [0] = data_item({ text = "you can uplift" }),
  data_item({ number = 20 }),
  data_item({ number = 13 }),
  data_item({ number = 19 }),
}
M.technologies_17_tooltip_rows_2 = data_row({ count = 4, items = M.technologies_17_tooltip_rows_2_items })
M.technologies_17_tooltip_rows_3_items = {
  [0] = data_item({ text = "using" }),
  data_item({ number = 118 }),
  data_item({ text = "forgeworlds" }),
}
M.technologies_17_tooltip_rows_3 = data_row({ count = 3, items = M.technologies_17_tooltip_rows_3_items })
M.technologies_17_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_17_tooltip_rows_1 }),
  data_item({ row = M.technologies_17_tooltip_rows_2 }),
  data_item({ row = M.technologies_17_tooltip_rows_3 }),
}
M.technologies_17_tooltip_rows = data_row({ count = 3, items = M.technologies_17_tooltip_rows_items })
M.technologies_18_tooltip_rows_1_items = {
  [0] = data_item({ text = "adds 3 months to each" }),
  data_item({ number = 15 }),
  data_item({ text = "year" }),
}
M.technologies_18_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_18_tooltip_rows_1_items })
M.technologies_18_tooltip_rows_items = { [0] = data_item({ row = M.technologies_18_tooltip_rows_1 }) }
M.technologies_18_tooltip_rows = data_row({ count = 1, items = M.technologies_18_tooltip_rows_items })
M.technologies_19_tooltip_rows_1_items = {
  [0] = data_item({ text = "build" }),
  data_item({ number = 127 }),
  data_item({ text = "ascension gates" }),
}
M.technologies_19_tooltip_rows_1 = data_row({ count = 3, items = M.technologies_19_tooltip_rows_1_items })
M.technologies_19_tooltip_rows_2_items = {
  [0] = data_item({ text = "to turn" }),
  data_item({ number = 19 }),
  data_item({ text = "into" }),
  data_item({ number = 25 }),
  data_item({ text = "happiness" }),
}
M.technologies_19_tooltip_rows_2 = data_row({ count = 5, items = M.technologies_19_tooltip_rows_2_items })
M.technologies_19_tooltip_rows_items = {
  [0] = data_item({ row = M.technologies_19_tooltip_rows_1 }),
  data_item({ row = M.technologies_19_tooltip_rows_2 }),
}
M.technologies_19_tooltip_rows = data_row({ count = 2, items = M.technologies_19_tooltip_rows_items })

M.technologies_table = {
  { name = "space folding", era = 1, icon_key = "t", icon = 22, cost = 4, tooltip_rows = M.technologies_1_tooltip_rows },
  { name = "geneseeds", era = 1, icon_key = "l", icon = 20, cost = 5, tooltip_rows = M.technologies_2_tooltip_rows },
  { name = "nanomaterials", era = 1, icon_key = "o", icon = 21, cost = 6, tooltip_rows = M.technologies_3_tooltip_rows },
  { name = "drillbots", era = 1, icon_key = "b", icon = 18, cost = 7, tooltip_rows = M.technologies_4_tooltip_rows },
  { name = "xenofoods", era = 2, icon_key = "l", icon = 20, cost = 10, tooltip_rows = M.technologies_5_tooltip_rows },
  { name = "slipgates", era = 2, icon_key = "t", icon = 22, cost = 12, tooltip_rows = M.technologies_6_tooltip_rows },
  { name = "geoharvesting", era = 2, icon_key = "o", icon = 21, cost = 14, tooltip_rows = M.technologies_7_tooltip_rows },
  { name = "trade league", era = 2, icon_key = "t", icon = 22, cost = 16, tooltip_rows = M.technologies_8_tooltip_rows },
  { name = "replication", era = 2, icon_key = "b", icon = 18, cost = 18, tooltip_rows = M.technologies_9_tooltip_rows },
  { name = "autoassemblers", era = 3, icon_key = "b", icon = 18, cost = 19, tooltip_rows = M.technologies_10_tooltip_rows },
  { name = "skill implants", era = 3, icon_key = "b", icon = 18, cost = 22, tooltip_rows = M.technologies_11_tooltip_rows },
  { name = "starbirth", era = 3, icon_key = "o", icon = 21, cost = 25, tooltip_rows = M.technologies_12_tooltip_rows },
  { name = "biome hacking", era = 3, icon_key = "l", icon = 20, cost = 28, tooltip_rows = M.technologies_13_tooltip_rows },
  { name = "infraspace", era = 3, icon_key = "t", icon = 22, cost = 31, tooltip_rows = M.technologies_14_tooltip_rows },
  { name = "superhuman ai", era = 4, icon_key = "t", icon = 22, cost = 34, tooltip_rows = M.technologies_15_tooltip_rows },
  { name = "void synthesis", era = 4, icon_key = "o", icon = 21, cost = 38, tooltip_rows = M.technologies_16_tooltip_rows },
  { name = "gene rewriting", era = 4, icon_key = "l", icon = 20, cost = 42, tooltip_rows = M.technologies_17_tooltip_rows },
  { name = "time compression", era = 4, icon_key = "b", icon = 18, cost = 46, tooltip_rows = M.technologies_18_tooltip_rows },
  { name = "ascension", era = 4, icon_key = "a", icon = 33, cost = 50, tooltip_rows = M.technologies_19_tooltip_rows },
}

function M.resource_code_lookup(key, out_resource)
  assert(key ~= nil, "resource_code_lookup: key must not be NULL")
  for i = 1, M.RESOURCE_CODE_COUNT do
    if M.resource_codes[i].key == key then
      out_resource = M.resource_codes[i].resource
      return true, out_resource
    end
  end
  return false, out_resource
end

function M.resource_names_lookup(key)
  assert(key ~= nil, "resource_names_lookup: key must not be NULL")
  for i = 1, M.RESOURCE_NAMES_COUNT do
    if M.resource_names_table[i].key == key then
      return M.resource_names_table[i].name
    end
  end
  return nil
end

function M.production_tables_lookup(key)
  assert(key ~= nil, "production_tables_lookup: key must not be NULL")
  for i = 1, M.PRODUCTION_TABLES_COUNT do
    if M.production_tables_table[i].key == key then
      return M.production_tables_table[i]
    end
  end
  return nil
end

function M.map_tables_technologies_lookup(px)
  if px == nil then
    return nil
  end
  for i = 1, M.TECHNOLOGIES_COUNT do
    if M.technologies_table[i].name == px then
      return M.technologies_table[i]
    end
  end
  return nil
end

return M
