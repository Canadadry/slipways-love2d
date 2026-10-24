return {
    callbacks = { "conf", "load", "update", "draw", "gamepadpressed", "gamepadreleased", "resize" },
    modules = {
        graphics = {
            "clear", "draw", "getDimensions", "newCanvas", "newImage", "newQuad", "points", "pop", "push",
            "rectangle", "setCanvas", "setColor", "setDefaultFilter", "translate",
        },
        image = { "newImageData" },
        joystick = { "getJoysticks" },
        touch = { "getPosition", "getTouches" },
        mouse = { "getPosition", "isDown" },
        filesystem = { "getInfo", "read", "write" },
    },
    methods = {
        Canvas = {},
        Image = {},
        Quad = {},
        ImageData = { "getPixel", "setPixel" },
        Joystick = { "isGamepadDown" },
    },
}
