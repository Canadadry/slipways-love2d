-- Copy of src/game/planet_trade.h then src/game/planet_trade.c.
--
-- Shapes, with the C field names:
--   trade_offer_t: { route, amount, accept, accepted_routes }, accept a function (offer, route),
--     accepted_routes a trade_route_list_t.
-- A planet's own `has` (PLANET_HAS_* bits) is `has_`: the header's `has` holds the ENTITY_HAS_*
-- bits on the same table.
--
-- Include cycle: game.planet and game.slipway are required at call time, inside the functions
-- that need them, so that this module loads before they exist.
local bit = require("bit")
local collect = require("engine.collect")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local pointer_list = require("engine.pointer_list")
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
    resource = 0,
    has_resource = 0,
    resource_is_bool = 0,
  }
end

-- A trade_route_list_t assigned by value: a new header sharing the items, as the C copy shares
-- its buffer.
local function trade_route_list_value(list)
  local values = list.values
  return {
    values = {
      items = values.items,
      count = values.count,
      capacity = values.capacity,
    },
  }
end

-- A pointer_list_t assigned by value into dst, the inline list of its owner.
local function pointer_list_assign(dst, src)
  dst.items = src.items
  dst.count = src.count
  dst.capacity = src.capacity
end

local function contains_trade(list, v)
  if list == nil then
    return 0
  end
  for i = 0, trade.trade_route_list_count(list) - 1 do
    if trade.trade_equal(trade.trade_route_list_at(list, i), v) ~= 0 then
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

    kept = trade.trade_copy(nil, a)
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

  entity_id_list.entity_id_list_add(self.slipways, entity_id.entity_id_link(slipway))

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
  local bonus_resource = { kind = trade.RESOURCE_NIL, number = 0 }
  local any_resource = { kind = trade.RESOURCE_NIL, number = 0 }
  local has_bonus_resource
  local has_any_resource
  local route
  local t_resource
  local needs = nil
  local iter
  local entry

  if self == nil or trade_offer == nil then
    return
  end
  route = trade_offer.route
  t_resource = trade.trade_resource(route)

  has_bonus_resource, resource = map_tables.resource_code_lookup("e", resource)
  if has_bonus_resource ~= 0 then
    bonus_resource = { kind = trade.RESOURCE_FIXED, number = fixed.p8_fixed_from_int(resource) }
  end
  has_any_resource, resource = map_tables.resource_code_lookup("?", resource)
  if has_any_resource ~= 0 then
    any_resource = { kind = trade.RESOURCE_FIXED, number = fixed.p8_fixed_from_int(resource) }
  end

  assert(bit.band(self.has_, planet.PLANET_HAS_NEEDS) ~= 0, "planet_offer_trade: an offer goes to a planet holding needs")
  needs = self.needs

  if has_bonus_resource ~= 0 and trade.resource_equal(t_resource, bonus_resource) ~= 0 and self.has_production_bonus == 0 then
    iter = pointer_list.pointer_list_all(needs)
    while true do
      local more
      more, entry = pointer_list.pointer_list_iter_next(iter, entry)
      if more == 0 then
        break
      end
      if entry ~= nil then
        local i = entry
        local gc = (i.has_satisfied ~= 0) and i.satisfied or fixed.p8_fixed_from_int(0)
        local resource_is_any

        resource_is_any = has_any_resource ~= 0 and trade.resource_equal(research.need_resource(i), any_resource) ~= 0
        if gc == fixed.p8_fixed_from_int(0) and not resource_is_any then
          pointer_list.pointer_list_del(needs, entry)
          break
        end
      end
    end

    do
      local fresh = research.need_new(nil, bonus_resource)
      local prefix = {}

      pointer_list.pointer_list_init(prefix)
      pointer_list.pointer_list_add(prefix, fresh)
      pointer_list_assign(self.needs, collect.collect_concat_pointer_list(prefix, needs))
    end
    self.production_bonus = fixed.p8_fixed_from_int(1)
    self.has_production_bonus = 1
  end

  iter = pointer_list.pointer_list_all(needs)
  while true do
    local more
    local i
    local resource_val

    more, entry = pointer_list.pointer_list_iter_next(iter, entry)
    if more == 0 then
      break
    end
    if entry == nil then
      goto continue
    end
    i = entry
    resource_val = research.need_resource(i)

    if has_any_resource ~= 0 and trade.resource_equal(resource_val, any_resource) ~= 0 then
      local accepted = 0

      if bit.band(self.class.lineage, entity_class.LINEAGE(entity_class.LINEAGE_STRUCTURE)) ~= 0
        and self.archetype.accepted_resources ~= nil then
        accepted = (t_resource.kind == trade.RESOURCE_FIXED
          and game_data.int_seq_has_fixed(self.archetype.accepted_resources, t_resource.number) ~= 0) and 1 or 0
      end
      if accepted ~= 0 then
        assert(t_resource.kind == trade.RESOURCE_FIXED, "a need's resource is a resource code, never a boolean")
        i.row.has_resource = 1
        i.row.resource = t_resource.number
        resource_val = t_resource
      end
    end

    if trade.resource_equal(t_resource, resource_val) ~= 0
      and not (has_any_resource ~= 0 and trade.resource_equal(resource_val, any_resource) ~= 0) then
      local extended = trade.trade_extend(nil, route, self, nil)
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
  local iter
  local w

  if self == nil then
    return nil
  end

  if bit.band(self.has_, planet.PLANET_HAS_OUTGOING_ROUTES) == 0 then
    trade.trade_route_list_init(self.outgoing_routes, nil)
    self.has_ = bit.bor(self.has_, planet.PLANET_HAS_OUTGOING_ROUTES)
  end

  do
    local kw = planet.planet_production_read(self)
    if kw.has_resource ~= 0 then
      keys.has_resource = 1
      keys.resource = kw.resource
    end
    amount = kw.amount
  end

  iter = entity_id_list.entity_id_list_all(self.slipways)
  while true do
    local more
    local lw = {}
    local other

    more, w = entity_id_list.entity_id_list_iter_next(iter, w)
    if more == 0 then
      break
    end

    -- New list tables: the route of the previous offer shares the old ones, as its C copy kept
    -- its own list headers.
    keys.planets = {}
    keys.slipways = {}
    entity_id_list.entity_id_list_init(keys.planets)
    entity_id_list.entity_id_list_add(keys.planets, entity_id.entity_id_of(self))
    entity_id_list.entity_id_list_init(keys.slipways)
    entity_id_list.entity_id_list_add(keys.slipways, entity_id.entity_id_of(w))

    lw.route = trade.trade_new(keys)
    -- Recomputed each iteration: an earlier offer's accept may have grown the outgoing routes.
    lw.amount = fixed.p8_fixed_sub(amount, fixed.p8_fixed_from_int(trade.trade_route_list_count(self.outgoing_routes)))
    lw.accept = planet_rebuild_trades_attach_slipway_closure

    if lw.amount == fixed.p8_fixed_from_int(0) then
      break
    end

    assert(w.class == slipway.slipway_class(), "a planet's slipways list holds slipways only: planet_attach_slipway adds to it")
    other = slipway.slipway_other_end(w, self)
    -- The offer is on the stack: a callee that keeps a pointer to it dangles.
    if other ~= nil and other.class.offer_trade ~= nil then
      lw.accepted_routes = trade_route_list_value(self.outgoing_routes)
      other.class.offer_trade(other, lw)
      self.outgoing_routes = trade_route_list_value(lw.accepted_routes)
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
