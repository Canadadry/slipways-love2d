-- A tuple's scalars and a row's items are read up to count only.
-- Scalars, tuples and cells are by value and never changed after their constructor: passing or
-- storing one shares the table.
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity_class = require("engine.entity_class")
local vec = require("engine.vec")
local hud = require("game.hud")
local map_tables = require("game.map_tables")
local text_image = require("game.text_image")
local cint = require("p8.cint")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

M.ROW_ITEM_NONE = 0
M.ROW_ITEM_TEXT = 1
M.ROW_ITEM_SPRITE = 2

M.ROW_CELL_NOTHING = 0
M.ROW_CELL_FIXED = 1
M.ROW_CELL_STRING = 2
M.ROW_CELL_TUPLE = 3
M.ROW_CELL_TRUE = 4

M.ROW_SCALAR_NOTHING = 0
M.ROW_SCALAR_FIXED = 1

local g_layout_up = false
local g_row_serial = 0

-- A NUL byte ends the text.
local function c_strlen(s)
  return (string.find(s, "\0", 1, true) or #s + 1) - 1
end

function M.row_scalar_nothing()
  local scalar = { kind = M.ROW_SCALAR_NOTHING, value = 0 }
  return scalar
end

function M.row_scalar_fixed(x)
  local scalar = { kind = M.ROW_SCALAR_FIXED, value = x }
  return scalar
end

function M.row_cell_nothing()
  local cell = { kind = M.ROW_CELL_NOTHING, as = {} }
  return cell
end

function M.row_cell_fixed(x)
  local cell = { kind = M.ROW_CELL_FIXED, as = { fixed = x } }
  return cell
end

function M.row_cell_string(s)
  local cell = { kind = M.ROW_CELL_STRING, as = { text = s } }

  assert(s ~= nil,
    "layout.row_cell_string: no producer writes a NULL string; its kind would have no reader in row_measure_item")
  return cell
end

function M.row_cell_tuple(tuple)
  local cell = { kind = M.ROW_CELL_TUPLE, as = { tuple = tuple } }

  assert(tuple.count >= 0, "layout.row_cell_tuple: a negative count")
  return cell
end

local function tuple_zero()
  return { count = 0, scalars = {} }
end

local function tuple_scalar_of_item(item)
  assert(item.row == nil and item.text == nil,
    "a tuple holds numbers only; a text one needs a pointer in the cell -- stop, measure first")
  if item.is_false or item.is_nil then
    return M.row_scalar_nothing()
  end
  if item.is_fixed then
    return M.row_scalar_fixed(item.fixed)
  end
  return M.row_scalar_fixed(fixed.from_int(item.number))
end

-- Copies the row's scalars: the cell keeps nothing of `row`. nil gives a nothing cell.
function M.row_cell_row(row)
  local tuple = tuple_zero()

  if row == nil then
    return M.row_cell_nothing()
  end
  if row.resource ~= nil then
    tuple.count = 2
    tuple.scalars[1] = M.row_scalar_fixed(row.resource)
    tuple.scalars[2] = M.row_scalar_fixed(fixed.from_int(5))
    return M.row_cell_tuple(tuple)
  end
  tuple.count = row.count
  for i = 1, row.count do
    tuple.scalars[i] = tuple_scalar_of_item(row.items[i])
  end
  return M.row_cell_tuple(tuple)
end

function M.row_cell_item(item)
  if item.row ~= nil then
    return M.row_cell_row(item.row)
  end
  if item.text ~= nil then
    return M.row_cell_string(item.text)
  end
  if item.is_false or item.is_nil then
    return M.row_cell_nothing()
  end
  if item.is_fixed then
    return M.row_cell_fixed(item.fixed)
  end
  return M.row_cell_fixed(fixed.from_int(item.number))
end

local function row_scalar_item(item)
  assert(item.row == nil, "a tuple or a line holds scalar items only; a row inside one has no reader here")
  return M.row_cell_item(item)
end

local function row_copy_text(s)
  return s:sub(1, c_strlen(s))
end

-- Resets every field of the caller's item in place.
local function item_zero(item)
  item.kind = M.ROW_ITEM_NONE
  item.text = nil
  item.color = 0
  item.sprite_id = 0
  item.sprite_palette_block = 0
  item.offset = vec.make(0, 0)
end

-- Positions 1-4 are sprite, width, palette block and spacing; nil for a nothing scalar or one past count.
local function tuple_fixed_at(tuple, i)
  local scalar = i <= tuple.count and tuple.scalars[i] or nil
  if scalar ~= nil and scalar.kind == M.ROW_SCALAR_FIXED then
    return scalar.value
  end
  return nil
end

local function row_measure_item(self, cell, out_item, out_w, out_spacing)
  local width = fixed.from_int(6)
  local palette_block = fixed.from_int(0)
  local have_spacing = false
  local spacing = fixed.from_int(0)

  item_zero(out_item)
  if cell.kind == M.ROW_CELL_TUPLE then
    local t = cell.as.tuple
    local sprite = tuple_fixed_at(t, 1)

    if sprite ~= nil then
      cell = M.row_cell_fixed(sprite)
    else
      cell = M.row_cell_nothing()
    end
    width = tuple_fixed_at(t, 2) or width
    palette_block = tuple_fixed_at(t, 3) or palette_block
    if tuple_fixed_at(t, 4) ~= nil then
      have_spacing = true
      spacing = tuple_fixed_at(t, 4)
    end
  end

  if cell.kind == M.ROW_CELL_STRING then
    local c = fixed.from_int(6)

    if self ~= nil and self.color ~= nil then
      c = self.color
    end
    out_item.kind = M.ROW_ITEM_TEXT
    out_item.text = cell.as.text
    out_item.color = c
    out_w = fixed.from_int(c_strlen(cell.as.text) * 4 + 3)
    out_spacing = fixed.from_int(1)
    return out_w, out_spacing
  end

  if cell.kind ~= M.ROW_CELL_NOTHING then
    out_item.kind = M.ROW_ITEM_SPRITE
    if cell.kind == M.ROW_CELL_FIXED then
      out_item.sprite_id = cell.as.fixed
    else
      out_item.sprite_id = fixed.from_int(0)
    end
    out_item.sprite_palette_block = palette_block
  else
    out_item.kind = M.ROW_ITEM_NONE
  end
  out_w = width
  if have_spacing then
    out_spacing = spacing
  else
    out_spacing = fixed.from_int(2)
  end
  return out_w, out_spacing
end

local function layout_row_construct(self, cells, n)
  local w = fixed.from_int(0)
  local fs = fixed.from_int(0)

  self.placed_items = {}
  for i = 1, n do
    local w_i = fixed.from_int(0)
    local spacing_i = fixed.from_int(0)
    local x = w

    self.placed_items[i] = {}
    w_i, spacing_i = row_measure_item(self, cells[i], self.placed_items[i], w_i, spacing_i)
    if self.placed_items[i].kind == M.ROW_ITEM_TEXT then
      self.placed_items[i].text = row_copy_text(self.placed_items[i].text)
    end
    self.placed_items[i].offset = vec.make(x, fixed.from_int(0))
    w = fixed.add(w, fixed.add(w_i, spacing_i))
    fs = spacing_i
  end
  self.placed_items_count = n
  self.width = fixed.sub(w, fs)
end

local function row_draw_at_item_draw(gfx, mem, item, p)
  local pos = vec.add(p, item.offset)

  if item.kind == M.ROW_ITEM_TEXT then
    hud.hud_print_outlined_default(gfx, mem, item.text, fixed.add(pos.x, fixed.from_int(2)),
      fixed.add(pos.y, fixed.from_int(1)),
      fixed.flr_int(item.color))
  elseif item.kind == M.ROW_ITEM_SPRITE then
    hud.hud_select_palette_block(mem, item.sprite_palette_block)
    gfx_module.spr_default(gfx, mem, fixed.flr_int(item.sprite_id),
      fixed.flr_int(pos.x),
      fixed.flr_int(pos.y))
    hud.hud_select_palette_block_default(mem)
  end
end

function M.row_draw_at(gfx, mem, self, p, a)
  local w

  if self == nil then
    return
  end
  assert(self.class == M.row_class_def,
    "layout.row_draw_at: every row reaching it (a planet's or a button's layout_row, a button's side_tooltip, "
    .. "a tooltip's rows, a message box's layout-row article) is a layout row; anything else would be read through "
    .. "the wrong layout")
  w = self.width
  p = vec.sub(p, vec.make(fixed.mul(a, w), fixed.from_int(0)))

  if self.placed_items == nil then
    return
  end
  for i = 1, self.placed_items_count do
    row_draw_at_item_draw(gfx, mem, self.placed_items[i], p)
  end
end

-- layout.row_draw_at's contract (same pixels, same draw palette after), each text item through
-- text_image.print_outlined. gfx's camera moves during a build and is restored.
function M.row_draw_at_text_image(gfx, mem, self, p, a)
  if self == nil then
    return
  end
  assert(self.class == M.row_class_def, "layout.row_draw_at_text_image: not a layout row")
  p = vec.sub(p, vec.make(fixed.mul(a, self.width), fixed.from_int(0)))
  if self.placed_items == nil then
    return
  end
  for i = 1, self.placed_items_count do
    local item = self.placed_items[i]
    if item.kind ~= M.ROW_ITEM_TEXT then
      row_draw_at_item_draw(gfx, mem, item, p)
      goto continue
    end
    do
      local pos = vec.add(p, item.offset)
      text_image.print_outlined(gfx, mem, item.text,
        fixed.flr_int(fixed.add(pos.x, fixed.from_int(2))),
        fixed.flr_int(fixed.add(pos.y, fixed.from_int(1))),
        bit.band(fixed.flr_int(item.color), 0x0F))
    end
    ::continue::
  end
end

function M.row_draw_at_default(gfx, mem, self, p)
  M.row_draw_at(gfx, mem, self, p, fixed.from_int(0))
end

local function keys_zero()
  return {
    items = {},
    count = 0,
    background_is_bool = false,
  }
end

local function tooltip_line(item)
  local keys = keys_zero()

  if item.text ~= nil then
    M.row_keys_append(keys, M.row_cell_string(item.text))
    return M.row_new(keys)
  end
  if item.row == nil then
    return M.row_new(nil)
  end
  assert(item.row.draw_function == map_tables.DRAW_FUNCTION_NONE,
    "tooltip_line: a line's only named key is its color; a draw_function has no member -- stop, do not add one")
  for i = 1, item.row.count do
    M.row_keys_append(keys, row_scalar_item(item.row.items[i]))
  end
  if item.row.color ~= nil then
    keys.color = fixed.from_int(item.row.color)
  end
  return M.row_new(keys)
end

local function tooltip_construct(self, row)
  local n = row ~= nil and row.count or 0

  -- A table even for no line: tooltip_draw_at() skips a nil `rows` but draws an empty tooltip's frame.
  self.rows = {}
  for i = 1, n do
    self.rows[i] = tooltip_line(row.items[i])
  end
  self.rows_count = n
end

function M.tooltip_draw_at(gfx, mem, self, om, a)
  local h
  local dn

  if self == nil or self.rows == nil then
    return
  end

  h = fixed.add(fixed.mul(fixed.from_int(self.rows_count), fixed.from_int(8)),
    fixed.from_int(2))
  om = fixed.sub(om, fixed.mul(h, a))
  dn = fixed.add(om, h)

  gfx_module.fillp(gfx, 0x0F0F)
  hud.hud_draw_bar(gfx, mem, om, dn, 0x10)
  gfx_module.fillp_reset(gfx)

  for i = 1, self.rows_count do
    local pos = vec.make(fixed.from_int(cint.div(vram.SCREEN_WIDTH, 2)),
      fixed.add(om, fixed.from_int(2)))
    M.row_draw_at_text_image(gfx, mem, self.rows[i], pos, fixed.LIT(0x00008000, 0.5))
    om = fixed.add(om, fixed.from_int(8))
  end
end

local function layout_row_draw_at_method(self, p, a)
  M.row_draw_at(dispatch_context.gfx(), dispatch_context.mem(), self, p, a)
end

local function tooltip_draw_at_method(self, p, a)
  M.tooltip_draw_at(dispatch_context.gfx(), dispatch_context.mem(), self,
    fixed.from_int(0), a)
end

M.row_class_def = {
  draw_at = layout_row_draw_at_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function row_struct_init(self)
  entity_class.init_header(self, M.row_class_def)
  self.color = nil
  self.background = nil
  self.background_is_bool = false
  self.score = nil
  self.placed_items = nil
  self.placed_items_count = 0
  self.width = 0
  self.serial = 0
end

local function row_fill(self, keys)
  -- Wraps to 0 after 2^32 - 1, then skips to 1.
  g_row_serial = (g_row_serial + 1) % 4294967296
  if g_row_serial == 0 then
    g_row_serial = 1
  end
  self.serial = g_row_serial
  if keys == nil then
    layout_row_construct(self, nil, 0)
    return
  end
  self.color = keys.color
  if keys.background ~= nil then
    self.background = keys.background
    self.background_is_bool = keys.background_is_bool
  end
  self.score = keys.score
  if keys.pos ~= nil then
    entity_class.set_pos(self, keys.pos)
  end
  layout_row_construct(self, keys.items, keys.count)
end

local function row_create(keys)
  local self = {}

  row_struct_init(self)
  row_fill(self, keys)
  return self
end

-- color, background and score stay nil when the keys lack them.
function M.row_init(self, keys)
  row_struct_init(self)
  row_fill(self, keys)
end

function M.row_keys_append(keys, cell)
  local n = keys.count + 1
  keys.items[n] = cell
  keys.count = n
end

function M.row_keys_of_row(row)
  local keys = keys_zero()

  assert(row.draw_function == map_tables.DRAW_FUNCTION_NONE and row.resource == nil,
    "layout.row_keys_of_row: a row passes positional items and a color; a draw_function or a resource has no member "
    .. "-- stop, do not add one")
  for i = 1, row.count do
    M.row_keys_append(keys, M.row_cell_item(row.items[i]))
  end
  if row.color ~= nil then
    keys.color = fixed.from_int(row.color)
  end
  return keys
end

M.tooltip_class_def = {
  draw_at = tooltip_draw_at_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function tooltip_struct_alloc()
  local self = {}

  entity_class.init_header(self, M.tooltip_class_def)
  self.rows = nil
  self.rows_count = 0
  return self
end

local function tooltip_create(row)
  local self

  assert(row == nil or (row.color == nil and row.draw_function == map_tables.DRAW_FUNCTION_NONE),
    "tooltip_create: every line is positional; a named key has no member -- stop, do not add one")
  self = tooltip_struct_alloc()
  tooltip_construct(self, row)
  return self
end

function M.init()
  M.shutdown()
  g_layout_up = true
end

function M.shutdown()
  g_layout_up = false
end

function M.row_ready()
  return g_layout_up
end

function M.tooltip_ready()
  return g_layout_up
end

function M.row_class()
  return M.row_class_def
end

function M.tooltip_class()
  return M.tooltip_class_def
end

function M.row_new(keys)
  return row_create(keys)
end

function M.tooltip_new(row)
  return tooltip_create(row)
end

-- A new table each call. color, background, score and pos stay nil when absent; background_is_bool
-- is read only with a background.
function M.row_keys_zero()
  return keys_zero()
end

return M
