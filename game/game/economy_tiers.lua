-- Copy of src/game/economy_tiers.h then src/game/economy_tiers.c.
--
-- Include cycle: game.planet, game.planet_trade and game.slipway are required at call time,
-- inside the functions that need them, so that this module loads before they exist.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local research = require("game.research")
local trade = require("game.trade")
local fixed = require("p8.fixed")

local M = {}

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

local function rebuild_incoming_routes_reset_visit(pt, ctx)
  local planet = require("game.planet")
  local jo
  if pt == nil then
    return
  end
  assert(planet.planet_is_family(pt) ~= 0, "rebuild_incoming_routes_reset_visit: the planet tag holds planets only")
  jo = pt
  if jo.incoming_routes == nil then
    jo.incoming_routes = {}
    return
  end
  for i = #jo.incoming_routes, 1, -1 do
    jo.incoming_routes[i] = nil
  end
end

local function rebuild_incoming_routes_route_visit(entity, context)
  local planet_module = require("game.planet")
  local planet_trade = require("game.planet_trade")
  local slipway = require("game.slipway")
  local from_planet
  local outgoing
  if entity == nil then
    return
  end

  assert(planet_module.planet_is_family(entity) ~= 0,
    "rebuild_incoming_routes_route_visit: the planet tag holds planets only")
  from_planet = entity
  outgoing = planet_trade.planet_rebuild_trades(from_planet)

  for route_index = 1, #outgoing do
    local route = outgoing[route_index]
    local slipways = route.slipways
    local planets = route.planets
    local to_val
    local found

    for index = 1, #slipways do
      local w_val = entity_id.entity_id_resolve(slipways[index])

      if index + 1 <= #planets then
        local planet_val = entity_id.entity_id_resolve(planets[index + 1])
        assert(planet_module.planet_is_family(planet_val) ~= 0,
          "rebuild_incoming_routes_route_visit: a trade's planets list holds planets only")
        local planet = planet_val
        if planet.planet_id ~= nil then
          assert(w_val.class == slipway.slipway_class(),
            "rebuild_incoming_routes_route_visit: a trade's slipways list holds slipways only")
          local w = w_val
          slipway.slipway_served_planets_add(w.served_planets, planet.planet_id)
        end
      end
    end

    found, to_val = trade.trade_last_planet(route, to_val)
    if found ~= 0 then
      local dest

      assert(planet_module.planet_is_family(to_val) ~= 0, "rebuild_incoming_routes_route_visit: a trade leads to a planet")
      dest = to_val
      if dest.incoming_routes ~= nil then
        trade.trade_route_list_add(dest.incoming_routes, route)
      end
    end
  end
end

function M.economy_tiers_rebuild_incoming_routes()
  for_each_tagged(entity_class.TAG_PLANET, rebuild_incoming_routes_reset_visit, nil)
  for_each_tagged(entity_class.TAG_PLANET, rebuild_incoming_routes_route_visit, nil)
end

local function update_tiers_visit(entity, context)
  local planet_module = require("game.planet")
  local changed = context
  local planet
  local tier
  local development_tier = fixed.p8_fixed_from_int(0)
  local production

  if entity == nil then
    return
  end
  tier = M.economy_tiers_compute_tier(entity)
  assert(planet_module.planet_is_family(entity) ~= 0, "update_tiers_visit: the planet tag holds planets only")
  planet = entity
  if planet.development_tier ~= nil then
    development_tier = planet.development_tier
  end

  if planet.development_tier == nil or tier > development_tier then
    if planet.development_tier ~= nil and development_tier == fixed.p8_fixed_from_int(1) then
      local hn = research.research_project_get(planet.project)
      if hn ~= nil and hn.byproduct ~= nil then
        if planet.needs ~= nil then
          local entry = research.need_new(
            { kind = trade.RESOURCE_FIXED, number = hn.byproduct })
          planet.needs[#planet.needs + 1] = entry
        end
      end
    end
    planet.development_tier = tier
    development_tier = tier
    changed.value = 1
  end

  production = planet_module.planet_production(planet)
  local _
  _, production.amount = M.economy_tiers_compute_production_amount(planet, production.amount)
end

-- Returns 0, leaving *out untouched, when the tier or the project gives no amount.
-- Returns found, out.
function M.economy_tiers_compute_production_amount(planet, out)
  local project
  local production_table
  local idx
  local clamped_idx
  local base = fixed.p8_fixed_from_int(0)
  local bonus = fixed.p8_fixed_from_int(0)

  assert(planet ~= nil and out ~= nil)
  if planet.development_tier == nil or planet.development_tier == fixed.p8_fixed_from_int(0) then
    return 0, out
  end
  project = research.research_project_get(planet.project)
  production_table = (project ~= nil) and research.research_project_production(project) or nil
  if production_table == nil then
    return 0, out
  end
  idx = fixed.p8_fixed_flr_int(planet.development_tier)
  clamped_idx = (idx < production_table.count) and idx or production_table.count
  if clamped_idx >= 1 then
    base = fixed.p8_fixed_from_int(production_table.values[clamped_idx - 1])
  end
  if planet.production_bonus ~= nil then
    bonus = planet.production_bonus
  end
  out = fixed.p8_fixed_add(base, bonus)
  return 1, out
end

function M.economy_tiers_update_tiers()
  local jg = { value = 0 }
  for_each_tagged(entity_class.TAG_PLANET, update_tiers_visit, jg)
  return jg.value
end

function M.economy_tiers_compute_tier(entity)
  local planet_module = require("game.planet")
  local planet
  local needs
  local incoming_routes
  local tier = fixed.p8_fixed_from_int(1)

  if entity == nil then
    return fixed.p8_fixed_from_int(0)
  end
  assert(planet_module.planet_is_family(entity) ~= 0, "economy_tiers_compute_tier: the entity is a planet")
  planet = entity
  if planet.state ~= entity_class.ENTITY_STATE_SETTLED then
    return fixed.p8_fixed_from_int(0)
  end

  needs = planet.needs
  incoming_routes = planet.incoming_routes
  if needs ~= nil then
    for index = 1, #needs do
      local i_val = needs[index]

      if i_val ~= nil then
        local want = i_val
        local resource_val = research.need_resource(want)
        local gc = 0

        if incoming_routes ~= nil then
          for j = 1, #incoming_routes do
            local im = incoming_routes[j]
            if im.resource ~= nil and trade.resource_equal(trade.trade_resource(im), resource_val) ~= 0 then
              gc = gc + 1
            end
          end
        end
        want.satisfied = fixed.p8_fixed_from_int(gc)
        tier = fixed.p8_fixed_mul(tier, fixed.p8_fixed_from_int(gc))
      end
    end
  end

  if tier == fixed.p8_fixed_from_int(0) then
    return fixed.p8_fixed_from_int(1)
  end

  if bit.band(entity.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_TRADING_HUB)) ~= 0 then
    local count = 0
    if needs ~= nil then
      for index = 1, #needs do
        local want = needs[index]
        if want ~= nil then
          if want.satisfied ~= nil and want.satisfied > fixed.p8_fixed_from_int(0) then
            count = count + 1
          end
        end
      end
    end
    return fixed_min(fixed.p8_fixed_from_int(count), fixed.p8_fixed_from_int(3))
  end

  if not planet.archetype.is_planet then
    if needs ~= nil and #needs > 0 then
      local last = needs[#needs]
      if last ~= nil then
        if last.satisfied ~= nil then
          return fixed_min(fixed.p8_fixed_add(last.satisfied, fixed.p8_fixed_from_int(1)), fixed.p8_fixed_from_int(4))
        end
      end
    end
    return fixed.p8_fixed_from_int(2)
  end

  do
    local by = planet.outgoing_routes
    local by_count = (by ~= nil) and #by or 0
    local incoming_routes_count = (incoming_routes ~= nil) and #incoming_routes or 0
    local mid_val = fixed.p8_fixed_mid(fixed.p8_fixed_from_int(by_count), fixed.p8_fixed_from_int(incoming_routes_count),
      fixed.p8_fixed_from_int(1))
    return fixed_min(fixed.p8_fixed_from_int(4), fixed.p8_fixed_add(fixed.p8_fixed_from_int(1), mid_val))
  end
end

return M
