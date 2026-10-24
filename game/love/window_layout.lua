local vram = require("p8.vram")

local M = {}

M.MODE_TALL = "tall"
M.MODE_OVERLAY = "overlay"

M.PAD_WIDTH = 160
M.PAD_HEIGHT = 120

local function fit(content_width, content_height, area_x, area_y, area_width, area_height)
  local scale = math.min(area_width / content_width, area_height / content_height)
  local width = content_width * scale
  local height = content_height * scale
  return {
    x = area_x + (area_width - width) / 2,
    y = area_y + (area_height - height) / 2,
    width = width,
    height = height,
    scale = scale,
  }
end

local function check_size(value, argument)
  assert(type(value) == "number" and value > 0,
    string.format("window_layout.compute: %s must be a number > 0, got %s", argument, tostring(value)))
end

function M.compute(width, height)
  check_size(width, "width")
  check_size(height, "height")
  if height > 2 * width then
    local half = height / 2
    return {
      mode = M.MODE_TALL,
      game = fit(vram.SCREEN_WIDTH, vram.SCREEN_HEIGHT, 0, 0, width, half),
      pad = fit(M.PAD_WIDTH, M.PAD_HEIGHT, 0, half, width, half),
    }
  end
  local game = fit(vram.SCREEN_WIDTH, vram.SCREEN_HEIGHT, 0, 0, width, height)
  return {
    mode = M.MODE_OVERLAY,
    game = game,
    pad = fit(M.PAD_WIDTH, M.PAD_HEIGHT, game.x, game.y, game.width, game.height),
  }
end

return M
