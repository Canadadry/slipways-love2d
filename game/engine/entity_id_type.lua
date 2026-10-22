-- Copy of src/engine/entity_id_type.h.
-- An entity_id_t is { index, generation } passed by value: every place that
-- creates, returns or stores one builds a new table, and ids are compared only
-- through entity_id_equal, never by table identity.

local M = {}

local ENTITY_ID_NONE = { index = 0, generation = 0 }
M.ENTITY_ID_NONE = ENTITY_ID_NONE

function M.entity_id_equal(a, b)
  return (a.index == b.index and a.generation == b.generation) and 1 or 0
end

function M.entity_id_is_none(id)
  return M.entity_id_equal(id, { index = ENTITY_ID_NONE.index, generation = ENTITY_ID_NONE.generation })
end

return M
