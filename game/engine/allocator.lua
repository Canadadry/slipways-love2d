-- Copy of src/engine/allocator.h then src/engine/allocator.c.
-- An allocator_t is { alloc, grow, release, bump }. A flip_zone_allocator_t,
-- whose first member is the handle, is one table holding the fields of both.
local zone_module = require("engine.zone")

local M = {}

-- Lua only: the memcpy of a whole block of unknown type, every key copied.
local function copy_block(q, p)
  for key, value in pairs(p) do
    q[key] = value
  end
end

-- Lua only: the assignment of a handle by value into the struct it heads.
local function store_handle(storage, z)
  storage.alloc = z.alloc
  storage.grow = z.grow
  storage.release = z.release
  storage.bump = z.bump
end

local function bump_alloc(z, n)
  return zone_module.bump_zone_alloc(z.bump, n)
end

local function bump_grow(z, p, old_n, new_n)
  local q = zone_module.bump_zone_alloc(z.bump, new_n)
  if old_n > 0 then
    copy_block(q, p)
  end
  return q
end

local function release_nothing(z, p)
end

function M.allocator_from_bump_zone(bump)
  assert(bump ~= nil, "handle on a missing zone")
  return { alloc = bump_alloc, grow = bump_grow, release = release_nothing, bump = bump }
end

local function flip_zone_of(z)
  return z.zone
end

local function flip_zone_handle_alloc(z, n)
  return zone_module.flip_zone_alloc(flip_zone_of(z), n)
end

local function flip_zone_handle_grow(z, p, old_n, new_n)
  local q = zone_module.flip_zone_alloc(flip_zone_of(z), new_n)
  if old_n > 0 then
    copy_block(q, p)
  end
  return q
end

function M.allocator_from_flip_zone(storage, zone)
  assert(storage ~= nil and zone ~= nil, "handle on a missing zone")
  local z = { alloc = flip_zone_handle_alloc, grow = flip_zone_handle_grow, release = release_nothing,
    bump = nil }
  store_handle(storage, z)
  storage.zone = zone
  return storage
end

-- A missing allocator allocates a new table, grows into a new table holding a copy, releases nothing.
function M.allocator_alloc(z, n)
  if z == nil then
    return {}
  end
  return z.alloc(z, n)
end

function M.allocator_grow(z, p, old_n, new_n)
  assert(old_n <= new_n and (p ~= nil or old_n == 0))
  if z == nil then
    local q = {}
    if old_n > 0 then
      copy_block(q, p)
    end
    return q
  end
  return z.grow(z, p, old_n, new_n)
end

function M.allocator_release(z, p)
  if z == nil then
    return
  end
  z.release(z, p)
end

return M
