local data = love.image.newImageData(6, 4)
for y = 0, 3 do
    for x = 0, 5 do
        data:setPixel(x, y, (40 + 30 * x) / 255, (60 * y) / 255, 200 / 255, (x + y) % 4 == 0 and 0 or 1)
    end
end
local image = love.graphics.newImage(data)
local whole = love.graphics.newQuad(0, 0, 6, 4, 6, 4)
local part = love.graphics.newQuad(2, 1, 3, 2, 6, 4)

-- {translate x, point x}: the float sum lights another column than the double sum.
local point_cases = {
    {37, -33.998045563697815},
    {-91, 95.001953840255737},
    {3, 0.0019544362994565745},
    {100, -95.998046159744263},
}
-- {translate x, x, w}: the far edge is (x + w) + translate, not (x + translate) + w.
local rectangle_cases = {
    {37, 32.157488867645952, 81.344477964683847},
    {37, 9.4861900185299888, -29.984235350377077},
    {3, 15.092026511109584, -11.590072818471622},
    {-91, 20.130568780484534, 119.37138868644311},
}

return function(canvas)
    local graphics = love.graphics
    graphics.push()
    graphics.translate(50, 40)
    graphics.clear(0.1, 0.2, 0.3, 1)
    graphics.pop()

    local pushed = 0
    while pcall(graphics.push) do
        pushed = pushed + 1
        if pushed > 200 then break end
    end
    for _ = 1, pushed do
        graphics.pop()
    end
    graphics.setColor(1, 1, 0)
    graphics.rectangle("fill", 0, 118, pushed, 1)
    if not pcall(graphics.pop) then
        graphics.rectangle("fill", 140, 118, 4, 1)
    end

    graphics.setColor(1, 1, 1)
    for row, case in ipairs(point_cases) do
        graphics.push()
        graphics.translate(case[1], 0)
        graphics.points(case[2], 99 + row + 0.5)
        graphics.pop()
    end
    for row, case in ipairs(rectangle_cases) do
        graphics.push()
        graphics.translate(case[1], 0)
        graphics.rectangle("fill", case[2], 104 + row, case[3], 1)
        graphics.pop()
    end

    graphics.setCanvas()
    graphics.translate(3, 3)
    graphics.setCanvas(canvas)

    graphics.setColor(1, 0.5, 0)
    for k = -6, 40 do
        graphics.points(k * 4 + 0.5, k % 7 - 5 + 0.5, k * 4 + 1.5, 20 + 0.5)
    end
    graphics.points(-3.5, 10.5, 156.5, 10.5, 157.5, 11.5, 10.5, -3.5, 10.5, 117.5)

    graphics.push()
    graphics.translate(20, 0)
    graphics.setColor(0, 1, 0.5)
    graphics.rectangle("fill", 0, 25, 10, 4)
    graphics.push()
    graphics.translate(-40, 10)
    graphics.rectangle("fill", 0, 25, 30, 3)
    graphics.rectangle("fill", 170, 25, 10, 3)
    graphics.pop()
    graphics.rectangle("fill", 15, 25, 5, 8)
    graphics.pop()
    graphics.rectangle("fill", 0, 45, 4, 4)

    graphics.setColor(1, 1, 1)
    graphics.push()
    graphics.translate(-30, 50)
    graphics.draw(image, whole, 40, 0)
    graphics.draw(image, part, 50, 0)
    graphics.draw(image, whole, 25, 2)
    graphics.draw(image, whole, 60, -54)
    graphics.translate(2 ^ 20 - 1, -(2 ^ 20 - 1))
    graphics.draw(image, whole, -(2 ^ 20 - 1) + 70, 2 ^ 20 - 1)
    graphics.points(-(2 ^ 20 - 1) + 80.5, 2 ^ 20 - 1 + 0.5)
    graphics.rectangle("fill", -(2 ^ 20 - 1) + 85, 2 ^ 20 - 1, 3, 2)
    graphics.pop()
    graphics.draw(image, whole, 153, 113)
    graphics.draw(image, part, 70, 80)
end
