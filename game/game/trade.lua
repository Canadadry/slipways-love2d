-- Copy of src/game/trade.h then src/game/trade.c.
--
-- Shapes, with the C field names:
--   trade_route_t, trade_keys_t: { planets, slipways, resource, has_resource,
--     resource_is_bool }, planets and slipways entity_id_list_t tables.
--   resource_t: { kind, number }, a new table at every creation.
--   trade_route_list_t: { values }, a value_list_t of trade_route_t.
-- The id lists are immutable once built: a value copy of a route is a new table with the five
-- fields whose two list tables are shared, as the C copies share their items.
local entity_id = require("engine.entity_id")
local entity_id_list = require("engine.entity_id_list")
local value_list = require("engine.value_list")

local M = {}

M.RESOURCE_NIL = 0
M.RESOURCE_FALSE = 1
M.RESOURCE_FIXED = 2

local g_trade_up = 0

local function route_value(planets, slipways, resource, has_resource, resource_is_bool)
  return {
    planets = planets,
    slipways = slipways,
    resource = resource,
    has_resource = has_resource,
    resource_is_bool = resource_is_bool,
  }
end

local function empty_id_list()
  return { items = nil, count = 0, capacity = 0 }
end

local function copy_route(route)
  return route_value(route.planets, route.slipways, route.resource, route.has_resource, route.resource_is_bool)
end

function M.trade_resource(self)
  if self == nil or self.has_resource == 0 then
    return { kind = M.RESOURCE_NIL, number = 0 }
  end
  assert(not (self.resource_is_bool ~= 0 and self.resource ~= 0),
    "no route of src/ holds true: resource_t has no kind for it")
  if self.resource_is_bool ~= 0 then
    return { kind = M.RESOURCE_FALSE, number = 0 }
  end
  return { kind = M.RESOURCE_FIXED, number = self.resource }
end

function M.resource_equal(a, b)
  return (a.kind == b.kind and (a.kind ~= M.RESOURCE_FIXED or a.number == b.number)) and 1 or 0
end

function M.trade_init()
  M.trade_shutdown()
  g_trade_up = 1
end

function M.trade_shutdown()
  g_trade_up = 0
end

function M.trade_ready()
  return g_trade_up
end

-- nil keys give a route with empty lists and no resource; the route shares the keys' lists.
function M.trade_new(keys)
  if keys == nil then
    return route_value(empty_id_list(), empty_id_list(), 0, 0, 0)
  end
  assert(keys.resource_is_bool == 0 or keys.resource == 0 or keys.resource == 1,
    "trade_new: a boolean key holds 0 or 1; trade_resource would read any other as a fixed")
  assert(keys.has_resource ~= 0 or (keys.resource_is_bool == 0 and keys.resource == 0),
    "trade_new: an absent key carries neither a kind nor a value; the member reads 0 when absent")
  return route_value(keys.planets, keys.slipways, keys.resource, keys.has_resource, keys.resource_is_bool)
end

-- The ids of list, then extra unless it is nil, in a new list; an empty list when there is no id.
local function id_list_extended(list, extra)
  local count = list.count + ((extra ~= nil) and 1 or 0)
  local out = empty_id_list()
  if count == 0 then
    return out
  end
  entity_id_list.entity_id_list_init_reserve(out, count)
  for i = 0, list.count - 1 do
    entity_id_list.entity_id_list_add(out, list.items[i])
  end
  if extra ~= nil then
    local id = entity_id.entity_id_of(extra)
    assert(id ~= nil,
      "trade_extend: a route holds constructed planets and slipways only")
    entity_id_list.entity_id_list_add(out, id)
  end
  return out
end

-- The copy's id lists are new lists.
function M.trade_copy(self)
  assert(self ~= nil, "trade_copy: a route value is always there to copy")
  local copy = copy_route(self)
  copy.planets = id_list_extended(self.planets, nil)
  copy.slipways = id_list_extended(self.slipways, nil)
  return copy
end

-- Returns found, out.
function M.trade_last_planet(self, out)
  if self == nil or self.planets.count == 0 then
    return 0, out
  end
  out = entity_id_list.entity_id_list_at(self.planets, self.planets.count - 1)
  return 1, out
end

-- The new route's id lists are new lists; a nil node or slipway adds nothing.
function M.trade_extend(self, node, slipway)
  assert(self ~= nil, "trade_extend: a route value is always there to extend")
  local extended = route_value(empty_id_list(), empty_id_list(), 0, 0, 0)
  extended.has_resource = 1
  extended.resource_is_bool = (self.has_resource ~= 0) and self.resource_is_bool or 1
  extended.resource = (self.has_resource ~= 0) and self.resource or 0
  extended.planets = id_list_extended(self.planets, node)
  extended.slipways = id_list_extended(self.slipways, slipway)
  return extended
end

function M.trade_equal(l, r)
  if l == nil or r == nil or l.planets.count == 0 or r.planets.count == 0 then
    return 0
  end
  if entity_id_list.entity_id_list_at(l.planets, 0) ~= entity_id_list.entity_id_list_at(r.planets, 0) then
    return 0
  end
  local l_found, l_last = M.trade_last_planet(l, nil)
  if l_found == 0 then
    return 0
  end
  local r_found, r_last = M.trade_last_planet(r, nil)
  if r_found == 0 then
    return 0
  end
  return (l_last == r_last) and 1 or 0
end

function M.trade_route_list_init(list)
  list.values = {}
  value_list.value_list_init(list.values)
end

function M.trade_route_list_free(list)
  value_list.value_list_free(list.values)
end

function M.trade_route_list_clear(list)
  value_list.value_list_clear(list.values)
end

-- Stores a copy of the route; its id lists are shared, not copied.
function M.trade_route_list_add(list, route)
  -- route may point into this list: copy it before the add grows the list.
  local copy = copy_route(route)
  value_list.value_list_add(list.values, copy)
end

function M.trade_route_list_count(list)
  return value_list.value_list_count(list.values)
end

-- Valid until the next add or free on this list: never store it.
function M.trade_route_list_at(list, index)
  return value_list.value_list_at(list.values, index)
end

return M
