-- Copy of src/engine/exit_queue.h then src/engine/exit_queue.c.
-- An entry_t is { entity, arena }; g_queue is a 0-indexed array of
-- EXIT_QUEUE_CAP entries, each slot overwritten field by field by put.
local allocator = require("engine.allocator")
local entity_id = require("engine.entity_id")
local fatal = require("p8.fatal")

local M = {}

M.EXIT_QUEUE_CAP = 315

local g_queue = {}
for i = 0, M.EXIT_QUEUE_CAP - 1 do
  g_queue[i] = { entity = nil, arena = nil }
end
local g_count = 0
local g_max = 0

local function put(entry)
  if g_count == M.EXIT_QUEUE_CAP then
    fatal.p8_fatal(string.format("exit queue full, limit %d", M.EXIT_QUEUE_CAP))
  end
  g_queue[g_count].entity = entry.entity
  g_queue[g_count].arena = entry.arena
  g_count = g_count + 1
  if g_count > g_max then g_max = g_count end
end

function M.exit_queue_put(e)
  local entry = { entity = e, arena = nil }
  assert(e ~= nil)
  put(entry)
end

function M.exit_queue_put_arena(arena)
  local entry = { entity = nil, arena = arena }
  assert(arena ~= nil)
  put(entry)
end

function M.exit_queue_count()
  return g_count
end

function M.exit_queue_max()
  return g_max
end

function M.exit_queue_flush()
  for i = 0, g_count - 1 do
    local e = g_queue[i].entity
    if e == nil then
      allocator.arena_release(g_queue[i].arena)
      goto continue
    end
    do
      local release = e.class.release
      -- The slot goes before the memory: no slot may point at a returned block.
      entity_id.entity_id_release(e)
      if release ~= nil then release(e) end
    end
    ::continue::
  end
  g_count = 0
end

return M
