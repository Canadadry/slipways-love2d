local function new_data(width, height, texel)
    local data = love.image.newImageData(width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local r, g, b, a = texel(x, y)
            data:setPixel(x, y, r / 255, g / 255, b / 255, a)
        end
    end
    return data
end

local sheet_data = new_data(128, 128, function(x, y)
    return (x * 37 + y * 11) % 256, (x * 5 + y * 29 + 100) % 256, (x * x + y) % 256, (x * 7 + y * 3) % 5 == 0 and 0 or 1
end)
local sheet = love.graphics.newImage(sheet_data)
sheet_data:setPixel(0, 0, 1, 1, 1, 1)
sheet_data:setPixel(1, 0, 0, 0, 0, 1)
local other = love.graphics.newImage(new_data(128, 128, function(x, y)
    return (x * 3) % 256, (y * 2) % 256, 200, (x + y) % 4 == 0 and 0 or 1
end))
local cell = love.graphics.newImage(new_data(3, 5, function(x, y)
    return 255, 241, 232, (x + y) % 2
end))
local font = love.graphics.newImage(new_data(48, 30, function(x, y)
    return x * 5, y * 8, 77, (x * y) % 3 == 0 and 1 or 0
end))
local strip = love.graphics.newImage(new_data(1024, 4, function(x, y)
    return x % 256, math.floor(x / 4), y * 60, x % 7 == 0 and 0 or 1
end))

return function(canvas)
    local graphics = love.graphics
    graphics.clear(0.2, 0.4, 0.6, 0)
    graphics.setColor(1, 0.5, 0)
    graphics.rectangle("fill", 0, 60, 160, 60)
    graphics.setColor(1, 1, 1)

    local corner = graphics.newQuad(0, 0, 16, 16, 128, 128)
    graphics.draw(sheet, corner, 0, 0)
    graphics.draw(sheet, graphics.newQuad(112, 112, 16, 16, 128, 128), 20, 0)
    graphics.draw(sheet, graphics.newQuad(0, 0, 128, 128, 128, 128), -120, 40)
    local texel = graphics.newQuad(5, 7, 1, 1, 128, 128)
    for k = 0, 7 do
        graphics.draw(sheet, texel, 40 + k, 0)
        graphics.draw(sheet, graphics.newQuad(5 + k, 7, 1, 1, 128, 128), 40 + k, 1)
    end
    local block = graphics.newQuad(30, 40, 24, 9, 128, 128)
    graphics.draw(sheet, block, 150, 20)
    graphics.draw(sheet, block, 60, 115)
    graphics.draw(sheet, block, 70, -5)
    graphics.draw(sheet, block, -24, 30)
    graphics.draw(sheet, block, 160, 30)
    graphics.draw(sheet, block, 40, 120)
    graphics.draw(sheet, block, 2 ^ 20 - 1, 0)
    graphics.draw(sheet, block, -(2 ^ 20 - 1), 0)
    graphics.draw(sheet, block, -0, 100)
    graphics.draw(other, block, 80, 70)
    graphics.draw(other, corner, 136, 60)
    graphics.draw(sheet, corner, 136, 60)
    graphics.draw(sheet, corner, 136, 60)
    local whole_cell = graphics.newQuad(0, 0, 3, 5, 3, 5)
    for k = 0, 5 do
        graphics.draw(cell, whole_cell, 90 + k * 2, 80 + k % 2)
        graphics.draw(cell, whole_cell, 90 + k * 2, 20 + k % 2)
    end
    graphics.draw(font, graphics.newQuad(45, 25, 3, 5, 48, 30), 130, 90)
    graphics.draw(font, graphics.newQuad(0, 0, 48, 30, 48, 30), 100, 30)
    graphics.draw(strip, graphics.newQuad(0, 0, 1024, 4, 1024, 4), -1000, 50)
    graphics.draw(strip, graphics.newQuad(0, 0, 1024, 4, 1024, 4), -1000, 56)

    graphics.setCanvas()
    graphics.draw(sheet, corner, 100, 0)
    graphics.setCanvas(canvas)

    graphics.setColor(0.5, 1, 0.25)
    graphics.draw(sheet, corner, 120, 40)
    graphics.draw(sheet, corner, 120, 100)
    graphics.setColor(1, 1, 1)
end
