-- Copy of src/engine/zone.h then src/engine/zone.c, for the 64-bit build
-- without ASan: one half per bump zone, POISON and UNPOISON dropped.
-- An allocation returns a new empty table, the block; a storage table stands
-- for the static cells and is never read.
local cint = require("p8.cint")
local fatal = require("p8.fatal")

local M = {}

M.SIZEOF = {
  zone_cell_t = 8,
  ["void *"] = 8,
}
local SIZEOF = M.SIZEOF

function M.ZONE_CELLS(bytes)
  return cint.div(bytes + SIZEOF.zone_cell_t - 1, SIZEOF.zone_cell_t)
end

function M.ZONE_BYTES(bytes)
  return M.ZONE_CELLS(bytes) * SIZEOF.zone_cell_t
end

assert(SIZEOF["void *"] == 4 or SIZEOF["void *"] == 8, "pointer width is 4 or 8")

function M.ZONE_BYTES_BY_WIDTH(bytes_64, bytes_32)
  assert(cint.mod(bytes_32, SIZEOF.zone_cell_t) == 0 and bytes_32 <= bytes_64,
    "bytes_32 is a whole number of cells, at most bytes_64")
  if SIZEOF["void *"] == 4 then
    return bytes_32
  end
  return bytes_64
end

M.BUMP_ZONE_HALVES = 1

function M.BUMP_ZONE_STORAGE(capacity)
  return {}
end

function M.FLIP_ZONE_STORAGE(capacity)
  return {}
end

local function zone_full(name, limit, unit)
  fatal.p8_fatal(string.format("zone %s full, limit %d %s", name, limit, unit))
end

-- Byte offsets stand for the C addresses: a block has none.
local function bump_half(zone)
  return zone.half * M.ZONE_CELLS(zone.capacity) * SIZEOF.zone_cell_t
end

function M.bump_zone_init(zone, name, storage, capacity)
  zone.name = name
  zone.storage = storage
  zone.capacity = capacity
  zone.used = 0
  zone.half = 0
  zone.never_reset = 0
end

function M.bump_zone_init_single(zone, name, storage, capacity)
  zone.name = name
  zone.storage = storage
  zone.capacity = capacity
  zone.used = 0
  zone.half = 0
  zone.never_reset = 1
end

function M.bump_zone_alloc(zone, size)
  local rounded = M.ZONE_CELLS(size) * SIZEOF.zone_cell_t
  if rounded > zone.capacity - zone.used then
    zone_full(zone.name, zone.capacity, "bytes")
  end
  local p = bump_half(zone) + zone.used
  zone.used = zone.used + rounded
  return {}
end

function M.bump_zone_reset(zone)
  assert(zone.never_reset == 0,
    "reset of a single-half zone: an arena is never reset, it is released")
  zone.half = cint.mod(zone.half + 1, M.BUMP_ZONE_HALVES)
  zone.used = 0
end

function M.bump_zone_rewind(zone, used)
  assert(used <= zone.used)
  zone.used = used
end

function M.flip_zone_init(zone, name, storage, capacity)
  zone.name = name
  zone.storage = storage
  zone.capacity = capacity
  zone.bottom = 0
  zone.top = 0
  zone.current_top = 0
  zone.pending = 0
  zone.max = 0
end

local function flip_zone_end(zone)
  return M.ZONE_CELLS(zone.capacity) * SIZEOF.zone_cell_t
end

function M.flip_zone_alloc(zone, size)
  local rounded = M.ZONE_CELLS(size) * SIZEOF.zone_cell_t
  if rounded > zone.capacity - zone.bottom - zone.top then
    zone_full(zone.name, zone.capacity, "bytes")
  end
  local p
  if zone.current_top ~= 0 then
    zone.top = zone.top + rounded
    p = flip_zone_end(zone) - zone.top
  else
    p = zone.bottom
    zone.bottom = zone.bottom + rounded
  end
  if zone.bottom + zone.top > zone.max then
    zone.max = zone.bottom + zone.top
  end
  return {}
end

function M.flip_zone_flip(zone)
  assert(zone.pending == 0,
    "flip while the old generation awaits its release: flush the exit queue between two game starts")
  zone.current_top = (zone.current_top == 0) and 1 or 0
  assert(((zone.current_top ~= 0) and zone.top or zone.bottom) == 0)
  zone.pending = 1
end

function M.flip_zone_pending(zone)
  return zone.pending
end

function M.flip_zone_release(zone)
  if zone.pending == 0 then
    return
  end
  if zone.current_top ~= 0 then
    zone.bottom = 0
  else
    zone.top = 0
  end
  zone.pending = 0
end

function M.flip_zone_reset(zone)
  zone.bottom = 0
  zone.top = 0
  zone.current_top = 0
  zone.pending = 0
end

function M.flip_zone_used(zone)
  return zone.bottom + zone.top
end

function M.flip_zone_max(zone)
  return zone.max
end

return M
