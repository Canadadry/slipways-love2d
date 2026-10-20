local fractions = {0, 1e-7, 1e-6, 1e-4, 1 / 1024, 3 / 1024, 1 / 256, 1 / 128, 1 / 4,
    0.5 - 1 / 256, 0.5 - 3 / 1024, 0.5 - 1 / 1024, 0.5 - 1e-6, 0.5, 0.5 + 1e-6, 0.5 + 1 / 1024,
    0.5 + 3 / 1024, 0.5 + 1 / 256, 3 / 4, 1 - 1e-6}
return function()
    local n = 0
    for _, low in ipairs(fractions) do
        for _, high in ipairs(fractions) do
            local cell_x, cell_y = 5 * (n % 32), 5 * math.floor(n / 32)
            local size = 2 + high - low
            if n % 2 == 0 then
                love.graphics.rectangle("fill", cell_x + 1 + low, cell_y + 1 + low, size, size)
            else
                love.graphics.rectangle("fill", cell_x + 3 + high, cell_y + 3 + high, -size, -size)
            end
            n = n + 1
        end
    end
end
