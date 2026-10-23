-- Copy of src/game/input_step.h then src/game/input_step.c.
--
-- An input_step_t is the caller's table { cheat, pad_nav, played_frame, played, session_bindings, attract },
-- filled by input_step_init.
local dispatch_context = require("engine.dispatch_context")
local attract_module = require("game.attract")
local bindings_module = require("game.bindings")
local bindings_default_module = require("game.bindings_default")
local cart = require("game.cart")
local cheat = require("game.cheat")
local pad_nav = require("game.pad_nav")

local M = {}

M.INPUT_STEP_BINDINGS_SESSION = 0
M.INPUT_STEP_BINDINGS_DEFAULT = 1

-- Returns a new bindings_t, never an alias of bindings.
local function copy_bindings(bindings)
  local copy = { masks = {} }
  for mode = 0, bindings_module.BINDINGS_MODE_COUNT - 1 do
    copy.masks[mode] = {}
    for action = 0, bindings_module.BINDINGS_ACTION_COUNT - 1 do
      copy.masks[mode][action] = bindings.masks[mode][action]
    end
  end
  return copy
end

function M.input_step_init(step, bindings)
  step.cheat = {}
  step.pad_nav = {}
  -- Zeroed in place: the dispatch context may still point at step.played, which must read empty as in C.
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
  step.played.current_frame = 0
  step.session_bindings = nil
  step.attract = {}
  cheat.cheat_init(step.cheat)
  pad_nav.pad_nav_init(step.pad_nav, bindings)
  step.session_bindings = copy_bindings(bindings)
  attract_module.attract_init(step.attract, attract_module.ATTRACT_AFTER_DEFAULT)
end

function M.input_step_set_attract_after(step, frames)
  attract_module.attract_init(step.attract, frames)
end

function M.input_step_switch_bindings(step, which)
  assert(which == M.INPUT_STEP_BINDINGS_SESSION or which == M.INPUT_STEP_BINDINGS_DEFAULT,
    "input_step_switch_bindings: which is the session or the default table")
  local session = copy_bindings(step.session_bindings)
  local attract = {
    attract_after = step.attract.attract_after,
    idle_frames = step.attract.idle_frames,
    phase = step.attract.phase,
    demo_frame = step.attract.demo_frame,
  }
  if which == M.INPUT_STEP_BINDINGS_DEFAULT then
    M.input_step_init(step, bindings_default_module.bindings_default)
  else
    M.input_step_init(step, session)
  end
  step.session_bindings = session
  step.attract = attract
end

function M.input_step_start_record(step, state)
  assert(step ~= nil and state ~= nil)
  M.input_step_switch_bindings(step, M.INPUT_STEP_BINDINGS_DEFAULT)
  cart.cart_resume_game(state)
end

function M.input_step_return_to_title(step)
  assert(step ~= nil)
  cart.cart_return_to_title()
  M.input_step_switch_bindings(step, M.INPUT_STEP_BINDINGS_SESSION)
end

function M.input_step_run(step, raw)
  local buttons = 0
  local action
  action, buttons = attract_module.attract_frame(step.attract, cart.cart_title_shown() ~= 0, raw.pad_buttons, buttons)
  if action == attract_module.ATTRACT_ACTION_START then
    M.input_step_start_record(step, attract_module.attract_demo_record())
    cart.cart_cut_save()
  elseif action == attract_module.ATTRACT_ACTION_RETURN then
    M.input_step_return_to_title(step)
  end
  local played_frame = step.played_frame
  played_frame.mouse_x = 0
  played_frame.mouse_y = 0
  played_frame.mouse_buttons = 0
  played_frame.pad_buttons = 0
  played_frame.pad_buttons = buttons
  pad_nav.pad_nav_step(step.pad_nav, buttons, step.played_frame)
  -- Set every frame, not at init: the struct may have been copied since.
  step.played.frames = { [0] = step.played_frame }
  step.played.frame_count = 1
  step.played.current_frame = 0
  local fired = cheat.cheat_step(step.cheat, buttons)
  dispatch_context.dispatch_context_set_input(step.played)
  local playing = (M.input_step_attract_phase(step) == attract_module.ATTRACT_PHASE_PLAYING) and 1 or 0
  if playing ~= 0 then
    cart.cart_set_demo_frame(M.input_step_attract_demo_frame(step) - 1)
  else
    cart.cart_set_demo_frame(cart.CART_NO_DEMO_FRAME)
  end
  return fired
end

function M.input_step_played_frame(step)
  return step.played_frame
end

function M.input_step_attract_phase(step)
  return attract_module.attract_phase(step.attract)
end

function M.input_step_attract_demo_frame(step)
  return attract_module.attract_demo_frame(step.attract)
end

return M
