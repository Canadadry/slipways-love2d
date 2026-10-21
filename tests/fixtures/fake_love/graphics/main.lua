local names = {}
for name in pairs(love.graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "main graphics " .. table.concat(names, " "))

local canvas

function love.load()
    canvas = love.graphics.newCanvas(160, 120)
    table.insert(log, "load canvas " .. type(canvas))
    love.graphics.clear()
    love.graphics.setCanvas(canvas)
    love.graphics.clear()
    love.graphics.clear(0.1, 0.3, 0.5)
    love.graphics.clear(1, 0.5, 0.25, 0)
    love.graphics.setCanvas()
    love.graphics.setCanvas(love.graphics.newCanvas(160, 120))
    love.graphics.setCanvas(nil)
end

function love.draw()
    love.graphics.setCanvas(canvas)
    love.graphics.clear(2, -1, 0.3, 1.5)
    love.graphics.setCanvas()
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
