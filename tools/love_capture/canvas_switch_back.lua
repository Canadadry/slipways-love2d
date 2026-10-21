local other = love.graphics.newCanvas(160, 120)

return function(canvas)
    love.graphics.setCanvas(other)
    love.graphics.clear(1, 0, 0, 1)
    love.graphics.setCanvas(canvas)
    love.graphics.clear(0, 1, 0, 1)
end
