-- Copy of src/game/tech_gate.h then src/game/tech_gate.c.
--
-- Shapes, with the C field names:
--   available_technologies_kept_t: { items, count }, items 0-indexed, each the very
--     technologies_table entry.
-- The main economy is read through game.probe at call time: these functions cannot run before
-- game.probe exists.
local economy_formulas = require("game.economy_formulas")
local main_economy = require("game.main_economy")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")
local bit = require("bit")

local M = {}

local TECHNOLOGIES_COUNT = map_tables.TECHNOLOGIES_COUNT
local technologies_table = map_tables.technologies_table

-- Lua only: t - technologies_table, the C index of each entry.
local technology_index_of = {}
for i = 0, TECHNOLOGIES_COUNT - 1 do
  technology_index_of[technologies_table[i]] = i
end

local g_affordable_flags = {}
for i = 0, TECHNOLOGIES_COUNT - 1 do
  g_affordable_flags[i] = 0
end

function M.tech_gate_is_unlocked(px)
  local fm = require("game.probe").main_economy_typed()
  local invented_list = nil
  if px == nil then
    return 1
  end
  if fm ~= nil then
    invented_list = fm.invented
  end
  if invented_list == nil then
    return 0
  end
  for i = 1, #invented_list do
    if invented_list[i] == px then
      return 1
    end
  end
  return 0
end

function M.tech_gate_if_unlocked(px, lk, mc)
  if M.tech_gate_is_unlocked(px) ~= 0 then
    return mc
  end
  return lk
end

local function research_cost_with_discount(od, has_planet_type, nk)
  local fm = require("game.probe").main_economy_typed()
  local fu = fixed.p8_fixed_from_int(0)
  local threshold = fixed.P8_FIXED_LIT(0x00008ccc, 0.55)
  local factor
  if has_planet_type ~= 0 and fm ~= nil and bit.band(fm.has_, main_economy.ECONOMY_HAS_DEMAND) ~= 0 then
    local _
    _, fu = economy_formulas.resource_map_get(fm.demand, nk, fu)
  end
  factor = (fu > threshold) and fu or threshold
  return fixed.p8_fixed_flr(fixed.p8_fixed_mul(od, factor))
end

function M.tech_gate_research_cost(t)
  if t == nil then
    return research_cost_with_discount(fixed.p8_fixed_from_int(0), 0, 0)
  end
  return research_cost_with_discount(fixed.p8_fixed_from_int(t.cost), 1, fixed.p8_fixed_from_int(t.icon))
end

local function available_technologies_predicate(t, bi)
  local probe = require("game.probe")
  local fm = probe.main_economy_typed()
  local provisional = probe.main_economy_placeholder()
  local jk = fixed.p8_fixed_from_int(0)
  local ew = fixed.p8_fixed_from_int(0)
  local affordable
  if fm ~= nil then
    jk = fm.research_points
  elseif provisional ~= nil and provisional.has_research_points ~= 0 then
    jk = provisional.research_points
  end
  affordable = (M.tech_gate_research_cost(t) <= jk) and 1 or 0
  -- Written for every entry, before any early return: tech_gate_affordable_flags() reads it back.
  g_affordable_flags[technology_index_of[t]] = affordable
  if fm ~= nil then
    ew = fm.era
  end
  if fixed.p8_fixed_from_int(t.era) ~= ew then
    return 0
  end
  if M.tech_gate_is_unlocked(t.name) ~= 0 then
    return 0
  end
  return (affordable ~= 0 or bi ~= 0) and 1 or 0
end

function M.tech_gate_available_technologies(bi)
  local result = { items = {}, count = 0 }
  for i = 0, TECHNOLOGIES_COUNT - 1 do
    if available_technologies_predicate(technologies_table[i], bi) ~= 0 then
      assert(result.count < TECHNOLOGIES_COUNT,
        "tech_gate_available_technologies: it keeps technologies_table entries only, so their count bounds the result")
      result.items[result.count] = technologies_table[i]
      result.count = result.count + 1
    end
  end
  return result
end

function M.tech_gate_affordable_flags(t)
  local index = technology_index_of[t]
  assert(index ~= nil and index >= 0 and index < TECHNOLOGIES_COUNT,
    "tech_gate_affordable_flags: the flag is kept for technologies_table entries only")
  return g_affordable_flags[index]
end

function M.tech_gate_reset_affordable_flags()
  for i = 0, TECHNOLOGIES_COUNT - 1 do
    g_affordable_flags[i] = 0
  end
end

return M
