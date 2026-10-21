local misuse = os.getenv("FAKE_LOVE_MISUSE")
local joystick = love.joystick.getJoysticks()[1]

function love.gamepadreleased()
    if misuse == "raise" then
        error("raised in love.gamepadreleased")
    end
end

function love.update()
    if misuse == "unknown name" then
        joystick:isGamepadDown("select")
    elseif misuse == "dot" then
        joystick.isGamepadDown("a")
    elseif misuse == "no name" then
        joystick:isGamepadDown()
    elseif misuse == "number" then
        joystick:isGamepadDown("a", 5)
    end
end

function debug_state()
    return "ran\n"
end

-- tmp/fake_love fails a frame that ends with no canvas 0 or a non-palette pixel on it.
local fixture_load = love.load
function love.load(...)
    love.graphics.setCanvas(love.graphics.newCanvas(160, 120))
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
    if fixture_load ~= nil then
        return fixture_load(...)
    end
end
