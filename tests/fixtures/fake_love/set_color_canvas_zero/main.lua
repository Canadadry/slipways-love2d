local game_canvas

function love.load()
    game_canvas = love.graphics.newCanvas(160, 120)
end

function love.draw()
    love.graphics.setCanvas(game_canvas)
    love.graphics.clear(0, 0, 0)
    love.graphics.setColor(0.3, 0, 0)
    love.graphics.rectangle("fill", 2, 3, 4, 4)
    love.graphics.setCanvas()
    love.graphics.setColor(1, 1, 1)
end

function debug_state()
    return "ran\n"
end
