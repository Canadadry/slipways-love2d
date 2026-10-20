local graphics = love.graphics
graphics.setDefaultFilter("nearest", "nearest")

local PALETTE = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
}

local function color(c)
    local rgb = PALETTE[c + 1]
    graphics.setColor(rgb[1] / 255, rgb[2] / 255, rgb[3] / 255)
end

local function palette_index(r, g, b)
    for c = 0, 15 do
        local rgb = PALETTE[c + 1]
        if r == rgb[1] / 255 and g == rgb[2] / 255 and b == rgb[3] / 255 then
            return c
        end
    end
    error("not a palette colour")
end

-- swap[c]: another colour, false for see-through, nil to keep c.
local function recolour(source, width, height, swap)
    local data = love.image.newImageData(width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local c = palette_index(source:getPixel(x, y))
            local n = swap[c]
            if n == nil then n = c end
            if n then
                local rgb = PALETTE[n + 1]
                data:setPixel(x, y, rgb[1] / 255, rgb[2] / 255, rgb[3] / 255)
            end
        end
    end
    return graphics.newImage(data)
end

local sprites = love.image.newImageData("files/sprites.png")
local font = love.image.newImageData("files/font.png")
local sheet = recolour(sprites, 128, 128, {[0] = false})
local swapped_sheet
local glyphs = (function()
    local data = love.image.newImageData(48, 30)
    for y = 0, 29 do
        for x = 0, 47 do
            if palette_index(font:getPixel(x, y)) == 7 then
                data:setPixel(x, y, 1, 1, 1)
            end
        end
    end
    return graphics.newImage(data)
end)()

local pattern = (function()
    local data = love.image.newImageData(4, 4)
    local bits = 0xA5A5
    for y = 0, 3 do
        for x = 0, 3 do
            local bit = 15 - (y * 4 + x)
            if math.floor(bits / 2 ^ bit) % 2 == 0 then
                data:setPixel(x, y, 1, 1, 1)
            end
        end
    end
    return graphics.newImage(data)
end)()

local scratch = graphics.newCanvas(160, 120)

local function spr(image, n, x, y, w, h)
    local quad = graphics.newQuad(n % 16 * 8, math.floor(n / 16) * 8, w * 8, h * 8, 128, 128)
    graphics.draw(image, quad, x, y)
end

local function rectfill(x0, y0, x1, y1, c)
    color(c)
    graphics.rectangle("fill", math.min(x0, x1), math.min(y0, y1), math.abs(x1 - x0) + 1, math.abs(y1 - y0) + 1)
end

local function pset(x, y, c)
    color(c)
    graphics.points(x + 0.5, y + 0.5)
end

local function circfill(cx, cy, r, c)
    color(c)
    for dy = -r, r do
        local half = math.floor(math.sqrt(r * r - dy * dy) + 0.5)
        graphics.rectangle("fill", cx - half, cy + dy, 2 * half + 1, 1)
    end
end

local function circ(cx, cy, r, c)
    color(c)
    local coordinates = {}
    for k = 0, 63 do
        local angle = k * math.pi / 32
        local x = cx + math.floor(r * math.cos(angle) + 0.5)
        local y = cy + math.floor(r * math.sin(angle) + 0.5)
        coordinates[#coordinates + 1] = x + 0.5
        coordinates[#coordinates + 1] = y + 0.5
    end
    graphics.points(coordinates)
end

-- The pattern is aligned to the screen: camera_x, camera_y undo the translation.
local function pattern_rectfill(x0, y0, x1, y1, c, camera_x, camera_y)
    color(c)
    local y = y0
    while y <= y1 do
        local qy = (y - camera_y) % 4
        local h = math.min(4 - qy, y1 - y + 1)
        local x = x0
        while x <= x1 do
            local qx = (x - camera_x) % 4
            local w = math.min(4 - qx, x1 - x + 1)
            graphics.draw(pattern, graphics.newQuad(qx, qy, w, h, 4, 4), x, y)
            x = x + w
        end
        y = y + h
    end
end

local function print_text(text, x, y, c)
    color(c)
    for i = 1, #text do
        local code = text:byte(i)
        if code < 32 or code > 126 then code = 127 end
        local cell = code - 32
        graphics.draw(glyphs, graphics.newQuad(cell % 16 * 3, math.floor(cell / 16) * 5, 3, 5, 48, 30), x + (i - 1) * 4, y)
    end
end

local function print_outlined(text, x, y, c)
    for dy = -1, 1 do
        for dx = -1, 1 do
            if dx ~= 0 or dy ~= 0 then print_text(text, x + dx, y + dy, 0) end
        end
    end
    print_text(text, x, y, c)
end

local function frame_one(camera_x, camera_y)
    graphics.clear(0, 0, 0, 1)
    rectfill(0, 0, 159, 7, 1)

    graphics.push()
    graphics.translate(-camera_x, -camera_y)
    rectfill(-20, 10, 140, 70, 1)
    graphics.setColor(1, 1, 1)
    for row = 0, 7 do
        for column = 0, 21 do
            spr(sheet, (row * 22 + column) % 128, -24 + column * 8, 2 + row * 8, 1, 1)
        end
    end
    pattern_rectfill(-15, 30, 61, 49, 12, camera_x, camera_y)
    pattern_rectfill(100, 60, 170, 135, 8, camera_x, camera_y)

    if not swapped_sheet then
        swapped_sheet = recolour(sprites, 128, 128, {[0] = 0, [3] = false, [7] = 8, [6] = 12})
    end
    local planets = {{60, 70, 6, 3}, {95, 88, 7, 9}, {140, 80, 5, 14}, {30, 60, 8, 4}, {190, 95, 6, 11}}
    for index, planet in ipairs(planets) do
        local x, y, r, c = planet[1], planet[2], planet[3], planet[4]
        circfill(x, y, r, c)
        circ(x, y, r + 3, 6)
        graphics.setColor(1, 1, 1)
        spr(index % 2 == 0 and swapped_sheet or sheet, 64 + index * 2, x - 4, y - 4, 2, 2)
        pset(x + r + 6, y, 10)
        pset(x, y - r - 6, 10)
    end
    color(11)
    graphics.points(30.5, 75.5, 31.5, 75.5, 32.5, 76.5, -40.5, 75.5, 200.5, 75.5)

    graphics.push()
    graphics.translate(10, -4)
    print_outlined("SLIPWAYS", 30, 66, 7)
    graphics.pop()
    print_text("x=" .. camera_x, 60, 100, 9)
    graphics.pop()

    rectfill(0, 112, 159, 119, 5)
    print_outlined("CREDITS 0123 abc~", 2, 113, 10)
    graphics.setColor(1, 1, 1)
    spr(sheet, 70, 120, 104, 1, 2)
    spr(sheet, 71, 152, 104, 1, 2)
    spr(swapped_sheet, 0, 140, 110, 2, 2)
    print_text("\127", 100, 2, 8)
    color(12)
    spr(sheet, 16, 4, 0, 2, 1)
end

local function scratch_frame(canvas)
    graphics.setCanvas(scratch)
    graphics.translate(5, 5)
    graphics.clear(1, 0, 0, 1)
    color(14)
    graphics.rectangle("fill", 0, 0, 160, 120)
    graphics.setCanvas(canvas)
    graphics.rectangle("fill", 150, 0, 4, 3)
    graphics.setColor(1, 1, 1)
    spr(sheet, 3, 40, 0, 1, 1)
    graphics.translate(-5, -5)
end

local function frame_two(canvas)
    graphics.setCanvas()
    graphics.setColor(1, 1, 1)
    graphics.draw(sheet, graphics.newQuad(0, 0, 128, 128, 128, 128), 0, 0)
    rectfill(0, 0, 50, 50, 8)
    graphics.setCanvas(canvas)

    graphics.push()
    graphics.translate(3, 0)
    rectfill(20, 84, 80, 98, 0)
    rectfill(21, 85, 79, 97, 13)
    print_outlined("BUILD?", 26, 88, 7)
    graphics.setColor(1, 1, 1)
    spr(sheet, 33, 70, 87, 1, 1)
    graphics.pop()
end

return function(canvas)
    frame_one(37, -9)
    scratch_frame(canvas)
    frame_two(canvas)
end
