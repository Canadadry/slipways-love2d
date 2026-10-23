-- Include cycle: game.ui is required at call time (button_draw).
local bit = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local button_image = require("game.button_image")
local game_data = require("game.game_data")
local hud = require("game.hud")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

local function c_strlen(s)
  return (string.find(s, "\0", 1, true) or #s + 1) - 1
end

local g_button_up = false
local g_modal_backdrop_up = false

local function ui_to_int(x)
  return fixed.flr_int(x)
end

local function button_update_method(self, t)
  M.button_update(self)
end

local function button_draw_overlay_method(self, screen_pos)
  M.button_draw_overlay(dispatch_context.gfx(), dispatch_context.mem(), self)
end

local function button_draw_method(self, screen_pos)
  M.button_draw(dispatch_context.gfx(), dispatch_context.mem(), self, screen_pos)
end

local function button_hit_test_method(self, mouse)
  return entity_geom.contains_cursor(self, mouse)
end

local function button_left_press_method(self, mouse)
  return M.button_left_press(self)
end

local function button_has_refresh(self)
  return self.refresh ~= nil
end

local function button_refresh_method(self)
  local js = self

  if js.refresh ~= nil then
    js.refresh(js)
  end
end

M.button_class_def = {
  draw = button_draw_method,
  draw_overlay = button_draw_overlay_method,
  hit_test = button_hit_test_method,
  refresh = button_refresh_method,
  has_refresh = button_has_refresh,
  left_press = button_left_press_method,
  states = { [entity_class.STATE_NORMAL] = button_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

-- slide_target, trade, type_bias and project_cost stay nil when the keys do not give them.
local function button_create(keys)
  local self = {}

  entity.init_instance(self, M.button_class_def, game_data.button_archetype.state,
    game_data.button_archetype.state_time)
  self.layout_row = nil
  self.width = 0
  self.side_tooltip = nil
  self.tooltip = nil
  self.on_click = nil
  self.is_hovered = false
  self.refresh = nil
  self.name = nil
  self.technology = nil
  self.project_planet = nil
  self.project = nil
  self.archetype = game_data.button_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  if keys ~= nil then
    if keys.layer ~= nil then
      entity_registry.set_layer(self, keys.layer)
    end
    if keys.pos ~= nil then
      entity_class.set_pos(self, keys.pos)
    end
    if keys.slide_target ~= nil then
      self.slide_target = vec.make(keys.slide_target.x, keys.slide_target.y)
    end
    self.on_click = keys.on_click
    self.refresh = keys.refresh
    self.name = keys.name
    self.trade = keys.trade
    self.type_bias = keys.type_bias
    self.technology = keys.technology
    self.project_planet = keys.project_planet
    self.project = keys.project
    self.project_cost = keys.project_cost
  end
  M.button_construct(self, keys)
  entity.construct(self)
  return self
end

function M.button_init()
  M.button_shutdown()
  g_button_up = true
end

function M.button_shutdown()
  g_button_up = false
end

function M.button_ready()
  return g_button_up
end

function M.button_class()
  return M.button_class_def
end

-- Copies `row` into the keys; nil leaves the tooltip row absent.
function M.button_keys_set_tooltip_row(keys, row)
  keys.tooltip_row = nil
  if row ~= nil then
    keys.tooltip_row = map_tables.make_data_row({
      count = row.count,
      items = row.items,
      color = row.color,
      draw_function = row.draw_function,
      resource = row.resource,
    })
  end
end

function M.button_new(keys)
  return button_create(keys)
end

-- The keys used when none are given: built once by button_keys_zero at the end of the file, never written.
local none

function M.button_construct(self, keys)
  if self == nil then
    return
  end
  if keys == nil then
    keys = none
  end

  self.layout_row = layout.layout_row_new(keys.layout_row_keys)
  self.width = fixed.add(self.layout_row.width, fixed.from_int(2))
  self.hit_box = { left = fixed.from_int(0), top = fixed.from_int(-1), right = self.width,
    bottom = fixed.from_int(11) }

  if keys.side_tooltip_row ~= nil then
    self.side_tooltip = layout.layout_row_new(keys.side_tooltip_row)
  end
  if keys.tooltip_row ~= nil then
    self.tooltip = layout.tooltip_new(keys.tooltip_row)
  end
end

function M.button_update(self)
  if self == nil then
    return
  end

  if self.slide_target ~= nil then
    local qf = self.slide_target
    local ra = entity_class.pos(self)
    local d = vec.sub(qf, ra)
    local length_squared = vec.length_squared(d)
    local delta
    if length_squared <= fixed.from_int(1) then
      delta = d
    else
      delta = vec.mul(d, fixed.LIT(0x00006666, 0.4))
    end
    entity_class.set_pos(self, vec.add(ra, delta))
  end
end

function M.button_draw_overlay(gfx, mem, self)
  if self == nil then
    return
  end

  if self.tooltip ~= nil and self.is_hovered then
    layout.tooltip_draw_at(gfx, mem, self.tooltip, fixed.from_int(vram.SCREEN_HEIGHT - 1),
      fixed.from_int(1))
  end
end

-- Text items go through the text image cache, or are printed directly when through_text_image is false.
local function button_draw_body(graphics, memory, self, draw_pos, body, top, through_text_image)
  local width = self.width
  local right = fixed.sub(fixed.add(draw_pos.x, width), fixed.from_int(1))
  local bottom = fixed.add(draw_pos.y, fixed.from_int(9))
  local left_pixel = ui_to_int(draw_pos.x)
  local top_pixel = ui_to_int(draw_pos.y)
  local right_pixel = ui_to_int(right)
  local bottom_pixel = ui_to_int(bottom)

  gfx_module.rect(graphics, memory, left_pixel - 1, top_pixel, right_pixel + 1, bottom_pixel + 1, 1)
  gfx_module.rectfill(graphics, memory, left_pixel, top_pixel, right_pixel, bottom_pixel, body)
  gfx_module.rectfill(graphics, memory, left_pixel, top_pixel, right_pixel, top_pixel, top)

  if self.layout_row ~= nil then
    local off = vec.add(draw_pos, vec.make(fixed.from_int(1), fixed.from_int(2)))
    if through_text_image then
      layout.layout_row_draw_at_text_image(graphics, memory, self.layout_row, off, fixed.from_int(0))
    else
      layout.layout_row_draw_at(graphics, memory, self.layout_row, off, fixed.from_int(0))
    end
  end

  if self.side_tooltip ~= nil then
    local c = self.side_tooltip
    local bg = 2
    local x2 = fixed.add(right, c.width)

    if c.background ~= nil and not c.background_is_bool then
      bg = ui_to_int(c.background)
    end
    gfx_module.rectfill(graphics, memory, ui_to_int(right) + 3, top_pixel + 1, ui_to_int(x2) + 4, bottom_pixel, bg)

    local off2 = vec.add(draw_pos, vec.make(fixed.add(width, fixed.from_int(3)),
      fixed.from_int(2)))
    if through_text_image then
      layout.layout_row_draw_at_text_image(graphics, memory, c, off2, fixed.from_int(0))
    else
      layout.layout_row_draw_at(graphics, memory, c, off2, fixed.from_int(0))
    end
  end
end

local function is_whole(x)
  return fixed.from_int(ui_to_int(x)) == x
end

-- Returns ok, extent: extent widened to the row's items drawn dx right of the button's border; ok false when
-- an item is off a whole offset or left of the border, so the image could not hold it.
local function row_extent(row, dx, extent)
  if row == nil or row.placed_items == nil then
    return true, extent
  end
  for i = 1, row.placed_items_count do
    local item = row.placed_items[i]
    if not is_whole(item.offset.x) or item.offset.y ~= 0 then
      return false, extent
    end
    local left = dx + ui_to_int(item.offset.x) + 1
    local end_ = left
    if item.kind == layout.LAYOUT_ROW_ITEM_TEXT then
      left = left + 1
      end_ = left + c_strlen(item.text) * gfx_module.FONT_CELL_WIDTH + 1
    elseif item.kind == layout.LAYOUT_ROW_ITEM_SPRITE then
      end_ = left + gfx_module.SPRITE_SIZE
    end
    if left < 0 then
      return false, extent
    end
    if end_ > extent then
      extent = end_
    end
  end
  return true, extent
end

-- The image's width, the border's left column to the rightmost pixel any call writes; 0 when the
-- button cannot be copied exactly.
local function button_image_width(self)
  local ok

  if not is_whole(self.width) then
    return 0
  end
  local width = ui_to_int(self.width)
  local extent = width + 2
  ok, extent = row_extent(self.layout_row, 1, extent)
  if not ok then
    return 0
  end
  local side = self.side_tooltip
  if side ~= nil then
    if not is_whole(side.width) then
      return 0
    end
    local fill_left = width + 3
    local fill_right = width + ui_to_int(side.width) + 4
    if fill_right < fill_left then
      fill_left = fill_right
      fill_right = width + 3
    end
    if fill_left < 0 then
      return 0
    end
    ok, extent = row_extent(side, width + 3, extent)
    if not ok then
      return 0
    end
    if fill_right + 1 > extent then
      extent = fill_right + 1
    end
  end
  return extent
end

local g_building_look = { body = 0, top = 0 }

-- context is the button. Draws no visual random number and reads no game state but its rows.
local function button_image_build_draw(gfx, mem, context)
  button_draw_body(gfx, mem, context, vec.make(fixed.from_int(1), fixed.from_int(0)),
    g_building_look.body, g_building_look.top, false)
end

local function button_draw_image(graphics, memory, self, draw_pos, look)
  if graphics.fillp_pattern ~= 0 or not hud.hud_palette_block_default_in_force(memory) then
    return false
  end
  local width = button_image_width(self)
  if width == 0 then
    return false
  end
  local key = { words = {
    (self.layout_row ~= nil) and self.layout_row.serial or 0,
    (self.side_tooltip ~= nil) and self.side_tooltip.serial or 0,
    bit.bor(look.body, bit.lshift(look.top, 8)),
    ui_to_int(self.width),
  } }
  g_building_look = { body = look.body, top = look.top }
  return button_image.button_image_draw(graphics, memory, key, ui_to_int(draw_pos.x) - 1, ui_to_int(draw_pos.y), width,
    button_image.BUTTON_IMAGE_HEIGHT, button_image_build_draw, self)
end

function M.button_draw(graphics, memory, self, draw_pos)
  local ui = require("game.ui")

  if self == nil then
    return
  end

  local clickable = self.on_click ~= nil
  local hovered = self.is_hovered or (ui.main_ui() ~= nil and ui.ui_pad_tab(ui.main_ui()) == self)
  local look = {
    body = clickable and (hovered and 12 or 13) or 5,
    top = clickable and 6 or 5,
  }

  if not button_draw_image(graphics, memory, self, draw_pos, look) then
    button_draw_body(graphics, memory, self, draw_pos, look.body, look.top, true)
  end
end

function M.button_left_press(self)
  local clickable = self ~= nil and self.on_click ~= nil
  local out = { stop = true, held = nil }

  if clickable then
    self.on_click(self)
  end

  return out
end

local function modal_backdrop_left_press_method(self, mouse)
  return M.modal_backdrop_left_press(self)
end

M.modal_backdrop_class_def = {
  hit_test = entity_registry.hit_test_always,
  left_press = modal_backdrop_left_press_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function modal_backdrop_create(keys)
  local self = {}

  entity.init_instance(self, M.modal_backdrop_class_def, game_data.modal_backdrop_archetype.state,
    game_data.modal_backdrop_archetype.state_time)
  self.archetype = game_data.modal_backdrop_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.on_click = nil
  self.is_dead = false
  self.is_hovered = false
  if keys ~= nil then
    self.on_click = keys.on_click
  end
  entity.construct(self)
  return self
end

function M.modal_backdrop_init()
  M.modal_backdrop_shutdown()
  g_modal_backdrop_up = true
end

function M.modal_backdrop_shutdown()
  g_modal_backdrop_up = false
end

function M.modal_backdrop_ready()
  return g_modal_backdrop_up
end

function M.modal_backdrop_class()
  return M.modal_backdrop_class_def
end

function M.modal_backdrop_new(keys)
  return modal_backdrop_create(keys)
end

function M.modal_backdrop_left_press(self)
  local out = { stop = true, held = nil }

  if self == nil then
    return out
  end
  self.is_dead = false
  if self.on_click ~= nil then
    local dies = self.on_click(self)
    assert(type(dies) == "boolean", "modal_backdrop_left_press: a backdrop's on_click returns whether the backdrop goes")
    self.is_dead = dies
  end
  return out
end

-- Writes every field of keys in place and returns keys; the optional ones (layer, side_tooltip_row,
-- tooltip_row, pos, slide_target, trade, type_bias, project_cost) are left as they are, nil when not given.
function M.button_keys_zero(keys)
  keys.layout_row_keys = layout.layout_row_keys_zero()
  keys.on_click = nil
  keys.refresh = nil
  keys.name = nil
  keys.technology = nil
  keys.project_planet = nil
  keys.project = nil
  return keys
end

function M.button_zero()
  local b = {}
  entity_class.init_header(b, nil)
  b.archetype = nil
  b.layout_row = nil
  b.width = 0
  b.side_tooltip = nil
  b.tooltip = nil
  b.on_click = nil
  b.refresh = nil
  b.name = nil
  b.technology = nil
  b.project_planet = nil
  b.project = nil
  return b
end

none = M.button_keys_zero({})

return M
