local image = love.image

local function sorted_names(t)
    local names = {}
    for name in pairs(t) do
        table.insert(names, name)
    end
    table.sort(names)
    return table.concat(names, " ")
end

table.insert(log, "image " .. sorted_names(image))

local data

local function values(...)
    local texts = {}
    for index = 1, select("#", ...) do
        table.insert(texts, string.format("%.17g", (select(index, ...))))
    end
    return table.concat(texts, " ")
end

local function read(label, x, y)
    table.insert(log, label .. " " .. values(data:getPixel(x, y)))
end

function love.load()
    data = image.newImageData(4, 3)
    table.insert(log, "type " .. type(data))
    table.insert(log, "methods " .. sorted_names(getmetatable(data).__index))
    read("fresh", 3, 2)
    table.insert(log, "count " .. select("#", data:getPixel(3, 2)))
    data:setPixel(0, 0, 1 / 255, 0.5, 2.5 / 255, 0.3)
    read("set", 0, 0)
    table.insert(log, "exact " .. tostring(data:getPixel(0, 0) == 1 / 255))
    data:setPixel(1, 0, 2, -1, 1e30)
    read("clamped", 1, 0)
    data:setPixel(2.9, 1.5, 0.2, 0.4, 0.6, 0)
    read("floored", 2, 1)
    data:setPixel(3, 0, (0.5 - 1e-9) / 255, 0, 0)
    read("half up in double", 3, 0)
    table.insert(log, "fractional read " .. values(data:getPixel(0.999, 0.5)))
    data:setPixel(0, 1, data:getPixel(0, 0))
    read("copied", 0, 1)
    local other = image.newImageData(4, 3)
    other:setPixel(1, 1, 1, 1, 1)
    read("independent", 1, 1)
    local large = image.newImageData(1024, 1024)
    large:setPixel(1023, 1023, 1, 1, 1)
    table.insert(log, "large ok")
    large = nil
    local before = collectgarbage("count")
    collectgarbage()
    table.insert(log, "collected " .. tostring(before - collectgarbage("count") >= 4096))
end

local function try(label, call, ...)
    local ok, message = pcall(call, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

local function misuse()
    local new = image.newImageData
    try("new none", new)
    try("new one", new, 4)
    try("new format", new, 4, 3, "rgba8")
    try("new string", new, "4", 3)
    try("new file", new, "sprites.png")
    try("new zero", new, 0, 3)
    try("new negative", new, 3, -1)
    try("new fractional", new, 1.5, 2)
    try("new wide", new, 1025, 1)
    try("new tall", new, 1, 1025)
    try("new nan", new, 0 / 0, 1)

    local set = data.setPixel
    try("set dot", set, 0, 0, 1, 1, 1)
    try("set four", set, data, 0, 0, 1, 0)
    try("set seven", set, data, 0, 0, 1, 0, 0, 1, 0)
    try("set table", set, data, 0, 0, {1, 0, 0})
    try("set string component", set, data, 0, 0, "1", 0, 0)
    try("set string coordinate", set, data, "0", 0, 1, 0, 0)
    try("set nil", set, data, 0, 0, 1, nil, 0)
    try("set boolean", set, data, 0, 0, 1, 0, true)
    try("set nan", set, data, 0, 0, 0 / 0, 0, 0)
    try("set infinity", set, data, 0, 0, 1, 1 / 0, 0)
    try("set nan alpha", set, data, 0, 0, 1, 0, 0, 0 / 0)
    try("set width", set, data, 4, 0, 1, 0, 0)
    try("set height", set, data, 0, 3, 1, 0, 0)
    try("set negative", set, data, -1, 0, 1, 0, 0)
    try("set negative half", set, data, -0.5, 0, 1, 0, 0)
    try("set nan x", set, data, 0 / 0, 0, 1, 0, 0)
    try("set infinite y", set, data, 0, 1 / 0, 1, 0, 0)

    local get = data.getPixel
    try("get dot", get, 0, 0)
    try("get one", get, data, 0)
    try("get three", get, data, 0, 0, 0)
    try("get string", get, data, "0", 0)
    try("get width", get, data, 4, 0)
    try("get negative half", get, data, 0, -0.5)
    try("get nan", get, data, 0 / 0, 0)
end

function love.update()
    if misuse then
        misuse()
        misuse = nil
        read("after refusals", 0, 0)
    end
end

function love.draw()
    read("draw", 0, 0)
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
