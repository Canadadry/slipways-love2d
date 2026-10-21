return function(canvas)
    assert(not pcall(love.graphics.rectangle, "fill", 10, 10, 0 / 0, 5))
    love.graphics.setCanvas()
    love.graphics.rectangle("fill", 0, 30, 160, 20)
    love.graphics.setCanvas(canvas)
end
