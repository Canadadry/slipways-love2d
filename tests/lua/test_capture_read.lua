-- Usage, from the repository root: luajit tests/lua/test_capture_read.lua
local FRAME_SIZE = 9600
local DIR = "tmp/test_capture_read"

local function expect(actual, expected, what)
  if actual ~= expected then
    error(string.format("%s: expected %s, got %s", what, tostring(expected), tostring(actual)), 2)
  end
end

-- frames: a list of { [byte offset] = byte } patches over an all-zero frame.
local function write_capture(name, frames)
  local path = DIR .. "/" .. name
  local file = assert(io.open(path, "wb"))
  for _, patches in ipairs(frames) do
    local bytes = {}
    for i = 1, FRAME_SIZE do
      bytes[i] = string.char(patches[i - 1] or 0)
    end
    file:write(table.concat(bytes))
  end
  file:close()
  return path
end

-- Returns the tool's stdout, stderr and exit status.
local function capture_read(args)
  local err_path = DIR .. "/stderr.txt"
  local pipe = assert(io.popen("luajit tools/capture_read.lua " .. args .. " 2>" .. err_path .. "; echo $?"))
  local out, status = pipe:read("*a"):match("^(.-)(%d+)\n$")
  pipe:close()
  local err_file = assert(io.open(err_path, "rb"))
  local err = err_file:read("*a")
  err_file:close()
  return out, err, tonumber(status)
end

local function lines_of(text)
  local lines = {}
  for line in text:gmatch("([^\n]*)\n") do
    lines[#lines + 1] = line
  end
  return lines
end

local function test_frame_prints_one_hex_digit_per_pixel_low_nibble_first()
  local path = write_capture("two_frames.bin", { {}, { [0] = 0x21, [80] = 0xf0, [FRAME_SIZE - 1] = 0xc0 } })
  local out, err, status = capture_read("frame " .. path .. " 1")
  expect(status, 0, "status")
  expect(err, "", "stderr")
  local lines = lines_of(out)
  expect(#lines, 120, "line count")
  for y, line in ipairs(lines) do
    expect(#line, 160, "length of line " .. y)
  end
  expect(lines[1]:sub(1, 3), "120", "row 0 starts with x 0 = 1, x 1 = 2")
  expect(lines[2]:sub(1, 2), "0f", "row 1 starts with x 0 = 0, x 1 = f")
  expect(lines[120]:sub(159, 160), "0c", "row 119 ends with x 158 = 0, x 159 = c")
  expect(out:gsub("[^0]", ""):len(), 160 * 120 - 4, "zero pixels")
end

local function test_frame_refuses_a_frame_past_the_end_or_a_torn_capture()
  local path = write_capture("one_frame.bin", { {} })
  local out, err, status = capture_read("frame " .. path .. " 1")
  expect(status, 2, "status past the end")
  expect(out, "", "stdout past the end")
  expect(err, "capture_read: " .. path .. ": no frame f1, the capture has 1 frames\n", "stderr past the end")
  out, err, status = capture_read("frame " .. path .. " -1")
  expect(status, 2, "status for a negative frame")
  local torn = DIR .. "/torn.bin"
  local file = assert(io.open(torn, "wb"))
  file:write(string.rep("\0", FRAME_SIZE + 1))
  file:close()
  out, err, status = capture_read("frame " .. torn .. " 0")
  expect(status, 2, "status torn")
  expect(err, "capture_read: " .. torn .. ": 9601 bytes, not a whole number of 9600-byte frames\n", "stderr torn")
end

local function test_compare_names_the_first_differing_frame_and_its_pixel_count()
  local a = write_capture("compare_a.bin", { {}, { [7] = 0x11 }, { [9] = 0x30 }, {} })
  local b = write_capture("compare_b.bin", { {}, { [7] = 0x12, [100] = 0x01 }, {}, { [0] = 1 } })
  local out, err, status = capture_read("compare " .. a .. " " .. b)
  expect(status, 1, "status")
  expect(err, "", "stderr")
  expect(out, "first difference: f1, 2 pixels differ\n", "stdout")
end

local function test_compare_says_two_equal_captures_are_identical()
  local a = write_capture("same_a.bin", { {}, { [7] = 0x11 } })
  local b = write_capture("same_b.bin", { {}, { [7] = 0x11 } })
  local out, err, status = capture_read("compare " .. a .. " " .. b)
  expect(status, 0, "status")
  expect(out, "identical: 2 frames\n", "stdout")
end

local function test_compare_says_which_capture_is_shorter_when_their_common_frames_match()
  local short = write_capture("short.bin", { { [7] = 0x11 } })
  local long = write_capture("long.bin", { { [7] = 0x11 }, { [3] = 0x40 } })
  local out, _, status = capture_read("compare " .. long .. " " .. short)
  expect(status, 1, "status")
  expect(out, "identical over 1 frames, then " .. short .. " ends: " .. long .. " has 2 frames, " .. short .. " 1\n", "stdout")
  local empty = write_capture("empty.bin", {})
  out, _, status = capture_read("compare " .. empty .. " " .. short)
  expect(status, 1, "status against an empty capture")
  expect(out, "identical over 0 frames, then " .. empty .. " ends: " .. empty .. " has 0 frames, " .. short .. " 1\n", "stdout against an empty capture")
end

os.execute("mkdir -p " .. DIR)

local tests = {
  { "frame prints one hex digit per pixel, low nibble first", test_frame_prints_one_hex_digit_per_pixel_low_nibble_first },
  { "frame refuses a frame past the end or a torn capture", test_frame_refuses_a_frame_past_the_end_or_a_torn_capture },
  { "compare names the first differing frame and its pixel count", test_compare_names_the_first_differing_frame_and_its_pixel_count },
  { "compare says two equal captures are identical", test_compare_says_two_equal_captures_are_identical },
  { "compare says which capture is shorter when their common frames match", test_compare_says_which_capture_is_shorter_when_their_common_frames_match },
}
local failures = 0
for _, test in ipairs(tests) do
  local ok, err = pcall(test[2])
  if not ok then
    failures = failures + 1
    io.stderr:write(string.format("test_capture_read: %s: %s\n", test[1], tostring(err)))
  end
end
os.exit(failures == 0 and 0 or 1)
