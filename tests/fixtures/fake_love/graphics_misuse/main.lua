local misuse = os.getenv("FAKE_LOVE_MISUSE")
local graphics = love.graphics

function love.load()
    local canvas = graphics.newCanvas(160, 120)
    if misuse == "newCanvas size" then
        graphics.newCanvas(100, 100)
    elseif misuse == "newCanvas nothing" then
        graphics.newCanvas()
    elseif misuse == "newCanvas settings" then
        graphics.newCanvas(160, 120, {})
    elseif misuse == "newCanvas strings" then
        graphics.newCanvas("160", "120")
    elseif misuse == "newCanvas 17th" then
        for _ = 1, 16 do
            graphics.newCanvas(160, 120)
        end
    elseif misuse == "setCanvas number" then
        graphics.setCanvas(5)
    elseif misuse == "setCanvas table" then
        graphics.setCanvas({canvas})
    elseif misuse == "setCanvas mipmap" then
        graphics.setCanvas(canvas, 1)
    elseif misuse == "clear two" then
        graphics.clear(1, 2)
    elseif misuse == "clear five" then
        graphics.clear(1, 0, 0, 1, 0)
    elseif misuse == "clear string" then
        graphics.clear("1", 0, 0)
    elseif misuse == "clear nan" then
        graphics.clear(0 / 0, 0, 0)
    elseif misuse == "clear infinity" then
        graphics.clear(0, 0, 0, math.huge)
    elseif misuse == "clear table" then
        graphics.clear({1, 0, 0})
    end
    if misuse == "selected in update" then
        function love.update()
            graphics.setCanvas(canvas)
        end
    elseif misuse == "selected in draw" then
        function love.draw()
            graphics.setCanvas(canvas)
        end
    end
end

function debug_state()
    return "ran\n"
end
