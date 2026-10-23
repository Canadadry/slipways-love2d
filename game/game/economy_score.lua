-- Include cycle: game.planet and game.probe are required at call time, inside the functions that
-- need them, so that this module loads before they exist. game.ui_button is required at load, one
-- direction only: g_difficulty is built by ui_button.button_zero(), and none of ui_button's
-- load-time requires reaches economy_score.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_registry = require("engine.entity_registry")
local economy_formulas = require("game.economy_formulas")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local trade = require("game.trade")
local ui_button = require("game.ui_button")
local fixed = require("p8.fixed")
local str = require("p8.str")

local M = {}

-- A new table per call.
local function final_score_zero()
  return {
    development = 0,
    population = 0,
    technology = 0,
    happiness = 0,
    total = 0,
    tier = 0,
  }
end

local function fixed_min(a, b)
  return (a < b) and a or b
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

local function is_colonized_truthy(pt)
  local planet = require("game.planet")
  if pt == nil then
    return false
  end
  assert(planet.planet_is_family(pt), "is_colonized_truthy: the entity is a planet")
  return planet.planet_is_colonized(pt)
end

function M.economy_score_planet_upkeep(pt)
  if pt ~= nil and bit.band(pt.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)) ~= 0
      and pt.archetype.is_structure then
    local structure_list = entity_registry.entities_by_tag(entity_class.TAG_STRUCTURE)
    return fixed.from_int(structure_list ~= nil and #structure_list or 0)
  end
  if is_colonized_truthy(pt) then
    local jo = pt
    local found
    local es
    if jo.development_tier ~= nil then
      found, es = game_data.int_seq_at(game_data.upkeep_by_tier_table, game_data.UPKEEP_BY_TIER_COUNT,
        fixed.flr_int(jo.development_tier), es)
      if found then
        return fixed.from_int(es)
      end
    end
  end
  return fixed.from_int(0)
end

local function colony_upkeep_predicate(e, ctx)
  if e == nil then
    return false
  end
  return is_colonized_truthy(e)
end

function M.economy_score_colony_upkeep()
  local planet_list = entity_registry.entities_by_tag(entity_class.TAG_PLANET)
  local mu = 0
  local result
  if planet_list ~= nil then
    for i = 1, #planet_list do
      if colony_upkeep_predicate(planet_list[i], nil) then
        mu = mu + 1
      end
    end
  end
  result = fixed.mul(fixed.LIT(0x00002e14, 0.18), fixed.from_int(mu * mu))
  return fixed.flr(result)
end

function M.economy_score_trade_income(entity)
  local planet_module = require("game.planet")
  local total = fixed.from_int(0)
  local planet
  local outgoing
  local difficulty = M.economy_score_difficulty()

  if entity == nil then
    return total
  end
  assert(planet_module.planet_is_family(entity), "economy_score_trade_income: the entity is a planet")
  planet = entity
  outgoing = planet.outgoing_routes

  if outgoing ~= nil then
    for i = 1, #outgoing do
      local found
      local partner
      found, partner = trade.trade_last_planet(outgoing[i], partner)
      if found then
        local oz
        assert(planet_module.planet_is_family(partner), "economy_score_trade_income: a trade leads to a planet")
        oz = partner
        if oz.archetype.is_planet then
          local trade_mult = fixed.from_int(0)
          local hs = planet.archetype.trade_multiplier
          local planet_bonus = fixed.from_int(0)
          local partner_bonus = fixed.from_int(0)
          local bonus
          local _

          if difficulty ~= nil and difficulty.trade ~= nil then
            trade_mult = difficulty.trade
          end
          if planet.development_tier ~= nil then
            _, planet_bonus = game_data.fixed_seq_at(game_data.partner_bonus_by_tier_table,
              game_data.PARTNER_BONUS_BY_TIER_COUNT, fixed.flr_int(planet.development_tier), planet_bonus)
          end
          if oz.development_tier ~= nil then
            _, partner_bonus = game_data.fixed_seq_at(game_data.partner_bonus_by_tier_table,
              game_data.PARTNER_BONUS_BY_TIER_COUNT, fixed.flr_int(oz.development_tier), partner_bonus)
          end
          bonus = fixed.add(fixed.from_int(1), fixed.add(planet_bonus, partner_bonus))
          total = fixed.add(total, fixed.mul(fixed.mul(trade_mult, hs), bonus))
        end
      end
    end
  end

  do
    local kw = planet_module.planet_production_read(planet)
    local found
    local w
    if kw.resource ~= nil then
      found, w = map_tables.resource_code_lookup("w", w)
      if found and kw.resource == fixed.from_int(w) then
        local mu = kw.amount
        local trade_mult = fixed.from_int(0)
        if difficulty ~= nil and difficulty.trade ~= nil then
          trade_mult = difficulty.trade
        end
        total = fixed.add(
          total, fixed.mul(fixed.mul(mu, trade_mult), fixed.LIT(0x00018000, 1.5)))
      end
    end
  end

  return total
end

local function sum_by_development_tier_visit(pt, ctx)
  local jo
  local found
  local v
  if not is_colonized_truthy(pt) then
    return
  end
  jo = pt
  if jo.development_tier ~= nil then
    found, v = game_data.int_seq_at(ctx.seq, ctx.count, fixed.flr_int(jo.development_tier), v)
    if found then
      ctx.total = fixed.add(ctx.total, fixed.from_int(v))
    end
  end
end

function M.economy_score_percent_text(value)
  return str.tostr_fixed(value) .. "%"
end

function M.economy_score_final()
  local development_ctx = {}
  local population = fixed.from_int(0)
  local base
  local technology
  local total
  local fm = require("game.probe").main_economy_typed()
  local happiness = (fm ~= nil and fm.happiness ~= nil) and fm.happiness or fixed.from_int(0)
  local invented_count = fm ~= nil and #fm.invented or 0
  local main_economy_production = (fm ~= nil) and fm.production or nil
  local result = final_score_zero()

  development_ctx.total = fixed.from_int(0)
  development_ctx.seq = game_data.development_score_by_tier_table
  development_ctx.count = game_data.DEVELOPMENT_SCORE_BY_TIER_COUNT
  for_each_tagged(entity_class.TAG_PLANET, sum_by_development_tier_visit, development_ctx)

  if main_economy_production ~= nil then
    local found
    local p
    local population_val
    found, p = map_tables.resource_code_lookup("p", p)
    if found then
      found, population_val = economy_formulas.resource_map_get(main_economy_production, fixed.from_int(p),
        population_val)
      if found then
        population = fixed.mul(population_val, fixed.from_int(40))
      end
    end
  end

  base = fixed.mul(fixed.from_int(invented_count), fixed.LIT(0x00013333, 1.2))
  technology = fixed.mul(fixed.flr(fixed.mul(base, base)), fixed.from_int(10))

  total = fixed.flr(fixed.mul(
    fixed.mul(fixed.add(fixed.add(development_ctx.total, population), technology),
      fixed.LIT(0x0000028f, 0.01)),
    happiness))

  result.development = development_ctx.total
  result.population = population
  result.technology = technology
  result.happiness = happiness
  result.total = total
  result.tier = fixed_min(fixed.flr(fixed.div(total, fixed.from_int(2000))),
    fixed.from_int(5))
  return result
end

function M.economy_score_happiness()
  local fm = require("game.probe").main_economy_typed()
  local main_economy_production = (fm ~= nil) and fm.production or nil
  local r_h = fixed.from_int(0)
  local r_w = fixed.from_int(0)
  local h_val
  local ctx = {}

  if main_economy_production ~= nil then
    local found
    local h_key
    local w_key
    local _
    found, h_key = map_tables.resource_code_lookup("h", h_key)
    if found then
      _, r_h = economy_formulas.resource_map_get(main_economy_production, fixed.from_int(h_key), r_h)
    end
    found, w_key = map_tables.resource_code_lookup("w", w_key)
    if found then
      _, r_w = economy_formulas.resource_map_get(main_economy_production, fixed.from_int(w_key), r_w)
    end
  end
  h_val =
    fixed.add(fixed.from_int(100), fixed.mul(fixed.from_int(2),
      fixed.add(r_h, r_w)))

  ctx.total = fixed.from_int(0)
  ctx.seq = game_data.happiness_by_tier_table
  ctx.count = game_data.HAPPINESS_BY_TIER_COUNT
  for_each_tagged(entity_class.TAG_PLANET, sum_by_development_tier_visit, ctx)

  return fixed.add(h_val, ctx.total)
end

-- The cart reads the difficulty all game long, after the menu button dies: keep a copy, never a link to the button.
local g_difficulty = nil

function M.economy_score_difficulty()
  return g_difficulty
end

-- Written in place, every field, so that a holder of the table economy_score_difficulty returned
-- sees the new values.
function M.economy_score_set_difficulty(dk)
  local copy = ui_button.button_zero()

  if dk == nil then
    g_difficulty = nil
    return
  end
  copy.name = dk.name
  copy.trade = dk.trade
  copy.type_bias = dk.type_bias
  if g_difficulty == nil then
    g_difficulty = {}
  end
  for key in pairs(g_difficulty) do
    g_difficulty[key] = nil
  end
  for key, value in pairs(copy) do
    g_difficulty[key] = value
  end
end

function M.economy_score_net_income(p)
  local planet = require("game.planet")
  local kr = fixed.flr(fixed.sub(M.economy_score_trade_income(p), M.economy_score_planet_upkeep(p)))
  local jo

  assert(planet.planet_is_family(p), "economy_score_net_income: the planet tag holds planets only")
  jo = p
  jo.net_income = kr
  return kr
end

return M
