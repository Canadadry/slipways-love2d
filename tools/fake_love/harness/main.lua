local canvas, draw

function love.load(args)
    canvas = love.graphics.newCanvas(160, 120)
    local program = assert(loadfile(args[1]))
    draw = program()
    assert(type(draw) == "function", args[1] .. " returned a " .. type(draw) .. ", not a function")
end

function love.draw()
    love.graphics.setCanvas(canvas)
    draw(canvas)
    love.graphics.setCanvas()
end
