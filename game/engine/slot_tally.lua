-- Copy of src/engine/slot_tally.h then src/engine/slot_tally.c.
-- A slot_tally_t is { kinds, zones, n, live, max_live, released_max_bytes,
-- zone_bytes, zone_max_bytes }, its arrays 0-indexed with
-- SLOT_TALLY_KINDS_MAX entries.
local allocator = require("engine.allocator")
local zone = require("engine.zone")

local M = {}

M.SLOT_TALLY_KINDS_MAX = 8

local function check_kind(t, kind)
  assert(kind >= 0 and kind < t.n, "slot tally: kind outside the group")
end

local function is_arena(t, kind)
  return (t.kinds[kind].slot_name ~= nil) and 1 or 0
end

local function arena_bytes(arena)
  return allocator.arena_used(arena)
end

function M.slot_tally_init(t, kinds, zones, n)
  assert(n > 0 and n <= M.SLOT_TALLY_KINDS_MAX)
  t.kinds = nil
  t.zones = nil
  t.n = 0
  t.live = {}
  t.max_live = {}
  t.released_max_bytes = {}
  for i = 0, M.SLOT_TALLY_KINDS_MAX - 1 do
    t.live[i] = 0
    t.max_live[i] = 0
    t.released_max_bytes[i] = 0
  end
  t.zone_bytes = 0
  t.zone_max_bytes = 0
  t.kinds = kinds
  t.zones = zones
  t.n = n
end

function M.slot_tally_take(t, kind)
  check_kind(t, kind)
  t.live[kind] = t.live[kind] + 1
  if t.live[kind] > t.max_live[kind] then
    t.max_live[kind] = t.live[kind]
  end
  t.zone_bytes = t.zone_bytes + zone.SLOT_ZONE_BYTES(t.kinds[kind].size, 1)
  if t.zone_bytes > t.zone_max_bytes then
    t.zone_max_bytes = t.zone_bytes
  end
end

function M.slot_tally_give_back(t, kind, slot)
  check_kind(t, kind)
  assert(t.live[kind] > 0, "slot tally: give back of a slot never taken")
  if is_arena(t, kind) ~= 0 then
    local used = arena_bytes(slot)
    if used > t.released_max_bytes[kind] then
      t.released_max_bytes[kind] = used
    end
  end
  t.live[kind] = t.live[kind] - 1
  t.zone_bytes = t.zone_bytes - zone.SLOT_ZONE_BYTES(t.kinds[kind].size, 1)
end

function M.slot_tally_live(t, kind)
  check_kind(t, kind)
  return t.live[kind]
end

function M.slot_tally_max_live(t, kind)
  check_kind(t, kind)
  return t.max_live[kind]
end

-- The slot at storage + i * ZONE_CELLS(slot_size) is the zone's block i.
function M.slot_tally_max_bytes(t, kind)
  check_kind(t, kind)
  local max = t.released_max_bytes[kind]
  if is_arena(t, kind) ~= 0 then
    local slot_zone = t.zones[kind]
    for i = 0, slot_zone.count - 1 do
      if slot_zone.taken[i] == 0 then
        goto continue
      end
      local used = arena_bytes(slot_zone.blocks[i])
      if used > max then
        max = used
      end
      ::continue::
    end
  end
  return max
end

function M.slot_tally_zone_bytes(t)
  return t.zone_bytes
end

function M.slot_tally_zone_max_bytes(t)
  return t.zone_max_bytes
end

function M.slot_tally_zone_reserved(t)
  local bytes = 0
  for i = 0, t.n - 1 do
    bytes = bytes + zone.SLOT_ZONE_BYTES(t.kinds[i].size, t.kinds[i].count)
  end
  return bytes
end

return M
