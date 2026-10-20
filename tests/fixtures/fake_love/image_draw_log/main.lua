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
    local data = love.image.newImageData(3, 2)
    data:setPixel(0, 0, 1, 1, 1, 1)
    local image = graphics.newImage(data)
    table.insert(log, "image " .. type(image) .. " getWidth " .. tostring(image.getWidth))
    local quad = graphics.newQuad(0, 0, 3, 2, 3, 2)
    table.insert(log, "quad " .. type(quad) .. " setViewport " .. tostring(quad.setViewport))
    local canvas = graphics.newCanvas(160, 120)

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
    graphics.setCanvas()
end

function debug_state()
    return table.concat(log, "\n") .. "\n"
end
