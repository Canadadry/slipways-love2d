-- Reads fake-LÖVE captures (tools/fake_love/frame_capture.h), from the
-- repository root:
--   luajit tools/capture_read.lua frame <capture> <k>
--     frame f<k> (from 0) as 120 lines of 160 hex digits, one palette
--     number per pixel, left to right; exit 0.
--   luajit tools/capture_read.lua compare <capture a> <capture b>
--     the first frame where they differ and its count of differing pixels,
--     or that one ends first; exit 1. Identical: exit 0.
-- Exit 2 on a bad argument or an unreadable capture.
local WIDTH = 160
local HEIGHT = 120
local FRAME_SIZE = WIDTH * HEIGHT / 2
local HEX = "0123456789abcdef"

local function fail(message)
  io.stderr:write("capture_read: " .. message .. "\n")
  os.exit(2)
end

-- Returns the bytes and the frame count.
local function read_capture(path)
  local file = io.open(path, "rb")
  if file == nil then
    fail(path .. ": cannot open")
  end
  local bytes = file:read("*a")
  file:close()
  if #bytes % FRAME_SIZE ~= 0 then
    fail(string.format("%s: %d bytes, not a whole number of %d-byte frames", path, #bytes, FRAME_SIZE))
  end
  return bytes, #bytes / FRAME_SIZE
end

-- The low nibble is the left pixel: frame_capture_pack writes n(x) | n(x + 1) << 4.
local function pixels_of(byte)
  local left = byte % 16
  return left, (byte - left) / 16
end

local function print_frame(path, k_text)
  local bytes, count = read_capture(path)
  local k = tonumber(k_text)
  if k == nil or k < 0 or k >= count or k % 1 ~= 0 then
    fail(string.format("%s: no frame f%s, the capture has %d frames", path, k_text, count))
  end
  local lines = {}
  for y = 0, HEIGHT - 1 do
    local digits = {}
    local row = k * FRAME_SIZE + y * WIDTH / 2
    for i = 1, WIDTH / 2 do
      local left, right = pixels_of(bytes:byte(row + i))
      digits[i] = HEX:sub(left + 1, left + 1) .. HEX:sub(right + 1, right + 1)
    end
    lines[y + 1] = table.concat(digits)
  end
  io.write(table.concat(lines, "\n"), "\n")
end

local function differing_pixels(a, b, offset)
  local count = 0
  for i = offset + 1, offset + FRAME_SIZE do
    local byte_a, byte_b = a:byte(i), b:byte(i)
    if byte_a ~= byte_b then
      local left_a, right_a = pixels_of(byte_a)
      local left_b, right_b = pixels_of(byte_b)
      count = count + (left_a ~= left_b and 1 or 0) + (right_a ~= right_b and 1 or 0)
    end
  end
  return count
end

local function compare(path_a, path_b)
  local a, count_a = read_capture(path_a)
  local b, count_b = read_capture(path_b)
  local common = math.min(count_a, count_b)
  for k = 0, common - 1 do
    local offset = k * FRAME_SIZE
    if a:sub(offset + 1, offset + FRAME_SIZE) ~= b:sub(offset + 1, offset + FRAME_SIZE) then
      io.write(string.format("first difference: f%d, %d pixels differ\n", k, differing_pixels(a, b, offset)))
      os.exit(1)
    end
  end
  if count_a == count_b then
    io.write(string.format("identical: %d frames\n", common))
    os.exit(0)
  end
  io.write(string.format("identical over %d frames, then %s ends: %s has %d frames, %s %d\n", common,
    count_a < count_b and path_a or path_b, path_a, count_a, path_b, count_b))
  os.exit(1)
end

if arg[1] == "frame" and #arg == 3 then
  print_frame(arg[2], arg[3])
elseif arg[1] == "compare" and #arg == 3 then
  compare(arg[2], arg[3])
else
  fail("usage: frame <capture> <k> | compare <capture a> <capture b>")
end
