-- game.probe is required at load: help_screen_class_def holds its targeting and particle methods by identity.
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local entity_geom = require("engine.entity_geom")
local vec = require("engine.vec")
local draw_functions = require("game.draw_functions")
local game_data = require("game.game_data")
local layout = require("game.layout")
local message_box = require("game.message_box")
local particle = require("game.particle")
local probe = require("game.probe")
local ui_button = require("game.ui_button")
local fixed = require("p8.fixed")

local M = {}

local g_help_screen_up = false

local function screen_start_game_placeholder(self)
end

local g_start_game_method = screen_start_game_placeholder

function M.help_start_game_field_value()
  return g_start_game_method
end

function M.help_set_start_game(fn)
  if fn ~= nil then
    g_start_game_method = fn
  else
    g_start_game_method = screen_start_game_placeholder
  end
end

local g_resume_game_method = nil

-- nil (the value after help_init and help_shutdown) means the title has no resume row.
function M.help_resume_game_field_value()
  return g_resume_game_method
end

function M.help_set_resume_game(fn)
  g_resume_game_method = fn
end

local function help_screen_hit_test_method(self, mouse)
  return entity_geom.contains_cursor(self, mouse)
end

function M.help_screen_construct(self)
  local bs = {}
  local frame = message_box.open_box_frame_zero()

  if self == nil then
    return
  end
  for i = 1, game_data.DIFFICULTY_OPTIONS_COUNT do
    local e = game_data.difficulty_options_table[i]

    bs[i] = ui_button.button_keys_zero({})
    bs[i].layout_row_keys = layout.row_keys_of_row(e.layout_row)
    bs[i].name = e.name
    bs[i].trade = fixed.from_int(e.trade)
    bs[i].type_bias = e.type_bias
    bs[i].on_click = M.help_start_game_field_value()
  end
  frame.fixed_pos = vec.make(self.archetype.fixed_pos.x, self.archetype.fixed_pos.y)
  frame.clear_ephemerals_after = true
  frame.back_color = self.archetype.back_color
  frame.row_step = vec.make(self.archetype.row_step.x, self.archetype.row_step.y)
  frame.pos = entity_class.pos(self)
  message_box.message_box_open_box(frame, bs, game_data.DIFFICULTY_OPTIONS_COUNT)

  -- After the difficulty rows: the pad focus opens on the first button registered, forgiving.
  if M.help_resume_game_field_value() ~= nil then
    local resume = ui_button.button_keys_zero({})

    resume.layout_row_keys = layout.row_keys_of_row(game_data.resume_layout_row)
    resume.name = "resume"
    resume.on_click = M.help_resume_game_field_value()
    frame.fixed_pos = vec.sub(frame.fixed_pos, frame.row_step)
    message_box.message_box_open_box(frame, { resume }, 1)
  end
end

function M.help_screen_draw(gfx, mem, self, p)
  -- The seed tick, result unused: without it every game started from boot gets the same galaxy.
  fixed.rnd_default()
  particle.emitter_update(gfx, mem, self)

  if gfx ~= nil and mem ~= nil then
    local rows = (self ~= nil) and self.archetype.rows or nil
    local n = (rows ~= nil) and rows.count or 0
    local items = {}

    for i = 1, n do
      items[i] = rows.items[i].row
    end
    draw_functions.draw_rows_default(gfx, mem, items, n)
  end
end

function M.help_screen_draw_method(self, screen_pos)
  M.help_screen_draw(dispatch_context.gfx(), dispatch_context.mem(), self,
    screen_pos)
end

M.help_screen_class_def = {
  draw = M.help_screen_draw_method,
  draw_overlay = probe.targeting_tooltip_method,
  hit_test = help_screen_hit_test_method,
  drag = probe.targeting_track_method,
  drop = probe.targeting_confirm_method,
  spawn_particle = probe.spawn_particle,
  release = probe.release_method,
  states = {},
  tags = entity_class.TAG(entity_class.TAG_PROBE),
  lineage = entity_class.LINEAGE(entity_class.LINEAGE_PROBE),
  skip_draw_off_screen = false,
}

local function screen_create()
  local self
  local hit_box

  assert(g_help_screen_up, "help_init() must run first")
  self = {}
  probe.init_instance(self, M.help_screen_class_def, game_data.help_screen_archetype)
  hit_box = self.archetype.hit_box
  self.hit_box = { left = hit_box.left, top = hit_box.top, right = hit_box.right, bottom = hit_box.bottom }
  self.is_hovered = false
  M.help_screen_construct(self)
  particle.emitter_init(self)
  entity.construct(self)
  return self
end

function M.help_init()
  assert(probe.ready(), "help_init() runs after probe.init()")

  M.help_shutdown()

  g_help_screen_up = true

  M.help_set_start_game(nil)
  M.help_set_resume_game(nil)
end

function M.help_shutdown()
  g_help_screen_up = false
  M.help_set_start_game(nil)
  M.help_set_resume_game(nil)
end

function M.help_screen_ready()
  return g_help_screen_up
end

function M.help_screen_class()
  return M.help_screen_class_def
end

function M.help_screen_new()
  return screen_create()
end

return M
