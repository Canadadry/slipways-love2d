local phase = os.getenv("FAKE_LOVE_RAISE")
local frame = 0
if phase == "main.lua" then
    error("raised in main.lua")
end

function love.load()
    if phase == "love.load" then
        error("raised in love.load")
    end
end

function love.update()
    if phase == "love.update" and frame == 1 then
        error("raised in love.update")
    end
end

function love.draw()
    if phase == "love.draw" and frame == 2 then
        error("raised in love.draw")
    end
    frame = frame + 1
end

function debug_state()
    if phase == "debug_state" then
        error("raised in debug_state")
    end
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
