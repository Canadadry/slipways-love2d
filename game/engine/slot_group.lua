-- Copy of src/engine/slot_group.h then src/engine/slot_group.c.
-- A slot_group_handle_t is one table holding its handle's five allocator_t
-- fields, group and kind: the cast from the handle is the identity.
local allocator = require("engine.allocator")
local slot_tally = require("engine.slot_tally")
local zone = require("engine.zone")

local M = {}

function M.SLOT_GROUP(kind_table, kind_count)
  local g = { kinds = kind_table, n = kind_count, ready = 0, zones = {}, slots = {}, handles = {},
    tally = { kinds = nil, zones = nil, n = 0, live = {}, max_live = {}, released_max_bytes = {},
      zone_bytes = 0, zone_max_bytes = 0 },
    max_elements = {} }
  for i = 0, slot_tally.SLOT_TALLY_KINDS_MAX - 1 do
    g.zones[i] = { name = nil, storage = nil, taken = nil, slot_size = 0, count = 0, cursor = 0 }
    g.slots[i] = {}
    g.handles[i] = { group = nil, kind = 0 }
    g.tally.live[i] = 0
    g.tally.max_live[i] = 0
    g.tally.released_max_bytes[i] = 0
    g.max_elements[i] = 0
  end
  return g
end

local function check_kind(g, kind)
  assert(kind >= 0 and kind < g.n, "slot group: kind outside the group")
end

local slot_alloc
local slot_release

local function init_once(g)
  if g.ready ~= 0 then
    return
  end
  assert(g.n > 0 and g.n <= slot_tally.SLOT_TALLY_KINDS_MAX)
  allocator.allocator_init_slot_kinds(g.kinds, g.n, g.zones, g.slots)
  slot_tally.slot_tally_init(g.tally, g.kinds, g.zones, g.n)
  for i = 0, g.n - 1 do
    g.handles[i] = { alloc = slot_alloc, grow = allocator.allocator_grow_refused, release = slot_release,
      bump = nil, slot = nil, group = g, kind = i }
  end
  g.ready = 1
end

function slot_alloc(z, n)
  local h = z
  local p = allocator.allocator_alloc(h.group.slots[h.kind], n)
  slot_tally.slot_tally_take(h.group.tally, h.kind)
  return p
end

function slot_release(z, p)
  local h = z
  if p == nil then
    return
  end
  slot_tally.slot_tally_give_back(h.group.tally, h.kind, p)
  allocator.allocator_release(h.group.slots[h.kind], p)
end

function M.slot_group_handle(g, kind)
  check_kind(g, kind)
  init_once(g)
  return g.handles[kind]
end

function M.slot_group_slot(g, kind)
  assert(M.slot_group_is_arena(g, kind) ~= 0, "slot_group_slot: a bare slot is not an arena")
  return allocator.arena_take(M.slot_group_handle(g, kind), g.kinds[kind].slot_name, g.kinds[kind].size)
end

function M.slot_group_live(g, kind)
  check_kind(g, kind)
  init_once(g)
  assert(slot_tally.slot_tally_live(g.tally, kind) == zone.slot_zone_taken(g.zones[kind]),
    "slot_group_live: a slot taken or given back without its kind's handle")
  return slot_tally.slot_tally_live(g.tally, kind)
end

function M.slot_group_max_live(g, kind)
  check_kind(g, kind)
  init_once(g)
  return slot_tally.slot_tally_max_live(g.tally, kind)
end

function M.slot_group_max_bytes(g, kind)
  check_kind(g, kind)
  init_once(g)
  return slot_tally.slot_tally_max_bytes(g.tally, kind)
end

function M.slot_group_used(g)
  init_once(g)
  return slot_tally.slot_tally_zone_bytes(g.tally)
end

function M.slot_group_max_used(g)
  init_once(g)
  return slot_tally.slot_tally_zone_max_bytes(g.tally)
end

function M.slot_group_reserved(g)
  init_once(g)
  return slot_tally.slot_tally_zone_reserved(g.tally)
end

function M.slot_group_count(g, kind)
  check_kind(g, kind)
  return g.kinds[kind].count
end

function M.slot_group_slot_size(g, kind)
  check_kind(g, kind)
  return g.kinds[kind].size
end

function M.slot_group_is_arena(g, kind)
  check_kind(g, kind)
  return (g.kinds[kind].slot_name ~= nil) and 1 or 0
end

function M.slot_group_record_elements(g, kind, count)
  check_kind(g, kind)
  if count > g.max_elements[kind] then
    g.max_elements[kind] = count
  end
end

function M.slot_group_max_elements(g, kind)
  check_kind(g, kind)
  return g.max_elements[kind]
end

return M
