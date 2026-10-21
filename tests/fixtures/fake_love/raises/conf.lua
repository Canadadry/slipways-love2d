local phase = os.getenv("FAKE_LOVE_RAISE")
if phase == "conf.lua" then
    error("raised in conf.lua")
end

function love.conf()
    if phase == "love.conf" then
        error("raised in love.conf")
    end
end
