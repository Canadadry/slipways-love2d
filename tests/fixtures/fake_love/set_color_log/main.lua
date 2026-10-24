local graphics = love.graphics
local log = {}

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function record(label, ok, message)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

local function try(label, ...)
    record(label, pcall(graphics.setColor, ...))
end

local function try_draws(where, image, quad)
    record(where .. " points", pcall(graphics.points, 1, 1))
    record(where .. " rectangle", pcall(graphics.rectangle, "fill", 0, 0, 4, 4))
    record(where .. " draw", pcall(graphics.draw, image, quad, 0, 0))
end

function love.load()
    local game_canvas = graphics.newCanvas(160, 120)
    local scratch_canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(game_canvas)
    graphics.clear(0, 0, 0)
    local data = love.image.newImageData(1, 1)
    data:setPixel(0, 0, 1, 1, 1, 1)
    local image = graphics.newImage(data)
    local quad = graphics.newQuad(0, 0, 1, 1, 1, 1)
    graphics.setCanvas()

    try("none")
    try("two", 1, 0)
    try("five", 1, 0, 0, 1, 0)
    try("table", {1, 0, 0})
    try("string", "1", 0, 0)
    try("nil", 1, nil, 0)
    try("boolean", 1, 0, true)
    try("nan", 0 / 0, 0, 0)
    try("infinity", 1, 1 / 0, 0)
    try("alpha infinity", 1, 0, 0, -1 / 0)
    try("alpha 0", 1, 0, 0, 0)
    try("alpha half", 1, 0, 0, 0.5)
    try("alpha 2", 1, 0, 0, 2)
    try("alpha nil", 1, 0, 0, nil)
    try("clamped", 2, -1, 77 / 255)
    try("alpha 1", 1, 0, 77 / 255, 1)
    try("white", 1, 1, 1)
    try("palette", 0x1D / 255, 0x2B / 255, 0x53 / 255)
    try("outside", 0.3, 0, 0)
    try("one byte off", 1, 0, 76 / 255)

    graphics.setColor(1, 0, 0, 0.5)
    graphics.setCanvas(game_canvas)
    try_draws("canvas half", image, quad)
    record("canvas bad argument", pcall(graphics.points, "x", 1))
    graphics.setColor(1, 0, 0, 0)
    record("canvas alpha 0 off the canvas", pcall(graphics.points, -5, -5))
    graphics.setCanvas()
    try_draws("screen alpha 0", image, quad)
    graphics.setColor(0.3, 0.6, 0.9, 0.5)
    try_draws("screen half", image, quad)
    graphics.setColor(0.3, 0, 0)
    graphics.setCanvas(scratch_canvas)
    try_draws("scratch outside", image, quad)
    graphics.setCanvas()
    graphics.setColor(1, 1, 1)
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
