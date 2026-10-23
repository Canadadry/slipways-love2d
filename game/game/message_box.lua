-- Include cycle: game.scroll, game.ui and game.ui_button are required at call time.
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_id = require("engine.entity_id")
local entity_registry = require("engine.entity_registry")
local vec = require("engine.vec")
local draw_functions = require("game.draw_functions")
local game_data = require("game.game_data")
local hud = require("game.hud")
local layout = require("game.layout")
local map_tables = require("game.map_tables")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")

local M = {}

M.MESSAGE_BOX_ARGUMENT_NUMBER = 0
M.MESSAGE_BOX_ARGUMENT_FALSE = 1
M.MESSAGE_BOX_ARGUMENT_NIL = 2
M.MESSAGE_BOX_ARGUMENT_TEXT = 3

M.MESSAGE_BOX_ARTICLE_SCALARS = 0
M.MESSAGE_BOX_ARTICLE_LAYOUT_ROW = 1
M.MESSAGE_BOX_ARTICLE_SHARED_ROW = 2

local function zero_vec()
  return vec.make(fixed.from_int(0), fixed.from_int(0))
end

local function vec_copy(v)
  return vec.make(v.x, v.y)
end

-- The three members of `as` are kept apart: only the member of `kind` is written or read. The layout
-- row is filled in place by layout_row_init; layout.pos and row_of are nil when absent.
local function article_zero()
  local row = {}

  entity_class.init_header(row, nil)
  row.background_is_bool = false
  row.placed_items = nil
  row.placed_items_count = 0
  row.width = 0
  row.serial = 0
  return {
    kind = M.MESSAGE_BOX_ARTICLE_SCALARS,
    draw_function = map_tables.DRAW_FUNCTION_NONE,
    as = {
      scalars = { count = 0, arguments = {}, text = "" },
      layout = { row = row, pos = nil, align = 0 },
      row_of = nil,
    },
  }
end

local g_message_box_up = false
local g_toast_up = false
local g_shown_message_box = nil

function M.shown_message_box()
  return entity_id.get(g_shown_message_box)
end

function M.message_box_set_shown(mh)
  g_shown_message_box = entity_id.link(mh)
end

-- Closes the shown box as its timeout does: marked dead, no box shown. Does nothing when none is shown.
function M.message_box_close_shown()
  local shown = M.shown_message_box()
  if shown ~= nil then
    shown.is_dead = true
    M.message_box_set_shown(nil)
  end
end

function M.message_box_construct(self)
  local previous

  if self == nil then
    return
  end

  previous = M.shown_message_box()
  if previous ~= nil then
    previous.is_dead = true
  end
  M.message_box_set_shown(self)
end

local function message_box_draw_overlay_method(self, screen_pos)
  M.message_box_draw_overlay(dispatch_context.gfx(), dispatch_context.mem(), self)
end

local function message_box_draw_article(gfx, mem, article)
  local items = {}
  local row

  if article.kind == M.MESSAGE_BOX_ARTICLE_SHARED_ROW then
    local shared = article.as.row_of()

    if shared ~= nil then
      draw_functions.draw_functions_draw_row_text_image(gfx, mem, shared)
    end
    return
  end
  if article.kind == M.MESSAGE_BOX_ARTICLE_LAYOUT_ROW then
    local p
    if article.as.layout.pos ~= nil then
      p = vec_copy(article.as.layout.pos)
    else
      p = zero_vec()
    end
    layout.layout_row_draw_at_text_image(gfx, mem, article.as.layout.row, p, article.as.layout.align)
    return
  end
  for i = 1, article.as.scalars.count do
    local argument = article.as.scalars.arguments[i]

    if argument.kind == M.MESSAGE_BOX_ARGUMENT_TEXT then
      items[i] = map_tables.make_data_item({ text = article.as.scalars.text })
    elseif argument.kind == M.MESSAGE_BOX_ARGUMENT_FALSE then
      items[i] = map_tables.make_data_item({ is_false = true })
    elseif argument.kind == M.MESSAGE_BOX_ARGUMENT_NIL then
      items[i] = map_tables.make_data_item({ is_nil = true })
    else
      items[i] = map_tables.make_data_item({ is_fixed = true, fixed = argument.number })
    end
  end
  row = map_tables.make_data_row({
    count = article.as.scalars.count,
    items = items,
    draw_function = article.draw_function,
  })
  draw_functions.draw_functions_draw_row_text_image(gfx, mem, row)
end

function M.message_box_draw_overlay(gfx, mem, self)
  local self_t, rh, t, nc, t3

  if self == nil then
    return
  end

  self_t = self.state_time
  rh = self.duration_scale
  t = fixed.sub(fixed.div(self_t, rh), fixed.from_int(10))

  if t >= 0 then
    if self.keep_open ~= nil and self.keep_open(self) then
      self.state_time = fixed.mul(rh, fixed.from_int(10))
      t = fixed.from_int(0)
    end
  end

  t3 = fixed.mul(fixed.mul(t, t), t)
  nc = fixed.div(fixed.neg(t3), fixed.from_int(9))

  if gfx ~= nil and mem ~= nil then
    for i = 1, self.articles_count do
      gfx_module.camera(gfx, fixed.flr_int(nc), 0)
      nc = fixed.neg(nc)
      message_box_draw_article(gfx, mem, self.articles[i])
    end
    gfx_module.camera_reset(gfx)
  end

  if t > 0x000a8000 then
    self.is_dead = true
    M.message_box_set_shown(nil)
    assert(M.shown_message_box() == nil,
      "message_box_draw_overlay: the closing box marks itself dead and clears the shown message box")
  end
end

M.message_box_class_def = {
  draw_overlay = message_box_draw_overlay_method,
  tags = entity_class.TAG(entity_class.TAG_MESSAGE_BOX),
  states = {},
  lineage = 0,
  skip_draw_off_screen = false,
}

local function message_box_next_article(self)
  local article = article_zero()
  local n = self.articles_count + 1

  self.articles[n] = article
  self.articles_count = n
  return article
end

function M.message_box_add_row(self, row)
  local article

  assert(row.draw_function ~= map_tables.DRAW_FUNCTION_DRAW_ROW,
    "message_box_add_row: a layout row goes through message_box_add_layout_row")
  article = message_box_next_article(self)
  article.kind = M.MESSAGE_BOX_ARTICLE_SCALARS
  article.draw_function = row.draw_function
  article.as.scalars.count = row.count
  article.as.scalars.text = ""
  for i = 1, row.count do
    local item = row.items[i]
    local argument = { kind = M.MESSAGE_BOX_ARGUMENT_NUMBER, number = 0 }

    article.as.scalars.arguments[i] = argument
    assert(item.row == nil, "message_box_add_row: a draw function reads no nested row")
    argument.number = fixed.from_int(0)
    if item.text ~= nil then
      assert(i == 1, "message_box_add_row: only print_outlined's first argument is read as text -- stop, do not widen")
      article.as.scalars.text = item.text
      argument.kind = M.MESSAGE_BOX_ARGUMENT_TEXT
    elseif item.is_false then
      argument.kind = M.MESSAGE_BOX_ARGUMENT_FALSE
    elseif item.is_nil then
      argument.kind = M.MESSAGE_BOX_ARGUMENT_NIL
    else
      argument.kind = M.MESSAGE_BOX_ARGUMENT_NUMBER
      if item.is_fixed then
        argument.number = item.fixed
      else
        argument.number = fixed.from_int(item.number)
      end
    end
  end
end

-- A nil pos draws the row at the zero vector.
function M.message_box_add_layout_row(self, keys, pos, align)
  local article = message_box_next_article(self)

  article.kind = M.MESSAGE_BOX_ARTICLE_LAYOUT_ROW
  article.draw_function = map_tables.DRAW_FUNCTION_DRAW_ROW
  layout.layout_row_init(article.as.layout.row, keys)
  article.as.layout.pos = pos and vec_copy(pos)
  article.as.layout.align = align
end

-- The row is read through `row_of` at each draw (nil draws nothing): the box never copies it.
function M.message_box_add_shared_row(self, row_of)
  local article = message_box_next_article(self)

  assert(row_of ~= nil, "message_box_add_shared_row: a shared row is read through its function at each draw")
  article.kind = M.MESSAGE_BOX_ARTICLE_SHARED_ROW
  article.draw_function = map_tables.DRAW_FUNCTION_NONE
  article.as.row_of = row_of
end

local function message_box_create(keys)
  local self = {}

  entity.init_instance(self, M.message_box_class_def, game_data.message_box_archetype.state,
    game_data.message_box_archetype.state_time)
  self.archetype = game_data.message_box_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.duration_scale = self.archetype.duration_scale
  self.keep_open = nil
  self.articles = {}
  self.is_dead = false
  self.articles_count = 0
  if keys ~= nil then
    if keys.duration_scale ~= nil then
      self.duration_scale = keys.duration_scale
    end
    self.keep_open = keys.keep_open
  end
  entity.construct(self)
  M.message_box_construct(self)
  return self
end

function M.message_box_init()
  M.message_box_shutdown()
  g_message_box_up = true
end

function M.message_box_shutdown()
  g_message_box_up = false
  M.message_box_set_shown(nil)
end

function M.message_box_ready()
  return g_message_box_up
end

function M.message_box_new(keys)
  return message_box_create(keys)
end

local function toast_pos_or_zero(e)
  if e ~= nil then
    return entity_class.pos(e)
  end
  return zero_vec()
end

-- The cart's toast shares its source's vector until its first update, drawn meanwhile (sometimes for several frames): a copy alone would miss a bob in between.
local function toast_follow_source(self)
  local source = entity_id.get(self.follow)

  if source ~= nil and entity_class.has_pos(source) then
    entity_class.set_pos(self, source.pos)
  end
end

function M.toast_construct(self)
  if self == nil then
    return
  end

  if not entity_class.has_pos(self) then
    local ui = require("game.ui")
    local scroll = require("game.scroll")
    local je = ui.main_mouse_typed()
    local mo = scroll.main_camera_typed()
    local main_mouse_pos = toast_pos_or_zero(je)
    local main_camera_pos = toast_pos_or_zero(mo)
    local offset = vec.make(fixed.from_int(0), fixed.from_int(5))
    entity_class.set_pos(self, vec.sub(vec.add(main_mouse_pos, main_camera_pos), offset))
  end
end

function M.toast_update(self)
  local new_l

  if self == nil then
    return
  end

  toast_follow_source(self)
  self.follow = nil
  entity_class.set_pos(self, vec.add(entity_class.pos(self), self.archetype.velocity))

  new_l = fixed.add(self.fade, 0x00001999)
  self.fade = new_l

  self.is_dead = new_l >= fixed.from_int(5)
end

local function toast_update_method(self, t)
  M.toast_update(self)
end

function M.toast_draw(gfx, mem, self, p)
  local color
  local a_center = 0x00008000

  if self == nil or gfx == nil or mem == nil then
    return
  end

  hud.hud_select_palette_block(mem, self.fade)

  color = fixed.flr_int(self.color)

  hud.hud_print_outlined(gfx, mem, self.text, p.x, p.y, color, a_center)

  hud.hud_select_palette_block_default(mem)
end

local function toast_draw_method(self, screen_pos)
  toast_follow_source(self)
  M.toast_draw(dispatch_context.gfx(), dispatch_context.mem(), self,
    entity_class.pos(self))
end

M.toast_class_def = {
  draw = toast_draw_method,
  states = { [entity_class.STATE_NORMAL] = toast_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function toast_create(keys)
  local self = {}

  entity.init_instance(self, M.toast_class_def, game_data.toast_archetype.state,
    game_data.toast_archetype.state_time)
  self.archetype = game_data.toast_archetype
  entity_registry.set_layer(self, self.archetype.layer)
  self.fade = self.archetype.fade
  self.follow = nil
  self.is_dead = false
  self.text = ""
  self.color = fixed.from_int(0)
  if keys ~= nil then
    local source = entity_id.get(keys.source)

    if source ~= nil and entity_class.has_pos(source) then
      entity_class.set_pos(self, source.pos)
      self.follow = keys.source
    elseif keys.pos ~= nil then
      entity_class.set_pos(self, keys.pos)
    end
    if keys.text ~= nil then
      self.text = keys.text
    end
    self.color = keys.color
  end
  M.toast_construct(self)
  entity.construct(self)
  return self
end

function M.toast_init()
  M.toast_shutdown()
  g_toast_up = true
end

function M.toast_shutdown()
  g_toast_up = false
end

function M.toast_ready()
  return g_toast_up
end

function M.toast_keys_set_text(keys, text)
  keys.text = text
end

function M.toast_new(keys)
  return toast_create(keys)
end

function M.open_box_frame_set_description(frame, description)
  frame.description = description
end

-- A copy of every field of row, one level deep (nested tables shared, as
-- button_create and button_construct only read the keys).
local function button_keys_copy(row)
  local b = {}
  for k, v in pairs(row) do
    b[k] = v
  end
  return b
end

local function open_box_step(ctx, frame, row)
  local ui_button = require("game.ui_button")
  local b = button_keys_copy(row)
  local gq

  b.pos = vec_copy(ctx.pos)
  b.slide_target = vec_copy(ctx.button_pos)

  if frame.back_color ~= nil then
    b.layer = frame.back_color
  end

  if frame.row_step ~= nil then
    gq = vec_copy(frame.row_step)
  else
    gq = zero_vec()
  end
  ctx.button_pos = vec.add(ctx.button_pos, gq)

  return ui_button.button_new(b)
end

-- A nil list drops the button.
local function open_box_add(le, button)
  if le ~= nil then
    assert(button ~= nil, "open_box_add: the ephemerals list holds button ids")
    le[#le + 1] = entity_id.link(button)
  end
end

function M.message_box_open_box(frame, buttons, button_count)
  local ui = require("game.ui")
  local ui_button = require("game.ui_button")
  local box_pos
  local step_context = {}
  local ephemerals
  local dispatcher
  local description_row = ui_button.button_keys_zero({})

  if frame == nil then
    return
  end
  assert(button_count == 0 or buttons[button_count] ~= nil,
    "message_box_open_box: buttons is an array of button_count keys, from 1")

  if frame.description ~= nil then
    layout.layout_row_keys_append(description_row.layout_row_keys, layout.layout_row_cell_string(frame.description))
    description_row.layout_row_keys.color = fixed.from_int(13)
  end

  if frame.fixed_pos ~= nil then
    box_pos = vec_copy(frame.fixed_pos)
  elseif frame.pos ~= nil then
    box_pos = vec_copy(frame.pos)
  else
    box_pos = zero_vec()
  end

  step_context.button_pos = vec.add(box_pos, zero_vec())
  step_context.pos = box_pos

  dispatcher = ui.main_ui()
  ui.ui_clear_ephemerals(dispatcher)

  if dispatcher ~= nil then
    ephemerals = ui.ui_ephemerals_new()
  end
  if frame.description ~= nil then
    open_box_add(ephemerals, open_box_step(step_context, frame, description_row))
  end
  for index = 1, button_count do
    open_box_add(ephemerals, open_box_step(step_context, frame, buttons[index]))
  end
  if dispatcher ~= nil then
    ui.ui_set_ephemerals(dispatcher, ephemerals)
  end

  if frame.clear_ephemerals_after and dispatcher ~= nil then
    ui.ui_set_ephemerals(dispatcher, ui.ui_ephemerals_new())
  end
end

-- A new table each call; text and pos are nil when not given.
function M.toast_keys_zero()
  return {
    source = nil,
    color = 0,
  }
end

-- A new table each call; description, fixed_pos, back_color, row_step and pos are nil when not given.
function M.open_box_frame_zero()
  return {
    clear_ephemerals_after = false,
  }
end

return M
