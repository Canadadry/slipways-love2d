-- Copy of src/game/economy.h then src/game/economy.c.
--
-- economy_t is the shape listed in game.main_economy. The economy's own `has` (ECONOMY_HAS_*
-- bits) is `has_`: the header's `has` holds the ENTITY_HAS_* bits on the same table.
-- end_status is a Lua string or nil.
--
-- Include cycle: game.economy_score, game.economy_tiers, game.message_box and game.planet are
-- required at call time, inside the functions that need them, so that this module loads before
-- they exist.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local economy_formulas = require("game.economy_formulas")
local main_economy = require("game.main_economy")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local tech_gate = require("game.tech_gate")
local fixed = require("p8.fixed")

local M = {}

local g_economy_up = 0

M.economy_class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

M.economy_archetype = {
  credits = 0x640000,
  era = 0x10000,
  year_ticks = 0x10000,
  year = 0xd490000,
}

local function economy_create()
  local self = {}

  entity_class.entity_init_header(self, M.economy_class_def)
  self.archetype = M.economy_archetype
  self.credits = fixed.p8_fixed_from_int(0)
  self.income = fixed.p8_fixed_from_int(0)
  self.research_points = fixed.p8_fixed_from_int(0)
  self.era = fixed.p8_fixed_from_int(0)
  self.year_ticks = fixed.p8_fixed_from_int(0)
  self.year = fixed.p8_fixed_from_int(0)
  -- Lua only: the inline sub-structs the C writes into in place.
  self.invented = {}
  self.production = { count = 0, entries = {} }
  self.demand = { count = 0, entries = {} }
  self.production.count = 0
  self.demand.count = 0
  self.has_ = 0
  self.happiness = fixed.p8_fixed_from_int(0)
  self.has_happiness = 0
  self.end_status = nil
  self.has_end_status = 0
  self.save_pending = 0

  self.credits = self.archetype.credits
  self.income = fixed.p8_fixed_from_int(0)
  self.research_points = fixed.p8_fixed_from_int(0)
  self.era = self.archetype.era
  self.year_ticks = self.archetype.year_ticks
  self.year = self.archetype.year

  -- A slot, never entity_register: in the live list the economy would join the update order.
  entity_id.entity_id_take(self)
  return self
end

local function assert_economy(self)
  assert(self == nil or self.class == M.economy_class_def,
    "assert_economy: every measured call passes the main economy; anything else would be read through the wrong layout")
end

function M.economy_init()
  M.economy_shutdown()
  g_economy_up = 1
end

function M.economy_shutdown()
  g_economy_up = 0
end

function M.economy_ready()
  return g_economy_up
end

function M.economy_class()
  return M.economy_class_def
end

function M.economy_new()
  return economy_create()
end

local function economy_refresh_after_figures(self)
  local refresh_list
  local qc

  if self.year >= fixed.p8_fixed_from_int(3426) then
    self.end_status = "final score"
  elseif self.income <= fixed.p8_fixed_from_int(0) and self.credits < fixed.p8_fixed_from_int(3) then
    self.end_status = "bankrupt"
  else
    self.end_status = nil
  end
  self.has_end_status = 1

  refresh_list = entity_registry.entities_by_message(entity_registry.ENTITY_MESSAGE_REFRESH)
  for i = 1, #refresh_list do
    qc = refresh_list[i]
    if qc.class.refresh ~= nil then
      qc.class.refresh(qc)
    end
  end
end

local function economy_add_ticks(self, hx, bring_up_to_date)
  local kx
  local lo

  assert_economy(self)
  if self == nil then
    return
  end
  self.save_pending = 1

  kx = tech_gate.tech_gate_if_unlocked("time compression", fixed.p8_fixed_from_int(12), fixed.p8_fixed_from_int(15))

  lo = fixed.p8_fixed_add(self.year_ticks, hx)
  self.year_ticks = lo

  bring_up_to_date(self)

  if lo > kx then
    self.year_ticks = fixed.p8_fixed_sub(lo, kx)
    M.economy_new_year(self)
  end
end

function M.economy_advance_clock(self, hx)
  economy_add_ticks(self, hx, M.economy_recompute)
end

-- economy_advance_clock without its recompute's figures, for a caller that
-- changed only the credits since the last one: the end status and the refresh
-- listeners are brought up to date. A new year still recomputes in full.
function M.economy_tick_clock(self, hx)
  economy_add_ticks(self, hx, economy_refresh_after_figures)
end

function M.economy_new_year(self)
  local credits
  local income
  local research_points
  local new_year
  local science_production = fixed.p8_fixed_from_int(0)
  local new_year_list
  local qc

  assert_economy(self)
  if self == nil then
    return
  end
  self.save_pending = 1

  credits = self.credits
  income = self.income
  research_points = self.research_points

  if bit.band(self.has_, main_economy.ECONOMY_HAS_PRODUCTION) ~= 0 then
    local found
    local k = 0
    found, k = map_tables.resource_code_lookup("k", k)
    if found ~= 0 then
      local _
      _, science_production = economy_formulas.resource_map_get(self.production, fixed.p8_fixed_from_int(k),
        science_production)
    end
  end

  self.credits = fixed.p8_fixed_add(credits, income)
  self.research_points = fixed.p8_fixed_add(fixed.p8_fixed_add(research_points, science_production),
    fixed.p8_fixed_from_int(1))
  new_year = fixed.p8_fixed_add(self.year, fixed.p8_fixed_from_int(1))
  self.year = new_year

  M.economy_recompute(self)

  new_year_list = entity_registry.entities_by_message(entity_registry.ENTITY_MESSAGE_NEW_YEAR)
  for i = 1, #new_year_list do
    qc = new_year_list[i]
    if qc.class.new_year ~= nil then
      qc.class.new_year(qc, new_year)
    end
  end
end

function M.economy_spend(self, jt)
  local message_box = require("game.message_box")
  local nz
  local keys = message_box.toast_keys_zero()

  assert_economy(self)
  nz = self ~= nil and self.credits or fixed.p8_fixed_from_int(0)

  if nz < jt then
    message_box.toast_keys_set_text(keys, "-no money-")
    keys.color = fixed.p8_fixed_from_int(8)
    message_box.toast_new(keys)
    return 0
  end

  if self ~= nil then
    self.credits = fixed.p8_fixed_sub(nz, jt)
    self.save_pending = 1
  end

  if jt ~= fixed.p8_fixed_from_int(0) then
    local color = 0
    local text
    text, color = economy_formulas.economy_formulas_format_signed(fixed.p8_fixed_neg(jt), 10, "$", color)
    message_box.toast_keys_set_text(keys, text)
    keys.color = fixed.p8_fixed_from_int(color)
    message_box.toast_new(keys)
  end

  return 1
end

function M.economy_recompute(self)
  local economy_score = require("game.economy_score")
  local economy_tiers = require("game.economy_tiers")
  local trade_total = fixed.p8_fixed_from_int(0)
  local income
  local happiness
  local skill_multiplier
  local planet_list

  assert_economy(self)
  if self == nil then
    return
  end

  -- Terminates only because update_tiers reports tier increases, capped at 4; a "changed" test spins forever.
  repeat
    economy_tiers.economy_tiers_rebuild_incoming_routes()
  until economy_tiers.economy_tiers_update_tiers() == 0

  M.economy_total_production(self.production)
  M.economy_crowding_penalty(self.demand)
  self.has_ = bit.bor(self.has_, main_economy.ECONOMY_HAS_PRODUCTION, main_economy.ECONOMY_HAS_DEMAND)

  planet_list = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  if planet_list ~= nil then
    for i = 1, #planet_list do
      local kr = economy_score.economy_score_net_income(planet_list[i])

      trade_total = fixed.p8_fixed_add(trade_total, kr)
    end
  end

  skill_multiplier = tech_gate.tech_gate_if_unlocked("skill implants", fixed.p8_fixed_from_int(1),
    fixed.P8_FIXED_LIT(0x00012666, 1.15))
  income = fixed.p8_fixed_sub(fixed.p8_fixed_flr(fixed.p8_fixed_mul(trade_total, skill_multiplier)),
    economy_score.economy_score_colony_upkeep())
  self.income = income

  happiness = economy_score.economy_score_happiness()
  self.happiness = happiness
  self.has_happiness = 1

  economy_refresh_after_figures(self)
end

function M.economy_invent(self, t)
  local jk
  local ew
  local count = 0

  assert_economy(self)
  if self == nil then
    return
  end
  self.save_pending = 1

  jk = self.research_points
  self.research_points = fixed.p8_fixed_sub(jk, tech_gate.tech_gate_research_cost(t))

  if t ~= nil and t.name ~= nil then
    self.invented[#self.invented + 1] = t.name
  end
  count = #self.invented

  ew = self.era
  if ew < fixed.p8_fixed_from_int(4) then
    local found
    local threshold = 0
    found, threshold = game_data.int_seq_at(game_data.tier_thresholds_table, game_data.TIER_THRESHOLDS_COUNT,
      fixed.p8_fixed_flr_int(ew), threshold)
    if found ~= 0 and fixed.p8_fixed_from_int(count) >= fixed.p8_fixed_from_int(threshold) then
      self.era = fixed.p8_fixed_add(ew, fixed.p8_fixed_from_int(1))
    end
  end

  M.economy_recompute(self)
end

local function for_each_tagged(tag, fn, ctx)
  local list = entity_registry.entities_by_tag(tag)
  if list == nil then
    return
  end
  for i = 1, #list do
    fn(list[i], ctx)
  end
end

local function total_production_visit(pt, ctx)
  local planet = require("game.planet")
  local p = ctx
  local jo
  local kw
  local found
  local current

  assert(planet.planet_is_family(pt) ~= 0, "total_production_visit: the planet tag holds planets only")
  jo = pt
  if bit.band(jo.has_, planet.PLANET_HAS_NEEDS) == 0 then
    return
  end
  kw = planet.planet_production_read(jo)
  if kw.has_resource == 0 then
    return
  end
  found, current = economy_formulas.resource_map_get(p, kw.resource, current)
  if found == 0 then
    current = fixed.p8_fixed_from_int(0)
  end
  economy_formulas.resource_map_set(p, kw.resource, fixed.p8_fixed_add(current, kw.amount))
end

function M.economy_total_production(out)
  economy_formulas.economy_formulas_fill_resource_map(out, fixed.p8_fixed_from_int(0))
  for_each_tagged(entity_class.TAG_PLANET, total_production_visit, out)
end

-- A planet with absent or empty needs is skipped where Lua would crash, like the malformed cases of entity_register. Making one site fatal alone breaks consistency.
local function crowding_penalty_visit(l, ctx)
  local planet = require("game.planet")
  local d = ctx
  local jy
  local last
  local last_val
  local found
  local current

  assert(planet.planet_is_family(l) ~= 0, "crowding_penalty_visit: the structure tag holds planets only")
  if bit.band(l.has_, planet.PLANET_HAS_NEEDS) == 0 then
    return
  end
  jy = l.needs
  if #jy == 0 then
    return
  end
  last_val = jy[#jy]
  if last_val == nil then
    return
  end
  last = last_val
  if last.row.has_resource == 0 then
    return
  end
  -- Fallback 1, not 0: the map was seeded with 1 for every resource code.
  found, current = economy_formulas.resource_map_get(d, last.row.resource, current)
  if found == 0 then
    current = fixed.p8_fixed_from_int(1)
  end
  economy_formulas.resource_map_set(d, last.row.resource,
    fixed.p8_fixed_sub(current, fixed.P8_FIXED_LIT(0x00002666, 0.15)))
end

function M.economy_crowding_penalty(out)
  economy_formulas.economy_formulas_fill_resource_map(out, fixed.p8_fixed_from_int(1))
  for_each_tagged(entity_class.TAG_STRUCTURE, crowding_penalty_visit, out)
end

return M
