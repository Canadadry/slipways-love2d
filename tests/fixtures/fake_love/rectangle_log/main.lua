local graphics = love.graphics
local log = {}

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function try(label, ...)
    local ok, message = pcall(graphics.rectangle, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

function love.load()
    local canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(canvas)
    try("line", "line", 1, 2, 3, 4)
    try("full", "full", 1, 2, 3, 4)
    try("number mode", 5, 1, 2, 3, 4)
    try("table mode", {"fill"}, 1, 2, 3, 4)
    try("four", "fill", 1, 2, 3)
    try("rx", "fill", 1, 2, 3, 4, 0)
    try("trailing nil", "fill", 1, 2, 3, 4, nil)
    try("string", "fill", "1", 2, 3, 4)
    try("nil", "fill", 1, nil, 3, 4)
    try("boolean", "fill", 1, 2, true, 4)
    try("nan", "fill", 0 / 0, 2, 3, 4)
    try("infinity", "fill", 1, 2, 3, 1 / 0)
    try("too far", "fill", 2 ^ 20, 2, 3, 4)
    try("too wide below", "fill", 1, 2, -2 ^ 20, 4)
    try("zero width", "fill", 1, 2, 0, 4)
    try("largest", "fill", -(2 ^ 20 - 1), 2 ^ 20 - 1, 2 ^ 20 - 1, -(2 ^ 20 - 1))
    graphics.setCanvas()
    try("no canvas", "fill", 10, 10, 5, 5)
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
