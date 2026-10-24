local graphics = love.graphics
local frame = 0
local canvas, generated, sheet
local generated_part, generated_whole, sheet_whole, sheet_row

function love.load()
    canvas = graphics.newCanvas(160, 120)
    graphics.setCanvas(canvas)
    graphics.clear(0x1D / 255, 0x2B / 255, 0x53 / 255)
    graphics.setCanvas()
    local data = love.image.newImageData(6, 4)
    for j = 0, 3 do
        for i = 0, 5 do
            data:setPixel(i, j, (i * 40 + 10) / 255, (j * 50 + 20) / 255, (i * 7 + j * 30 + 3) / 255, 1)
        end
    end
    generated = graphics.newImage(data)
    sheet = graphics.newImage(love.image.newImageData("pad.png"))
    generated_part = graphics.newQuad(1, 1, 3, 2, 6, 4)
    generated_whole = graphics.newQuad(0, 0, 6, 4, 6, 4)
    sheet_whole = graphics.newQuad(0, 0, 8, 2, 8, 2)
    sheet_row = graphics.newQuad(0, 1, 4, 1, 8, 2)
end

function love.draw()
    if frame == 0 then
        graphics.draw(generated, generated_part, 10.25, 7.6, 0, 2.4375, 1.7)
        graphics.draw(generated, generated_whole, -3.5, 20.5, 0, 0.75, 3)
    else
        graphics.draw(canvas, 0, 64.75, 0, 2.4375, 2.4375)
        graphics.draw(sheet, sheet_whole, 5.5, 70.5, 0, 10, 10)
        graphics.setColor(0.5, 1, 0.25, 0.5)
        graphics.draw(sheet, sheet_row, 100.25, 70.5, 0, 10, 10)
        graphics.push()
        graphics.translate(3, 4)
        graphics.draw(sheet, sheet_whole, 200, 100)
        graphics.pop()
        graphics.setColor(1, 1, 1, 1)
    end
    frame = frame + 1
end

function debug_state()
    return "ok\n"
end
