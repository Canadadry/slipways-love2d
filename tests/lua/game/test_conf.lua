local check = require("check")

local expect = check.expect

local function run_conf()
  local saved_love = love
  love = {}
  local ok, result = pcall(function()
    dofile("game/conf.lua")
    local t = { window = {}, modules = {} }
    love.conf(t)
    return t
  end)
  love = saved_love
  assert(ok, result)
  return result
end

local function test_the_window_opens_at_640_by_480_resizable_and_highdpi()
  local t = run_conf()
  expect(t.window.width, 640, "width")
  expect(t.window.height, 480, "height")
  expect(t.window.resizable, true, "resizable")
  expect(t.window.highdpi, true, "highdpi")
end

return check.run({
  { "the window opens at 640 by 480 resizable and highdpi", test_the_window_opens_at_640_by_480_resizable_and_highdpi },
})
