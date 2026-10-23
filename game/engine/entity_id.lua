-- An id is a number from a counter that never resets, not even across
-- games, so a stale id never resolves to a newer entity.
local M = {}

local g_last_id = 0
local g_entities = nil

-- Every id of the previous table resolves to nil afterwards.
function M.table_init()
  g_entities = {}
end

-- Drops the table, for after a game is quit.
function M.table_forget()
  g_entities = nil
end

-- Also writes the id into e.id.
function M.take(e)
  assert(g_entities ~= nil, "entity_registry.init() must run before entity.construct")
  assert(e ~= nil)
  g_last_id = g_last_id + 1
  g_entities[g_last_id] = e
  e.id = g_last_id
  return g_last_id
end

-- Drops the entry only when it holds exactly e: an entity of a previous
-- table drops nothing in this one.
function M.release(e)
  if g_entities ~= nil and e.id ~= nil and g_entities[e.id] == e then
    g_entities[e.id] = nil
  end
end

-- nil for a nil id, a released or unknown id, or no table.
function M.get(id)
  if g_entities == nil or id == nil then
    return nil
  end
  return g_entities[id]
end

function M.resolve(id)
  local e = M.get(id)
  assert(e ~= nil, "a game id list holds no dead id: remove it where the entity dies")
  return e
end

-- nil when e holds no entry of the current table.
function M.of(e)
  assert(e ~= nil)
  return (M.get(e.id) == e) and e.id or nil
end

-- nil for nil; asserts a non-nil e holds an entry.
function M.link(e)
  if e == nil then
    return nil
  end
  local id = M.of(e)
  assert(id ~= nil,
    "a link names a constructed entity: one without a slot would read NULL at once")
  return id
end

return M
