local new = love.image.newImageData
local keys = {}
for key in pairs(love.image) do
    table.insert(keys, key)
end
table.insert(log, "image " .. table.concat(keys, " "))

local function values(data, x, y)
    local texts = {}
    for _, value in ipairs({data:getPixel(x, y)}) do
        table.insert(texts, string.format("%.17g", value))
    end
    return table.concat(texts, " ")
end

local function dump(label, data, width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            table.insert(log, string.format("%s (%d, %d) %s", label, x, y, values(data, x, y)))
        end
    end
end

local function try(label, ...)
    local ok, message = pcall(new, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

function love.load()
    dump("rgb", new("rgb.png"), 2, 2)
    dump("rgba", new("rgba.png"), 3, 2)
    dump("tile", new("art/tile.png"), 2, 1)

    local edited = new("rgb.png")
    edited:setPixel(0, 0, 0, 0, 1, 0)
    table.insert(log, "edited " .. values(edited, 0, 0))
    table.insert(log, "reloaded " .. values(new("rgb.png"), 0, 0))

    local opaque_ok = pcall(love.graphics.newImage, new("rgb.png"))
    table.insert(log, "image of rgb " .. tostring(opaque_ok))
    local translucent_ok, translucent = pcall(love.graphics.newImage, new("rgba.png"))
    table.insert(log, "image of rgba: " .. (translucent_ok and type(translucent) or translucent))

    try("none")
    try("missing", "missing.png")
    try("directory", "art")
    try("conf", "conf.lua")
    love.filesystem.write("text.png", "not a png")
    try("text", "text.png")
    try("empty", "")
    try("rooted", "/rgb.png")
    try("parent", "../rgb.png")
    try("backslash", "a\\b")
    try("table", {})
    try("boolean", true)
    try("number", 5)
    try("format", "rgb.png", "rgba8")
    try("trailing nil", "rgb.png", nil)

    love.filesystem.write("rgba.png", (love.filesystem.read("rgb.png")))
    dump("shadow", new("rgba.png"), 2, 2)
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
