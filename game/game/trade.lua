-- Copy of src/game/trade.h then src/game/trade.c.
--
-- Shapes, with the C field names:
--   trade_route_t, trade_keys_t: { planets, slipways, resource, resource_is_bool }, planets
--     and slipways plain 1-based arrays of ids, resource nil when absent.
--   resource_t: { kind, number }, a new table at every creation.
--   trade_route_list_t: a plain 1-based array of trade_route_t.
-- The id lists are immutable once built: a value copy of a route is a new table with the four
-- fields whose two id arrays are shared.
local entity_id = require("engine.entity_id")

local M = {}

M.RESOURCE_NIL = 0
M.RESOURCE_FALSE = 1
M.RESOURCE_FIXED = 2

local g_trade_up = false

local function route_value(planets, slipways, resource, resource_is_bool)
  return {
    planets = planets,
    slipways = slipways,
    resource = resource,
    resource_is_bool = resource_is_bool,
  }
end

local function copy_route(route)
  return route_value(route.planets, route.slipways, route.resource, route.resource_is_bool)
end

function M.trade_resource(self)
  if self == nil or self.resource == nil then
    return { kind = M.RESOURCE_NIL, number = 0 }
  end
  assert(not (self.resource_is_bool and self.resource ~= 0),
    "no route of src/ holds true: resource_t has no kind for it")
  if self.resource_is_bool then
    return { kind = M.RESOURCE_FALSE, number = 0 }
  end
  return { kind = M.RESOURCE_FIXED, number = self.resource }
end

function M.resource_equal(a, b)
  return a.kind == b.kind and (a.kind ~= M.RESOURCE_FIXED or a.number == b.number)
end

function M.trade_init()
  M.trade_shutdown()
  g_trade_up = true
end

function M.trade_shutdown()
  g_trade_up = false
end

function M.trade_ready()
  return g_trade_up
end

-- nil keys give a route with empty lists and no resource; the route shares the keys' lists.
function M.trade_new(keys)
  if keys == nil then
    return route_value({}, {}, nil, false)
  end
  assert(keys.resource ~= nil or not keys.resource_is_bool,
    "trade_new: an absent resource carries no kind")
  assert(not keys.resource_is_bool or keys.resource == 0 or keys.resource == 1,
    "trade_new: a boolean key holds 0 or 1; trade_resource would read any other as a fixed")
  return route_value(keys.planets, keys.slipways, keys.resource, keys.resource_is_bool)
end

-- The ids of list, then extra unless it is nil, in a new list; an empty list when there is no id.
local function id_list_extended(list, extra)
  local out = {}
  for i = 1, #list do
    out[i] = list[i]
  end
  if extra ~= nil then
    local id = entity_id.entity_id_of(extra)
    assert(id ~= nil,
      "trade_extend: a route holds constructed planets and slipways only")
    out[#out + 1] = id
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
  if self == nil or #self.planets == 0 then
    return false, out
  end
  out = entity_id.entity_id_resolve(self.planets[#self.planets])
  return true, out
end

-- The new route's id lists are new lists; a nil node or slipway adds nothing.
function M.trade_extend(self, node, slipway)
  assert(self ~= nil, "trade_extend: a route value is always there to extend")
  local extended = route_value({}, {}, nil, false)
  extended.resource_is_bool = self.resource == nil or self.resource_is_bool
  extended.resource = self.resource or 0
  extended.planets = id_list_extended(self.planets, node)
  extended.slipways = id_list_extended(self.slipways, slipway)
  return extended
end

function M.trade_equal(l, r)
  if l == nil or r == nil or #l.planets == 0 or #r.planets == 0 then
    return false
  end
  if entity_id.entity_id_resolve(l.planets[1]) ~= entity_id.entity_id_resolve(r.planets[1]) then
    return false
  end
  local l_found, l_last = M.trade_last_planet(l, nil)
  if not l_found then
    return false
  end
  local r_found, r_last = M.trade_last_planet(r, nil)
  if not r_found then
    return false
  end
  return l_last == r_last
end

-- Stores a copy of the route; its id lists are shared, not copied.
function M.trade_route_list_add(list, route)
  assert(route ~= nil, "a trade route list holds routes")
  list[#list + 1] = copy_route(route)
end

return M
