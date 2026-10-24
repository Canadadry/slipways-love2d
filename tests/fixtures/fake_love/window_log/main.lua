local log = {}
local frame = 0
local canvas

local function size()
    local width, height = love.graphics.getDimensions()
    return string.format("%sx%s", tostring(width), tostring(height))
end

table.insert(log, "main " .. size())

-- In its own closure, so luaL_error names this line rather than pcall.
local ok, message = pcall(function() love.graphics.getDimensions(1) end)
table.insert(log, string.format("argument %s %s", tostring(ok), tostring(message)))

function love.load()
    canvas = love.graphics.newCanvas(160, 120)
    love.graphics.setCanvas(canvas)
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
    table.insert(log, "load " .. size())
end

if os.getenv("FAKE_LOVE_NO_RESIZE") == nil then
    function love.resize(width, height)
        table.insert(log, string.format("f%d resize %s %s %s %s", frame, type(width), tostring(width),
            tostring(height), size()))
    end
end

function love.gamepadpressed(_, name)
    table.insert(log, string.format("f%d pressed %s %s", frame, name, size()))
end

function love.gamepadreleased(_, name)
    table.insert(log, string.format("f%d released %s %s", frame, name, size()))
end

function love.update()
    table.insert(log, string.format("f%d update %s", frame, size()))
end

function love.draw()
    love.graphics.setCanvas(canvas)
    table.insert(log, string.format("f%d draw %s", frame, size()))
    love.graphics.setCanvas()
    frame = frame + 1
end

function debug_state()
    table.insert(log, "end " .. size())
    return table.concat(log, "\n") .. "\n"
end
