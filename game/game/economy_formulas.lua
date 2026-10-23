-- Copy of src/game/economy_formulas.h then src/game/economy_formulas.c.
--
-- Shapes, with the C field names:
--   resource_map_t: { count, entries }, entries 0-indexed of { resource, value }; filled in
--     place by its init and set functions; the caller creates entries. A value copy of a map
--     copies entries and each entry, since set changes an entry in place.
local entity_class = require("engine.entity_class")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

local function fixed_max(a, b)
  return (a > b) and a or b
end

-- Returns found, out: out unchanged when the key is missing or map is nil.
function M.resource_map_get(map, qe, out)
  local key = fixed.p8_fixed_flr_int(qe)
  local i = 0
  while map ~= nil and i < map.count do
    if map.entries[i].resource == key then
      out = map.entries[i].value
      return true, out
    end
    i = i + 1
  end
  return false, out
end

function M.resource_map_set(map, qe, v)
  local key = fixed.p8_fixed_flr_int(qe)
  for i = 0, map.count - 1 do
    if map.entries[i].resource == key then
      map.entries[i].value = v
      return
    end
  end
  map.entries[map.count] = { resource = key, value = v }
  map.count = map.count + 1
end

function M.economy_formulas_fill_resource_map(out, jm)
  out.count = 0
  for i = 1, map_tables.RESOURCE_CODE_COUNT do
    M.resource_map_set(out, fixed.p8_fixed_from_int(map_tables.resource_codes[i].resource), jm)
  end
end

function M.economy_formulas_route_base_cost(from_pos, to_pos)
  local diff = vec.vec_sub(to_pos, from_pos)
  local length_squared = vec.vec_length_squared(diff)
  local x = fixed.p8_fixed_div(length_squared, game_data.game_data_route_length_factor())
  local ec = fixed.p8_fixed_pow075(x)
  local clamped = fixed_max(fixed.P8_FIXED_LIT(0x00008000, 0.5), ec)
  local scale = fixed.p8_fixed_mul(game_data.game_data_route_cost_factor(), M.economy_formulas_cost_scale())
  local cost = fixed.p8_fixed_flr(fixed.p8_fixed_mul(scale, clamped))
  return cost
end

function M.economy_formulas_cost_scale()
  local jc = entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY)
  local count = (jc ~= nil) and #jc or 0
  local sq = fixed.p8_fixed_sqrt(fixed.p8_fixed_from_int(count))
  local val = fixed.p8_fixed_sub(fixed.p8_fixed_mul(sq, fixed.P8_FIXED_LIT(0x0000451e, 0.27)),
    fixed.P8_FIXED_LIT(0x00001999, 0.1))
  return fixed_max(fixed.p8_fixed_from_int(1), val)
end

-- Returns text, color: out_color is ignored, the color is c from zero up and 8 below.
function M.economy_formulas_format_signed(d, c, ku, out_color)
  local num_str = str.p8_tostr_fixed(d)
  local suffix = (ku ~= nil) and ku or ""
  local result

  if d >= 0 then
    out_color = c
    result = "+" .. num_str
  else
    out_color = 8
    result = num_str
  end
  return result .. suffix, out_color
end

return M
