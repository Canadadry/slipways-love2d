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
