local graphics = love.graphics
local log = {}

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function try(label, ...)
    local ok, message = pcall(graphics.setColor, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

function love.load()
    try("none")
    try("two", 1, 0)
    try("five", 1, 0, 0, 1, 0)
    try("table", {1, 0, 0})
    try("string", "1", 0, 0)
    try("nil", 1, nil, 0)
    try("boolean", 1, 0, true)
    try("nan", 0 / 0, 0, 0)
    try("infinity", 1, 1 / 0, 0)
    try("alpha 0", 1, 0, 0, 0)
    try("alpha half", 1, 0, 0, 0.5)
    try("alpha 2", 1, 0, 0, 2)
    try("alpha nil", 1, 0, 0, nil)
    try("clamped", 2, -1, 0.3)
    try("alpha 1", 1, 0, 0, 1)
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
