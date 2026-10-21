local fractions = {}
for i = 0, 15 do fractions[#fractions + 1] = i / 16 end
for _, f in ipairs({1e-7, 1e-6, 1e-4, 1 / 1024, 3 / 1024, 1 / 256, 1 / 128, 0.5 - 1e-6, 0.5 + 1e-6, 1 - 1e-6}) do
    fractions[#fractions + 1] = f
end
return function()
    local n = 0
    for _, fy in ipairs(fractions) do
        for _, fx in ipairs(fractions) do
            love.graphics.points(3 * (n % 53) + 1 + fx, 3 * math.floor(n / 53) + 1 + fy)
            n = n + 1
        end
    end
end
