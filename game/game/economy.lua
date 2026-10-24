-- Include cycle: game.economy_score, game.economy_tiers, game.message_box and game.planet are
-- required at call time, inside the functions that need them, so that this module loads before
-- they exist.
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local tech_gate = require("game.tech_gate")
local fixed = require("p8.fixed")

local M = {}

local g_economy_up = false

M.class_def = {
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

M.archetype = {
  credits = 0x640000,
  era = 0x10000,
  year_ticks = 0x10000,
  year = 0xd490000,
}

-- production, demand and happiness stay nil until the first economy.recompute.
local function economy_create()
  local self = {}

  entity_class.init_header(self, M.class_def)
  self.archetype = M.archetype
  self.credits = fixed.from_int(0)
  self.income = fixed.from_int(0)
  self.research_points = fixed.from_int(0)
  self.era = fixed.from_int(0)
  self.year_ticks = fixed.from_int(0)
  self.year = fixed.from_int(0)
  self.invented = {}
  self.save_pending = false

  self.credits = self.archetype.credits
  self.income = fixed.from_int(0)
  self.research_points = fixed.from_int(0)
  self.era = self.archetype.era
  self.year_ticks = self.archetype.year_ticks
  self.year = self.archetype.year

  -- A slot, never entity_registry.register: in the live list the economy would join the update order.
  entity_id.take(self)
  return self
end

local function assert_economy(self)
  assert(self == nil or self.class == M.class_def,
    "assert_economy: every measured call passes the main economy; anything else would be read through the wrong layout")
end

function M.init()
  M.shutdown()
  g_economy_up = true
end

function M.shutdown()
  g_economy_up = false
end

function M.ready()
  return g_economy_up
end

function M.class()
  return M.class_def
end

function M.new()
  return economy_create()
end

local function economy_refresh_after_figures(self)
  local refresh_list
  local qc

  if self.year >= fixed.from_int(3426) then
    self.end_status = "final score"
  elseif self.income <= fixed.from_int(0) and self.credits < fixed.from_int(3) then
    self.end_status = "bankrupt"
  else
    self.end_status = nil
  end

  refresh_list = entity_registry.entities_by_message(entity_registry.MESSAGE_REFRESH)
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
  self.save_pending = true

  kx = tech_gate.if_unlocked("time compression", fixed.from_int(12), fixed.from_int(15))

  lo = fixed.add(self.year_ticks, hx)
  self.year_ticks = lo

  bring_up_to_date(self)

  if lo > kx then
    self.year_ticks = fixed.sub(lo, kx)
    M.new_year(self)
  end
end

function M.advance_clock(self, hx)
  economy_add_ticks(self, hx, M.recompute)
end

-- economy.advance_clock without its recompute's figures, for a caller that
-- changed only the credits since the last one: the end status and the refresh
-- listeners are brought up to date. A new year still recomputes in full.
function M.tick_clock(self, hx)
  economy_add_ticks(self, hx, economy_refresh_after_figures)
end

function M.new_year(self)
  local credits
  local income
  local research_points
  local new_year
  local science_production = fixed.from_int(0)
  local new_year_list
  local qc

  assert_economy(self)
  if self == nil then
    return
  end
  self.save_pending = true

  credits = self.credits
  income = self.income
  research_points = self.research_points

  if self.production ~= nil then
    local found
    local k = 0
    found, k = map_tables.resource_code_lookup("k", k)
    if found then
      local _
      _, science_production = economy_formulas.resource_map_get(self.production, fixed.from_int(k),
        science_production)
    end
  end

  self.credits = fixed.add(credits, income)
  self.research_points = fixed.add(fixed.add(research_points, science_production),
    fixed.from_int(1))
  new_year = fixed.add(self.year, fixed.from_int(1))
  self.year = new_year

  M.recompute(self)

  new_year_list = entity_registry.entities_by_message(entity_registry.MESSAGE_NEW_YEAR)
  for i = 1, #new_year_list do
    qc = new_year_list[i]
    if qc.class.new_year ~= nil then
      qc.class.new_year(qc, new_year)
    end
  end
end

function M.spend(self, jt)
  local message_box = require("game.message_box")
  local nz
  local keys = message_box.toast_keys_zero()

  assert_economy(self)
  nz = self ~= nil and self.credits or fixed.from_int(0)

  if nz < jt then
    message_box.toast_keys_set_text(keys, "-no money-")
    keys.color = fixed.from_int(8)
    message_box.toast_new(keys)
    return false
  end

  if self ~= nil then
    self.credits = fixed.sub(nz, jt)
    self.save_pending = true
  end

  if jt ~= fixed.from_int(0) then
    local color = 0
    local text
    text, color = economy_formulas.format_signed(fixed.neg(jt), 10, "$", color)
    message_box.toast_keys_set_text(keys, text)
    keys.color = fixed.from_int(color)
    message_box.toast_new(keys)
  end

  return true
end

function M.recompute(self)
  local economy_score = require("game.economy_score")
  local economy_tiers = require("game.economy_tiers")
  local trade_total = fixed.from_int(0)
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
    economy_tiers.rebuild_incoming_routes()
  until not economy_tiers.update_tiers()

  -- Filled in place once created: a reader may hold either map across recomputes.
  if self.production == nil then
    self.production = { count = 0, entries = {} }
  end
  if self.demand == nil then
    self.demand = { count = 0, entries = {} }
  end
  M.total_production(self.production)
  M.crowding_penalty(self.demand)

  planet_list = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  if planet_list ~= nil then
    for i = 1, #planet_list do
      local kr = economy_score.net_income(planet_list[i])

      trade_total = fixed.add(trade_total, kr)
    end
  end

  skill_multiplier = tech_gate.if_unlocked("skill implants", fixed.from_int(1),
    fixed.LIT(0x00012666, 1.15))
  income = fixed.sub(fixed.flr(fixed.mul(trade_total, skill_multiplier)),
    economy_score.colony_upkeep())
  self.income = income

  happiness = economy_score.happiness()
  self.happiness = happiness

  economy_refresh_after_figures(self)
end

function M.invent(self, t)
  local jk
  local ew
  local count = 0

  assert_economy(self)
  if self == nil then
    return
  end
  self.save_pending = true

  jk = self.research_points
  self.research_points = fixed.sub(jk, tech_gate.research_cost(t))

  if t ~= nil and t.name ~= nil then
    self.invented[#self.invented + 1] = t.name
  end
  count = #self.invented

  ew = self.era
  if ew < fixed.from_int(4) then
    local found
    local threshold = 0
    found, threshold = game_data.int_seq_at(game_data.tier_thresholds_table, game_data.TIER_THRESHOLDS_COUNT,
      fixed.flr_int(ew), threshold)
    if found and fixed.from_int(count) >= fixed.from_int(threshold) then
      self.era = fixed.add(ew, fixed.from_int(1))
    end
  end

  M.recompute(self)
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

  assert(planet.is_family(pt), "total_production_visit: the planet tag holds planets only")
  jo = pt
  if jo.needs == nil then
    return
  end
  kw = planet.production_read(jo)
  if kw.resource == nil then
    return
  end
  found, current = economy_formulas.resource_map_get(p, kw.resource, current)
  if not found then
    current = fixed.from_int(0)
  end
  economy_formulas.resource_map_set(p, kw.resource, fixed.add(current, kw.amount))
end

function M.total_production(out)
  economy_formulas.fill_resource_map(out, fixed.from_int(0))
  for_each_tagged(entity_class.TAG_PLANET, total_production_visit, out)
end

-- A planet with absent or empty needs is skipped where Lua would crash, like the malformed cases of entity_registry.register. Making one site fatal alone breaks consistency.
local function crowding_penalty_visit(l, ctx)
  local planet = require("game.planet")
  local d = ctx
  local jy
  local last
  local last_val
  local found
  local current

  assert(planet.is_family(l), "crowding_penalty_visit: the structure tag holds planets only")
  if l.needs == nil then
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
  if last.row.resource == nil then
    return
  end
  -- Fallback 1, not 0: the map was seeded with 1 for every resource code.
  found, current = economy_formulas.resource_map_get(d, last.row.resource, current)
  if not found then
    current = fixed.from_int(1)
  end
  economy_formulas.resource_map_set(d, last.row.resource,
    fixed.sub(current, fixed.LIT(0x00002666, 0.15)))
end

function M.crowding_penalty(out)
  economy_formulas.fill_resource_map(out, fixed.from_int(1))
  for_each_tagged(entity_class.TAG_STRUCTURE, crowding_penalty_visit, out)
end

return M
