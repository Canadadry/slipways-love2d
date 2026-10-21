local graphics = love.graphics

local FRAME_BYTES = 9600
local ROW_BYTES = 80

local PALETTE = {
    [0] = { 0x00, 0x00, 0x00 }, { 0x1D, 0x2B, 0x53 }, { 0x7E, 0x25, 0x53 }, { 0x00, 0x87, 0x51 },
    { 0xAB, 0x52, 0x36 }, { 0x5F, 0x57, 0x4F }, { 0xC2, 0xC3, 0xC7 }, { 0xFF, 0xF1, 0xE8 },
    { 0xFF, 0x00, 0x4D }, { 0xFF, 0xA3, 0x00 }, { 0xFF, 0xEC, 0x27 }, { 0x00, 0xE4, 0x36 },
    { 0x29, 0xAD, 0xFF }, { 0x83, 0x76, 0x9C }, { 0xFF, 0x77, 0xA8 }, { 0xFF, 0xCC, 0xAA },
}

local game_canvas
local capture
local frame = 0

local function draw_run(colour, x0, y, length)
    local rgb = PALETTE[colour]
    graphics.setColor(rgb[1] / 255, rgb[2] / 255, rgb[3] / 255)
    graphics.rectangle("fill", x0, y, length, 1)
end

function love.load()
    game_canvas = graphics.newCanvas(160, 120)
    capture = love.filesystem.read("capture.bin")
end

function love.draw()
    local base = frame * FRAME_BYTES
    if base + FRAME_BYTES > #capture then
        error("capture.bin holds no frame " .. frame)
    end
    graphics.setCanvas(game_canvas)
    for y = 0, 119 do
        local row = base + y * ROW_BYTES
        local colour = capture:byte(row + 1) % 16
        local x0 = 0
        for x = 1, 159 do
            local byte = capture:byte(row + 1 + math.floor(x / 2))
            local pixel
            if x % 2 == 0 then
                pixel = byte % 16
            else
                pixel = math.floor(byte / 16)
            end
            if pixel ~= colour then
                draw_run(colour, x0, y, x - x0)
                colour = pixel
                x0 = x
            end
        end
        draw_run(colour, x0, y, 160 - x0)
    end
    graphics.setCanvas()
    frame = frame + 1
end
