local bit = require("bit")
local draw_meter = require("p8.draw_meter")
local fatal = require("p8.fatal")
local gfx_module = require("p8.gfx")
local sprite_sheet = require("p8.sprite_sheet")
local vram = require("p8.vram")
local backend = require("love.backend")
local pad_view = require("love.pad_view")
local screen = require("love.screen")
local virtual_pad_module = require("love.virtual_pad")
local dispatch_context = require("engine.dispatch_context")
local attract = require("game.attract")
local bindings = require("game.bindings")
local bindings_default = require("game.bindings_default")
local cart = require("game.cart")
local input_step = require("game.input_step")
local state_format = require("devtools.state_format")

-- Button n is bit n - 1 of pad_buttons, as p8.input's PAD_BUTTON_n.
local PAD_BUTTON_NAMES = {
  "dpup", "dpdown", "dpleft", "dpright", "a", "b", "x", "y", "leftshoulder", "rightshoulder", "start", "back",
}
local ATTRACT_AFTER_FLAG = "--attract-after"
local ATTRACT_AFTER_MAX = 2147483647

local gfx = {}
local mem = {}
local step = {}
local virtual_pad = {}
local view = {}
local cheat_activations = 0

local function read_bindings()
  local text = love.filesystem.read("bindings.cfg")
  if text == nil then
    return bindings_default.bindings
  end
  local parsed = {}
  local error_report = {}
  if not bindings.parse("bindings.cfg", text, parsed, error_report) then
    fatal.raise(error_report.text)
  end
  return parsed
end

local function read_attract_after(args)
  local frames = nil
  local i = 1
  while args[i] ~= nil do
    if args[i] == ATTRACT_AFTER_FLAG then
      local value = args[i + 1]
      if frames ~= nil then
        fatal.raise(string.format("%s given twice (second value %s)", ATTRACT_AFTER_FLAG, tostring(value)))
      end
      if value == nil then
        fatal.raise(string.format("%s needs a value", ATTRACT_AFTER_FLAG))
      end
      if not value:match("^[1-9]%d*$") or tonumber(value) > ATTRACT_AFTER_MAX then
        fatal.raise(string.format("%s: %s is not a decimal from 1 to %d", ATTRACT_AFTER_FLAG, tostring(value),
          ATTRACT_AFTER_MAX))
      end
      frames = tonumber(value)
      i = i + 2
    else
      i = i + 1
    end
  end
  return frames or attract.AFTER_DEFAULT
end

local function read_pad_buttons()
  local pad = love.joystick.getJoysticks()[1]
  if pad == nil then
    return 0
  end
  local mask = 0
  for n, name in ipairs(PAD_BUTTON_NAMES) do
    if pad:isGamepadDown(name) then
      mask = bit.bor(mask, bit.lshift(1, n - 1))
    end
  end
  return mask
end

local function read_touches()
  local touches = {}
  for index, id in ipairs(love.touch.getTouches()) do
    local x, y = love.touch.getPosition(id)
    touches[index] = { id = id, x = x, y = y }
  end
  return touches
end

local function read_mouse()
  local x, y = love.mouse.getPosition()
  return { x = x, y = y, down = love.mouse.isDown(1) }
end

function love.load(args)
  screen.init()
  vram.init(mem)
  gfx_module.init(gfx)
  sprite_sheet.load(mem)
  gfx_module.draw_backend_set(backend.get())
  dispatch_context.set_gfx(gfx)
  dispatch_context.set_mem(mem)
  local zero_frame = { mouse_x = 0, mouse_y = 0, mouse_buttons = 0, pad_buttons = 0 }
  dispatch_context.set_input({ frames = { zero_frame }, frame_count = 1, current_frame = 1 })
  draw_meter.install()
  cart.load()
  cart.init()
  input_step.init(step, read_bindings())
  input_step.set_attract_after(step, read_attract_after(args))
  virtual_pad_module.init(virtual_pad)
  pad_view.init(view)
end

function love.update(dt)
  local pad_buttons = virtual_pad_module.update(virtual_pad, read_pad_buttons(), read_touches(), read_mouse(),
    screen.get_dimensions())
  local raw = { mouse_x = 0, mouse_y = 0, mouse_buttons = 0, pad_buttons = pad_buttons }
  if input_step.run(step, raw) then
    cheat_activations = cheat_activations + 1
  end
  cart.update60()
end

function love.draw()
  screen.frame_begin()
  cart.draw()
  screen.frame_end()
  draw_meter.frame_end()
  screen.present()
  pad_view.draw(view, virtual_pad)
end

function debug_state()
  local lines = {}
  local sink = {
    write_line = function(context, line)
      context[#context + 1] = line
    end,
    context = lines,
  }
  state_format.dump(sink)
  state_format.cheat_activations(sink, cheat_activations)
  return table.concat(lines)
end
