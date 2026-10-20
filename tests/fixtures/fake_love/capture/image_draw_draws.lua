local data = love.image.newImageData(4, 3)
for y = 0, 2 do
    for x = 0, 3 do
        local alpha = ((x == 1 and y == 1) or (x == 3 and y == 0)) and 0 or 1
        data:setPixel(x, y, (x * 60 + 10) / 255, (y * 80 + 20) / 255, ((x + y) * 30 + 5) / 255, alpha)
    end
end
local image = love.graphics.newImage(data)

return function()
    local graphics = love.graphics
    local whole = graphics.newQuad(0, 0, 4, 3, 4, 3)
    graphics.draw(image, whole, -1, -1)
    graphics.draw(image, whole, 10, 10)
    graphics.draw(image, whole, 158, 118)
    graphics.draw(image, graphics.newQuad(2, 1, 2, 2, 4, 3), 30, 10)
    graphics.setColor(0.5, 1, 0.25)
    graphics.draw(image, whole, 20, 10)
    graphics.setColor(1, 1, 1)
end
