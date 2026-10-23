local dispatch_context = require("engine.dispatch_context")
local attract_module = require("game.attract")
local bindings_module = require("game.bindings")
local bindings_default_module = require("game.bindings_default")
local cart = require("game.cart")
local cheat = require("game.cheat")
local pad_nav = require("game.pad_nav")

local M = {}

M.BINDINGS_SESSION = 0
M.BINDINGS_DEFAULT = 1

-- Returns a new bindings table, never an alias of bindings.
local function copy_bindings(bindings)
  local copy = { masks = {} }
  for mode = 1, bindings_module.MODE_COUNT do
    copy.masks[mode] = {}
    for action = 1, bindings_module.ACTION_COUNT do
      copy.masks[mode][action] = bindings.masks[mode][action]
    end
  end
  return copy
end

function M.init(step, bindings)
  step.cheat = {}
  step.pad_nav = {}
  -- Zeroed in place: the dispatch context may still hold step.played, which must read empty.
  if step.played_frame == nil then
    step.played_frame = {}
  end
  step.played_frame.mouse_x = 0
  step.played_frame.mouse_y = 0
  step.played_frame.mouse_buttons = 0
  step.played_frame.pad_buttons = 0
  if step.played == nil then
    step.played = {}
  end
  step.played.frames = nil
  step.played.frame_count = 0
  step.played.current_frame = 1
  step.session_bindings = nil
  step.attract = {}
  cheat.init(step.cheat)
  pad_nav.pad_nav_init(step.pad_nav, bindings)
  step.session_bindings = copy_bindings(bindings)
  attract_module.init(step.attract, attract_module.AFTER_DEFAULT)
end

function M.set_attract_after(step, frames)
  attract_module.init(step.attract, frames)
end

function M.switch_bindings(step, which)
  assert(which == M.BINDINGS_SESSION or which == M.BINDINGS_DEFAULT,
    "input_step.switch_bindings: which is the session or the default table")
  local session = copy_bindings(step.session_bindings)
  local attract = {
    attract_after = step.attract.attract_after,
    idle_frames = step.attract.idle_frames,
    phase = step.attract.phase,
    demo_frame = step.attract.demo_frame,
  }
  if which == M.BINDINGS_DEFAULT then
    M.init(step, bindings_default_module.bindings)
  else
    M.init(step, session)
  end
  step.session_bindings = session
  step.attract = attract
end

function M.start_record(step, state)
  assert(step ~= nil and state ~= nil)
  M.switch_bindings(step, M.BINDINGS_DEFAULT)
  cart.resume_game(state)
end

function M.return_to_title(step)
  assert(step ~= nil)
  cart.return_to_title()
  M.switch_bindings(step, M.BINDINGS_SESSION)
end

function M.run(step, raw)
  local buttons = 0
  local action
  action, buttons = attract_module.frame(step.attract, cart.title_shown(), raw.pad_buttons, buttons)
  if action == attract_module.ACTION_START then
    M.start_record(step, attract_module.demo_record())
    cart.cut_save()
  elseif action == attract_module.ACTION_RETURN then
    M.return_to_title(step)
  end
  local played_frame = step.played_frame
  played_frame.mouse_x = 0
  played_frame.mouse_y = 0
  played_frame.mouse_buttons = 0
  played_frame.pad_buttons = 0
  played_frame.pad_buttons = buttons
  pad_nav.pad_nav_step(step.pad_nav, buttons, step.played_frame)
  -- Set every frame, not at init: input_step.init leaves step.played empty.
  step.played.frames = { step.played_frame }
  step.played.frame_count = 1
  step.played.current_frame = 1
  local fired = cheat.step(step.cheat, buttons)
  dispatch_context.set_input(step.played)
  if M.attract_phase(step) == attract_module.PHASE_PLAYING then
    cart.set_demo_frame(M.attract_demo_frame(step) - 1)
  else
    cart.set_demo_frame(cart.NO_DEMO_FRAME)
  end
  return fired
end

function M.played_frame(step)
  return step.played_frame
end

function M.attract_phase(step)
  return attract_module.phase(step.attract)
end

function M.attract_demo_frame(step)
  return attract_module.demo_frame(step.attract)
end

return M
