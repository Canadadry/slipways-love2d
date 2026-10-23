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
  M.mouse_update(dispatch_context.dispatch_context_input(), self)
end

M.mouse_class_def = {
  states = { [entity_class.ENTITY_STATE_NORMAL] = mouse_update_method },
  tags = 0,
  lineage = 0,
  skip_draw_off_screen = false,
}

local function mouse_create()
  local self = {}

  entity.entity_init_instance(self, M.mouse_class_def, game_data.mouse_archetype.state,
    game_data.mouse_archetype.state_time)
  self.archetype = game_data.mouse_archetype
  entity_class.entity_set_pos(self, vec.vec_make(fixed.p8_fixed_from_int(0), fixed.p8_fixed_from_int(0)))
  entity.entity_construct(self)
  return self
end

function M.mouse_init()
  M.mouse_shutdown()
  assert(entity.entity_ready(), "mouse_init: entity_init() runs first")
  g_mouse_up = true
  g_left_button.held = nil
  g_left_button.pressed = false
  g_left_button.released = false
end

function M.mouse_shutdown()
  g_mouse_up = false
end

function M.mouse_left_button()
  if g_mouse_up then
    return g_left_button
  end
  return nil
end

function M.mouse_ready()
  return g_mouse_up
end

function M.mouse_class()
  return M.mouse_class_def
end

function M.mouse_new()
  return mouse_create()
end

function M.mouse_update(input, self)
  if self == nil or input == nil then
    return
  end

  local mouse = input_module.p8_input_read_mouse(input)
  entity_class.entity_set_pos(self, vec.vec_make(fixed.p8_fixed_from_int(mouse.x), fixed.p8_fixed_from_int(mouse.y)))

  if g_mouse_up then
    mouse_button.mouse_button_update(g_left_button, bit_module.band(mouse.buttons, input_module.P8_MOUSE_LEFT) ~= 0)
  end
end

return M
