local bit = require("bit")
local vec = require("engine.vec")
local hud = require("game.hud")
local image_slot = require("game.image_slot")
local layout = require("game.layout")
local fixed = require("p8.fixed")
local gfx_module = require("p8.gfx")
local image = require("p8.image")

local M = {}

local ROW_IMAGE_HEIGHT = gfx_module.SPRITE_SIZE
local SPRITE_COUNT = 256
local BLOCK_COUNT = 16

local function u32(x)
  return x % 4294967296
end

-- (a * b) mod 2^32 for a, b below 2^32: the full product passes 2^53, so b times each 16-bit half.
local function mul_u32(a, b)
  local lo = a % 65536
  local hi = (a - lo) / 65536
  return u32(lo * b + u32(hi * b) % 65536 * 65536)
end

-- A key's entries are numbers in 0 .. 2^32 - 1, read up to count only.
local function key_zero()
  return { mem = nil, hash = 0, count = 0, extent = 0, entries = {} }
end

local function key_copy(key)
  local copy = key_zero()
  copy.mem = key.mem
  copy.hash = key.hash
  copy.count = key.count
  copy.extent = key.extent
  for i = 1, key.count do
    copy.entries[i] = key.entries[i]
  end
  return copy
end

local g_slots = {}
for i = 1, image_slot.ROW_COUNT do
  g_slots[i] = { key = key_zero(), stamp = 0 }
end
local g_stamp = 0

local function row_image_width()
  return image.size(image_slot.ROW_FIRST).width
end

local function entry_of(item)
  return u32(bit.bor(fixed.flr_int(item.sprite_id), bit.lshift(fixed.flr_int(item.sprite_palette_block), 8),
    bit.lshift(fixed.flr_int(item.offset.x), 12)))
end

-- Fills key from the row; returns false when the row cannot be copied exactly.
local function key_of(row, mem, key)
  local hash

  key.mem = mem
  key.count = 0
  key.extent = 0
  if row == nil or row.placed_items == nil then
    return false
  end
  for i = 1, row.placed_items_count do
    local item = row.placed_items[i]
    if item.kind == layout.ROW_ITEM_NONE then
      goto continue
    end
    if item.kind ~= layout.ROW_ITEM_SPRITE then
      return false
    end
    do
      local sprite = fixed.flr_int(item.sprite_id)
      local block = fixed.flr_int(item.sprite_palette_block)
      local x = fixed.flr_int(item.offset.x)
      if fixed.from_int(x) ~= item.offset.x or item.offset.y ~= 0 or x < 0
        or x + gfx_module.SPRITE_SIZE > row_image_width() or sprite < 0 or sprite >= SPRITE_COUNT or block < 0
        or block >= BLOCK_COUNT then
        return false
      end
      local n = key.count + 1
      key.entries[n] = entry_of(item)
      key.count = n
      if x + gfx_module.SPRITE_SIZE > key.extent then
        key.extent = x + gfx_module.SPRITE_SIZE
      end
    end
    ::continue::
  end
  if key.count == 0 then
    return false
  end
  -- The hash mixes in 0 for mem: same_key compares mem by identity, and every entry.
  hash = u32(bit.bxor(2166136261, 0))
  for i = 1, key.count do
    hash = mul_u32(u32(bit.bxor(hash, key.entries[i])), 16777619)
  end
  key.hash = hash
  return true
end

-- true when row_image.draw_at copies the row from an image slot: only sprite and nothing items, at
-- least one sprite, each at a whole, non-negative x offset, a y offset of 0, within a slot.
function M.cacheable(row)
  local key = key_zero()
  return key_of(row, nil, key)
end

local function same_key(a, b)
  if not (a.hash == b.hash and a.mem == b.mem and a.count == b.count) then
    return false
  end
  for i = 1, a.count do
    if a.entries[i] ~= b.entries[i] then
      return false
    end
  end
  return true
end

-- Draws no visual random number and reads no game state: only the row's own items, each at its
-- whole offset, so at (offset, 0) whatever the caller's alignment.
local function row_image_build_draw(gfx, mem, context)
  layout.row_draw_at(gfx, mem, context, vec.make(0, 0), 0)
end

local function slot_for(key)
  local oldest = 1
  for i = 1, image_slot.ROW_COUNT do
    if g_slots[i].key.mem ~= nil and same_key(g_slots[i].key, key) then
      return i, true
    end
    if g_slots[i].stamp < g_slots[oldest].stamp then
      oldest = i
    end
  end
  return oldest, false
end

-- layout.row_draw_at's contract (same pixels, same draw palette after), through an image slot
-- keyed by the row's content when the row is cacheable. gfx's camera moves during a build and is
-- restored.
function M.draw_at(gfx, mem, row, p, a)
  local key = key_zero()
  local built
  local i
  local origin

  if not key_of(row, mem, key) then
    layout.row_draw_at(gfx, mem, row, p, a)
    return
  end
  i, built = slot_for(key)
  if not built then
    image.build(gfx, mem, image_slot.ROW_FIRST + i - 1, key.extent, ROW_IMAGE_HEIGHT,
      row_image_build_draw, row)
    g_slots[i].key = key_copy(key)
  end
  g_stamp = g_stamp + 1
  g_slots[i].stamp = g_stamp
  origin = vec.sub(p, vec.make(fixed.mul(a, row.width), 0))
  image.draw(gfx, mem, image_slot.ROW_FIRST + i - 1, fixed.flr_int(origin.x),
    fixed.flr_int(origin.y))
  hud.select_palette_block_default(mem)
end

return M
