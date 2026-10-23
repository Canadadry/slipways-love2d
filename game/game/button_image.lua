local bit = require("bit")
local image = require("p8.image")
local image_slot = require("game.image_slot")

local M = {}

M.BUTTON_IMAGE_HEIGHT = 11
M.BUTTON_IMAGE_ICON_WIDTH = 16
M.BUTTON_IMAGE_WIDE_WIDTH = 112

local BUTTON_IMAGE_SLOT_COUNT = image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT + image_slot.IMAGE_SLOT_BUTTON_WIDE_COUNT

assert(image_slot.IMAGE_SLOT_BUTTON_WIDE_FIRST
  == image_slot.IMAGE_SLOT_BUTTON_ICON_FIRST + image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT,
  "button_image_slots_follow_each_other")

local g_slots = {}
for i = 1, BUTTON_IMAGE_SLOT_COUNT do
  g_slots[i] = { key = { words = { 0, 0, 0, 0 } }, used = false, stamp = 0, frame = 0 }
end
M.g_icon_pool = { first = 1, count = image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT }
M.g_wide_pool = { first = image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT + 1, count = image_slot.IMAGE_SLOT_BUTTON_WIDE_COUNT }
local g_stamp = 0
local g_frame = 1

-- Starts a draw frame: a slot copied earlier in the frame is never rebuilt before the next call.
function M.button_image_frame_begin()
  g_frame = g_frame + 1
end

-- Compares the four words through bit.tobit on both sides, whatever sign a caller's word carries.
local function same_key(a, b)
  for k = 1, 4 do
    if bit.tobit(a.words[k]) ~= bit.tobit(b.words[k]) then
      return false
    end
  end
  return true
end

-- Returns the slot holding key and true, else the least recently used slot not copied this
-- frame and false, else nil and false. Never writes pool.
local function slot_for(pool, key)
  local oldest = nil
  for i = pool.first, pool.first + pool.count - 1 do
    local slot = g_slots[i]
    if slot.used and same_key(slot.key, key) then
      return i, true
    end
    if slot.frame ~= g_frame and (oldest == nil or slot.stamp < g_slots[oldest].stamp) then
      oldest = i
    end
  end
  return oldest, false
end

-- key is { words }, four 32-bit words; the caller owns its meaning: two equal keys must mean the same pixels.
-- Draws key's image, w x h, top-left at world (x, y) under gfx's camera, built through draw on the
-- first frame it is seen (image pixel (0, 0) is the image's top-left; gfx's camera moves during the
-- build and is restored). An image at most BUTTON_IMAGE_ICON_WIDTH wide never evicts a wider one,
-- nor the reverse. Returns false, drawing nothing, when w x h is over BUTTON_IMAGE_WIDE_WIDTH x
-- BUTTON_IMAGE_HEIGHT or every slot of its pool was copied this frame.
function M.button_image_draw(graphics, mem, key, x, y, w, h, draw, context)
  if w <= 0 or h <= 0 or w > M.BUTTON_IMAGE_WIDE_WIDTH or h > M.BUTTON_IMAGE_HEIGHT then
    return false
  end
  local pool
  if w <= M.BUTTON_IMAGE_ICON_WIDTH then
    pool = M.g_icon_pool
  else
    pool = M.g_wide_pool
  end
  local i, built = slot_for(pool, key)
  if i == nil then
    return false
  end
  local slot = g_slots[i]
  if not built then
    slot.key = { words = { key.words[1], key.words[2], key.words[3], key.words[4] } }
    slot.used = true
    image.p8_image_build(graphics, mem, image_slot.IMAGE_SLOT_BUTTON_ICON_FIRST + i - 1, w, h, draw, context)
  end
  g_stamp = g_stamp + 1
  slot.stamp = g_stamp
  slot.frame = g_frame
  image.p8_image_draw(graphics, mem, image_slot.IMAGE_SLOT_BUTTON_ICON_FIRST + i - 1, x, y)
  return true
end

return M
