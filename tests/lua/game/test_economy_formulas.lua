local check = require("check")
local economy_formulas = require("game.economy_formulas")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")

local expect = check.expect
local resource = fixed.p8_fixed_from_int

local function new_map()
  return { count = 0, entries = {} }
end

local function test_set_appends_a_new_resource_then_updates_it_in_place()
  local map = new_map()
  economy_formulas.resource_map_set(map, resource(28), 3)
  economy_formulas.resource_map_set(map, resource(21), 5)
  economy_formulas.resource_map_set(map, resource(28), 7)
  expect(map.count, 2, "count after two keys")
  local found, value = economy_formulas.resource_map_get(map, resource(28), -1)
  expect(found, 1, "food found")
  expect(value, 7, "food value after update")
end

local function test_get_leaves_out_unchanged_for_a_missing_key_or_a_nil_map()
  local map = new_map()
  economy_formulas.resource_map_set(map, resource(28), 3)
  local found, value = economy_formulas.resource_map_get(map, resource(21), 42)
  expect(found, 0, "ore found")
  expect(value, 42, "out for a missing key")
  found, value = economy_formulas.resource_map_get(nil, resource(28), 42)
  expect(found, 0, "found in a nil map")
  expect(value, 42, "out for a nil map")
end

local function test_fill_gives_every_resource_code_the_same_value()
  local map = new_map()
  economy_formulas.resource_map_set(map, resource(99), 1)
  economy_formulas.economy_formulas_fill_resource_map(map, 9)
  expect(map.count, map_tables.RESOURCE_CODE_COUNT, "count after fill")
  for i = 0, map_tables.RESOURCE_CODE_COUNT - 1 do
    local code = map_tables.resource_codes[i].resource
    local found, value = economy_formulas.resource_map_get(map, resource(code), -1)
    expect(found, 1, "resource " .. code .. " found")
    expect(value, 9, "resource " .. code .. " value")
  end
end

local function test_format_signed_prefixes_a_plus_and_keeps_the_color_from_zero_up()
  local text, color = economy_formulas.economy_formulas_format_signed(resource(12), 10, "$", 3)
  expect(text, "+12$", "positive text")
  expect(color, 10, "positive color")
  text, color = economy_formulas.economy_formulas_format_signed(resource(0), 0, nil, 3)
  expect(text, "+0", "zero text")
  expect(color, 0, "zero color")
end

local function test_format_signed_gives_a_negative_value_color_8_and_no_plus()
  local text, color = economy_formulas.economy_formulas_format_signed(fixed.p8_fixed_neg(resource(5)), 10, "$", 3)
  expect(text, "-5$", "negative text")
  expect(color, 8, "negative color")
end

check.run({
  { "set appends a new resource then updates it in place", test_set_appends_a_new_resource_then_updates_it_in_place },
  { "get leaves out unchanged for a missing key or a nil map", test_get_leaves_out_unchanged_for_a_missing_key_or_a_nil_map },
  { "fill gives every resource code the same value", test_fill_gives_every_resource_code_the_same_value },
  { "format signed prefixes a plus and keeps the color from zero up", test_format_signed_prefixes_a_plus_and_keeps_the_color_from_zero_up },
  { "format signed gives a negative value color 8 and no plus", test_format_signed_gives_a_negative_value_color_8_and_no_plus },
})
