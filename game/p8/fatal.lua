local M = {}

-- Level 0: the error carries the message alone, with no position prefix.
function M.raise(message)
  error(message, 0)
end

return M
