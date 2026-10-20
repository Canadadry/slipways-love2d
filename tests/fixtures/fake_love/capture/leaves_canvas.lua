local other = love.graphics.newCanvas(160, 120)

function love.draw()
    love.graphics.setCanvas(other)
end

return function() end
