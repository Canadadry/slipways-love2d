return function(canvas)
    local graphics = love.graphics
    graphics.clear(0.2, 0.4, 0.6, 0)
    graphics.points(10.5, 2.5, 12.5, 2.5, 14.5, 2.5)
    graphics.points({10.5, 4.5, 12.5, 4.5, 14.5, 4.5})
    assert(not pcall(graphics.points, 30.5, 4.5, 32.5))
    graphics.points()
    graphics.points({})
    graphics.points(20.5, 2.5, 20.5, 2.5)
    local band = {}
    for i = 0, 9999 do
        band[#band + 1] = i % 160 + 0.5
        band[#band + 1] = 60 + math.floor(i / 160) % 50 + 0.5
    end
    graphics.points(band)
    graphics.setCanvas()
    graphics.points(30.5, 2.5)
    graphics.setCanvas(canvas)
end
