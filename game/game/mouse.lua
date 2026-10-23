local bit_module = require("bit")
local dispatch_context = require("engine.dispatch_context")
local entity = require("engine.entity")
local entity_class = require("engine.entity_class")
local vec = require("engine.vec")
local game_data = require("game.game_data")
local mouse_button = require("game.mouse_button")
local fixed = require("p8.fixed")
local input_module = require("p8.input")

local M = {}

local g_mouse_up = false
local g_left_button = { held = nil, pressed = false, released = false }

local function mouse_update_method(self, t)
  M.update(dispatch_context.input(), self)
end

M.class_def = {
  states = { [entity_class.STATE_NORMAL] = mouse_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function mouse_create()
  local self = {}

  entity.init_instance(self, M.class_def, game_data.mouse_archetype.state,
    game_data.mouse_archetype.state_time)
  self.archetype = game_data.mouse_archetype
  entity_class.set_pos(self, vec.make(fixed.from_int(0), fixed.from_int(0)))
  entity.construct(self)
  return self
end

function M.init()
  M.shutdown()
  assert(entity.ready(), "mouse.init: entity.init() runs first")
  g_mouse_up = true
  g_left_button.held = nil
  g_left_button.pressed = false
  g_left_button.released = false
end

function M.shutdown()
  g_mouse_up = false
end

function M.left_button()
  if g_mouse_up then
    return g_left_button
  end
  return nil
end

function M.ready()
  return g_mouse_up
end

function M.class()
  return M.class_def
end

function M.new()
  return mouse_create()
end

function M.update(input, self)
  if self == nil or input == nil then
    return
  end

  local mouse = input_module.read_mouse(input)
  entity_class.set_pos(self, vec.make(fixed.from_int(mouse.x), fixed.from_int(mouse.y)))

  if g_mouse_up then
    mouse_button.update(g_left_button, bit_module.band(mouse.buttons, input_module.MOUSE_LEFT) ~= 0)
  end
end

return M
