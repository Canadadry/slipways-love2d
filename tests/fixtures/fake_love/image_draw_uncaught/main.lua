local image, quad

function love.load()
    image = love.graphics.newImage(love.image.newImageData(2, 2))
    quad = love.graphics.newQuad(0, 0, 2, 2, 2, 2)
end

function love.draw()
    love.graphics.draw(image, quad, 0.5, 0)
end

function debug_state()
    return "ran\n"
end
