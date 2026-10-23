-- Copy of src/engine/value_list.h then src/engine/value_list.c.
-- A value_list_t is { items, element_size, count, capacity, zone }, items a
-- block indexed from 0 holding one value per element.
local allocator = require("engine.allocator")

local M = {}

local VALUE_LIST_INITIAL_CAPACITY = 4

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

function M.value_list_init(list, zone, element_size)
  assert(element_size > 0)
  list.items = nil
  list.element_size = element_size
  list.count = 0
  list.capacity = 0
  list.zone = zone
end

function M.value_list_free(list)
  if list.items ~= nil then
    allocator.allocator_release(list.zone, list.items)
  end
  list.items = nil
  list.count = 0
  list.capacity = 0
end

function M.value_list_clear(list)
  list.count = 0
end

-- A new list in zone holding a copy of every value of list.
function M.value_list_copy(zone, list)
  local out = {}
  M.value_list_init(out, zone, list.element_size)
  if list.count == 0 then
    return out
  end
  local bytes = list.count * list.element_size
  out.items = allocator.allocator_alloc(zone, bytes)
  for i = 0, list.count - 1 do
    out.items[i] = copy_value(list.items[i])
  end
  out.count = list.count
  out.capacity = list.count
  return out
end

local function grow(list)
  local new_capacity = (list.capacity == 0) and VALUE_LIST_INITIAL_CAPACITY or list.capacity * 2
  list.items = allocator.allocator_grow(list.zone, list.items, list.capacity * list.element_size,
    new_capacity * list.element_size)
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
