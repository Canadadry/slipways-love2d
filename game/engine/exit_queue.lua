local entity_id = require("engine.entity_id")

local M = {}

local g_queue = {}

function M.exit_queue_put(e)
  assert(e ~= nil)
  g_queue[#g_queue + 1] = e
end

function M.exit_queue_flush()
  local n = #g_queue
  for i = 1, n do
    local e = g_queue[i]
    local release = e.class.release
    -- The entity stops resolving before its class release runs.
    entity_id.entity_id_release(e)
    if release ~= nil then release(e) end
  end
  -- An entity queued by a release during this flush is dropped unreleased:
  -- releasing it here would move the replays.
  for i = #g_queue, 1, -1 do g_queue[i] = nil end
end

return M
