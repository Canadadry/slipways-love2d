-- Copy of src/engine/entity_id.h then src/engine/entity_id.c.
-- The table's block is the entities array (indexed 0 .. ENTITY_TABLE_CAP - 1,
-- nil for a free slot); generations and free_indexes are 0-indexed tables
-- standing for the C offsets into the same block, which nothing reads.
local allocator = require("engine.allocator")
local zone_sizes = require("engine.zone_sizes")
local entity_id_type = require("engine.entity_id_type")
local fatal = require("p8.fatal")

local M = {}

local ENTITY_TABLE_CAP = zone_sizes.ENTITY_TABLE_CAP
local ENTITY_TABLE_BYTES = zone_sizes.ENTITY_TABLE_BYTES
local UINT16_MAX = 65535
local ENTITY_ID_NONE = entity_id_type.ENTITY_ID_NONE

local g_table = { entities = nil, generations = nil, free_indexes = nil, free_count = 0 }
local g_max_live = 0
-- The next table starts every slot above any generation handed out so far,
-- so an old id never matches a new entity on a reused index.
local g_highest_generation = 0

local function next_generation(generation)
  return (generation == UINT16_MAX) and 1 or generation + 1
end

-- Takes the table's ENTITY_TABLE_BYTES block from zone; every id of the
-- previous table resolves to nil afterwards.
function M.entity_id_table_init(zone)
  local block = allocator.allocator_alloc(zone, ENTITY_TABLE_BYTES)
  g_table.entities = block
  g_table.generations = {}
  g_table.free_indexes = {}
  local epoch = next_generation(g_highest_generation)
  g_highest_generation = epoch
  for i = 0, ENTITY_TABLE_CAP - 1 do
    g_table.entities[i] = nil
    g_table.generations[i] = epoch
    g_table.free_indexes[i] = ENTITY_TABLE_CAP - 1 - i
  end
  g_table.free_count = ENTITY_TABLE_CAP
end

-- Drops the table, for after a game is quit.
function M.entity_id_table_forget()
  g_table.entities = nil
  g_table.generations = nil
  g_table.free_indexes = nil
  g_table.free_count = 0
end

-- Also writes the id into e.id.
function M.entity_id_take(e)
  assert(g_table.entities ~= nil, "entity_registry_init() must run before entity_construct")
  assert(e ~= nil)
  if g_table.free_count == 0 then
    fatal.p8_fatal(string.format("entity table full, limit %d", ENTITY_TABLE_CAP))
  end
  g_table.free_count = g_table.free_count - 1
  local index = g_table.free_indexes[g_table.free_count]
  g_table.entities[index] = e
  local id = { index = index, generation = g_table.generations[index] }
  assert(id.generation ~= 0, "generation 0 is reserved for ENTITY_ID_NONE")
  e.id = { index = id.index, generation = id.generation }
  if M.entity_id_live() > g_max_live then
    g_max_live = M.entity_id_live()
  end
  return id
end

local function slot_of(e)
  assert(e ~= nil, "a free slot holds NULL: no entity is NULL")
  return (M.entity_get(e.id) == e) and e.id.index or -1
end

-- Frees the slot that holds exactly e, if any: an entity of a previous
-- table frees nothing in this one.
function M.entity_id_release(e)
  local index = slot_of(e)
  if index < 0 then
    return
  end
  g_table.entities[index] = nil
  local generation = next_generation(g_table.generations[index])
  g_table.generations[index] = generation
  if generation > g_highest_generation then
    g_highest_generation = generation
  end
  g_table.free_indexes[g_table.free_count] = index
  g_table.free_count = g_table.free_count + 1
end

-- nil for the "no link" id, a free slot, a stale generation or no table.
function M.entity_get(id)
  if g_table.entities == nil or id.index >= ENTITY_TABLE_CAP then
    return nil
  end
  if g_table.generations[id.index] ~= id.generation then
    return nil
  end
  return g_table.entities[id.index]
end

-- ENTITY_ID_NONE when e holds no slot of the current table.
function M.entity_id_of(e)
  if slot_of(e) < 0 then
    return { index = ENTITY_ID_NONE.index, generation = ENTITY_ID_NONE.generation }
  end
  return { index = e.id.index, generation = e.id.generation }
end

-- ENTITY_ID_NONE for nil; asserts a non-nil e holds a slot.
function M.entity_id_link(e)
  if e == nil then
    return { index = ENTITY_ID_NONE.index, generation = ENTITY_ID_NONE.generation }
  end
  local id = M.entity_id_of(e)
  assert(entity_id_type.entity_id_is_none(id) == 0,
    "a link names a constructed entity: one without a slot would read NULL at once")
  return id
end

function M.entity_id_live()
  return (g_table.entities == nil) and 0 or ENTITY_TABLE_CAP - g_table.free_count
end

function M.entity_id_max()
  return g_max_live
end

return M
