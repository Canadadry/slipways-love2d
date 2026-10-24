-- Each file must end with `return check.run({...})`: a file that returns no
-- failure count fails, so a test file cannot pass by never running its tests.
local files = { ... }
table.sort(files)

local failed_files = 0
for _, file in ipairs(files) do
  local before = {}
  for name in pairs(package.loaded) do
    before[name] = true
  end
  arg[0] = file
  local ok, failures = pcall(dofile, file)
  if not ok then
    io.stderr:write(string.format("%s: load: %s\n", file, tostring(failures)))
    failed_files = failed_files + 1
  elseif type(failures) ~= "number" then
    io.stderr:write(string.format("%s: load: does not return check.run's failure count\n", file))
    failed_files = failed_files + 1
  elseif failures > 0 then
    failed_files = failed_files + 1
  end
  for name in pairs(package.loaded) do
    if not before[name] then
      package.loaded[name] = nil
    end
  end
end

if failed_files > 0 then
  io.stderr:write(string.format("lua-test-game: %d of %d files fail\n", failed_files, #files))
  os.exit(1)
end
