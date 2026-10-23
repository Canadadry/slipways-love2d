-- The save's storage backend, over love.filesystem.
--
-- One opaque record of at most save_state.MAX_BYTES bytes in save.bin, never decoded here; a
-- byte buffer is a 1-based array of integers 0 .. 255, byte i of the file at position i. The
-- design's love.filesystem has no remove: an empty save.bin is "no record".
local save_state = require("game.save_state")

local M = {}

local SAVE_FILE = "save.bin"

-- Asserts 1 <= count <= save_state.MAX_BYTES. Returns true when stored, false when the write failed.
function M.write(bytes, count)
  local chars = {}
  assert(count >= 1 and count <= save_state.MAX_BYTES, "to clear the save, call save_storage.erase")
  for i = 1, count do
    chars[i] = string.char(bytes[i])
  end
  return love.filesystem.write(SAVE_FILE, table.concat(chars)) == true
end

-- Returns the record's byte count, 0 when there is none or it cannot be read; on 0, bytes is
-- unspecified.
function M.read(bytes)
  local content = love.filesystem.read(SAVE_FILE)
  if content == nil or #content == 0 or #content > save_state.MAX_BYTES then
    return 0
  end
  for i = 1, #content do
    bytes[i] = string.byte(content, i)
  end
  return #content
end

-- Returns true when no record remains afterwards, false when the write failed.
function M.erase()
  return love.filesystem.write(SAVE_FILE, "") == true
end

return M
