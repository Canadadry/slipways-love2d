-- Copy of src/game/draw_functions.h then src/game/draw_functions.c.
-- A draw_functions_fn_t is a Lua function taking the data_row_t; draw_functions_by_id is
-- keyed by the DRAW_FUNCTION_* constants of map_tables, [DRAW_FUNCTION_NONE] absent.
local hud = require("game.hud")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local str = require("p8.str")

local M = {}

local g_draw_functions_gfx = nil
local g_draw_functions_mem = nil

local function draw_functions_arg(e, i)
  local item

  if i > e.count then
    return nil
  end
  item = e.items[i]
  assert(item.row == nil, "a row drawn by draw_functions holds no nested row")
  return item
end

local function is_number(e, i)
  local item = draw_functions_arg(e, i)
  if item == nil or item.text ~= nil then
    return false
  end
  return not (item.is_false or item.is_nil)
end

local function number(e, i)
  local item = draw_functions_arg(e, i)
  if not is_number(e, i) then
    return fixed.p8_fixed_from_int(0)
  end
  return item.is_fixed and item.fixed or fixed.p8_fixed_from_int(item.number)
end

local function draw_bar_entry(e)
  local y1 = number(e, 1)
  local col
  col = fixed.p8_fixed_flr_int(number(e, 3))
  if is_number(e, 2) then
    hud.hud_draw_bar(g_draw_functions_gfx, g_draw_functions_mem, y1, number(e, 2), col)
  else
    hud.hud_draw_bar_y1(g_draw_functions_gfx, g_draw_functions_mem, y1, col)
  end
end

local function print_outlined_direct(gfx, mem, t, x, y, c, a)
  hud.hud_print_outlined(gfx, mem, t, x, y, c, a)
end

-- `a1` is often a number: substituting "" for one blanks the year and tech counter of the top bar.
local function print_outlined_with(e, print)
  local a1 = draw_functions_arg(e, 1)
  local owned = nil
  if is_number(e, 1) then
    owned = str.p8_tostr_fixed(number(e, 1))
  end
  local t
  if a1 ~= nil and a1.text ~= nil then
    t = a1.text
  elseif owned ~= nil then
    t = owned
  else
    t = ""
  end
  local x = number(e, 2)
  local y = number(e, 3)
  local c = fixed.p8_fixed_flr_int(number(e, 4))
  local a = is_number(e, 5) and number(e, 5) or fixed.p8_fixed_from_int(0)
  print(g_draw_functions_gfx, g_draw_functions_mem, t, x, y, c, a)
end

local function print_outlined_entry(e)
  print_outlined_with(e, print_outlined_direct)
end

local function print_outlined_text_image_entry(e)
  print_outlined_with(e, hud.hud_print_outlined_text_image)
end

local function draw_help_frame_entry(e)
  hud.hud_draw_help_frame(g_draw_functions_gfx, g_draw_functions_mem, number(e, 1), number(e, 2), number(e, 3))
end

local function spr_entry(e)
  local n = fixed.p8_fixed_flr_int(number(e, 1))
  local x = fixed.p8_fixed_flr_int(number(e, 2))
  local y = fixed.p8_fixed_flr_int(number(e, 3))
  if is_number(e, 4) and is_number(e, 5) then
    local w = fixed.p8_fixed_flr_int(number(e, 4))
    local h = fixed.p8_fixed_flr_int(number(e, 5))
    gfx_module.p8_spr(g_draw_functions_gfx, g_draw_functions_mem, n, x, y, w, h)
  else
    gfx_module.p8_spr_default(g_draw_functions_gfx, g_draw_functions_mem, n, x, y)
  end
end

M.draw_functions_by_id = {
  [map_tables.DRAW_FUNCTION_DRAW_BAR] = draw_bar_entry,
  [map_tables.DRAW_FUNCTION_PRINT_OUTLINED] = print_outlined_entry,
  [map_tables.DRAW_FUNCTION_DRAW_HELP_FRAME] = draw_help_frame_entry,
  [map_tables.DRAW_FUNCTION_SPR] = spr_entry,
}
local draw_functions_by_id = M.draw_functions_by_id

local function draw_rows_dispatch_one(gfx, mem, e, print_outlined)
  local fn

  assert(e.draw_function >= 0 and e.draw_function < map_tables.DRAW_FUNCTION_COUNT,
    "a row's draw_function is one of draw_function_t")
  assert(e.draw_function ~= map_tables.DRAW_FUNCTION_DRAW_ROW, "a layout row is drawn by its message box")
  if e.draw_function == map_tables.DRAW_FUNCTION_PRINT_OUTLINED then
    fn = print_outlined
  else
    fn = draw_functions_by_id[e.draw_function]
  end
  g_draw_functions_gfx = gfx
  g_draw_functions_mem = mem

  if fn ~= nil then
    fn(e)
  end
end

function M.draw_functions_draw_row(gfx, mem, row)
  draw_rows_dispatch_one(gfx, mem, row, print_outlined_entry)
end

-- draw_functions_draw_row's contract, a PRINT_OUTLINED row through hud_print_outlined_text_image.
function M.draw_functions_draw_row_text_image(gfx, mem, row)
  draw_rows_dispatch_one(gfx, mem, row, print_outlined_text_image_entry)
end

function M.draw_functions_draw_rows_default(gfx, mem, rb, n)
  for i = 1, n do
    draw_rows_dispatch_one(gfx, mem, rb[i], print_outlined_entry)
  end
end

return M
