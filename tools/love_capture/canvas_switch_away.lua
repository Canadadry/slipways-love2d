local other = love.graphics.newCanvas(160, 120)

return function()
    love.graphics.clear(0, 0, 1, 1)
    love.graphics.setCanvas(other)
    love.graphics.clear(1, 0, 0, 1)
    love.graphics.setCanvas()
    love.graphics.clear(0, 1, 0, 1)
end
