local data

function love.load()
    data = love.image.newImageData(160, 120)
end

function love.draw()
    data:setPixel(160, 0, 1, 1, 1)
end

function debug_state()
    return "ran\n"
end
