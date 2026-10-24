-- Image slots: the LÖVE side of p8.gfx_software's slot store. Each
-- build becomes one image: a canvas is never read back.
local fatal = require("p8.fatal")
local gfx_software = require("p8.gfx_software")
local vram = require("p8.vram")
local screen = require("love.screen")

local M = {}

-- Indexed by slot: { image, width, height } of its last build, nil for a
-- build of zero width or height.
local slot_images = {}

-- Turns the slot's last software build into its image: a fresh ImageData is
-- alpha 0 everywhere, so pixels the build never wrote are skipped.
function M.build(slot)
  local store, first, width, height, empty = gfx_software.slot_pixels(slot)
  slot_images[slot] = nil
  if width == 0 or height == 0 then
    return
  end
  local data = love.image.newImageData(width, height)
  for y = 0, height - 1 do
    local row = first + y * width
    for x = 0, width - 1 do
      local col = store[row + x]
      if col ~= empty then
        if col > 15 then
          fatal.raise(string.format("love backend image slot %d: colour %d is not 0..15", slot, col))
        end
        local color = screen.COLORS[col]
        data:setPixel(x, y, color.red, color.green, color.blue, 1)
      end
    end
  end
  slot_images[slot] = { image = love.graphics.newImage(data), width = width, height = height }
end

-- Draws the slot's image with its top-left at screen (sx, sy), clipped to the
-- screen.
function M.draw(slot, sx, sy)
  local slot_image = slot_images[slot]
  if slot_image == nil then
    return
  end
  local width = slot_image.width
  local height = slot_image.height
  local ix_first = math.max(0, -sx)
  local ix_end = math.min(width, vram.SCREEN_WIDTH - sx)
  local iy_first = math.max(0, -sy)
  local iy_end = math.min(height, vram.SCREEN_HEIGHT - sy)
  if ix_first >= ix_end or iy_first >= iy_end then
    return
  end
  local quad = love.graphics.newQuad(ix_first, iy_first, ix_end - ix_first, iy_end - iy_first, width, height)
  love.graphics.setColor(1, 1, 1)
  love.graphics.draw(slot_image.image, quad, sx + ix_first, sy + iy_first)
end

return M
