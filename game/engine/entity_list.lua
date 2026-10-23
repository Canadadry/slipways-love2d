-- Copy of src/engine/entity_list.h then src/engine/entity_list.c.
-- An entity_list_t is { items, count, capacity }, items a block indexed
-- from 0 holding entity tables; an entity_list_iter_t is
-- { entity_list, index, last_count }.

local M = {}

local ENTITY_LIST_INITIAL_CAPACITY = 4

-- Lua only: a list that grows gets a fresh items table holding a copy, so a
-- header copied by value keeps its own items until its next add.
local function grown_items(items, capacity)
  local copy = {}
  if capacity > 0 then
    for key, value in pairs(items) do
      copy[key] = value
    end
  end
  return copy
end

function M.entity_list_init(entity_list)
  entity_list.items = nil
  entity_list.count = 0
  entity_list.capacity = 0
end

function M.entity_list_init_reserve(entity_list, capacity)
  assert(capacity >= 0)
  M.entity_list_init(entity_list)
  if capacity == 0 then
    return
  end
  entity_list.items = {}
  entity_list.capacity = capacity
end

function M.entity_list_free(entity_list)
  assert(entity_list ~= nil, "entity_list primitive called on a NULL container")
  entity_list.items = nil
  entity_list.count = 0
  entity_list.capacity = 0
end

function M.entity_list_clear(entity_list)
  assert(entity_list ~= nil, "entity_list primitive called on a NULL container")
  entity_list.count = 0
end

local function entity_list_grow(entity_list)
  local new_capacity = (entity_list.capacity == 0) and ENTITY_LIST_INITIAL_CAPACITY or entity_list.capacity * 2
  entity_list.items = grown_items(entity_list.items, entity_list.capacity)
  entity_list.capacity = new_capacity
end

function M.entity_list_add(entity_list, e)
  assert(entity_list ~= nil, "entity_list primitive called on a NULL container")
  if entity_list.count == entity_list.capacity then
    entity_list_grow(entity_list)
  end
  entity_list.items[entity_list.count] = e
  entity_list.count = entity_list.count + 1
end

function M.entity_list_del(entity_list, e)
  assert(entity_list ~= nil, "entity_list primitive called on a NULL container")
  for i = 0, entity_list.count - 1 do
    if entity_list.items[i] == e then
      for j = i, entity_list.count - 2 do
        entity_list.items[j] = entity_list.items[j + 1]
      end
      entity_list.count = entity_list.count - 1
      return 1
    end
  end
  return 0
end

function M.entity_list_has(entity_list, e)
  if entity_list == nil then
    return 0
  end
  for i = 0, entity_list.count - 1 do
    if entity_list.items[i] == e then
      return 1
    end
  end
  return 0
end

function M.entity_list_concat(a, b)
  local out = {}
  assert(a ~= nil and b ~= nil, "entity_list_concat: a NULL list is not tolerated, unlike entity_list_has")
  M.entity_list_init(out)
  for i = 0, a.count - 1 do
    M.entity_list_add(out, a.items[i])
  end
  for i = 0, b.count - 1 do
    M.entity_list_add(out, b.items[i])
  end
  return out
end

function M.entity_list_filter(qk, ow, ctx)
  local f = {}
  M.entity_list_init(f)
  if qk == nil then
    return f
  end
  local it = M.entity_list_all(qk)
  local e
  while true do
    local more
    more, e = M.entity_list_iter_next(it, e)
    if more == 0 then
      break
    end
    if ow(e, ctx) ~= 0 then
      M.entity_list_add(f, e)
    end
  end
  return f
end

-- nil when the list is nil or nothing matches; asks the predicate about nothing after the match.
function M.entity_list_find_first(entity_list, predicate, ctx)
  if entity_list == nil then
    return nil
  end
  local it = M.entity_list_all(entity_list)
  local e
  while true do
    local more
    more, e = M.entity_list_iter_next(it, e)
    if more == 0 then
      break
    end
    if predicate(e, ctx) ~= 0 then
      return e
    end
  end
  return nil
end

function M.entity_list_all(entity_list)
  assert(entity_list ~= nil, "entity_list primitive called on a NULL container")
  return { entity_list = entity_list, index = -1, last_count = entity_list.count }
end

function M.entity_list_iter_next(iter, out)
  local next_index

  if iter.index < 0 then
    next_index = 0
  elseif iter.entity_list.count < iter.last_count then
    next_index = iter.index
  else
    next_index = iter.index + 1
  end

  if next_index >= iter.entity_list.count then
    return 0, out
  end

  out = iter.entity_list.items[next_index]
  iter.index = next_index
  iter.last_count = iter.entity_list.count
  return 1, out
end

return M
