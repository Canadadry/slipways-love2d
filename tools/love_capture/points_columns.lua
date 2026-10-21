local fractions = {0, 1 / 1024, 3 / 1024, 1 / 256, 0.25, 0.5, 0.75, 1 - 1 / 256, 1 - 3 / 1024, 1 - 1 / 1024, 1 - 1 / 512, 0.5 + 1 / 512}
for k = -3, 3 do fractions[#fractions + 1] = 1 / 512 + k * 2 ^ -17 end
return function()
    local row = 0
    for _, f in ipairs(fractions) do
        for phase = 0, 2 do
            for c = phase, 160, 3 do love.graphics.points(c + f, row + 0.5) end
            row = row + 1
        end
    end
end
