local graphics = love.graphics

local PALETTE = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
}

local game_canvas
local scratch_canvas
local sprite_image
local sprite_quad
local font_image
local letter_quad
local frames_drawn = 0

local function components(colour)
    local rgb = PALETTE[colour + 1]
    return rgb[1] / 255, rgb[2] / 255, rgb[3] / 255
end

local function set_colour(colour)
    graphics.setColor(components(colour))
end

function love.load()
    graphics.setDefaultFilter("nearest", "nearest")
    game_canvas = graphics.newCanvas(160, 120)
    scratch_canvas = graphics.newCanvas(160, 120)
    local sprite = love.image.newImageData(2, 2)
    sprite:setPixel(0, 0, components(8))
    sprite:setPixel(1, 0, 1, 1, 1, 0)
    sprite:setPixel(1, 1, components(12))
    sprite_image = graphics.newImage(sprite)
    sprite_quad = graphics.newQuad(0, 0, 2, 2, 2, 2)
    local font = love.image.newImageData(4, 1)
    font:setPixel(0, 0, 1, 1, 1)
    font:setPixel(2, 0, 1, 1, 1)
    font:setPixel(3, 0, 1, 1, 1)
    font_image = graphics.newImage(font)
    letter_quad = graphics.newQuad(0, 0, 3, 1, 4, 1)
end

local function draw_scene(offset)
    graphics.push()
    graphics.translate(offset, 0)
    set_colour(11)
    graphics.rectangle("fill", 0, 10, 4, 2)
    set_colour(10)
    graphics.points(5 + 0.5, 10 + 0.5)
    graphics.push()
    graphics.translate(0, 20)
    graphics.setColor(1, 1, 1)
    graphics.draw(sprite_image, sprite_quad, 0, 0)
    set_colour(8)
    graphics.draw(font_image, letter_quad, 0, 5)
    graphics.pop()
    graphics.pop()
end

local function draw_scratch()
    graphics.setCanvas(scratch_canvas)
    graphics.clear()
    graphics.clear(0.3, 0.3, 0.3)
    graphics.setColor(1, 1, 1)
    graphics.points(0.5, 0.5, 1.5, 0.5)
    set_colour(9)
    graphics.draw(sprite_image, sprite_quad, 2, 0)
end

local function draw_on_screen()
    graphics.setCanvas()
    set_colour(7)
    graphics.rectangle("fill", 0, 0, 160, 120)
    graphics.setColor(1, 1, 1)
    graphics.draw(sprite_image, sprite_quad, 0, 0)
end

function love.draw()
    graphics.setCanvas(game_canvas)
    if frames_drawn == 0 then
        graphics.clear(components(1))
    end
    draw_scene(20 * frames_drawn)
    draw_scratch()
    draw_on_screen()
    frames_drawn = frames_drawn + 1
end

function debug_state()
    return "frames " .. frames_drawn .. "\n"
end
