-- Copy of src/p8/input.h then src/p8/input.c. p8_input_load, p8_input_save,
-- p8_input_append and p8_input_free are defined in src/devtools/ (not ported).
-- A p8_input_t is { frames, frame_count, current_frame }, frames indexed from
-- 0, each { mouse_x, mouse_y, mouse_buttons, pad_buttons }.
local M = {}

M.P8_PAD_BUTTON_1 = 1
M.P8_PAD_BUTTON_2 = 2
M.P8_PAD_BUTTON_3 = 4
M.P8_PAD_BUTTON_4 = 8
M.P8_PAD_BUTTON_5 = 16
M.P8_PAD_BUTTON_6 = 32
M.P8_PAD_BUTTON_7 = 64
M.P8_PAD_BUTTON_8 = 128
M.P8_PAD_BUTTON_9 = 256
M.P8_PAD_BUTTON_10 = 512
M.P8_PAD_BUTTON_11 = 1024
M.P8_PAD_BUTTON_12 = 2048
M.P8_PAD_BUTTON_COUNT = 12

M.P8_MOUSE_LEFT = 1

function M.p8_input_init(input)
  input.frames = nil
  input.frame_count = 0
  input.current_frame = 0
end

function M.p8_input_step(input)
  assert(input.current_frame + 1 < input.frame_count)
  input.current_frame = input.current_frame + 1
end

function M.p8_input_read_mouse(input)
  local mouse = { x = 0, y = 0, buttons = 0 }
  if input.frame_count == 0 or input.current_frame >= input.frame_count then
    return mouse
  end
  local f = input.frames[input.current_frame]
  mouse.x = f.mouse_x
  mouse.y = f.mouse_y
  mouse.buttons = f.mouse_buttons
  return mouse
end

return M
