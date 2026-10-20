local updates = 0

function love.update()
    updates = updates + 1
end

function debug_state()
    return "updates " .. updates .. "\n"
end
