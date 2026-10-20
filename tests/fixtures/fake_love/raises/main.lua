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
