-- Plays scene.txt through game/p8/ with the LÖVE backend, frame k of the
-- scene in the k-th love.draw. tests/test_love_runtime.c plays the same file
-- through the C runtime and compares the frames and the meter's counts.
local bit = require("bit")
local draw_meter = require("p8.draw_meter")
local gfx_module = require("p8.gfx")
local image = require("p8.image")
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

-- The text of a print line, as tests/test_love_runtime.c's split_text cuts
-- and decodes it; returns the line without the text, then the text.
local function split_text(line, line_number)
  local head, raw = line:match("^(%s*%S+%s+%S+%s+%S+%s+%S+) (.*)$")
  if head == nil then
    error(string.format("scene.txt:%d: print takes x y c, one space, then the text", line_number), 0)
  end
  local text = raw:gsub("\\(.?.?.?)", function(escape)
    local hex = escape:match("^x(%x%x)$")
    if hex == nil or tonumber(hex, 16) == 0 then
      error(string.format("scene.txt:%d: a backslash in text must start \\xNN, NN not 00", line_number), 0)
    end
    return string.char(tonumber(hex, 16))
  end)
  return head, text
end

-- Returns the frames, each a list of calls { operation, line_number, values }.
-- A build is one call whose build_calls are the calls up to its end.
local function parse_scene(text)
  local parsed = {}
  local calls = {}
  local build = nil
  local line_number = 0
  for line in (text .. "\n"):gmatch("([^\n]*)\n") do
    line_number = line_number + 1
    line = line:gsub("#.*", "")
    local text = nil
    local operation = line:match("^%s*(%S+)")
    if operation == "print" or operation == "print_outlined" then
      line, text = split_text(line, line_number)
    end
    local tokens = {}
    for token in line:gmatch("%S+") do
      tokens[#tokens + 1] = token
    end
    if #tokens == 0 then
      goto continue
    end
    if tokens[1] == "end" then
      if build == nil or #tokens ~= 1 then
        error(string.format("scene.txt:%d: end outside a build", line_number), 0)
      end
      calls = build
      build = nil
      goto continue
    end
    if tokens[1] == "frame" then
      if build ~= nil then
        error(string.format("scene.txt:%d: frame inside a build", line_number), 0)
      end
      parsed[#parsed + 1] = calls
      calls = {}
      goto continue
    end
    do
      local values = {}
      for i = 2, #tokens do
        values[i - 2] = parse_integer(tokens[i], line_number)
      end
      local call = {
        operation = tokens[1], line_number = line_number, values = values, count = #tokens - 1, text = text,
      }
      calls[#calls + 1] = call
      if call.operation == "build" then
        if build ~= nil then
          error(string.format("scene.txt:%d: build inside a build", line_number), 0)
        end
        call.build_calls = {}
        build = calls
        calls = call.build_calls
      end
    end
    ::continue::
  end
  if build ~= nil then
    error("scene.txt: build without end", 0)
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

local play_call

local function draw_build(_, _, build_calls)
  for _, call in ipairs(build_calls) do
    play_call(call)
  end
end

function play_call(call)
  local v = call.values
  local operation = call.operation
  if operation == "cls" then
    require_count(call, 0)
    gfx_module.cls(mem)
  elseif operation == "camera" then
    require_count(call, 2)
    gfx_module.camera(gfx, v[0], v[1])
  elseif operation == "pset" then
    require_count(call, 3)
    gfx_module.pset(gfx, mem, v[0], v[1], v[2])
  elseif operation == "psets" then
    if call.count % 3 ~= 0 then
      error(string.format("scene.txt:%d: psets takes x y c triples", call.line_number), 0)
    end
    local points = {}
    for i = 1, call.count / 3 do
      local first = 3 * (i - 1)
      points[i] = { x = v[first], y = v[first + 1], col = v[first + 2] }
    end
    gfx_module.psets(gfx, mem, points, call.count / 3)
  elseif operation == "line" then
    require_count(call, 5)
    gfx_module.line(gfx, mem, v[0], v[1], v[2], v[3], v[4])
  elseif operation == "lines" then
    if call.count % 4 ~= 1 then
      error(string.format("scene.txt:%d: lines takes a colour then x1 y1 x2 y2 quadruples", call.line_number), 0)
    end
    local segments = {}
    local count = (call.count - 1) / 4
    for i = 1, count do
      local first = 1 + 4 * (i - 1)
      segments[i] = { x1 = v[first], y1 = v[first + 1], x2 = v[first + 2], y2 = v[first + 3] }
    end
    gfx_module.lines(gfx, mem, segments, count, v[0])
  elseif operation == "rect" then
    require_count(call, 5)
    gfx_module.rect(gfx, mem, v[0], v[1], v[2], v[3], v[4])
  elseif operation == "circ" then
    require_count(call, 4)
    gfx_module.circ(gfx, mem, v[0], v[1], v[2], v[3])
  elseif operation == "circfill" then
    require_count(call, 4)
    gfx_module.circfill(gfx, mem, v[0], v[1], v[2], v[3])
  elseif operation == "fillp" then
    require_count(call, 1)
    gfx_module.fillp(gfx, v[0])
  elseif operation == "fillp_transparent" then
    require_count(call, 1)
    gfx_module.fillp_transparent(gfx, v[0])
  elseif operation == "fillp_reset" then
    require_count(call, 0)
    gfx_module.fillp_reset(gfx)
  elseif operation == "rectfill" then
    require_count(call, 5)
    gfx_module.rectfill(gfx, mem, v[0], v[1], v[2], v[3], v[4])
  elseif operation == "palt" then
    require_count(call, 2)
    gfx_module.palt(mem, v[0], v[1] ~= 0)
  elseif operation == "palette" then
    require_count(call, 2)
    if v[0] < 0 or v[0] > 15 then
      error(string.format("scene.txt:%d: palette takes a colour 0..15", call.line_number), 0)
    end
    mem.draw_palette[v[0]] = bit.band(v[1], 0xFF)
  elseif operation == "spr" then
    require_count(call, 5)
    gfx_module.spr(gfx, mem, v[0], v[1], v[2], v[3], v[4])
  elseif operation == "mset" then
    require_count(call, 3)
    gfx_module.mset(mem, v[0], v[1], v[2])
  elseif operation == "map" then
    require_count(call, 6)
    gfx_module.map(gfx, mem, v[0], v[1], v[2], v[3], v[4], v[5])
  elseif operation == "print" then
    require_count(call, 3)
    gfx_module.print(gfx, mem, call.text, v[0], v[1], v[2])
  elseif operation == "print_outlined" then
    require_count(call, 3)
    gfx_module.print_outlined(gfx, mem, call.text, v[0], v[1], v[2])
  elseif operation == "build" then
    require_count(call, 3)
    image.build(gfx, mem, v[0], v[1], v[2], draw_build, call.build_calls)
  elseif operation == "image" then
    require_count(call, 3)
    image.draw(gfx, mem, v[0], v[1], v[2])
  else
    error(string.format("scene.txt:%d: unknown operation %s", call.line_number, operation), 0)
  end
end

function love.load()
  screen.screen_init()
  vram.init(mem)
  gfx_module.init(gfx)
  sprite_sheet.load(mem)
  gfx_module.draw_backend_set(backend.backend_get())
  draw_meter.install()
  frames = parse_scene(love.filesystem.read("scene.txt"))
end

function love.draw()
  assert(next_frame < #frames, "scene.txt: the replay has more frames than the scene")
  screen.screen_frame_begin()
  for _, call in ipairs(frames[next_frame + 1]) do
    play_call(call)
  end
  screen.screen_frame_end()
  draw_meter.frame_end()
  next_frame = next_frame + 1
end

local function format_counts(counts)
  return string.format("%d %d %d %d %d\n", counts.bytes, counts.build_bytes, counts.primitives, counts.cluts,
    counts.patterns)
end

function debug_state()
  return format_counts(draw_meter.last_frame()) .. format_counts(draw_meter.peak())
end
