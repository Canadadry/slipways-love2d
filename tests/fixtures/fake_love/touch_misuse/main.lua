local misuse = os.getenv("FAKE_LOVE_MISUSE")
local last = {}
local log = {}

-- Each misuse sits in its own closure, so luaL_error names its line here rather than pcall.
local MISUSES = {
    {"number id", function() love.touch.getPosition(1) end},
    {"touches argument", function() love.touch.getTouches(1) end},
    {"right button", function() love.mouse.isDown(2) end},
    {"no button", function() love.mouse.isDown() end},
    {"mouse argument", function() love.mouse.getPosition(1) end},
    {"callback", function() love.touchpressed = function() end end},
    {"pressure", function() love.touch.getPressure(1) end},
}

function love.update()
    if misuse == "lifted" then
        for _, id in ipairs(last) do
            love.touch.getPosition(id)
        end
        last = love.touch.getTouches()
    elseif #log == 0 then
        for _, entry in ipairs(MISUSES) do
            local ok, message = pcall(entry[2])
            table.insert(log, string.format("%s %s %s", entry[1], tostring(ok), tostring(message)))
        end
    end
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end

-- tmp/fake_love fails a frame that ends with no canvas 0 or a non-palette pixel on it.
function love.load()
    love.graphics.setCanvas(love.graphics.newCanvas(160, 120))
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
end
