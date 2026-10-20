local graphics = love.graphics
local log = {}

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function try(label, ...)
    local ok, message = pcall(graphics.points, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

function love.load()
    local canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(canvas)
    try("odd arguments", 1, 2, 3)
    try("odd table", {1, 2, 3})
    try("table then numbers", {1, 2}, 3, 4)
    try("strings", "5", "50")
    try("boolean", true, 1)
    try("nil", nil, 1)
    try("pairs", {{1, 2}})
    try("coloured", {{1, 2, 0, 1, 0, 0.5}})
    try("hole", {1, nil, 3, 4})
    try("nan", 0 / 0, 1)
    try("infinity", 1 / 0, 1)
    try("table nan", {1, 0 / 0})
    try("too far", 2 ^ 20, 1)
    try("too far below", 1, -2 ^ 20)
    try("table too far", {2 ^ 20, 1})
    try("nothing")
    try("empty table", {})
    try("largest", 2 ^ 20 - 1, -(2 ^ 20 - 1))
    graphics.setCanvas()
    try("no canvas", 10.5, 10.5)
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
