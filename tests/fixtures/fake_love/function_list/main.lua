local lines = {}

for module_name, module in pairs(love) do
    table.insert(lines, "module " .. module_name)
    for function_name in pairs(module) do
        table.insert(lines, "function love." .. module_name .. "." .. function_name)
    end
end

local image_data = love.image.newImageData(1, 1)
local objects = {
    Canvas = love.graphics.newCanvas(160, 120),
    Image = love.graphics.newImage(image_data),
    Quad = love.graphics.newQuad(0, 0, 1, 1, 1, 1),
    ImageData = image_data,
    Joystick = love.joystick.getJoysticks()[1],
}
for kind, object in pairs(objects) do
    table.insert(lines, "kind " .. kind)
    for method_name in pairs(getmetatable(object).__index) do
        table.insert(lines, "method " .. kind .. ":" .. method_name)
    end
end

table.sort(lines)

function debug_state()
    return table.concat(lines, "\n") .. "\n"
end
