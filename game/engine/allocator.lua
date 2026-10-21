-- Copy of src/engine/allocator.h then src/engine/allocator.c.
-- An allocator_t is { alloc, grow, release, bump, slot }. A struct whose first
-- member is the handle (flip_zone_allocator_t, arena_t, and the block an arena
-- lives in) is one table holding the fields of both: the cast is the
-- identity, and an arena's bump is both its inline zone and the handle's
-- pointer to it.
local cint = require("p8.cint")
local zone_module = require("engine.zone")

local M = {}

M.SIZEOF = {
  arena_t = 96,
  zone_cell_t = 8,
}
local SIZEOF = M.SIZEOF

M.ARENA_HEAD_BYTES = zone_module.ZONE_BYTES(SIZEOF.arena_t)

function M.ARENA_LIST_BYTES(list_head_bytes, capacity, element_bytes)
  return M.ARENA_HEAD_BYTES + zone_module.ZONE_BYTES(list_head_bytes) + zone_module.ZONE_BYTES(capacity * element_bytes)
end

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
  storage.slot = z.slot
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
  return { alloc = bump_alloc, grow = bump_grow, release = release_nothing, bump = bump, slot = nil }
end

local function slot_alloc(z, n)
  assert(n <= z.slot.slot_size, "slot zone: block larger than a slot")
  return zone_module.slot_zone_alloc(z.slot)
end

function M.allocator_grow_refused(z, p, old_n, new_n)
  assert(false, "slot zone: a slot does not grow")
  return nil
end

local function slot_release(z, p)
  if p ~= nil then
    zone_module.slot_zone_release(z.slot, p)
  end
end

function M.allocator_from_slot_zone(slot)
  assert(slot ~= nil, "handle on a missing zone")
  return { alloc = slot_alloc, grow = M.allocator_grow_refused, release = slot_release, bump = nil, slot = slot }
end

-- s is a 0-indexed array of slot_kind_zone_t { zone_name, slot_name, cells,
-- taken, size, count }; zones and handles are 0-indexed arrays the caller owns.
function M.allocator_init_slot_kinds(s, n, zones, handles)
  for i = 0, n - 1 do
    zone_module.slot_zone_init(zones[i], s[i].zone_name, s[i].cells, s[i].taken, s[i].size, s[i].count)
    handles[i] = M.allocator_from_slot_zone(zones[i])
  end
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
    bump = nil, slot = nil }
  store_handle(storage, z)
  storage.zone = zone
  return storage
end

function M.allocator_alloc(z, n)
  assert(z ~= nil, "alloc without an allocator")
  return z.alloc(z, n)
end

function M.allocator_grow(z, p, old_n, new_n)
  assert(z ~= nil, "grow without an allocator")
  assert(old_n <= new_n and (p ~= nil or old_n == 0))
  return z.grow(z, p, old_n, new_n)
end

function M.allocator_release(z, p)
  assert(z ~= nil, "release without an allocator")
  z.release(z, p)
end

local ARENA_HEAD_CELLS = cint.div(M.ARENA_HEAD_BYTES, SIZEOF.zone_cell_t)

-- An arena is the table the "is this an arena" asserts recognise by its from.
function M.arena_take(from, name, size)
  local cells = cint.div(size, SIZEOF.zone_cell_t)
  assert(cells > ARENA_HEAD_CELLS, "arena smaller than its header")
  local block = M.allocator_alloc(from, size)
  local arena = block
  arena.bump = {}
  zone_module.bump_zone_init_single(arena.bump, name, {}, (cells - ARENA_HEAD_CELLS) * SIZEOF.zone_cell_t)
  store_handle(arena, M.allocator_from_bump_zone(arena.bump))
  arena.from = from
  return arena
end

function M.arena_size_for(n)
  return ARENA_HEAD_CELLS * SIZEOF.zone_cell_t + zone_module.ZONE_BYTES(n)
end

function M.arena_size(arena)
  local a = arena
  assert(arena ~= nil and a.from ~= nil, "size of a handle that is not an arena")
  return ARENA_HEAD_CELLS * SIZEOF.zone_cell_t + a.bump.capacity
end

local function arena_spill_alloc(z, n)
  local a = z
  if zone_module.ZONE_BYTES(n) <= a.bump.capacity - a.bump.used then
    return zone_module.bump_zone_alloc(a.bump, n)
  end
  return M.allocator_alloc(a.from, n)
end

local function arena_spill_grow(z, p, old_n, new_n)
  local q = arena_spill_alloc(z, new_n)
  if old_n > 0 then
    copy_block(q, p)
  end
  return q
end

function M.arena_spill(arena)
  local a = arena
  assert(arena ~= nil and a.from ~= nil, "spill of a handle that is not an arena")
  a.alloc = arena_spill_alloc
  a.grow = arena_spill_grow
end

function M.arena_used(arena)
  local a = arena
  assert(arena ~= nil and a.from ~= nil, "usage of a handle that is not an arena")
  return ARENA_HEAD_CELLS * SIZEOF.zone_cell_t + a.bump.used
end

function M.arena_flush(arena)
  local a = arena
  assert(arena ~= nil and a.from ~= nil, "flush of a handle that is not an arena")
  zone_module.bump_zone_init_single(a.bump, a.bump.name, a.bump.storage, a.bump.capacity)
  store_handle(a, M.allocator_from_bump_zone(a.bump))
end

function M.arena_release(arena)
  local a = arena
  assert(arena ~= nil and a.from ~= nil, "release of a handle that is not an arena")
  M.allocator_release(a.from, a)
end

return M
