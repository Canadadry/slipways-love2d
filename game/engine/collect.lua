-- Copy of src/engine/collect.h then src/engine/collect.c.
-- A filter_pointer_list_predicate_fn_t is fn(e, ctx) returning 1 or 0; a
-- map_pointer_list_fn_t is fn(e, ctx) returning found, value.
local pointer_list = require("engine.pointer_list")
local str = require("p8.str")

local M = {}

function M.collect_filter_pointer_list(qk, ow, ctx)
  local f = {}
  local it
  local e
  pointer_list.pointer_list_init(f)
  if qk == nil then
    return f
  end
  it = pointer_list.pointer_list_all(qk)
  while true do
    local more
    more, e = pointer_list.pointer_list_iter_next(it, e)
    if more == 0 then
      break
    end
    if ow(e, ctx) ~= 0 then
      pointer_list.pointer_list_add(f, e)
    end
  end
  return f
end

function M.collect_map_pointer_list(qk, kp, ctx)
  local m = {}
  local it
  local e
  assert(qk ~= nil, "collect_map_pointer_list: a NULL list is not tolerated here, unlike collect_contains_string and collect_filter_pointer_list")
  pointer_list.pointer_list_init(m)
  it = pointer_list.pointer_list_all(qk)
  while true do
    local more
    more, e = pointer_list.pointer_list_iter_next(it, e)
    if more == 0 then
      break
    end
    local found, v = kp(e, ctx)
    if found ~= 0 then
      pointer_list.pointer_list_add(m, v)
    end
  end
  return m
end

local function append_pointer_list(e, ctx)
  pointer_list.pointer_list_add(ctx, e)
end

function M.collect_concat_pointer_list(a, b)
  local t = {}
  assert(a ~= nil and b ~= nil, "collect_concat_pointer_list: a NULL list is not tolerated here, unlike collect_contains_string and collect_filter_pointer_list")
  pointer_list.pointer_list_init(t)
  pointer_list.pointer_list_foreach(a, append_pointer_list, t)
  pointer_list.pointer_list_foreach(b, append_pointer_list, t)
  return t
end

function M.collect_contains_string(qk, kb)
  local it
  local e
  if qk == nil then
    return 0
  end
  it = pointer_list.pointer_list_all(qk)
  while true do
    local more
    more, e = pointer_list.pointer_list_iter_next(it, e)
    if more == 0 then
      break
    end
    if e ~= nil and e == kb then
      return 1
    end
  end
  return 0
end

function M.collect_format_signed(d, c, ku, out_color)
  local num_str = str.p8_tostr_fixed(d)
  local suffix = (ku ~= nil) and ku or ""
  local result

  if d >= 0 then
    out_color = c
    result = "+" .. num_str
  else
    out_color = 8
    result = num_str
  end
  result = result .. suffix
  return result, out_color
end

return M
