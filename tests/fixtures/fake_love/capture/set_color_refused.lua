love.graphics.setColor(1, 0, 0)

return function()
    assert(not pcall(love.graphics.setColor, "0", 1, 0))
    assert(not pcall(love.graphics.setColor, {0, 1, 0}))
    love.graphics.rectangle("fill", 0, 0, 10, 10)
end
