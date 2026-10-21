-- Copy of src/game/mouse_button.h then src/game/mouse_button.c.
--
-- Shapes, with the C field names:
--   mouse_button_t: { has_held, held, pressed, released }, each 1/0.
local M = {}

function M.mouse_button_update(button, down)
  local was_held = 0

  assert(button ~= nil, "mouse_button_update dereferences the button")

  if button.has_held ~= 0 then
    was_held = button.held
  end

  button.has_held = 1
  button.held = down
  button.pressed = (down ~= 0 and was_held == 0) and 1 or 0
  button.released = (was_held ~= 0 and down == 0) and 1 or 0
end

return M
