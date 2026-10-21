return function()
    local graphics = love.graphics
    graphics.rectangle("fill", 0, 0, 1, 1)
    graphics.rectangle("fill", 159, 0, 1, 1)
    graphics.rectangle("fill", 0, 119, 1, 1)
    graphics.rectangle("fill", 159, 119, 1, 1)
    graphics.rectangle("fill", 10, 10, 5, 3)
    graphics.rectangle("fill", 25, 10, -5, -3)
    graphics.rectangle("fill", 30, 10, 0, 3)
    graphics.rectangle("fill", 35, 10, 3, 0)
    graphics.rectangle("fill", 40, 10, 0, 0)
    graphics.rectangle("fill", 45, 10, 2, 2)
    graphics.rectangle("fill", 46, 11, 2, 2)
    for column = 0, 159 do
        graphics.rectangle("fill", column, 20 + column % 4 * 2, 1, 1 + column % 2)
    end
    for row = 0, 119 do
        graphics.rectangle("fill", 60 + row % 4 * 2, row, 1 + row % 2, 1)
    end
end
