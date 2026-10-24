local log = {conf_saw}
local frame = 0
local canvas
local labels = {}
local label_count = 0

local function label(id)
    if labels[id] == nil then
        label_count = label_count + 1
        labels[id] = string.char(64 + label_count)
        local adds = pcall(function() return id + 1 end)
        table.insert(log, string.format("new %s %s adds %s", labels[id], type(id), tostring(adds)))
    end
    return labels[id]
end

local function input()
    local words = {"touches"}
    for _, id in ipairs(love.touch.getTouches()) do
        local x, y = love.touch.getPosition(id)
        table.insert(words, string.format("%s %s,%s", label(id), tostring(x), tostring(y)))
    end
    local x, y = love.mouse.getPosition()
    table.insert(words, string.format("mouse %s,%s %s", tostring(x), tostring(y),
        love.mouse.isDown(1) and "down" or "up"))
    return table.concat(words, " ")
end

table.insert(log, string.format("main new list %s %s", tostring(love.touch.getTouches() ~= love.touch.getTouches()),
    input()))

function love.load()
    canvas = love.graphics.newCanvas(160, 120)
    love.graphics.setCanvas(canvas)
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
    table.insert(log, "load " .. input())
end

function love.resize()
    table.insert(log, string.format("f%d resize %s", frame, input()))
end

function love.gamepadpressed(_, name)
    table.insert(log, string.format("f%d pressed %s %s", frame, name, input()))
end

function love.gamepadreleased(_, name)
    table.insert(log, string.format("f%d released %s %s", frame, name, input()))
end

function love.update()
    table.insert(log, string.format("f%d update %s", frame, input()))
end

function love.draw()
    love.graphics.setCanvas(canvas)
    table.insert(log, string.format("f%d draw %s", frame, input()))
    love.graphics.setCanvas()
    frame = frame + 1
end

function debug_state()
    table.insert(log, "end " .. input())
    return table.concat(log, "\n") .. "\n"
end
