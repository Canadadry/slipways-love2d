-- Lua only: the storage backend of src/game/save_storage.h, over love.filesystem.
--
-- One opaque record of at most SAVE_STATE_MAX_BYTES bytes in save.bin, never decoded here; a
-- byte buffer is a 0-indexed array of integers 0 .. 255. The design's love.filesystem has no
-- remove: an empty save.bin is "no record".
local save_state = require("game.save_state")

local M = {}

local SAVE_FILE = "save.bin"

-- Asserts 1 <= count <= SAVE_STATE_MAX_BYTES. Returns 1 when stored, 0 when the write failed.
function M.save_storage_write(bytes, count)
  local chars = {}
  assert(count >= 1 and count <= save_state.SAVE_STATE_MAX_BYTES, "to clear the save, call save_storage_erase")
  for i = 0, count - 1 do
    chars[i + 1] = string.char(bytes[i])
  end
  return (love.filesystem.write(SAVE_FILE, table.concat(chars)) == true) and 1 or 0
end

-- Asserts cap >= SAVE_STATE_MAX_BYTES. Returns the record's byte count, 0 when there is none or
-- it cannot be read; on 0, bytes is unspecified.
function M.save_storage_read(bytes, cap)
  local content
  assert(cap >= save_state.SAVE_STATE_MAX_BYTES, "a smaller buffer would lose the end of a record")
  content = love.filesystem.read(SAVE_FILE)
  if content == nil or #content == 0 or #content > save_state.SAVE_STATE_MAX_BYTES then
    return 0
  end
  for i = 1, #content do
    bytes[i - 1] = string.byte(content, i)
  end
  return #content
end

-- Returns 1 when no record remains afterwards, 0 when the write failed.
function M.save_storage_erase()
  return (love.filesystem.write(SAVE_FILE, "") == true) and 1 or 0
end

return M
