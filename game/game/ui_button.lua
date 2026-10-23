-- Copy of src/game/ui_button.h then src/game/ui_button.c.
--
-- Shapes, with the C field names:
--   button_t: one table holding the entity header and its own fields; its own `has`
--     (BUTTON_HAS_* bits) is `has_`, since the header's `has` holds the ENTITY_HAS_* bits.
--   modal_backdrop_t: one table holding the entity header and { archetype, on_click }.
--   button_keys_t: the fields of button_keys_zero; tooltip_row a data_row_t table.
--   button_look_t: { body, top }.
--
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
local research = require("game.research")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")

local M = {}

M.BUTTON_HAS_SLIDE_TARGET = bit.lshift(1, 0)

local function research_project_id_none()
  local none = research.RESEARCH_PROJECT_ID_NONE
  return { index = none.index, epoch = none.epoch }
end

local function c_strlen(s)
  return (string.find(s, "\0", 1, true) or #s + 1) - 1
end

local g_button_up = 0
local g_modal_backdrop_up = 0

local function ui_to_int(x)
  return fixed.p8_fixed_flr_int(x)
end

local function button_update_method(self, t)
  M.button_update(self)
end

local function button_draw_overlay_method(self, screen_pos)
  M.button_draw_overlay(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self)
end

local function button_draw_method(self, screen_pos)
  M.button_draw(dispatch_context.dispatch_context_gfx(), dispatch_context.dispatch_context_mem(), self, screen_pos)
end

local function button_hit_test_method(self, mouse)
  return entity_geom.entity_geom_contains_cursor(self, mouse)
end

local function button_left_press_method(self, mouse)
  return M.button_left_press(self)
end

local function button_has_refresh(self)
  return (self.refresh ~= nil) and 1 or 0
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
  states = { [entity_class.ENTITY_STATE_NORMAL] = button_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function button_create(keys)
  local self = {}

  entity.entity_init_instance(self, M.button_class_def, game_data.button_archetype.state,
    game_data.button_archetype.state_time)
  self.layout_row = nil
  self.width = 0
  self.side_tooltip = nil
  self.has_side_tooltip = 0
  self.tooltip = nil
  self.has_ = 0
  self.slide_target = vec.vec_make(0, 0)
  self.on_click = nil
  self.has_on_click = 0
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_HOVERED)
  self.is_hovered = 0
  self.refresh = nil
  self.name = nil
  self.trade = 0
  self.has_trade = 0
  self.type_bias = 0
  self.has_type_bias = 0
  self.technology = nil
  self.project_planet = nil
  self.project = research_project_id_none()
  self.project_cost = 0
  self.has_project_cost = 0
  self.archetype = game_data.button_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  if keys ~= nil then
    if keys.has_layer ~= 0 then
      entity_registry.entity_registry_set_layer(self, keys.layer)
    end
    if keys.has_pos ~= 0 then
      entity_class.entity_set_pos(self, keys.pos)
    end
    if keys.has_slide_target ~= 0 then
      self.slide_target = vec.vec_make(keys.slide_target.x, keys.slide_target.y)
      self.has_ = bit.bor(self.has_, M.BUTTON_HAS_SLIDE_TARGET)
    end
    self.on_click = keys.on_click
    self.has_on_click = keys.has_on_click
    self.refresh = keys.refresh
    self.name = keys.name
    self.trade = keys.trade
    self.has_trade = keys.has_trade
    self.type_bias = keys.type_bias
    self.has_type_bias = keys.has_type_bias
    self.technology = keys.technology
    self.project_planet = keys.project_planet
    self.project = { index = keys.project.index, epoch = keys.project.epoch }
    self.project_cost = keys.project_cost
    self.has_project_cost = keys.has_project_cost
  end
  M.button_construct(self, keys)
  entity.entity_construct(self)
  return self
end

function M.button_init()
  M.button_shutdown()
  g_button_up = 1
end

function M.button_shutdown()
  g_button_up = 0
end

function M.button_ready()
  return g_button_up
end

function M.button_class()
  return M.button_class_def
end

-- Copies `row` into the keys; nil leaves the tooltip row absent.
function M.button_keys_set_tooltip_row(keys, row)
  keys.has_tooltip_row = (row ~= nil) and 1 or 0
  if row ~= nil then
    keys.tooltip_row = map_tables.map_tables_make_data_row({
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

-- static const button_keys_t none: built once by button_keys_zero at the end of the file, never written.
local none

function M.button_construct(self, keys)
  if self == nil then
    return
  end
  if keys == nil then
    keys = none
  end

  self.layout_row = layout.layout_row_new(keys.layout_row_keys)
  self.width = fixed.p8_fixed_add(self.layout_row.width, fixed.p8_fixed_from_int(2))
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_HIT_BOX)
  self.hit_box = { left = fixed.p8_fixed_from_int(0), top = fixed.p8_fixed_from_int(-1), right = self.width,
    bottom = fixed.p8_fixed_from_int(11) }

  if keys.has_side_tooltip_row ~= 0 then
    self.side_tooltip = layout.layout_row_new(keys.side_tooltip_row)
    self.has_side_tooltip = 1
  elseif keys.has_side_tooltip ~= 0 and keys.side_tooltip_is_false ~= 0 then
    self.side_tooltip = nil
    self.has_side_tooltip = 1
  end
  if keys.has_tooltip_row ~= 0 then
    self.tooltip = layout.tooltip_new(keys.tooltip_row)
  end
end

function M.button_update(self)
  if self == nil then
    return
  end

  if bit.band(self.has_, M.BUTTON_HAS_SLIDE_TARGET) ~= 0 then
    local qf = self.slide_target
    local ra = entity_class.entity_pos(self)
    local d = vec.vec_sub(qf, ra)
    local length_squared = vec.vec_length_squared(d)
    local delta
    if length_squared <= fixed.p8_fixed_from_int(1) then
      delta = d
    else
      delta = vec.vec_mul(d, fixed.P8_FIXED_LIT(0x00006666, 0.4))
    end
    entity_class.entity_set_pos(self, vec.vec_add(ra, delta))
  end
end

function M.button_draw_overlay(gfx, mem, self)
  if self == nil then
    return
  end

  if self.tooltip ~= nil and self.is_hovered ~= 0 then
    layout.tooltip_draw_at(gfx, mem, self.tooltip, fixed.p8_fixed_from_int(vram.P8_SCREEN_HEIGHT - 1),
      fixed.p8_fixed_from_int(1))
  end
end

-- Text items go through the text image cache, or are printed directly when through_text_image is 0.
local function button_draw_body(graphics, memory, self, draw_pos, body, top, through_text_image)
  local width = self.width
  local right = fixed.p8_fixed_sub(fixed.p8_fixed_add(draw_pos.x, width), fixed.p8_fixed_from_int(1))
  local bottom = fixed.p8_fixed_add(draw_pos.y, fixed.p8_fixed_from_int(9))
  local left_pixel = ui_to_int(draw_pos.x)
  local top_pixel = ui_to_int(draw_pos.y)
  local right_pixel = ui_to_int(right)
  local bottom_pixel = ui_to_int(bottom)

  gfx_module.p8_rect(graphics, memory, left_pixel - 1, top_pixel, right_pixel + 1, bottom_pixel + 1, 1)
  gfx_module.p8_rectfill(graphics, memory, left_pixel, top_pixel, right_pixel, bottom_pixel, body)
  gfx_module.p8_rectfill(graphics, memory, left_pixel, top_pixel, right_pixel, top_pixel, top)

  if self.layout_row ~= nil then
    local off = vec.vec_add(draw_pos, vec.vec_make(fixed.p8_fixed_from_int(1), fixed.p8_fixed_from_int(2)))
    if through_text_image ~= 0 then
      layout.layout_row_draw_at_text_image(graphics, memory, self.layout_row, off, fixed.p8_fixed_from_int(0))
    else
      layout.layout_row_draw_at(graphics, memory, self.layout_row, off, fixed.p8_fixed_from_int(0))
    end
  end

  if self.side_tooltip ~= nil then
    local c = self.side_tooltip
    local bg = 2
    local x2 = fixed.p8_fixed_add(right, c.width)

    if c.background ~= nil and c.background_is_bool == 0 then
      bg = ui_to_int(c.background)
    end
    gfx_module.p8_rectfill(graphics, memory, ui_to_int(right) + 3, top_pixel + 1, ui_to_int(x2) + 4, bottom_pixel, bg)

    local off2 = vec.vec_add(draw_pos, vec.vec_make(fixed.p8_fixed_add(width, fixed.p8_fixed_from_int(3)),
      fixed.p8_fixed_from_int(2)))
    if through_text_image ~= 0 then
      layout.layout_row_draw_at_text_image(graphics, memory, c, off2, fixed.p8_fixed_from_int(0))
    else
      layout.layout_row_draw_at(graphics, memory, c, off2, fixed.p8_fixed_from_int(0))
    end
  end
end

local function is_whole(x)
  return fixed.p8_fixed_from_int(ui_to_int(x)) == x
end

-- Returns ok, extent: extent widened to the row's items drawn dx right of the button's border; ok 0 when
-- an item is off a whole offset or left of the border, so the image could not hold it.
local function row_extent(row, dx, extent)
  if row == nil or row.placed_items == nil then
    return 1, extent
  end
  for i = 0, row.placed_items_count - 1 do
    local item = row.placed_items[i]
    if not is_whole(item.offset.x) or item.offset.y ~= 0 then
      return 0, extent
    end
    local left = dx + ui_to_int(item.offset.x) + 1
    local end_ = left
    if item.kind == layout.LAYOUT_ROW_ITEM_TEXT then
      left = left + 1
      end_ = left + c_strlen(item.text) * gfx_module.P8_FONT_CELL_WIDTH + 1
    elseif item.kind == layout.LAYOUT_ROW_ITEM_SPRITE then
      end_ = left + gfx_module.P8_SPRITE_SIZE
    end
    if left < 0 then
      return 0, extent
    end
    if end_ > extent then
      extent = end_
    end
  end
  return 1, extent
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
  if ok == 0 then
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
    if ok == 0 then
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
  button_draw_body(gfx, mem, context, vec.vec_make(fixed.p8_fixed_from_int(1), fixed.p8_fixed_from_int(0)),
    g_building_look.body, g_building_look.top, 0)
end

local function button_draw_image(graphics, memory, self, draw_pos, look)
  if graphics.fillp_pattern ~= 0 or hud.hud_palette_block_default_in_force(memory) == 0 then
    return 0
  end
  local width = button_image_width(self)
  if width == 0 then
    return 0
  end
  local key = { words = {
    [0] = (self.layout_row ~= nil) and self.layout_row.serial or 0,
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

  local has_on_click = self.on_click ~= nil
  local hovered = self.is_hovered ~= 0 or (ui.main_ui() ~= nil and ui.ui_pad_tab(ui.main_ui()) == self)
  local look = {
    body = has_on_click and (hovered and 12 or 13) or 5,
    top = has_on_click and 6 or 5,
  }

  if button_draw_image(graphics, memory, self, draw_pos, look) == 0 then
    button_draw_body(graphics, memory, self, draw_pos, look.body, look.top, 1)
  end
end

function M.button_left_press(self)
  local has_on_click = self ~= nil and self.on_click ~= nil
  local out = { stop = 1, held = nil }

  if has_on_click then
    self.on_click(self)
  end

  return out
end

local function modal_backdrop_left_press_method(self, mouse)
  return M.modal_backdrop_left_press(self)
end

M.modal_backdrop_class_def = {
  hit_test = entity_registry.entity_registry_hit_test_always,
  left_press = modal_backdrop_left_press_method,
  states = {},
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = 0,
}

local function modal_backdrop_create(keys)
  local self = {}

  entity.entity_init_instance(self, M.modal_backdrop_class_def, game_data.modal_backdrop_archetype.state,
    game_data.modal_backdrop_archetype.state_time)
  self.archetype = game_data.modal_backdrop_archetype
  entity_registry.entity_registry_set_layer(self, self.archetype.layer)
  self.on_click = nil
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_DEAD)
  self.is_dead = 0
  self.has = bit.bor(self.has, entity_class.ENTITY_HAS_IS_HOVERED)
  self.is_hovered = 0
  if keys ~= nil then
    self.on_click = keys.on_click
  end
  entity.entity_construct(self)
  return self
end

function M.modal_backdrop_init()
  M.modal_backdrop_shutdown()
  g_modal_backdrop_up = 1
end

function M.modal_backdrop_shutdown()
  g_modal_backdrop_up = 0
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
  local out = { stop = 1, held = nil }

  if self == nil then
    return out
  end
  self.is_dead = 0
  if self.on_click ~= nil then
    self.is_dead = (self.on_click(self) ~= 0) and 1 or 0
  end
  return out
end

-- Lua only: the memset of a button_keys_t; writes every field of keys at its C zero and returns keys.
function M.button_keys_zero(keys)
  keys.has_layer = 0
  keys.layer = 0
  keys.layout_row_keys = layout.layout_row_keys_zero()
  keys.has_side_tooltip = 0
  keys.side_tooltip_is_false = 0
  keys.has_side_tooltip_row = 0
  keys.side_tooltip_row = layout.layout_row_keys_zero()
  keys.has_tooltip = 0
  keys.has_tooltip_row = 0
  keys.tooltip_row = map_tables.map_tables_make_data_row({})
  keys.has_pos = 0
  keys.pos = vec.vec_make(0, 0)
  keys.has_slide_target = 0
  keys.slide_target = vec.vec_make(0, 0)
  keys.has_on_click = 0
  keys.on_click = nil
  keys.refresh = nil
  keys.name = nil
  keys.has_trade = 0
  keys.trade = 0
  keys.has_type_bias = 0
  keys.type_bias = 0
  keys.technology = nil
  keys.project_planet = nil
  keys.project = research_project_id_none()
  keys.has_project_cost = 0
  keys.project_cost = 0
  return keys
end

-- Lua only: button_t b = {0}, a new table.
function M.button_zero()
  local b = {}
  entity_class.entity_init_header(b, nil)
  b.has_ = 0
  b.archetype = nil
  b.layout_row = nil
  b.width = 0
  b.side_tooltip = nil
  b.has_side_tooltip = 0
  b.tooltip = nil
  b.slide_target = vec.vec_make(0, 0)
  b.on_click = nil
  b.has_on_click = 0
  b.refresh = nil
  b.name = nil
  b.trade = 0
  b.has_trade = 0
  b.type_bias = 0
  b.has_type_bias = 0
  b.technology = nil
  b.project_planet = nil
  b.project = research_project_id_none()
  b.project_cost = 0
  b.has_project_cost = 0
  return b
end

none = M.button_keys_zero({})

return M
