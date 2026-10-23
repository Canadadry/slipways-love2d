-- A button: { held, pressed, released }, each a boolean; held nil until the first update.
local M = {}

function M.update(button, down)
  assert(button ~= nil, "mouse_button.update dereferences the button")

  assert(type(down) == "boolean", "mouse_button.update: down is a boolean")

  local was_held = button.held or false

  button.held = down
  button.pressed = down and not was_held
  button.released = was_held and not down
end

return M
