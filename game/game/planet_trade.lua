-- Copy of src/game/planet_trade.h then src/game/planet_trade.c.
--
-- Shapes, with the C field names:
--   trade_offer_t: { route, amount, accept, accepted_routes }, accept a function (offer, route),
--     accepted_routes a trade_route_list_t.
--
-- Include cycle: game.planet and game.slipway are required at call time, inside the functions
-- that need them, so that this module loads before they exist.
local bit = require("bit")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local game_data = require("game.game_data")
local map_tables = require("game.map_tables")
local research = require("game.research")
local trade = require("game.trade")
local fixed = require("p8.fixed")

local M = {}

-- trade_keys_t keys = {0}.
local function trade_keys_zero()
  return {
    planets = { items = nil, count = 0, capacity = 0 },
    slipways = { items = nil, count = 0, capacity = 0 },
    resource_is_bool = 0,
  }
end

local function route_list_copy(list)
  local copy = {}
  for i = 1, #list do
    copy[i] = list[i]
  end
  return copy
end

-- In place: whoever holds dst sees src's elements.
local function needs_refill(dst, src)
  for i = #dst, 1, -1 do
    dst[i] = nil
  end
  for i = 1, #src do
    dst[i] = src[i]
  end
end

local function contains_trade(list, v)
  if list == nil then
    return 0
  end
  for i = 1, #list do
    if trade.trade_equal(list[i], v) ~= 0 then
      return 1
    end
  end
  return 0
end

local function planet_rebuild_trades_attach_slipway_closure(self, a)
  if self == nil then
    return
  end
  if contains_trade(self.accepted_routes, a) == 0 then
    local kept

    kept = trade.trade_copy(a)
    trade.trade_route_list_add(self.accepted_routes, kept)
    self.amount = fixed.p8_fixed_sub(self.amount, fixed.p8_fixed_from_int(1))
  end
end

function M.planet_attach_slipway(self, slipway)
  local planet = require("game.planet")
  local other = nil

  if self == nil then
    return
  end

  assert(slipway ~= nil, "planet_attach_slipway: a planet's slipways list holds slipway ids")
  self.slipways[#self.slipways + 1] = entity_id.entity_id_link(slipway)

  if slipway ~= nil then
    local slipway_module = require("game.slipway")
    assert(slipway.class == slipway_module.slipway_class(), "planet_attach_slipway: the slipway is one slipway_drop registers")
    other = slipway_module.slipway_other_end(slipway, self)
  end
  if other ~= nil then
    assert(planet.planet_is_family(other) ~= 0, "planet_attach_slipway: the other end of a registered slipway is a planet")
    if other.fold_anchor ~= 0 then
      self.fold_anchor = fixed.p8_fixed_from_int(1)
    end
  end

  planet.planet_restart_shake(self)
end

function M.planet_offer_trade(self, trade_offer)
  local planet = require("game.planet")
  local resource = 0
  local bonus_resource = nil
  local any_resource = nil
  local found
  local route
  local t_resource
  local needs = nil

  if self == nil or trade_offer == nil then
    return
  end
  route = trade_offer.route
  t_resource = trade.trade_resource(route)

  found, resource = map_tables.resource_code_lookup("e", resource)
  if found ~= 0 then
    bonus_resource = { kind = trade.RESOURCE_FIXED, number = fixed.p8_fixed_from_int(resource) }
  end
  found, resource = map_tables.resource_code_lookup("?", resource)
  if found ~= 0 then
    any_resource = { kind = trade.RESOURCE_FIXED, number = fixed.p8_fixed_from_int(resource) }
  end

  assert(self.needs ~= nil, "planet_offer_trade: an offer goes to a planet holding needs")
  needs = self.needs

  if bonus_resource ~= nil and trade.resource_equal(t_resource, bonus_resource) ~= 0 and self.production_bonus == nil then
    for index = 1, #needs do
      local entry = needs[index]
      if entry ~= nil then
        local i = entry
        local gc = i.satisfied or fixed.p8_fixed_from_int(0)
        local resource_is_any

        resource_is_any = any_resource ~= nil and trade.resource_equal(research.need_resource(i), any_resource) ~= 0
        if gc == fixed.p8_fixed_from_int(0) and not resource_is_any then
          table.remove(needs, index)
          break
        end
      end
    end

    do
      local prefixed = { research.need_new(bonus_resource) }

      for index = 1, #needs do
        prefixed[#prefixed + 1] = needs[index]
      end
      needs_refill(self.needs, prefixed)
    end
    self.production_bonus = fixed.p8_fixed_from_int(1)
  end

  for index = 1, #needs do
    local entry = needs[index]
    local i
    local resource_val

    if entry == nil then
      goto continue
    end
    i = entry
    resource_val = research.need_resource(i)

    if any_resource ~= nil and trade.resource_equal(resource_val, any_resource) ~= 0 then
      local accepted = 0

      if bit.band(self.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)) ~= 0
        and self.archetype.accepted_resources ~= nil then
        accepted = (t_resource.kind == trade.RESOURCE_FIXED
          and game_data.int_seq_has_fixed(self.archetype.accepted_resources, t_resource.number) ~= 0) and 1 or 0
      end
      if accepted ~= 0 then
        assert(t_resource.kind == trade.RESOURCE_FIXED, "a need's resource is a resource code, never a boolean")
        i.row.resource = t_resource.number
        resource_val = t_resource
      end
    end

    if trade.resource_equal(t_resource, resource_val) ~= 0
      and not (any_resource ~= nil and trade.resource_equal(resource_val, any_resource) ~= 0) then
      local extended = trade.trade_extend(route, self, nil)
      if trade_offer.accept ~= nil then
        trade_offer.accept(trade_offer, extended)
      end
      return
    end
    ::continue::
  end
end

-- Updates self's outgoing routes in place and returns them, valid for the call only; nil for a nil planet.
function M.planet_rebuild_trades(self)
  local planet = require("game.planet")
  local slipway = require("game.slipway")
  local keys = trade_keys_zero()
  local amount = fixed.p8_fixed_from_int(0)

  if self == nil then
    return nil
  end

  if self.outgoing_routes == nil then
    self.outgoing_routes = {}
  end

  do
    local kw = planet.planet_production_read(self)
    keys.resource = kw.resource
    amount = kw.amount
  end

  local slipways = self.slipways
  for i = 1, #slipways do
    local w = entity_id.entity_id_resolve(slipways[i])
    local lw = {}
    local other

    -- New arrays: the route of the previous offer shares the old ones.
    local self_id = entity_id.entity_id_of(self)
    assert(self_id ~= nil, "planet_rebuild_trades: a route starts at a constructed planet")
    keys.planets = { self_id }
    keys.slipways = { entity_id.entity_id_of(w) }

    lw.route = trade.trade_new(keys)
    -- Recomputed each iteration: an earlier offer's accept may have grown the outgoing routes.
    lw.amount = fixed.p8_fixed_sub(amount, fixed.p8_fixed_from_int(#self.outgoing_routes))
    lw.accept = planet_rebuild_trades_attach_slipway_closure

    if lw.amount == fixed.p8_fixed_from_int(0) then
      break
    end

    assert(w.class == slipway.slipway_class(), "a planet's slipways list holds slipways only: planet_attach_slipway adds to it")
    other = slipway.slipway_other_end(w, self)
    -- The offer is on the stack: a callee that keeps a pointer to it dangles. The outgoing
    -- routes keep their old contents until the offer returns.
    if other ~= nil and other.class.offer_trade ~= nil then
      lw.accepted_routes = route_list_copy(self.outgoing_routes)
      other.class.offer_trade(other, lw)
      self.outgoing_routes = lw.accepted_routes
    end
  end

  return self.outgoing_routes
end

function M.planet_attach_slipway_method(self, slipway)
  M.planet_attach_slipway(self, slipway)
end

function M.planet_offer_trade_method(self, offer)
  M.planet_offer_trade(self, offer)
end

return M
