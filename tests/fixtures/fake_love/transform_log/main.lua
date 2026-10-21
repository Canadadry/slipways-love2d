local graphics = love.graphics
local limit = 2 ^ 20

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function try(label, function_name, ...)
    local ok, message = pcall(graphics[function_name], ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

-- translate(2^20 - v) succeeds exactly when the offset is below v, for v >= 1.
local function below(axis, v)
    local dx, dy = 0, 0
    if axis == "x" then dx = limit - v else dy = limit - v end
    if pcall(graphics.translate, dx, dy) then
        graphics.translate(-dx, -dy)
        return true
    end
    return false
end

local function offset(axis)
    for v = 1, 64 do
        if below(axis, v) then
            return v - 1
        end
    end
    return "64 or more"
end

local function describe_offset()
    return offset("x") .. " " .. offset("y")
end

local function free_depth()
    local pushed = 0
    while pcall(graphics.push) do
        pushed = pushed + 1
    end
    for _ = 1, pushed do
        graphics.pop()
    end
    return pushed
end

function love.load()
    try("push nil", "push", nil)
    try("push all", "push", "all")
    try("push transform", "push", "transform")
    try("pop argument", "pop", 1)
    try("translate none", "translate")
    try("translate one", "translate", 1)
    try("translate three", "translate", 1, 2, 3)
    try("translate strings", "translate", "1", "2")
    try("translate nil", "translate", 1, nil)
    try("translate half", "translate", 0.5, 0)
    try("translate nan", "translate", 0, 0 / 0)
    try("translate infinity", "translate", 1 / 0, 0)
    try("translate 2^20", "translate", limit, 0)
    table.insert(log, "offset after refusals: " .. describe_offset())

    graphics.translate(limit - 1, 0)
    try("total x", "translate", 1, 0)
    graphics.translate(-(limit - 1), -(limit - 1))
    try("total y", "translate", 0, -1)
    graphics.translate(0, limit - 1)
    table.insert(log, "offset after totals: " .. describe_offset())

    local pushed = 0
    for _ = 1, 128 do
        graphics.push()
        pushed = pushed + 1
    end
    table.insert(log, "pushed: " .. pushed)
    try("push 129", "push")
    local popped = 0
    while pcall(graphics.pop) do
        popped = popped + 1
    end
    table.insert(log, "popped: " .. popped)
    try("pop empty", "pop")

    local image = graphics.newImage(love.image.newImageData(2, 2))
    local quad = graphics.newQuad(0, 0, 2, 2, 2, 2)
    graphics.push()
    graphics.translate(limit - 10, 0)
    try("points x in range", "points", 9.5, 0)
    try("points x", "points", 10, 0)
    try("points table", "points", {0, 0, 10, 0})
    try("rectangle x in range", "rectangle", "fill", 9, 0, 1, 1)
    try("rectangle x", "rectangle", "fill", 10, 0, 1, 1)
    try("draw x in range", "draw", image, quad, 9, 0)
    try("draw x", "draw", image, quad, 10, 0)
    graphics.pop()
    graphics.push()
    graphics.translate(0, -(limit - 10))
    try("points y", "points", 0, -10)
    try("rectangle y", "rectangle", "fill", 0, -10, 1, 1)
    try("draw y", "draw", image, quad, 0, -10)
    graphics.pop()

    graphics.push()
    graphics.translate(3, 4)
    local canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(canvas)
    table.insert(log, "offset on the canvas: " .. describe_offset())
    graphics.setCanvas()
    graphics.pop()
    table.insert(log, "offset after pop: " .. describe_offset())

    graphics.translate(7, 9)
    graphics.push()
end

local frame = 0

function love.update()
    table.insert(log, "f" .. frame .. " update offset: " .. describe_offset())
end

function love.draw()
    table.insert(log, "f" .. frame .. " draw offset: " .. describe_offset() .. ", free depth: " .. free_depth())
    graphics.push()
    graphics.translate(2, 3)
    frame = frame + 1
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
