local graphics = love.graphics
local image, quad

function love.load()
    graphics.setCanvas(graphics.newCanvas(160, 120))
    graphics.clear(0, 0, 0)
    graphics.setCanvas()
    local data = love.image.newImageData(1, 1)
    data:setPixel(0, 0, 1, 1, 1, 1)
    image = graphics.newImage(data)
    quad = graphics.newQuad(0, 0, 1, 1, 1, 1)
end

function love.draw()
    graphics.setColor(0.3, 0.6, 0.9, 0.5)
    graphics.points(1, 1)
    graphics.rectangle("fill", 0, 0, 4, 4)
    graphics.draw(image, quad, 0, 0)
    graphics.setColor(1, 1, 1)
end

function debug_state()
    return "ran\n"
end
