local opaque_data = love.image.newImageData(2, 2)
for y = 0, 1 do
    for x = 0, 1 do
        opaque_data:setPixel(x, y, 1, 1, 1, 1)
    end
end
local opaque = love.graphics.newImage(opaque_data)
local later_data = love.image.newImageData(2, 2)
local later = love.graphics.newImage(later_data)
for y = 0, 1 do
    for x = 0, 1 do
        later_data:setPixel(x, y, 1, 1, 1, 1)
    end
end

return function(canvas)
    local graphics = love.graphics
    local whole = graphics.newQuad(0, 0, 2, 2, 2, 2)
    assert(not pcall(graphics.draw, opaque, whole, 10.5, 10))
    assert(not pcall(graphics.draw, opaque, graphics.newQuad(0, 0, 2, 2, 4, 4), 10, 10))
    assert(not pcall(graphics.draw, opaque, whole, 10, 10, 0))
    graphics.draw(later, whole, 20, 20)
    graphics.setCanvas()
    graphics.draw(opaque, whole, 30, 30)
    graphics.setCanvas(canvas)
end
