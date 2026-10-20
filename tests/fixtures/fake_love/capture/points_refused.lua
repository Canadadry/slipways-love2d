return function(canvas)
    assert(not pcall(love.graphics.points, 10.5, 10.5, 0 / 0, 1))
    love.graphics.setCanvas()
    love.graphics.points(30.5, 2.5)
    love.graphics.setCanvas(canvas)
end
