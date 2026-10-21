local NAMES = {
    "dpup", "dpdown", "dpleft", "dpright", "a", "b",
    "x", "y", "leftshoulder", "rightshoulder", "start", "back",
}
local log = {}
local frame = 0
local joystick

local function held()
    local words = {"held"}
    for _, name in ipairs(NAMES) do
        if joystick:isGamepadDown(name) then
            table.insert(words, name)
        end
    end
    return table.concat(words, " ")
end

local function log_change(kind, argument, name)
    table.insert(log, string.format("f%d %s %s down %s same %s", frame, kind, name,
        tostring(joystick:isGamepadDown(name)), tostring(argument == love.joystick.getJoysticks()[1])))
end

function love.load()
    local joysticks = love.joystick.getJoysticks()
    joystick = joysticks[1]
    table.insert(log, string.format("load joysticks %d a %s", #joysticks, tostring(joystick:isGamepadDown("a"))))
end

function love.gamepadreleased(argument, name)
    log_change("released", argument, name)
end

function love.gamepadpressed(argument, name)
    log_change("pressed", argument, name)
end

function love.update()
    table.insert(log, string.format("f%d update %s", frame, held()))
end

function love.draw()
    table.insert(log, string.format("f%d draw %s", frame, held()))
    frame = frame + 1
end

function debug_state()
    table.insert(log, "end " .. held())
    return table.concat(log, "\n") .. "\n"
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
