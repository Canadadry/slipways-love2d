local M = {}

function M.expect(actual, expected, what)
  if actual ~= expected then
    error(string.format("%s: expected %s, got %s", what, tostring(expected), tostring(actual)), 2)
  end
end

-- tests: an ordered list of { name, function }. Runs all of them, then exits 1
-- when any failed.
function M.run(tests)
  local failures = 0
  for _, test in ipairs(tests) do
    local ok, err = pcall(test[2])
    if not ok then
      failures = failures + 1
      io.stderr:write(string.format("%s: %s: %s\n", arg[0], test[1], tostring(err)))
    end
  end
  if failures > 0 then
    os.exit(1)
  end
end

return M
