-- Copy of src/game/button_image.h then src/game/button_image.c.
--
-- Shapes, with the C field names:
--   button_image_key_t: { words }, words 0-indexed, four 32-bit words. The caller owns its
--     meaning: two equal keys must mean the same pixels.
--   button_image_slot_t: { key, used, stamp, frame }.
--   button_image_pool_t: { first, count }: slots [first, first + count) of g_slots, image slot
--     IMAGE_SLOT_BUTTON_ICON_FIRST + index.
-- The unsigned long counters are Lua numbers: exact to 2^53, the C's wrap is unreachable.
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
for i = 0, BUTTON_IMAGE_SLOT_COUNT - 1 do
  g_slots[i] = { key = { words = { [0] = 0, 0, 0, 0 } }, used = 0, stamp = 0, frame = 0 }
end
M.g_icon_pool = { first = 0, count = image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT }
M.g_wide_pool = { first = image_slot.IMAGE_SLOT_BUTTON_ICON_COUNT, count = image_slot.IMAGE_SLOT_BUTTON_WIDE_COUNT }
local g_stamp = 0
local g_frame = 1

-- Starts a draw frame: a slot copied earlier in the frame is never rebuilt before the next call.
function M.button_image_frame_begin()
  g_frame = g_frame + 1
end

-- memcmp over uint32_t words: bit.tobit on both sides, whatever sign a caller's word carries.
local function same_key(a, b)
  for k = 0, 3 do
    if bit.tobit(a.words[k]) ~= bit.tobit(b.words[k]) then
      return false
    end
  end
  return true
end

-- Returns the slot holding key and 1, else the least recently used slot not copied this
-- frame and 0, else -1 and 0. Never writes pool.
local function slot_for(pool, key)
  local oldest = -1
  for i = pool.first, pool.first + pool.count - 1 do
    local slot = g_slots[i]
    if slot.used ~= 0 and same_key(slot.key, key) then
      return i, 1
    end
    if slot.frame ~= g_frame and (oldest < 0 or slot.stamp < g_slots[oldest].stamp) then
      oldest = i
    end
  end
  return oldest, 0
end

-- Draws key's image, w x h, top-left at world (x, y) under gfx's camera, built through draw on the
-- first frame it is seen (image pixel (0, 0) is the image's top-left; gfx's camera moves during the
-- build and is restored). An image at most BUTTON_IMAGE_ICON_WIDTH wide never evicts a wider one,
-- nor the reverse. Returns 0, drawing nothing, when w x h is over BUTTON_IMAGE_WIDE_WIDTH x
-- BUTTON_IMAGE_HEIGHT or every slot of its pool was copied this frame.
function M.button_image_draw(graphics, mem, key, x, y, w, h, draw, context)
  if w <= 0 or h <= 0 or w > M.BUTTON_IMAGE_WIDE_WIDTH or h > M.BUTTON_IMAGE_HEIGHT then
    return 0
  end
  local pool
  if w <= M.BUTTON_IMAGE_ICON_WIDTH then
    pool = M.g_icon_pool
  else
    pool = M.g_wide_pool
  end
  local i, built = slot_for(pool, key)
  if i < 0 then
    return 0
  end
  local slot = g_slots[i]
  if built == 0 then
    slot.key = { words = { [0] = key.words[0], key.words[1], key.words[2], key.words[3] } }
    slot.used = 1
    image.p8_image_build(graphics, mem, image_slot.IMAGE_SLOT_BUTTON_ICON_FIRST + i, w, h, draw, context)
  end
  g_stamp = g_stamp + 1
  slot.stamp = g_stamp
  slot.frame = g_frame
  image.p8_image_draw(graphics, mem, image_slot.IMAGE_SLOT_BUTTON_ICON_FIRST + i, x, y)
  return 1
end

return M
