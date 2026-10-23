# When to Use a Test Double

Replace only **system boundaries**:

- Input: a scripted events file, never a real pad or mouse
- Output: the draw backend of `game/p8/gfx.lua` — the LÖVE backend's frames read back as a fake-LÖVE capture (`tools/capture_read.lua`), the software backend's through the vram — never a window

Don't replace:

- Your own modules
- Internal collaborators
- Any module of `game/` a test can call for real
- Randomness: `p8_fixed_rnd` already starts from a fixed seed, so a run is reproducible as is

## Designing for Test Doubles

**1. Pass the dependency in**

The module receives its graphics state and its memory as parameters instead of reaching for a global:

```lua
-- Easy to test: the caller chooses the state (real signature)
function M.p8_pset(gfx, mem, x, y, col)

-- Hard to test: hidden dependency on the game's screen
function M.p8_pset(x, y, col)
```

The test then builds tables of its own, and installs a recording backend where the game installs the LÖVE one:

```lua
local function new_runtime()
  local graphics, mem = {}, {}
  gfx.p8_gfx_init(graphics)
  vram.p8_vram_init(mem)
  return graphics, mem
end

-- Returns the backend and the list of { n, x, y } of every spr it received.
local function spr_recorder()
  local calls = {}
  local recorder = {
    spr = function(_, _, n, x, y, w, h)
      calls[#calls + 1] = { n = n, x = x, y = y }
    end,
  }
  return recorder, calls
end
```

**2. One narrow function per boundary operation**

A draw backend has one field per operation (`cls`, `pset`, `rectfill`, `spr`, ...) — not one `draw(op, ...)` entry point with a switch. Each double then implements one plain behavior:

- No conditional logic in the test setup
- Easy to see which operations a test exercises
- A refusing double (`spr` set to a function that asserts) proves in one line that a path never draws a sprite
