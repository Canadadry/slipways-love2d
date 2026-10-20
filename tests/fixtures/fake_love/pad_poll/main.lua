local log = {}
local frame = 0
local joystick

local function keys(t)
    local names = {}
    for key in pairs(t) do
        table.insert(names, key)
    end
    table.sort(names)
    return table.concat(names, " ")
end

function love.load()
    local first = love.joystick.getJoysticks()
    local second = love.joystick.getJoysticks()
    joystick = first[1]
    table.insert(log, string.format("love.joystick %s, new list %s, same joystick %s, count %d",
        keys(love.joystick), tostring(first ~= second), tostring(joystick == second[1]), #second))
end

function love.update()
    table.insert(log, string.format("f%d dpup or b %s, guide %s, leftstick or rightstick %s", frame,
        tostring(joystick:isGamepadDown("dpup", "b")), tostring(joystick:isGamepadDown("guide")),
        tostring(joystick:isGamepadDown("leftstick", "rightstick"))))
    frame = frame + 1
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
