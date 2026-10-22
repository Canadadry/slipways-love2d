-- Copy of src/game/attract.h then src/game/attract.c.
--
-- An attract_t is the caller's table { attract_after, idle_frames, phase, demo_frame }, filled by attract_init.
local demo_data = require("game.demo_data")
local save_state = require("game.save_state")

local M = {}

M.ATTRACT_AFTER_DEFAULT = 1200

M.ATTRACT_PHASE_IDLE = 0
M.ATTRACT_PHASE_PLAYING = 1
M.ATTRACT_PHASE_SWALLOWING = 2

M.ATTRACT_ACTION_NONE = 0
M.ATTRACT_ACTION_START = 1
M.ATTRACT_ACTION_RETURN = 2

local g_demo_record = {}

function M.attract_init(attract, attract_after)
  assert(attract_after >= 1, "a threshold of 0 would start a demo with no cart_update60 since the return")
  attract.attract_after = attract_after
  attract.idle_frames = 0
  attract.phase = M.ATTRACT_PHASE_IDLE
  attract.demo_frame = 0
end

-- Returns the action, then played_buttons.
function M.attract_frame(attract, on_title, raw_buttons, played_buttons)
  if attract.phase == M.ATTRACT_PHASE_PLAYING then
    if raw_buttons ~= 0 or attract.demo_frame == demo_data.demo_data_frame_count then
      if raw_buttons ~= 0 then
        attract.phase = M.ATTRACT_PHASE_SWALLOWING
      else
        attract.phase = M.ATTRACT_PHASE_IDLE
      end
      attract.idle_frames = 0
      played_buttons = 0
      return M.ATTRACT_ACTION_RETURN, played_buttons
    end
    played_buttons = demo_data.demo_data_pad_buttons[attract.demo_frame]
    attract.demo_frame = attract.demo_frame + 1
    return M.ATTRACT_ACTION_NONE, played_buttons
  end
  if attract.phase == M.ATTRACT_PHASE_SWALLOWING then
    if raw_buttons ~= 0 then
      played_buttons = 0
      return M.ATTRACT_ACTION_NONE, played_buttons
    end
    attract.phase = M.ATTRACT_PHASE_IDLE
  end
  if raw_buttons ~= 0 or on_title == 0 then
    attract.idle_frames = 0
    played_buttons = raw_buttons
    return M.ATTRACT_ACTION_NONE, played_buttons
  end
  if attract.idle_frames >= attract.attract_after then
    attract.phase = M.ATTRACT_PHASE_PLAYING
    attract.demo_frame = 1
    played_buttons = demo_data.demo_data_pad_buttons[0]
    return M.ATTRACT_ACTION_START, played_buttons
  end
  attract.idle_frames = attract.idle_frames + 1
  played_buttons = 0
  return M.ATTRACT_ACTION_NONE, played_buttons
end

function M.attract_phase(attract)
  return attract.phase
end

function M.attract_demo_frame(attract)
  return attract.demo_frame
end

-- Returns the module's save state, refilled on each call.
function M.attract_demo_record()
  local decoded = save_state.save_state_decode(demo_data.demo_data_record, demo_data.demo_data_record_count,
    g_demo_record)
  assert(decoded ~= 0, "the compiled demo record decodes: test_demo_data proves it")
  return g_demo_record
end

return M
