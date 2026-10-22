-- Copy of src/p8/fatal.h; the C definition lives in src/platform/ (not ported).
local M = {}

-- Level 0: the error carries the message alone, as the C prints it.
function M.p8_fatal(message)
  error(message, 0)
end

return M
