-- A resource map is { count, entries }, entries of { resource, value }, filled in place by
-- resource_map_set: a copy of a map copies each entry too, since set changes an entry in place.
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
  local key = fixed.flr_int(qe)
  local i = 1
  while map ~= nil and i <= map.count do
    if map.entries[i].resource == key then
      out = map.entries[i].value
      return true, out
    end
    i = i + 1
  end
  return false, out
end

function M.resource_map_set(map, qe, v)
  local key = fixed.flr_int(qe)
  for i = 1, map.count do
    if map.entries[i].resource == key then
      map.entries[i].value = v
      return
    end
  end
  local n = map.count + 1
  map.entries[n] = { resource = key, value = v }
  map.count = n
end

function M.economy_formulas_fill_resource_map(out, jm)
  out.count = 0
  for i = 1, map_tables.RESOURCE_CODE_COUNT do
    M.resource_map_set(out, fixed.from_int(map_tables.resource_codes[i].resource), jm)
  end
end

function M.economy_formulas_route_base_cost(from_pos, to_pos)
  local diff = vec.sub(to_pos, from_pos)
  local length_squared = vec.length_squared(diff)
  local x = fixed.div(length_squared, game_data.game_data_route_length_factor())
  local ec = fixed.pow075(x)
  local clamped = fixed_max(fixed.LIT(0x00008000, 0.5), ec)
  local scale = fixed.mul(game_data.game_data_route_cost_factor(), M.economy_formulas_cost_scale())
  local cost = fixed.flr(fixed.mul(scale, clamped))
  return cost
end

function M.economy_formulas_cost_scale()
  local jc = entity_registry.entities_by_tag(entity_class.TAG_SLIPWAY)
  local count = (jc ~= nil) and #jc or 0
  local sq = fixed.sqrt(fixed.from_int(count))
  local val = fixed.sub(fixed.mul(sq, fixed.LIT(0x0000451e, 0.27)),
    fixed.LIT(0x00001999, 0.1))
  return fixed_max(fixed.from_int(1), val)
end

-- Returns text, color: out_color is ignored, the color is c from zero up and 8 below.
function M.economy_formulas_format_signed(d, c, ku, out_color)
  local num_str = str.tostr_fixed(d)
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
