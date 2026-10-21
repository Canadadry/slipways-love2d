local step = 2 ^ -17
local deltas = {0, step, -step, 3 * step, 1 / 1024, -1 / 1024, -1 / 512,
    1 / 512 - step, 1 / 512, 1 / 512 + step, 1 / 256, -1 / 256, 0.5 - step, -0.5 + step}
return function()
    local rectangle = love.graphics.rectangle
    local line = 0
    for _, delta in ipairs(deltas) do
        for phase = 0, 3 do
            for c = phase, 161, 4 do
                rectangle("fill", c + 0.5 + delta, line, 1.75, 1)
                rectangle("fill", c - 1.25, line + 4, 1.25 + 0.5 + delta, 1)
            end
            line = line + 1
        end
        line = line + 4
    end
    for phase = 0, 3 do
        for c = phase, 161, 4 do
            rectangle("fill", c + 0.3, line, 0.2 + 1 / 512 + 1e-6, 1)
            rectangle("fill", c + 0.1, line + 4, 0.4 + 1 / 512 + 1.6e-6, 1)
        end
        line = line + 1
    end
end
