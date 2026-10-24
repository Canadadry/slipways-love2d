local misuse = os.getenv("FAKE_LOVE_MISUSE")
local last = {}

function love.update()
    if misuse == "lifted" then
        for _, id in ipairs(last) do
            love.touch.getPosition(id)
        end
        last = love.touch.getTouches()
    elseif misuse == "number id" then
        love.touch.getPosition(1)
    elseif misuse == "touches argument" then
        love.touch.getTouches(1)
    elseif misuse == "right button" then
        love.mouse.isDown(2)
    elseif misuse == "no button" then
        love.mouse.isDown()
    elseif misuse == "mouse argument" then
        love.mouse.getPosition(1)
    elseif misuse == "callback" then
        love.touchpressed = function() end
    elseif misuse == "pressure" then
        love.touch.getPressure(1)
    end
end

function debug_state()
    return "ran\n"
end

-- tmp/fake_love fails a frame that ends with no canvas 0 or a non-palette pixel on it.
function love.load()
    love.graphics.setCanvas(love.graphics.newCanvas(160, 120))
    love.graphics.clear(0, 0, 0)
    love.graphics.setCanvas()
end
