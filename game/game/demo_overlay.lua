-- Copy of src/game/demo_overlay.h then src/game/demo_overlay.c.
--
-- A line buffer char[DEMO_OVERLAY_LINE_LENGTH + 1] is a Lua string; a pointer into the caption is its 1-based position.
local cint = require("p8.cint")
local gfx_module = require("p8.gfx")
local vram = require("p8.vram")
local demo_data = require("game.demo_data")

local M = {}

local DEMO_OVERLAY_LINE_LENGTH = 32

local BAND_FILL = 1
local TEXT_COLOR = 7
local TAG_FILL = 8
local PADDING = 1
local LINE_GAP = 1
local TOP_HUD_HEIGHT = 11
local BOTTOM_HUD_HEIGHT = 11
local BAND_BOTTOM = vram.P8_SCREEN_HEIGHT - BOTTOM_HUD_HEIGHT - 1
local BAND_HEIGHT = 2 * PADDING + 2 * gfx_module.P8_FONT_GLYPH_HEIGHT + LINE_GAP
local BAND_TOP = BAND_BOTTOM - BAND_HEIGHT + 1

M.TAG_TEXT = "demo"

function M.demo_overlay_find_caption(captions, count, frame)
  for i = 0, count - 1 do
    if captions[i].from <= frame and frame < captions[i].to then
      return i
    end
  end
  return -1
end

local function text_width(length)
  return length * gfx_module.P8_FONT_CELL_WIDTH - 1
end

local function print_centred(gfx, mem, line, y)
  local x = cint.div(vram.P8_SCREEN_WIDTH - text_width(#line), 2)
  gfx_module.p8_print(gfx, mem, line, x, y, TEXT_COLOR)
end

local function draw_tag(gfx, mem)
  local right = vram.P8_SCREEN_WIDTH - 1
  local left = right - text_width(#M.TAG_TEXT) - 2 * PADDING + 1
  local bottom = TOP_HUD_HEIGHT + gfx_module.P8_FONT_GLYPH_HEIGHT + 2 * PADDING - 1
  gfx_module.p8_rectfill(gfx, mem, left, TOP_HUD_HEIGHT, right, bottom, TAG_FILL)
  gfx_module.p8_print(gfx, mem, M.TAG_TEXT, left + PADDING, TOP_HUD_HEIGHT + PADDING, TEXT_COLOR)
end

-- Returns the line: the length bytes of text from position start.
local function copy_line(text, start, length)
  assert(length <= DEMO_OVERLAY_LINE_LENGTH,
    "a caption line over 32 characters: the generator refuses it; a hand-made unit would overflow the line buffer")
  return string.sub(text, start, start + length - 1)
end

local function draw_caption(gfx, mem, text)
  gfx_module.p8_rectfill(gfx, mem, 0, BAND_TOP, vram.P8_SCREEN_WIDTH - 1, BAND_BOTTOM, BAND_FILL)
  local newline = string.find(text, "\n", 1, true)
  if newline == nil then
    local first = copy_line(text, 1, #text)
    print_centred(gfx, mem, first, BAND_TOP + cint.div(BAND_HEIGHT - gfx_module.P8_FONT_GLYPH_HEIGHT, 2))
    return
  end
  assert(string.find(text, "\n", newline + 1, true) == nil, "a caption of more than 2 lines: the generator refuses it")
  local first = copy_line(text, 1, newline - 1)
  local second = copy_line(text, newline + 1, #text - newline)
  print_centred(gfx, mem, first, BAND_TOP + PADDING)
  print_centred(gfx, mem, second, BAND_TOP + PADDING + gfx_module.P8_FONT_GLYPH_HEIGHT + LINE_GAP)
end

-- Lua only: a by-value p8_graphics_t.
local function graphics_copy(gfx)
  return {
    camera_x = gfx.camera_x,
    camera_y = gfx.camera_y,
    fillp_pattern = gfx.fillp_pattern,
    fillp_transparent = gfx.fillp_transparent,
    pen_color = gfx.pen_color,
  }
end

function M.demo_overlay_draw(gfx, mem, frame)
  if frame < 0 then
    return
  end
  local own = graphics_copy(gfx)
  gfx_module.p8_camera_reset(own)
  gfx_module.p8_fillp_reset(own)
  draw_tag(own, mem)
  local caption = M.demo_overlay_find_caption(demo_data.demo_data_captions, demo_data.demo_data_caption_count, frame)
  if caption >= 0 then
    draw_caption(own, mem, demo_data.demo_data_captions[caption].text)
  end
end

return M
