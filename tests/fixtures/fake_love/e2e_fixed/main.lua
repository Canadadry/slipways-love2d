local graphics = love.graphics

local game_canvas

function love.load()
    game_canvas = graphics.newCanvas(160, 120)
end

function love.draw()
    graphics.setCanvas(game_canvas)
    graphics.clear(0x1D / 255, 0x2B / 255, 0x53 / 255)
    graphics.setColor(0xFF / 255, 0x00 / 255, 0x4D / 255)
    graphics.rectangle("fill", 0, 0, 2, 1)
    graphics.setCanvas()
end
