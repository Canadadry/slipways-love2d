local graphics = love.graphics
local frame = 0
local frame_canvas, gradient, sparse

local function painted(data)
    local canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(canvas)
    graphics.clear(0, 0, 0, 0)
    graphics.draw(graphics.newImage(data), graphics.newQuad(0, 0, 160, 120, 160, 120), 0, 0)
    graphics.setCanvas()
    return canvas
end

function love.load()
    local data = love.image.newImageData(160, 120)
    data:setPixel(0, 0, 1, 0, 0, 1)
    data:setPixel(5, 3, 0, 1, 0, 1)
    data:setPixel(159, 119, 0, 0, 1, 1)
    frame_canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(frame_canvas)
    graphics.clear(0, 0, 0)
    graphics.setCanvas()
    sparse = painted(data)
    if os.getenv("FAKE_LOVE_SCREENS_TRANSLUCENT") ~= nil then
        return
    end
    data = love.image.newImageData(160, 120)
    for j = 0, 119 do
        for i = 0, 159 do
            data:setPixel(i, j, i / 255, j / 255, 0x80 / 255, 1)
        end
    end
    gradient = painted(data)
end

local PRESENTS = {
    [0] = {{"gradient", 0, 64.75, 2.4375}},
    [1] = {{"gradient", 0, 0, 1}},
    [2] = {{"gradient", -10.5, 7.25, 3.1}},
    [3] = {},
    [4] = {{"gradient", 0, 0, 1}, {"sparse", 60.5, 50.5, 0.75}},
}

function love.draw()
    if frame == 2 and os.getenv("FAKE_LOVE_SCREENS_FAIL") ~= nil then
        error("fails at f2")
    end
    local canvases = {gradient = gradient, sparse = sparse}
    if os.getenv("FAKE_LOVE_SCREENS_TRANSLUCENT") ~= nil then
        graphics.setCanvas(sparse)
        graphics.clear(1, 0, 0, 0.5)
        graphics.setCanvas()
        graphics.draw(sparse, 0, 0, 0, 1, 1)
    end
    for _, present in ipairs(PRESENTS[frame]) do
        local scale = present[4]
        graphics.draw(canvases[present[1]], present[2], present[3], 0, scale, scale)
    end
    frame = frame + 1
end

function debug_state()
    return "ok\n"
end
