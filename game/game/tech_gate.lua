-- The main economy is read through game.probe at call time: these functions cannot run before
-- game.probe exists.
local economy_formulas = require("game.economy_formulas")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")

local M = {}

local TECHNOLOGIES_COUNT = map_tables.TECHNOLOGIES_COUNT
local technologies_table = map_tables.technologies_table

local technology_index_of = {}
for i = 1, TECHNOLOGIES_COUNT do
  technology_index_of[technologies_table[i]] = i
end

local g_affordable_flags = {}
for i = 1, TECHNOLOGIES_COUNT do
  g_affordable_flags[i] = false
end

function M.tech_gate_is_unlocked(px)
  local fm = require("game.probe").main_economy_typed()
  local invented_list = nil
  if px == nil then
    return true
  end
  if fm ~= nil then
    invented_list = fm.invented
  end
  if invented_list == nil then
    return false
  end
  for i = 1, #invented_list do
    if invented_list[i] == px then
      return true
    end
  end
  return false
end

function M.tech_gate_if_unlocked(px, lk, mc)
  if M.tech_gate_is_unlocked(px) then
    return mc
  end
  return lk
end

local function research_cost_with_discount(od, nk)
  local fm = require("game.probe").main_economy_typed()
  local fu = fixed.from_int(0)
  local threshold = fixed.LIT(0x00008ccc, 0.55)
  local factor
  if nk ~= nil and fm ~= nil and fm.demand ~= nil then
    local _
    _, fu = economy_formulas.resource_map_get(fm.demand, nk, fu)
  end
  factor = (fu > threshold) and fu or threshold
  return fixed.flr(fixed.mul(od, factor))
end

function M.tech_gate_research_cost(t)
  if t == nil then
    return research_cost_with_discount(fixed.from_int(0), nil)
  end
  return research_cost_with_discount(fixed.from_int(t.cost), fixed.from_int(t.icon))
end

local function available_technologies_predicate(t, bi)
  local probe = require("game.probe")
  local fm = probe.main_economy_typed()
  local provisional = probe.main_economy_placeholder()
  local jk = fixed.from_int(0)
  local ew = fixed.from_int(0)
  local affordable
  if fm ~= nil then
    jk = fm.research_points
  elseif provisional ~= nil and provisional.research_points ~= nil then
    jk = provisional.research_points
  end
  affordable = M.tech_gate_research_cost(t) <= jk
  -- Written for every entry, before any early return: tech_gate_affordable_flags() reads it back.
  g_affordable_flags[technology_index_of[t]] = affordable
  if fm ~= nil then
    ew = fm.era
  end
  if fixed.from_int(t.era) ~= ew then
    return false
  end
  if M.tech_gate_is_unlocked(t.name) then
    return false
  end
  return affordable or bi
end

function M.tech_gate_available_technologies(bi)
  assert(type(bi) == "boolean", "tech_gate_available_technologies: bi is a boolean, and 0 is truthy")
  local result = { items = {}, count = 0 }
  for i = 1, TECHNOLOGIES_COUNT do
    if available_technologies_predicate(technologies_table[i], bi) then
      local n = result.count + 1
      result.items[n] = technologies_table[i]
      result.count = n
    end
  end
  return result
end

function M.tech_gate_affordable_flags(t)
  local index = technology_index_of[t]
  assert(index ~= nil and index >= 1 and index <= TECHNOLOGIES_COUNT,
    "tech_gate_affordable_flags: the flag is kept for technologies_table entries only")
  return g_affordable_flags[index]
end

function M.tech_gate_reset_affordable_flags()
  for i = 1, TECHNOLOGIES_COUNT do
    g_affordable_flags[i] = false
  end
end

return M
