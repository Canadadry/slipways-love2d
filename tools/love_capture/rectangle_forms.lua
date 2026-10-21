return function(canvas)
    local graphics = love.graphics
    graphics.clear(0.2, 0.4, 0.6, 0)
    graphics.rectangle("fill", 10, 10, 20, 10)
    graphics.rectangle("fill", 20, 15, 20, 10)
    graphics.rectangle("fill", 50, 10, 4, 4)
    graphics.rectangle("fill", 50, 10, 4, 4)
    assert(not pcall(graphics.rectangle, "full", 60, 10, 5, 5))
    assert(not pcall(graphics.rectangle, "fill", 60, 20, 5))
    for i = 0, 999 do
        graphics.rectangle("fill", i % 40 * 4, 60 + math.floor(i / 40) % 25 * 2, 3, 1)
    end
    graphics.setCanvas()
    graphics.rectangle("fill", 0, 30, 160, 20)
    graphics.setCanvas(canvas)
end
