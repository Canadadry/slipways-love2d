table.insert(log, "main")

function love.load(args)
    table.insert(log, "load " .. type(args))
end

function love.update(dt)
    table.insert(log, dt == 1 / 60 and "update 1/60" or "update " .. tostring(dt))
end

function love.draw()
    table.insert(log, "draw")
end

function debug_state()
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
