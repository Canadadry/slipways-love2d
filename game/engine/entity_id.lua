-- An id is a number from a counter that never resets, not even across
-- games, so a stale id never resolves to a newer entity.
local M = {}

local g_last_id = 0
local g_entities = nil

-- Every id of the previous table resolves to nil afterwards.
function M.entity_id_table_init()
  g_entities = {}
end

-- Drops the table, for after a game is quit.
function M.entity_id_table_forget()
  g_entities = nil
end

-- Also writes the id into e.id.
function M.entity_id_take(e)
  assert(g_entities ~= nil, "entity_registry_init() must run before entity_construct")
  assert(e ~= nil)
  g_last_id = g_last_id + 1
  g_entities[g_last_id] = e
  e.id = g_last_id
  return g_last_id
end

-- Drops the entry only when it holds exactly e: an entity of a previous
-- table drops nothing in this one.
function M.entity_id_release(e)
  if g_entities ~= nil and e.id ~= nil and g_entities[e.id] == e then
    g_entities[e.id] = nil
  end
end

-- nil for a nil id, a released or unknown id, or no table.
function M.entity_get(id)
  if g_entities == nil or id == nil then
    return nil
  end
  return g_entities[id]
end

-- nil when e holds no entry of the current table.
function M.entity_id_of(e)
  assert(e ~= nil)
  return (M.entity_get(e.id) == e) and e.id or nil
end

-- nil for nil; asserts a non-nil e holds an entry.
function M.entity_id_link(e)
  if e == nil then
    return nil
  end
  local id = M.entity_id_of(e)
  assert(id ~= nil,
    "a link names a constructed entity: one without a slot would read NULL at once")
  return id
end

return M
