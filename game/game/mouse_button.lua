-- Copy of src/game/mouse_button.h then src/game/mouse_button.c.
--
-- Shapes, with the C field names:
--   mouse_button_t: { held, pressed, released }, each a boolean; held nil until the first update.
local M = {}

function M.mouse_button_update(button, down)
  assert(button ~= nil, "mouse_button_update dereferences the button")

  assert(type(down) == "boolean", "mouse_button_update: down is a boolean")

  local was_held = button.held or false

  button.held = down
  button.pressed = down and not was_held
  button.released = was_held and not down
end

return M
