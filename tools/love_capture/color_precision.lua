local factors = {1 - 2 ^ -30, 1 - 2 ^ -26, 1 - 2 ^ -25, 1 - 2 ^ -24, 1 - 2 ^ -23, 1 - 2 ^ -22, 1 - 2 ^ -21, 1 + 2 ^ -24, 1 + 2 ^ -22}

return function()
    local graphics = love.graphics
    for k = 0, 255 * #factors - 1 do
        local byte, factor = k % 255 + 1, factors[math.floor(k / 255) + 1]
        local value = byte / 255 * factor
        graphics.setColor(value, value, value)
        graphics.rectangle("fill", k % 160, math.floor(k / 160), 1, 1)
    end
end
