-- Copy of src/engine/entity_id_list.h then src/engine/entity_id_list.c.
-- An entity_id_list_t is { items, count, capacity, zone }, items a block
-- indexed from 0 holding each id by value (a copy, compared through
-- entity_id_equal); an entity_id_list_iter_t is { list, index, last_count }.
local allocator = require("engine.allocator")
local entity_id_type = require("engine.entity_id_type")
local entity_id = require("engine.entity_id")

local M = {}

M.SIZEOF = {
  entity_id_t = 4,
}
local SIZEOF = M.SIZEOF

local ENTITY_ID_LIST_INITIAL_CAPACITY = 4

function M.entity_id_list_init(list, zone)
  assert(zone ~= nil, "entity_id_list_init: a list without a zone would grow nowhere")
  list.items = nil
  list.count = 0
  list.capacity = 0
  list.zone = zone
end

function M.entity_id_list_init_reserve(list, zone, capacity)
  assert(capacity >= 0)
  M.entity_id_list_init(list, zone)
  if capacity == 0 then
    return
  end
  list.items = allocator.allocator_alloc(zone, capacity * SIZEOF.entity_id_t)
  list.capacity = capacity
end

function M.entity_id_list_clear(list)
  list.count = 0
end

function M.entity_id_list_free(list)
  if list.items ~= nil then
    allocator.allocator_release(list.zone, list.items)
  end
  list.items = nil
  list.count = 0
  list.capacity = 0
end

local function grow(list)
  local new_capacity = (list.capacity == 0) and ENTITY_ID_LIST_INITIAL_CAPACITY or list.capacity * 2
  assert(list.zone ~= nil, "entity_id_list_add on a list entity_id_list_init never gave a zone")
  list.items = allocator.allocator_grow(list.zone, list.items,
    list.capacity * SIZEOF.entity_id_t, new_capacity * SIZEOF.entity_id_t)
  list.capacity = new_capacity
end

function M.entity_id_list_add(list, id)
  if list.count == list.capacity then
    grow(list)
  end
  list.items[list.count] = { index = id.index, generation = id.generation }
  list.count = list.count + 1
end

-- Removes the first id equal to id; 1 when one was removed.
function M.entity_id_list_del(list, id)
  for i = 0, list.count - 1 do
    if entity_id_type.entity_id_equal(list.items[i], id) ~= 0 then
      for j = i, list.count - 2 do
        list.items[j] = { index = list.items[j + 1].index, generation = list.items[j + 1].generation }
      end
      list.count = list.count - 1
      return 1
    end
  end
  return 0
end

-- The id at index, resolved; asserts on an id that resolves to nil.
function M.entity_id_list_at(list, index)
  assert(index >= 0 and index < list.count, "entity_id_list_at: index out of range")
  local e = entity_id.entity_get(list.items[index])
  assert(e ~= nil, "a game id list holds no dead id: remove it where the entity dies")
  return e
end

-- 0 for a nil list.
function M.entity_id_list_has(list, id)
  if list == nil then
    return 0
  end
  for i = 0, list.count - 1 do
    if entity_id_type.entity_id_equal(list.items[i], id) ~= 0 then
      return 1
    end
  end
  return 0
end

function M.entity_id_list_count(list)
  return list.count
end

function M.entity_id_list_concat(zone, a, b)
  assert(a ~= nil and b ~= nil, "entity_id_list_concat: a NULL list is not tolerated, unlike entity_id_list_has")
  local out = {}
  M.entity_id_list_init_reserve(out, zone, a.count + b.count)
  for i = 0, a.count - 1 do
    M.entity_id_list_add(out, a.items[i])
  end
  for i = 0, b.count - 1 do
    M.entity_id_list_add(out, b.items[i])
  end
  return out
end

-- The predicate sees each id resolved; a nil list gives an empty list.
function M.entity_id_list_filter(zone, list, predicate, ctx)
  local out = {}
  M.entity_id_list_init(out, zone)
  if list == nil then
    return out
  end
  local it = M.entity_id_list_all(list)
  local e
  while true do
    local more
    more, e = M.entity_id_list_iter_next(it, e)
    if more == 0 then
      break
    end
    if predicate(e, ctx) ~= 0 then
      M.entity_id_list_add(out, it.list.items[it.index])
    end
  end
  return out
end

function M.entity_id_list_all(list)
  assert(list ~= nil, "entity_id_list primitive called on a NULL container")
  return { list = list, index = -1, last_count = list.count }
end

-- Hands out each id resolved; asserts on an id that resolves to nil.
function M.entity_id_list_iter_next(iter, out)
  local next_index
  if iter.index < 0 then
    next_index = 0
  elseif iter.list.count < iter.last_count then
    next_index = iter.index
  else
    next_index = iter.index + 1
  end
  if next_index >= iter.list.count then
    return 0, out
  end
  out = M.entity_id_list_at(iter.list, next_index)
  iter.index = next_index
  iter.last_count = iter.list.count
  return 1, out
end

return M
