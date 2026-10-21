local frames_drawn = 0

function love.draw()
    frames_drawn = frames_drawn + 1
end

function debug_state()
    return "frames " .. frames_drawn .. "\n"
end
