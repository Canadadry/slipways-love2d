local names = {"rgb_stored", "rgba_filters", "rgb_interlaced", "rgb_ancillary", "rgb_key"}
local loaded = {}
for i, name in ipairs(names) do
    loaded[i] = love.image.newImageData("files/" .. name .. ".png")
end
local edited = love.image.newImageData("files/rgba_filters.png")
for x = 0, 20 do
    edited:setPixel(x, 0, 1, 0, 0, 1)
end
local reloaded = love.image.newImageData("files/rgba_filters.png")

local function show(data, width, height, left, top)
    local graphics = love.graphics
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local r, g, b, a = data:getPixel(x, y)
            graphics.setColor(r, g, b)
            graphics.rectangle("fill", left + x, top + y, 1, 1)
            graphics.setColor(a, a, a)
            graphics.rectangle("fill", left + x, top + height + 2 + y, 1, 1)
        end
    end
end

local sizes = {{24, 16}, {21, 13}, {23, 15}, {17, 11}, {19, 9}}

return function()
    for i, data in ipairs(loaded) do
        show(data, sizes[i][1], sizes[i][2], 2 + (i - 1) * 32, 2)
    end
    show(edited, 21, 13, 2, 40)
    show(reloaded, 21, 13, 34, 40)
end
