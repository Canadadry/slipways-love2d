local misuse = os.getenv("FAKE_LOVE_MISUSE")

if misuse == "not function" then
    love.resize = 5
elseif misuse == "raise" then
    function love.resize()
        error("raised in love.resize")
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
