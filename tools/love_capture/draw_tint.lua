local data = love.image.newImageData(160, 1)
for x = 0, 159 do
    data:setPixel(x, 0, x / 255, (x + 96) / 255, (255 - x) / 255)
end
local ramp = love.graphics.newImage(data)

return function(canvas)
    local graphics = love.graphics
    local row = graphics.newQuad(0, 0, 160, 1, 160, 1)
    for y = 0, 119 do
        graphics.setColor(y / 255, (120 + y) / 255, (255 - y) / 255)
        graphics.draw(ramp, row, 0, y)
    end
end
