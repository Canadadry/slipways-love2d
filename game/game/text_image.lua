local gfx = require("p8.gfx")
local image = require("p8.image")
local image_slot = require("game.image_slot")

local M = {}

-- The outline's pixel on each side of the glyphs: 4 a character plus 1 wide, 7 high.
local OUTLINE = 1
local TEXT_IMAGE_HEIGHT = gfx.P8_FONT_GLYPH_HEIGHT + 2 * OUTLINE

local g_slots = {}
for i = 1, image_slot.IMAGE_SLOT_TEXT_COUNT do
  g_slots[i] = { key = { col = 0, length = 0, text = "" }, stamp = 0, frame = 0 }
end
local g_stamp = 0
local g_frame = 1

local function image_width(length)
  return length * gfx.P8_FONT_CELL_WIDTH + OUTLINE
end

-- A NUL byte ends the text.
local function text_length(str)
  return (string.find(str, "\0", 1, true) or #str + 1) - 1
end

-- true when str fits a text slot: text_image_print_outlined then copies it, unless every slot was
-- used this frame.
function M.text_image_cacheable(str)
  local length = text_length(str)
  return length > 0 and image_width(length) <= image.p8_image_size(image_slot.IMAGE_SLOT_TEXT_FIRST).width
end

-- Starts a draw frame: a slot copied earlier in the frame is never rebuilt before the next call.
function M.text_image_frame_begin()
  g_frame = g_frame + 1
end

local function same_key(a, b)
  return a.col == b.col and a.length == b.length and a.text == b.text
end

-- Returns the slot holding key and true, else the least recently used slot not copied this
-- frame and false, else nil and false.
local function slot_for(key)
  local oldest = nil
  for i = 1, image_slot.IMAGE_SLOT_TEXT_COUNT do
    if g_slots[i].key.length > 0 and same_key(g_slots[i].key, key) then
      return i, true
    end
    if g_slots[i].frame ~= g_frame and (oldest == nil or g_slots[i].stamp < g_slots[oldest].stamp) then
      oldest = i
    end
  end
  return oldest, false
end

-- Draws no visual random number and reads no game state.
local function text_image_build_draw(graphics, mem, context)
  local key = context
  gfx.p8_print_outlined(graphics, mem, key.text, OUTLINE, OUTLINE, key.col)
end

-- p8_print_outlined's contract (same pixels, same draw palette after), through a text slot keyed
-- by str and col when str is cacheable and a slot is free. gfx's camera moves during a build and
-- is restored.
function M.text_image_print_outlined(graphics, mem, str, x, y, col)
  if not M.text_image_cacheable(str) then
    gfx.p8_print_outlined(graphics, mem, str, x, y, col)
    return
  end
  local key = { col = 0, length = 0, text = "" }
  key.col = col
  key.length = text_length(str)
  key.text = str:sub(1, key.length)
  local i, built = slot_for(key)
  if i == nil then
    gfx.p8_print_outlined(graphics, mem, str, x, y, col)
    return
  end
  if not built then
    g_slots[i].key = { col = key.col, length = key.length, text = key.text }
    image.p8_image_build(graphics, mem, image_slot.IMAGE_SLOT_TEXT_FIRST + i - 1, image_width(key.length),
      TEXT_IMAGE_HEIGHT, text_image_build_draw, g_slots[i].key)
  end
  g_stamp = g_stamp + 1
  g_slots[i].stamp = g_stamp
  g_slots[i].frame = g_frame
  image.p8_image_draw(graphics, mem, image_slot.IMAGE_SLOT_TEXT_FIRST + i - 1, x - OUTLINE, y - OUTLINE)
end

return M
