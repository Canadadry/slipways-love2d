-- Copy of src/engine/value_list.h then src/engine/value_list.c.
-- A value_list_t is { items, count, capacity }, items a block indexed from 0
-- holding one value per element.

local M = {}

local VALUE_LIST_INITIAL_CAPACITY = 4

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

-- Lua only: the memcpy of one element of unknown type, a table copied one
-- level deep. An inline sub-table is shared with the source: the caller copies
-- a mutable one before the add.
local function copy_value(value)
  if type(value) ~= "table" then
    return value
  end
  local copy = {}
  for key, field in pairs(value) do
    copy[key] = field
  end
  return copy
end

function M.value_list_init(list)
  list.items = nil
  list.count = 0
  list.capacity = 0
end

function M.value_list_free(list)
  list.items = nil
  list.count = 0
  list.capacity = 0
end

function M.value_list_clear(list)
  list.count = 0
end

-- A new list holding a copy of every value of list.
function M.value_list_copy(list)
  local out = {}
  M.value_list_init(out)
  if list.count == 0 then
    return out
  end
  out.items = {}
  for i = 0, list.count - 1 do
    out.items[i] = copy_value(list.items[i])
  end
  out.count = list.count
  out.capacity = list.count
  return out
end

local function grow(list)
  local new_capacity = (list.capacity == 0) and VALUE_LIST_INITIAL_CAPACITY or list.capacity * 2
  list.items = grown_items(list.items, list.capacity)
  list.capacity = new_capacity
end

function M.value_list_add(list, value)
  if list.count == list.capacity then
    grow(list)
  end
  list.items[list.count] = copy_value(value)
  list.count = list.count + 1
end

function M.value_list_count(list)
  return list.count
end

-- Valid until the next add or free: never store it.
function M.value_list_at(list, index)
  assert(index >= 0 and index < list.count, "value_list_at: index out of range")
  return list.items[index]
end

return M
