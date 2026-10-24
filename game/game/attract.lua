local demo_data = require("game.demo_data")
local save_state = require("game.save_state")

local M = {}

M.AFTER_DEFAULT = 1200

M.PHASE_IDLE = 0
M.PHASE_PLAYING = 1
M.PHASE_SWALLOWING = 2

M.ACTION_NONE = 0
M.ACTION_START = 1
M.ACTION_RETURN = 2

local g_demo_record = {}

function M.init(attract, attract_after)
  assert(attract_after >= 1, "a threshold of 0 would start a demo with no cart.update60 since the return")
  attract.attract_after = attract_after
  attract.idle_frames = 0
  attract.phase = M.PHASE_IDLE
  attract.demo_frame = 0
end

-- Returns the action, then played_buttons.
function M.frame(attract, on_title, raw_buttons, played_buttons)
  assert(type(on_title) == "boolean", "attract.frame: on_title is a boolean")
  if attract.phase == M.PHASE_PLAYING then
    if raw_buttons ~= 0 or attract.demo_frame == demo_data.frame_count then
      if raw_buttons ~= 0 then
        attract.phase = M.PHASE_SWALLOWING
      else
        attract.phase = M.PHASE_IDLE
      end
      attract.idle_frames = 0
      played_buttons = 0
      return M.ACTION_RETURN, played_buttons
    end
    played_buttons = demo_data.pad_buttons[attract.demo_frame]
    attract.demo_frame = attract.demo_frame + 1
    return M.ACTION_NONE, played_buttons
  end
  if attract.phase == M.PHASE_SWALLOWING then
    if raw_buttons ~= 0 then
      played_buttons = 0
      return M.ACTION_NONE, played_buttons
    end
    attract.phase = M.PHASE_IDLE
  end
  if raw_buttons ~= 0 or not on_title then
    attract.idle_frames = 0
    played_buttons = raw_buttons
    return M.ACTION_NONE, played_buttons
  end
  if attract.idle_frames >= attract.attract_after then
    attract.phase = M.PHASE_PLAYING
    attract.demo_frame = 1
    played_buttons = demo_data.pad_buttons[0]
    return M.ACTION_START, played_buttons
  end
  attract.idle_frames = attract.idle_frames + 1
  played_buttons = 0
  return M.ACTION_NONE, played_buttons
end

function M.phase(attract)
  return attract.phase
end

function M.demo_frame(attract)
  return attract.demo_frame
end

-- Returns the module's save state, refilled on each call.
function M.demo_record()
  local decoded = save_state.decode(demo_data.record, demo_data.record_count,
    g_demo_record)
  assert(decoded, "the compiled demo record decodes: test_demo_data proves it")
  return g_demo_record
end

return M
