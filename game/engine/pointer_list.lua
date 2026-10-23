-- Copy of src/engine/pointer_list.h then src/engine/pointer_list.c.
-- A pointer_list_t is { items, count, capacity }, items a block indexed
-- from 0; a pointer_list_iter_t is { pointer_list, index, last_count }.

local M = {}

local POINTER_LIST_INITIAL_CAPACITY = 4

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

function M.pointer_list_init(pointer_list)
  pointer_list.items = nil
  pointer_list.count = 0
  pointer_list.capacity = 0
end

function M.pointer_list_init_reserve(pointer_list, capacity)
  assert(capacity > 0)
  M.pointer_list_init(pointer_list)
  pointer_list.items = {}
  pointer_list.capacity = capacity
end

function M.pointer_list_free(pointer_list)
  assert(pointer_list ~= nil, "pointer_list primitive called on a NULL container")
  pointer_list.items = nil
  pointer_list.count = 0
  pointer_list.capacity = 0
end

local function pointer_list_grow(pointer_list)
  local new_capacity = (pointer_list.capacity == 0) and POINTER_LIST_INITIAL_CAPACITY or pointer_list.capacity * 2
  pointer_list.items = grown_items(pointer_list.items, pointer_list.capacity)
  pointer_list.capacity = new_capacity
end

function M.pointer_list_add(pointer_list, e)
  assert(pointer_list ~= nil, "pointer_list primitive called on a NULL container")
  if pointer_list.count == pointer_list.capacity then
    pointer_list_grow(pointer_list)
  end
  pointer_list.items[pointer_list.count] = e
  pointer_list.count = pointer_list.count + 1
end

function M.pointer_list_del(pointer_list, e)
  assert(pointer_list ~= nil, "pointer_list primitive called on a NULL container")
  for i = 0, pointer_list.count - 1 do
    if pointer_list.items[i] == e then
      for j = i, pointer_list.count - 2 do
        pointer_list.items[j] = pointer_list.items[j + 1]
      end
      pointer_list.count = pointer_list.count - 1
      return 1
    end
  end
  return 0
end

function M.pointer_list_all(pointer_list)
  assert(pointer_list ~= nil, "pointer_list primitive called on a NULL container")
  return { pointer_list = pointer_list, index = -1, last_count = pointer_list.count }
end

function M.pointer_list_iter_next(iter, out)
  local next_index

  if iter.index < 0 then
    next_index = 0
  elseif iter.pointer_list.count < iter.last_count then
    next_index = iter.index
  else
    next_index = iter.index + 1
  end

  if next_index >= iter.pointer_list.count then
    return 0, out
  end

  out = iter.pointer_list.items[next_index]
  iter.index = next_index
  iter.last_count = iter.pointer_list.count
  return 1, out
end

-- The bound is read again after each call, as the C loop does: fn may change the list.
function M.pointer_list_foreach(pointer_list, fn, ctx)
  assert(pointer_list ~= nil, "pointer_list primitive called on a NULL container")
  local i = 0
  while i < pointer_list.count do
    fn(pointer_list.items[i], ctx)
    i = i + 1
  end
end

return M
