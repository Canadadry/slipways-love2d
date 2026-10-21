local graphics = love.graphics

local function show(data, size, x0, y0)
    for y = 0, size - 1 do
        for x = 0, size - 1 do
            local r, g, b, a = data:getPixel(x, y)
            graphics.setColor(r, g, b)
            graphics.rectangle("fill", x0 + x, y0 + y, 1, 1)
            graphics.setColor(a, a, a)
            graphics.rectangle("fill", x0 + size + x, y0 + y, 1, 1)
        end
    end
end

local function mark(ok, x, y)
    if ok then
        graphics.setColor(1, 0, 0.5)
    else
        graphics.setColor(0, 1, 0.5)
    end
    graphics.rectangle("fill", x, y, 1, 1)
end

return function(canvas)
    graphics.clear(0.2, 0.4, 0.6, 0)

    local fresh = love.image.newImageData(8, 8)
    show(fresh, 8, 0, 0)
    local count = select("#", fresh:getPixel(7, 7))
    graphics.setColor(count / 255, count / 255, count / 255)
    graphics.rectangle("fill", 16, 0, 1, 1)

    local first = love.image.newImageData(16, 16)
    first:setPixel(2.7, 1.2, 1, 0, 0)
    first:setPixel(0.999, 15.999, 0, 1, 0, 1)
    first:setPixel(-0, 3, 0, 0, 1, 1)
    first:setPixel(15.5, 0, 2, -1, 1e30, -1e30)
    first:setPixel(4, 4, 0.2, 0.4, 0.6, 0)
    first:setPixel(5, 5, 1, 1, 1, 1)
    first:setPixel(5, 5, 0.1, 0.2, 0.3, 0.5)
    first:setPixel(6, 6, 0.5, 0.5, 0.5, 0.5)
    first:setPixel(7, 7, first:getPixel(2.9, 1.99))
    show(first, 16, 0, 10)

    local second = love.image.newImageData(16, 16)
    second:setPixel(2, 1, 0, 1, 1, 1)
    show(second, 16, 32, 10)
    show(first, 16, 64, 10)

    local ramp = love.image.newImageData(16, 16)
    for byte = 0, 255 do
        local v = byte / 255
        ramp:setPixel(byte % 16, math.floor(byte / 16), v, v, v, v)
    end
    for byte = 0, 255 do
        local x, y = byte % 16, math.floor(byte / 16)
        local v = byte / 255
        local r, g, b, a = ramp:getPixel(x, y)
        mark(r == v and g == v and b == v and a == v, 100 + x, 10 + y)
        graphics.setColor(r, g, b)
        graphics.rectangle("fill", 100 + x, 30 + y, 1, 1)
    end

    local outside = {-1, -0.5, -1e-300, 16, 16.5, 1 / 0, -1 / 0, 0 / 0, 2 ^ 31, 2 ^ 32 + 3, -2 ^ 32 + 3, 1e300}
    for i, v in ipairs(outside) do
        mark(pcall(first.setPixel, first, v, 0, 1, 1, 1, 1), i - 1, 50)
        mark(pcall(first.setPixel, first, 0, v, 1, 1, 1, 1), i - 1, 51)
        mark(pcall(first.getPixel, first, v, 0), i - 1, 52)
        mark(pcall(first.getPixel, first, 0, v), i - 1, 53)
    end
    for i, v in ipairs({0, -0, 0.5, 15, 15.999}) do
        mark(pcall(first.getPixel, first, v, v), 19 + i, 52)
    end
    mark(pcall(love.image.newImageData, 0, 4), 0, 55)
    mark(pcall(love.image.newImageData, 4, -1), 1, 55)
    mark(pcall(first.setPixel, first, 0, 0, 1, 0), 2, 55)
    mark(pcall(first.getPixel, first, 0), 3, 55)
    mark(pcall(first.setPixel, first, 0, 0, 1, 0, true), 4, 55)

    show(first, 16, 0, 60)
end
