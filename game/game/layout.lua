-- Copy of src/game/layout.h then src/game/layout.c.
--
-- Shapes, with the C field names:
--   layout_row_item_t: { kind, text, color, sprite_id, sprite_palette_block, offset }.
--   layout_row_scalar_t: { kind, value }.
--   layout_row_tuple_t: { count, scalars }, scalars 0-indexed of LAYOUT_ROW_TUPLE_CAP.
--   layout_row_cell_t: { kind, as }, the union as a table holding the member the kind uses
--     (fixed, text or tuple).
--   layout_row_keys_t: { items, count, has_color, color, has_background, background_is_bool,
--     background, has_score, score, has_pos, pos }, items 0-indexed of LAYOUT_ROW_KEYS_CAP cells.
--   layout_row_t: one table holding the entity header and { zone, color, has_color, background,
--     has_background, background_is_bool, score, has_score, placed_items, placed_items_count,
--     width, serial }, placed_items 0-indexed.
--   tooltip_t: one table holding the entity header and { zone, rows, rows_count }, rows 0-indexed.
-- Scalars, tuples and cells are by value and never changed after their constructor: passing or
-- storing one shares the table.
local bit = require("bit")
local allocator = require("engine.allocator")
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

M.LAYOUT_ROW_ITEM_NONE = 0
M.LAYOUT_ROW_ITEM_TEXT = 1
M.LAYOUT_ROW_ITEM_SPRITE = 2

M.LAYOUT_ROW_CELL_NOTHING = 0
M.LAYOUT_ROW_CELL_FIXED = 1
M.LAYOUT_ROW_CELL_STRING = 2
M.LAYOUT_ROW_CELL_TUPLE = 3
M.LAYOUT_ROW_CELL_TRUE = 4

M.LAYOUT_ROW_SCALAR_NOTHING = 0
M.LAYOUT_ROW_SCALAR_FIXED = 1

M.LAYOUT_ROW_TUPLE_CAP = 4

M.LAYOUT_ROW_KEYS_CAP = 16

M.SIZEOF = {
  layout_row_item_t = 40,
  layout_row_t = 144,
  ["layout_row_t *"] = 8,
  tooltip_t = 104,
}
local SIZEOF = M.SIZEOF

local g_layout_up = 0
local g_row_serial = 0

-- strlen: a NUL byte ends the text.
local function c_strlen(s)
  return (string.find(s, "\0", 1, true) or #s + 1) - 1
end

function M.layout_row_scalar_nothing()
  local scalar = { kind = M.LAYOUT_ROW_SCALAR_NOTHING, value = 0 }
  return scalar
end

function M.layout_row_scalar_fixed(x)
  local scalar = { kind = M.LAYOUT_ROW_SCALAR_FIXED, value = x }
  return scalar
end

function M.layout_row_cell_nothing()
  local cell = { kind = M.LAYOUT_ROW_CELL_NOTHING, as = {} }
  return cell
end

function M.layout_row_cell_fixed(x)
  local cell = { kind = M.LAYOUT_ROW_CELL_FIXED, as = { fixed = x } }
  return cell
end

function M.layout_row_cell_string(s)
  local cell = { kind = M.LAYOUT_ROW_CELL_STRING, as = { text = s } }

  assert(s ~= nil,
    "layout_row_cell_string: no producer writes a NULL string; its kind would have no reader in row_measure_item")
  return cell
end

function M.layout_row_cell_tuple(tuple)
  local cell = { kind = M.LAYOUT_ROW_CELL_TUPLE, as = { tuple = tuple } }

  assert(tuple.count >= 0 and tuple.count <= M.LAYOUT_ROW_TUPLE_CAP,
    "layout_row_cell_tuple: row_measure_item reads four scalars at most")
  return cell
end

-- layout_row_tuple_t tuple = {0}.
local function tuple_zero()
  local scalars = {}
  for i = 0, M.LAYOUT_ROW_TUPLE_CAP - 1 do
    scalars[i] = M.layout_row_scalar_nothing()
  end
  return { count = 0, scalars = scalars }
end

local function tuple_scalar_of_item(item)
  assert(item.row == nil and item.text == nil,
    "a tuple holds numbers only; a text one needs a pointer in the cell -- stop, measure first")
  if item.is_false ~= 0 or item.is_nil ~= 0 then
    return M.layout_row_scalar_nothing()
  end
  if item.is_fixed ~= 0 then
    return M.layout_row_scalar_fixed(item.fixed)
  end
  return M.layout_row_scalar_fixed(fixed.p8_fixed_from_int(item.number))
end

-- Copies the row's scalars: the cell keeps nothing of `row`. NULL gives a nothing cell.
function M.layout_row_cell_row(row)
  local tuple = tuple_zero()

  if row == nil then
    return M.layout_row_cell_nothing()
  end
  if row.has_resource ~= 0 then
    tuple.count = 2
    tuple.scalars[0] = M.layout_row_scalar_fixed(row.resource)
    tuple.scalars[1] = M.layout_row_scalar_fixed(fixed.p8_fixed_from_int(5))
    return M.layout_row_cell_tuple(tuple)
  end
  assert(row.count <= M.LAYOUT_ROW_TUPLE_CAP,
    "layout_row_cell_row: a tuple has four scalars at most; row_measure_item never read a fifth -- stop, do not widen")
  tuple.count = row.count
  for i = 0, row.count - 1 do
    tuple.scalars[i] = tuple_scalar_of_item(row.items[i])
  end
  return M.layout_row_cell_tuple(tuple)
end

function M.layout_row_cell_item(item)
  if item.row ~= nil then
    return M.layout_row_cell_row(item.row)
  end
  if item.text ~= nil then
    return M.layout_row_cell_string(item.text)
  end
  if item.is_false ~= 0 or item.is_nil ~= 0 then
    return M.layout_row_cell_nothing()
  end
  if item.is_fixed ~= 0 then
    return M.layout_row_cell_fixed(item.fixed)
  end
  return M.layout_row_cell_fixed(fixed.p8_fixed_from_int(item.number))
end

local function row_scalar_item(item)
  assert(item.row == nil, "a tuple or a line holds scalar items only; a row inside one has no reader here")
  return M.layout_row_cell_item(item)
end

local function row_copy_text(zone, s)
  local n = c_strlen(s) + 1

  allocator.allocator_alloc(zone, n)
  return s:sub(1, n - 1)
end

-- memset(out_item, 0, sizeof *out_item) on the caller's table.
local function item_zero(item)
  item.kind = M.LAYOUT_ROW_ITEM_NONE
  item.text = nil
  item.color = 0
  item.sprite_id = 0
  item.sprite_palette_block = 0
  item.offset = vec.vec_make(0, 0)
end

local function row_measure_item(self, cell, out_item, out_w, out_spacing)
  local width = fixed.p8_fixed_from_int(6)
  local palette_block = fixed.p8_fixed_from_int(0)
  local have_spacing = 0
  local spacing = fixed.p8_fixed_from_int(0)

  -- Zones never clear memory: without this, the fields a kind leaves unset hold stale bytes.
  item_zero(out_item)
  if cell.kind == M.LAYOUT_ROW_CELL_TUPLE then
    local t = cell.as.tuple
    local f = {}

    for i = 0, M.LAYOUT_ROW_TUPLE_CAP - 1 do
      if i < t.count then
        f[i] = t.scalars[i]
      else
        f[i] = M.layout_row_scalar_nothing()
      end
    end
    if f[0].kind == M.LAYOUT_ROW_SCALAR_FIXED then
      cell = M.layout_row_cell_fixed(f[0].value)
    else
      cell = M.layout_row_cell_nothing()
    end
    if f[1].kind == M.LAYOUT_ROW_SCALAR_FIXED then
      width = f[1].value
    end
    if f[2].kind == M.LAYOUT_ROW_SCALAR_FIXED then
      palette_block = f[2].value
    end
    if f[3].kind == M.LAYOUT_ROW_SCALAR_FIXED then
      have_spacing = 1
      spacing = f[3].value
    end
  end

  if cell.kind == M.LAYOUT_ROW_CELL_STRING then
    local c = fixed.p8_fixed_from_int(6)

    if self ~= nil and self.has_color ~= 0 then
      c = self.color
    end
    out_item.kind = M.LAYOUT_ROW_ITEM_TEXT
    out_item.text = cell.as.text
    out_item.color = c
    out_w = fixed.p8_fixed_from_int(c_strlen(cell.as.text) * 4 + 3)
    out_spacing = fixed.p8_fixed_from_int(1)
    return out_w, out_spacing
  end

  if cell.kind ~= M.LAYOUT_ROW_CELL_NOTHING then
    out_item.kind = M.LAYOUT_ROW_ITEM_SPRITE
    if cell.kind == M.LAYOUT_ROW_CELL_FIXED then
      out_item.sprite_id = cell.as.fixed
    else
      out_item.sprite_id = fixed.p8_fixed_from_int(0)
    end
    out_item.sprite_palette_block = palette_block
  else
    out_item.kind = M.LAYOUT_ROW_ITEM_NONE
  end
  out_w = width
  if have_spacing ~= 0 then
    out_spacing = spacing
  else
    out_spacing = fixed.p8_fixed_from_int(2)
  end
  return out_w, out_spacing
end

local function layout_row_construct(self, cells, n)
  local w = fixed.p8_fixed_from_int(0)
  local fs = fixed.p8_fixed_from_int(0)

  self.placed_items = allocator.allocator_alloc(self.zone, (n > 0 and n or 1) * SIZEOF.layout_row_item_t)
  for i = 0, n - 1 do
    local w_i = fixed.p8_fixed_from_int(0)
    local spacing_i = fixed.p8_fixed_from_int(0)
    local x = w

    self.placed_items[i] = {}
    w_i, spacing_i = row_measure_item(self, cells[i], self.placed_items[i], w_i, spacing_i)
    if self.placed_items[i].kind == M.LAYOUT_ROW_ITEM_TEXT then
      self.placed_items[i].text = row_copy_text(self.zone, self.placed_items[i].text)
    end
    self.placed_items[i].offset = vec.vec_make(x, fixed.p8_fixed_from_int(0))
    w = fixed.p8_fixed_add(w, fixed.p8_fixed_add(w_i, spacing_i))
    fs = spacing_i
  end
  self.placed_items_count = n
  self.width = fixed.p8_fixed_sub(w, fs)
end

local function row_draw_at_item_draw(gfx, mem, item, p)
  local pos = vec.vec_add(p, item.offset)

  if item.kind == M.LAYOUT_ROW_ITEM_TEXT then
    hud.hud_print_outlined_default(gfx, mem, item.text, fixed.p8_fixed_add(pos.x, fixed.p8_fixed_from_int(2)),
      fixed.p8_fixed_add(pos.y, fixed.p8_fixed_from_int(1)),
      fixed.p8_fixed_flr_int(item.color))
  elseif item.kind == M.LAYOUT_ROW_ITEM_SPRITE then
    hud.hud_select_palette_block(mem, item.sprite_palette_block)
    gfx_module.p8_spr_default(gfx, mem, fixed.p8_fixed_flr_int(item.sprite_id),
      fixed.p8_fixed_flr_int(pos.x),
      fixed.p8_fixed_flr_int(pos.y))
    hud.hud_select_palette_block_default(mem)
  end
end

function M.layout_row_draw_at(gfx, mem, self, p, a)
  local w

  if self == nil then
    return
  end
  assert(self.class == M.layout_row_class_def,
    "layout_row_draw_at: every row reaching it (a planet's or a button's layout_row, a button's side_tooltip, "
    .. "a tooltip's rows, a message box's layout-row article) is a layout row; anything else would be read through "
    .. "the wrong layout")
  w = self.width
  p = vec.vec_sub(p, vec.vec_make(fixed.p8_fixed_mul(a, w), fixed.p8_fixed_from_int(0)))

  if self.placed_items == nil then
    return
  end
  for i = 0, self.placed_items_count - 1 do
    row_draw_at_item_draw(gfx, mem, self.placed_items[i], p)
  end
end

-- layout_row_draw_at's contract (same pixels, same draw palette after), each text item through
-- text_image_print_outlined. gfx's camera moves during a build and is restored.
function M.layout_row_draw_at_text_image(gfx, mem, self, p, a)
  if self == nil then
    return
  end
  assert(self.class == M.layout_row_class_def, "layout_row_draw_at_text_image: not a layout row")
  p = vec.vec_sub(p, vec.vec_make(fixed.p8_fixed_mul(a, self.width), fixed.p8_fixed_from_int(0)))
  if self.placed_items == nil then
    return
  end
  for i = 0, self.placed_items_count - 1 do
    local item = self.placed_items[i]
    if item.kind ~= M.LAYOUT_ROW_ITEM_TEXT then
      row_draw_at_item_draw(gfx, mem, item, p)
      goto continue
    end
    do
      local pos = vec.vec_add(p, item.offset)
      text_image.text_image_print_outlined(gfx, mem, item.text,
        fixed.p8_fixed_flr_int(fixed.p8_fixed_add(pos.x, fixed.p8_fixed_from_int(2))),
        fixed.p8_fixed_flr_int(fixed.p8_fixed_add(pos.y, fixed.p8_fixed_from_int(1))),
        bit.band(fixed.p8_fixed_flr_int(item.color), 0x0F))
    end
    ::continue::
  end
end

function M.layout_row_draw_at_default(gfx, mem, self, p)
  M.layout_row_draw_at(gfx, mem, self, p, fixed.p8_fixed_from_int(0))
end

-- layout_row_keys_t keys = {0}.
local function keys_zero()
  local items = {}
  for i = 0, M.LAYOUT_ROW_KEYS_CAP - 1 do
    items[i] = M.layout_row_cell_nothing()
  end
  return {
    items = items,
    count = 0,
    has_color = 0,
    color = 0,
    has_background = 0,
    background_is_bool = 0,
    background = 0,
    has_score = 0,
    score = 0,
    has_pos = 0,
    pos = vec.vec_make(0, 0),
  }
end

local function tooltip_line(zone, item)
  local keys = keys_zero()

  if item.text ~= nil then
    M.layout_row_keys_append(keys, M.layout_row_cell_string(item.text))
    return M.layout_row_new(zone, keys)
  end
  if item.row == nil then
    return M.layout_row_new(zone, nil)
  end
  assert(item.row.draw_function == map_tables.DRAW_FUNCTION_NONE,
    "tooltip_line: a line's only named key is its color; a draw_function has no member -- stop, do not add one")
  for i = 0, item.row.count - 1 do
    M.layout_row_keys_append(keys, row_scalar_item(item.row.items[i]))
  end
  keys.has_color = item.row.has_color
  keys.color = fixed.p8_fixed_from_int(item.row.color)
  return M.layout_row_new(zone, keys)
end

local function tooltip_construct(self, row)
  local n = row ~= nil and row.count or 0

  -- Allocated even for no line: tooltip_draw_at() skips a NULL `rows` but draws an empty tooltip's frame.
  self.rows = allocator.allocator_alloc(self.zone, (n > 0 and n or 1) * SIZEOF["layout_row_t *"])
  for i = 0, n - 1 do
    self.rows[i] = tooltip_line(self.zone, row.items[i])
  end
  self.rows_count = n
end

function M.tooltip_draw_at(gfx, mem, self, om, a)
  local h
  local dn

  if self == nil or self.rows == nil then
    return
  end

  h = fixed.p8_fixed_add(fixed.p8_fixed_mul(fixed.p8_fixed_from_int(self.rows_count), fixed.p8_fixed_from_int(8)),
    fixed.p8_fixed_from_int(2))
  om = fixed.p8_fixed_sub(om, fixed.p8_fixed_mul(h, a))
  dn = fixed.p8_fixed_add(om, h)

  gfx_module.p8_fillp(gfx, 0x0F0F)
  hud.hud_draw_bar(gfx, mem, om, dn, 0x10)
  gfx_module.p8_fillp_reset(gfx)

  for i = 0, self.rows_count - 1 do
    local pos = vec.vec_make(fixed.p8_fixed_from_int(cint.div(vram.P8_SCREEN_WIDTH, 2)),
      fixed.p8_fixed_add(om, fixed.p8_fixed_from_int(2)))
    M.layout_row_draw_at_text_image(gfx, mem, self.rows[i], pos, fixed.P8_FIXED_LIT(0x00008000, 0.5))
    om = fixed.p8_fixed_add(om, fixed.p8_fixed_from_int(8))
  end
end

local function layout_row_draw_at_method(self, p, a)
  M.layout_row_draw_at(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, p, a)
end

local function tooltip_draw_at_method(self, p, a)
  M.tooltip_draw_at(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self,
    fixed.p8_fixed_from_int(0), a)
end

M.layout_row_class_def = {
  draw_at = layout_row_draw_at_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function row_struct_init(self, zone)
  assert(zone ~= nil, "row_struct_init: a row without a zone would have nowhere to keep its placed items")
  entity_class.entity_init_header(self, M.layout_row_class_def)
  self.color = 0
  self.has_color = 0
  self.background = 0
  self.has_background = 0
  self.background_is_bool = 0
  self.score = 0
  self.has_score = 0
  self.placed_items = nil
  self.placed_items_count = 0
  self.width = 0
  self.serial = 0
  self.zone = zone
end

local function row_fill(self, keys)
  -- uint32_t: wraps to 0 after 2^32 - 1, then skips to 1.
  g_row_serial = (g_row_serial + 1) % 4294967296
  if g_row_serial == 0 then
    g_row_serial = 1
  end
  self.serial = g_row_serial
  if keys == nil then
    layout_row_construct(self, nil, 0)
    return
  end
  if keys.has_color ~= 0 then
    self.color = keys.color
    self.has_color = 1
  end
  if keys.has_background ~= 0 then
    self.background = keys.background
    self.has_background = 1
    self.background_is_bool = keys.background_is_bool
  end
  if keys.has_score ~= 0 then
    self.score = keys.score
    self.has_score = 1
  end
  if keys.has_pos ~= 0 then
    entity_class.entity_set_pos(self, keys.pos)
  end
  layout_row_construct(self, keys.items, keys.count)
end

local function row_create(zone, keys)
  local self

  assert(zone ~= nil, "row_create: a row without a zone would have nowhere to keep its placed items")
  self = allocator.allocator_alloc(zone, SIZEOF.layout_row_t)
  row_struct_init(self, zone)
  row_fill(self, keys)
  return self
end

-- Lays the row out into `self`; its placed items and their texts go in `zone`, which must outlive `self`.
function M.layout_row_init(self, zone, keys)
  row_struct_init(self, zone)
  row_fill(self, keys)
end

function M.layout_row_keys_append(keys, cell)
  assert(keys.count >= 0 and keys.count < M.LAYOUT_ROW_KEYS_CAP,
    "layout_row_keys_append: a row's cells are measured: stop, raise LAYOUT_ROW_KEYS_CAP after measuring")
  keys.items[keys.count] = cell
  keys.count = keys.count + 1
end

function M.layout_row_keys_of_row(row)
  local keys = keys_zero()

  assert(row.draw_function == map_tables.DRAW_FUNCTION_NONE and row.has_resource == 0,
    "layout_row_keys_of_row: a row passes positional items and a color; a draw_function or a resource has no member "
    .. "-- stop, do not add one")
  for i = 0, row.count - 1 do
    M.layout_row_keys_append(keys, M.layout_row_cell_item(row.items[i]))
  end
  keys.has_color = row.has_color
  keys.color = fixed.p8_fixed_from_int(row.color)
  return keys
end

M.tooltip_class_def = {
  draw_at = tooltip_draw_at_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function tooltip_struct_alloc(zone)
  local self

  assert(zone ~= nil, "tooltip_struct_alloc: a tooltip without a zone would have nowhere to keep its lines")
  self = allocator.allocator_alloc(zone, SIZEOF.tooltip_t)

  entity_class.entity_init_header(self, M.tooltip_class_def)
  self.zone = zone
  self.rows = nil
  self.rows_count = 0
  return self
end

local function tooltip_create(zone, row)
  local self

  assert(row == nil or (row.has_color == 0 and row.draw_function == map_tables.DRAW_FUNCTION_NONE),
    "tooltip_create: every line is positional; a named key has no member -- stop, do not add one")
  self = tooltip_struct_alloc(zone)
  tooltip_construct(self, row)
  return self
end

function M.layout_init()
  M.layout_shutdown()
  g_layout_up = 1
end

function M.layout_shutdown()
  g_layout_up = 0
end

function M.layout_row_ready()
  return g_layout_up
end

function M.tooltip_ready()
  return g_layout_up
end

function M.layout_row_class()
  return M.layout_row_class_def
end

function M.tooltip_class()
  return M.tooltip_class_def
end

function M.layout_row_new(zone, keys)
  return row_create(zone, keys)
end

function M.tooltip_new(zone, row)
  return tooltip_create(zone, row)
end

-- Lua only: a new layout_row_keys_t at its C zero, for the keys other modules hold inline.
function M.layout_row_keys_zero()
  return keys_zero()
end

return M
