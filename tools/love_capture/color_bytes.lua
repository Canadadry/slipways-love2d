local offsets = {-0.5, -1e-6, 0, 1e-6, 0.25, 0.49, 0.5, 0.51, 0.75, 0.999}

return function()
    local graphics = love.graphics
    for k = 0, 9599 do
        local byte, family = k % 256, math.floor(k / 256)
        local x, y = k % 160, math.floor(k / 160)
        graphics.setColor(byte / 255, (byte + 0.5) / 255, (byte + offsets[family % 10 + 1]) / 255)
        graphics.rectangle("fill", x, y, 1, 1)
        graphics.points(x + 0.5, y + 60.5)
    end
end
