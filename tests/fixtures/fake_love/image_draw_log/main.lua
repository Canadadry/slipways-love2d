local graphics = love.graphics

local names = {}
for name in pairs(graphics) do
    table.insert(names, name)
end
table.sort(names)
table.insert(log, "graphics " .. table.concat(names, " "))

local function try(label, call, ...)
    local ok, message = pcall(call, ...)
    table.insert(log, label .. ": " .. (ok and "ok" or message))
end

function love.load()
    local set_default_filter = graphics.setDefaultFilter
    try("filter before images", set_default_filter, "nearest", "nearest")
    local data = love.image.newImageData(3, 2)
    data:setPixel(0, 0, 1, 1, 1, 1)
    local image = graphics.newImage(data)
    table.insert(log, "image " .. type(image) .. " getWidth " .. select(2, pcall(function() return image.getWidth end)):gsub("^.-:%d+: ", ""))
    local quad = graphics.newQuad(0, 0, 3, 2, 3, 2)
    table.insert(log, "quad " .. type(quad) .. " setViewport " .. select(2, pcall(function() return quad.setViewport end)):gsub("^.-:%d+: ", ""))
    local canvas = graphics.newCanvas(160, 120)
    try("filter after images", set_default_filter, "nearest", "nearest")

    local new_image = graphics.newImage
    try("image none", new_image)
    try("image file", new_image, "sprites.png")
    try("image settings", new_image, data, {})
    try("image canvas", new_image, canvas)
    try("image image", new_image, image)
    try("image table", new_image, {})
    local translucent = love.image.newImageData(4, 3)
    translucent:setPixel(3, 0, 1, 1, 1, 1)
    translucent:setPixel(2, 1, 1, 1, 1, 0.5)
    translucent:setPixel(3, 2, 1, 1, 1, 0.5)
    try("image translucent", new_image, translucent)
    try("image accepted", new_image, data)

    local new_quad = graphics.newQuad
    try("quad four", new_quad, 0, 0, 2, 2)
    try("quad five", new_quad, 0, 0, 2, 2, 4)
    try("quad seven", new_quad, 0, 0, 2, 2, 4, 4, 0)
    try("quad string", new_quad, "0", 0, 2, 2, 4, 4)
    try("quad nan", new_quad, 0, 0 / 0, 2, 2, 4, 4)
    try("quad infinity", new_quad, 0, 0, 2, 2, 1 / 0, 4)
    try("quad fractional", new_quad, 0.5, 0, 2, 2, 4, 4)
    try("quad empty", new_quad, 0, 0, 0, 2, 4, 4)
    try("quad negative width", new_quad, 0, 0, -1, 2, 4, 4)
    try("quad negative y", new_quad, 0, -1, 2, 2, 4, 4)
    try("quad no reference", new_quad, 0, 0, 1, 1, 0, 4)
    try("quad wide reference", new_quad, 0, 0, 1, 1, 1025, 4)
    try("quad past the reference", new_quad, 3, 0, 2, 2, 4, 4)
    try("quad accepted", new_quad, 45, 25, 3, 5, 48, 30)

    graphics.setCanvas(canvas)
    try("filter on a canvas", set_default_filter, "nearest", "nearest")
    local draw = graphics.draw
    local font = graphics.newImage(love.image.newImageData(48, 30))
    local sheet_quad = new_quad(0, 0, 3, 5, 128, 128)
    try("draw three", draw, image, quad, 0)
    try("draw five", draw, image, quad, 0, 0, 0)
    try("draw trailing nil", draw, image, quad, 0, 0, nil)
    try("draw image data", draw, data, quad, 0, 0)
    try("draw canvas", draw, canvas, quad, 0, 0)
    try("draw table", draw, {}, quad, 0, 0)
    try("draw string", draw, "image", quad, 0, 0)
    try("draw table quad", draw, image, {}, 0, 0)
    try("draw image quad", draw, image, image, 0, 0)
    try("draw half x", draw, image, quad, 0.5, 0)
    try("draw fractional y", draw, image, quad, 0, 1 / 1024)
    try("draw far x", draw, image, quad, 2 ^ 20, 0)
    try("draw nan", draw, image, quad, 0, 0 / 0)
    try("draw string x", draw, image, quad, "3", 0)
    try("draw other reference", draw, font, sheet_quad, 0, 0)
    try("draw accepted", draw, image, quad, -(2 ^ 20 - 1), 2 ^ 20 - 1)

    try("filter none", set_default_filter)
    try("filter one", set_default_filter, "nearest")
    try("filter anisotropy", set_default_filter, "nearest", "nearest", 1)
    try("filter trailing nil", set_default_filter, "nearest", "nearest", nil)
    try("filter number", set_default_filter, 1, "nearest")
    try("filter table", set_default_filter, {}, "nearest")
    try("filter nil", set_default_filter, nil, "nearest")
    try("filter boolean", set_default_filter, "nearest", true)
    try("filter bogus", set_default_filter, "bogus", "nearest")
    try("filter upper case", set_default_filter, "nearest", "NEAREST")
    try("filter linear", set_default_filter, "linear", "linear")
    try("filter mixed", set_default_filter, "nearest", "linear")
    try("filter none name", set_default_filter, "none", "nearest")
    try("filter then draw", draw, image, quad, 0, 0)
    graphics.setCanvas()
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
