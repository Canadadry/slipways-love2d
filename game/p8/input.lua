-- frames is indexed from 1, each { mouse_x, mouse_y, mouse_buttons,
-- pad_buttons }; current_frame is the position of the frame read, past
-- frame_count when there is none.
local M = {}

M.PAD_BUTTON_1 = 1
M.PAD_BUTTON_2 = 2
M.PAD_BUTTON_3 = 4
M.PAD_BUTTON_4 = 8
M.PAD_BUTTON_5 = 16
M.PAD_BUTTON_6 = 32
M.PAD_BUTTON_7 = 64
M.PAD_BUTTON_8 = 128
M.PAD_BUTTON_9 = 256
M.PAD_BUTTON_10 = 512
M.PAD_BUTTON_11 = 1024
M.PAD_BUTTON_12 = 2048
M.PAD_BUTTON_COUNT = 12

M.MOUSE_LEFT = 1

function M.init(input)
  input.frames = nil
  input.frame_count = 0
  input.current_frame = 1
end

function M.step(input)
  assert(input.current_frame < input.frame_count)
  input.current_frame = input.current_frame + 1
end

function M.read_mouse(input)
  local mouse = { x = 0, y = 0, buttons = 0 }
  if input.current_frame > input.frame_count then
    return mouse
  end
  local f = input.frames[input.current_frame]
  mouse.x = f.mouse_x
  mouse.y = f.mouse_y
  mouse.buttons = f.mouse_buttons
  return mouse
end

return M
