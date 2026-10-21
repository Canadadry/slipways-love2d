local case = os.getenv("FAKE_LOVE_UNLISTED")
if case == "love.timer" then
    local timer = love.timer
elseif case == "love.keypressed" then
    function love.keypressed() end
elseif case == "love.graphics.extra" then
    love.graphics.extra = 1
end

local canvas

function love.load()
    canvas = love.graphics.newCanvas(160, 120)
    if case == "love.graphics.getWidth" then
        love.graphics.getWidth()
    elseif case == "love.filesystem.lines" then
        love.filesystem.lines("save.bin")
    elseif case == "ImageData:mapPixel" then
        love.image.newImageData(1, 1):mapPixel(function() end)
    elseif case == "Quad:getViewport" then
        love.graphics.newQuad(0, 0, 1, 1, 1, 1):getViewport()
    end
end

function love.update()
    if case == "Joystick:getName" then
        love.joystick.getJoysticks()[1]:getName()
    end
end

function love.draw()
    if case == "Canvas:getWidth" then
        canvas:getWidth()
    end
end
