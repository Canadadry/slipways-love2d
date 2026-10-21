-- Plays scene.txt through game/p8/ with the LÖVE backend, frame k of the
-- scene in the k-th love.draw. tests/test_love_runtime.c plays the same file
-- through the C runtime and compares the frames.
local gfx_module = require("p8.gfx")
local sprite_sheet = require("p8.sprite_sheet")
local vram = require("p8.vram")
local backend = require("love.backend")
local screen = require("love.screen")

local gfx = {}
local mem = {}
local frames = {}
local next_frame = 0

-- Decimal or 0x hex, with an optional minus sign.
local function parse_integer(token, line_number)
  local negative = token:sub(1, 1) == "-"
  local digits = negative and token:sub(2) or token
  local value
  if digits:match("^0[xX]%x+$") then
    value = tonumber(digits:sub(3), 16)
  elseif digits:match("^%d+$") then
    value = tonumber(digits, 10)
  end
  if value == nil then
    error(string.format("scene.txt:%d: not an integer: %s", line_number, token), 0)
  end
  return negative and -value or value
end

-- Returns the frames, each a list of calls { operation, line_number, values }.
local function parse_scene(text)
  local parsed = {}
  local calls = {}
  local line_number = 0
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do
    line_number = line_number + 1
    line = line:gsub("#.*", "")
    local tokens = {}
    for token in line:gmatch("%S+") do
      tokens[#tokens + 1] = token
    end
    if #tokens == 0 then
      goto continue
    end
    if tokens[1] == "frame" then
      parsed[#parsed + 1] = calls
      calls = {}
      goto continue
    end
    do
      local values = {}
      for i = 2, #tokens do
        values[i - 2] = parse_integer(tokens[i], line_number)
      end
      calls[#calls + 1] = { operation = tokens[1], line_number = line_number, values = values, count = #tokens - 1 }
    end
    ::continue::
  end
  if #calls > 0 then
    error("scene.txt: calls after the last frame", 0)
  end
  return parsed
end

local function require_count(call, count)
  if call.count ~= count then
    error(string.format("scene.txt:%d: wrong argument count for %s", call.line_number, call.operation), 0)
  end
end

local function play_call(call)
  local v = call.values
  local operation = call.operation
  if operation == "cls" then
    require_count(call, 0)
    gfx_module.p8_cls(mem)
  elseif operation == "camera" then
    require_count(call, 2)
    gfx_module.p8_camera(gfx, v[0], v[1])
  elseif operation == "pset" then
    require_count(call, 3)
    gfx_module.p8_pset(gfx, mem, v[0], v[1], v[2])
  elseif operation == "psets" then
    if call.count % 3 ~= 0 then
      error(string.format("scene.txt:%d: psets takes x y c triples", call.line_number), 0)
    end
    local points = {}
    for i = 0, call.count / 3 - 1 do
      points[i] = { x = v[3 * i], y = v[3 * i + 1], col = v[3 * i + 2] }
    end
    gfx_module.p8_psets(gfx, mem, points, call.count / 3)
  elseif operation == "rectfill" then
    require_count(call, 5)
    gfx_module.p8_rectfill(gfx, mem, v[0], v[1], v[2], v[3], v[4])
  else
    error(string.format("scene.txt:%d: unknown operation %s", call.line_number, operation), 0)
  end
end

function love.load()
  screen.screen_init()
  vram.p8_vram_init(mem)
  gfx_module.p8_gfx_init(gfx)
  sprite_sheet.p8_sprite_sheet_load(mem)
  gfx_module.p8_draw_backend_set(backend.backend_get())
  frames = parse_scene(love.filesystem.read("scene.txt"))
end

function love.draw()
  assert(next_frame < #frames, "scene.txt: the replay has more frames than the scene")
  screen.screen_frame_begin()
  for _, call in ipairs(frames[next_frame + 1]) do
    play_call(call)
  end
  screen.screen_frame_end()
  next_frame = next_frame + 1
end
