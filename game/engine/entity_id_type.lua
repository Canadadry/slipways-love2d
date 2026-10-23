local M = {}

function M.entity_id_equal(a, b)
  return (a == b) and 1 or 0
end

function M.entity_id_is_none(id)
  return (id == nil) and 1 or 0
end

return M
