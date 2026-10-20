local graphics = love.graphics

local PALETTE = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
}

local game_canvas
local scratch_canvas
local pair_image
local pair_quad
local frames_drawn = 0

local function components(colour)
    local rgb = PALETTE[colour + 1]
    return rgb[1] / 255, rgb[2] / 255, rgb[3] / 255
end

local function set_colour(colour)
    graphics.setColor(components(colour))
end

local function clear_to(colour)
    graphics.clear(components(colour))
end

function love.load()
    graphics.setDefaultFilter("nearest", "nearest")
    game_canvas = graphics.newCanvas(160, 120)
    scratch_canvas = graphics.newCanvas(160, 120)
    local data = love.image.newImageData(2, 1)
    data:setPixel(0, 0, components(9))
    data:setPixel(1, 0, components(10))
    pair_image = graphics.newImage(data)
    pair_quad = graphics.newQuad(0, 0, 2, 1, 2, 1)
end

local function draw_bands()
    graphics.setCanvas(game_canvas)
    clear_to(0)
    for colour = 0, 15 do
        set_colour(colour)
        graphics.rectangle("fill", 10 * colour, 0, 10, 120)
    end
    graphics.setCanvas()
end

local function draw_single_pixels()
    graphics.setCanvas(game_canvas)
    clear_to(0)
    set_colour(1)
    graphics.points(0.5, 0.5)
    set_colour(2)
    graphics.points(1.5, 0.5)
    set_colour(15)
    graphics.rectangle("fill", 159, 119, 1, 1)
    graphics.setColor(1, 1, 1)
    graphics.draw(pair_image, pair_quad, 2, 1)
    graphics.setCanvas()
end

local function draw_elsewhere()
    graphics.setCanvas(scratch_canvas)
    clear_to(8)
    graphics.setCanvas()
    set_colour(8)
    graphics.rectangle("fill", 0, 0, 160, 120)
end

local function draw_foreign_pixel()
    graphics.setCanvas(game_canvas)
    graphics.setColor(0.3, 0, 0)
    graphics.points(0.5, 0.5)
    graphics.setCanvas()
end

local DRAWS = {draw_bands, draw_single_pixels, draw_elsewhere, draw_foreign_pixel}

function love.draw()
    frames_drawn = frames_drawn + 1
    DRAWS[frames_drawn]()
end

function debug_state()
    return "frames " .. frames_drawn .. "\n"
end
