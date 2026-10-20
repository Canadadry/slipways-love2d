return function(canvas)
    local graphics = love.graphics
    local data = love.image.newImageData(160, 60)
    local offsets = {-0.5, -1e-6, 0, 1e-6, 0.25, 0.49, 0.5 - 1e-9, 0.5, 0.5 + 1e-9, 0.51, 0.75, 0.999}
    for k = 0, 9599 do
        local byte, family = k % 256, math.floor(k / 256)
        local function component(channel)
            return (byte + offsets[(family + 3 * channel) % #offsets + 1]) / 255
        end
        data:setPixel(k % 160, math.floor(k / 160), component(0), component(1), component(2), component(3))
    end
    for y = 0, 59 do
        for x = 0, 159 do
            local r, g, b, a = data:getPixel(x, y)
            graphics.setColor(r, g, b)
            graphics.rectangle("fill", x, y, 1, 1)
            graphics.setColor(a, a, a)
            graphics.rectangle("fill", x, y + 60, 1, 1)
        end
    end
end
