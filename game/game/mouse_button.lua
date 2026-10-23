-- Copy of src/game/mouse_button.h then src/game/mouse_button.c.
--
-- Shapes, with the C field names:
--   mouse_button_t: { held, pressed, released }, each 1/0; held nil until the first update.
local M = {}

function M.mouse_button_update(button, down)
  assert(button ~= nil, "mouse_button_update dereferences the button")

  local was_held = button.held or 0

  button.held = down
  button.pressed = (down ~= 0 and was_held == 0) and 1 or 0
  button.released = (was_held ~= 0 and down == 0) and 1 or 0
end

return M
