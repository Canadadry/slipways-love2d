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

local function checker(x, y)
    if (x + y) % 2 == 0 then
        return 255, 241, 232, (x * 3 + y) % 7 == 0 and 0 or 1
    end
    return 29, 43, 83, 1
end
local function stripes(x, y)
    return (x * 37) % 256, (y * 53 + 40) % 256, x % 2 * 255, (x + 2 * y) % 5 == 0 and 0 or 1
end

local graphics = love.graphics
local before = graphics.newImage(new_data(16, 12, checker))
graphics.setDefaultFilter("nearest", "nearest")
local after = graphics.newImage(new_data(16, 12, checker))
local wide = graphics.newImage(new_data(64, 6, stripes))
local keyed = graphics.newImage(love.image.newImageData("files/rgb_key.png"))

return function(canvas)
    graphics.clear(0.4, 0.1, 0.2, 0)
    graphics.setColor(0, 0.5, 0.25)
    graphics.rectangle("fill", 0, 80, 160, 40)
    graphics.setDefaultFilter("nearest", "nearest")
    local late = graphics.newImage(new_data(16, 12, checker))
    graphics.setColor(1, 1, 1)

    local whole = graphics.newQuad(0, 0, 16, 12, 16, 12)
    local inner = graphics.newQuad(3, 2, 9, 7, 16, 12)
    local texel = graphics.newQuad(5, 4, 1, 1, 16, 12)
    for column, image in ipairs({before, after, late}) do
        local left = (column - 1) * 20
        graphics.draw(image, whole, left, 0)
        graphics.draw(image, inner, left, 14)
        for k = 0, 5 do
            graphics.draw(image, texel, left + k * 2, 24 + k % 2)
        end
        graphics.draw(image, whole, left + 2, 74)
        graphics.draw(image, whole, left - 7 + column, -5)
    end
    graphics.draw(after, whole, -9, 40)
    graphics.draw(after, whole, 153, 40)
    graphics.draw(after, whole, 70, 115)
    graphics.draw(after, whole, 2 ^ 20 - 1, 40)
    graphics.draw(after, whole, -(2 ^ 20 - 1), 40)

    local strip = graphics.newQuad(0, 0, 64, 6, 64, 6)
    local slice = graphics.newQuad(17, 1, 23, 4, 64, 6)
    graphics.draw(wide, strip, 64, 2)
    graphics.draw(wide, slice, 64, 10)
    graphics.draw(wide, strip, 120, 30)
    graphics.draw(keyed, graphics.newQuad(0, 0, 19, 9, 19, 9), 70, 30)
    graphics.draw(keyed, graphics.newQuad(4, 2, 11, 5, 19, 9), 70, 42)

    graphics.setColor(1, 0.5, 0.25)
    graphics.draw(after, whole, 90, 50)
    graphics.draw(before, whole, 110, 50)
    graphics.draw(wide, slice, 64, 90)
    graphics.setColor(41 / 255, 173 / 255, 1)
    graphics.draw(keyed, graphics.newQuad(0, 0, 19, 9, 19, 9), 130, 90)
    graphics.setColor(1, 1, 1)

    graphics.setCanvas()
    graphics.setDefaultFilter("nearest", "nearest")
    graphics.draw(after, whole, 0, 100)
    graphics.setCanvas(canvas)
end
