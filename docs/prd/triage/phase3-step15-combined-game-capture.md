---
title: "One real-LÖVE capture drawing a game frame the way the Lua game will"
description: "Each drawing function of the fake LÖVE matches real LÖVE 11.5 on its own, but nothing yet proves that their state (colour, transform, selected canvas, transparent texels) interacts in the fake as it does in real LÖVE once a game frame combines them."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 14, every function of design.md's list has its own
capture program, and the fake matches each one pixel for pixel. Each
capture, though, looks at one function and its edge cases. A game frame of
plans 2–3 mixes all of them, and their state carries from one call to the
next:

- the colour set for a rectangle or a point is still current when an image
  is drawn, and tints it;
- the camera's `translate` holds over points, rectangles and image draws
  alike, nested under another `push`/`translate` for a text block;
- see-through texels of sprites, letters and fill patterns land over
  content drawn before them in the same frame;
- the game leaves its canvas for the screen at the end of a frame (the
  window upscale), and may draw on another canvas, and comes back to find
  its pixels, its colour and its transform as they were.

Plan 4 deletes the C game, and from then on the fake LÖVE is the only
judge of the Lua game's frames. If one of these interactions differs
between the fake and real LÖVE, an e2e replay would pass in the fake while
the real game draws something else. The plan names this step as the only
point where interacting state is checked against real LÖVE before that
happens.

## Solution

One capture program, `game_frame`, draws a frame the way design.md says
the Lua game will, from the game's own assets:

- `setDefaultFilter("nearest", "nearest")` first, then `sprites.png` and
  `font.png` loaded with `love.image.newImageData(name)`;
- a see-through sprite sheet (colour 0 made transparent) and a recoloured
  copy (PICO-8 `palt`/`pal`: 0 opaque, 3 see-through, 7 → 8, 6 → 12), both
  built through `getPixel`/`setPixel`, the second one lazily inside the draw
  function, as the game builds its copies "the first time it is needed";
- one white glyph image from `font.png`, drawn letter by letter through
  quads under `setColor(palette colour)`, outlined text drawn shifted one
  pixel in the eight directions in colour 0, then on top;
- a 4×4 fill-pattern image (PICO-8 `fillp(0xA5A5)`), tiled through
  sub-quads and aligned to the screen under the camera;
- a camera `push()`, `translate(-x, -y)`, `pop()` with an integer camera,
  over a map of 8×8 sprite cells, pattern rectangles, discs of rectangle
  spans, rings of `points`, sprites, single pixels and text in a nested
  `push`/`translate`, partly off every edge;
- a HUD without the camera, with outlined text, the placeholder glyph and
  sprites over a bar;
- a canvas switch to another canvas, with the transform and colour set
  there carried back; a switch to the screen and draws there (the upscale's
  place); then a second "frame" drawn over the first one's pixels: a
  message box under a camera.

Real LÖVE pins its PNG on every CI push, and `make test` compares the fake
with it, like every other capture. **It already matches.** At commit
`bdd5152` (step 14's fake binding), the draft below, run in real LÖVE 11.5
and in today's fake, gives the same 19 200 pixels; so do 60 random scenes
mixing the same calls (see Implementation Decisions). The step therefore
adds the program, its two asset fixtures, its PNG and design.md's settled
conventions. If the committed PNG disagrees with the fake when the step is
implemented, finding and fixing the fake becomes the step's work (chunk 3).

## User Stories

1. As a plan-2 developer, I want one capture that draws a frame the way my game will, so that the conventions I follow have been proven together in real LÖVE and not only one by one.
2. As a plan-2 developer, I want the capture to load `sprites.png` and `font.png` with `love.image.newImageData(name)`, so that the bytes of the game's own assets go through both PNG decoders and both drawing paths.
3. As a plan-2 developer, I want a see-through sprite sheet built from `sprites.png` with `getPixel`/`setPixel`, so that the way I port PICO-8's default `palt` is pinned.
4. As a plan-2 developer, I want a recoloured, see-through copy of the sheet built lazily inside `love.draw`, with a canvas selected, so that building an Image in the middle of a frame is proven harmless.
5. As a plan-2 developer, I want the colour left by a rectangle or a point to tint the next image draw in the capture, so that "colour before `draw`" is pinned: `setColor(1, 1, 1)` restores an image's own colours, and a forgotten reset is visible.
6. As a plan-2 developer, I want text drawn from one white glyph image under `setColor(palette colour)`, so that I know a palette tint over white texels gives exactly the palette byte, and I need not build 16 letter images.
7. As a plan-2 developer, I want outlined text drawn as eight shifted copies in colour 0, then the text, so that the outline's transparent letter backgrounds over the map are pinned.
8. As a plan-2 developer, I want the placeholder glyph (code 127) and lowercase letters drawn, so that the font cell rule of design.md is exercised with the real `font.png`.
9. As a plan-2 developer, I want a 4×4 fill-pattern image tiled with sub-quads and aligned to the screen while the camera is translated, so that the fill-pattern port of design.md is proven, edges included.
10. As a plan-2 developer, I want `translate` with a negative and a positive camera component over `points`, `rectangle` and `draw` in the same frame, so that the three share one offset as in real LÖVE.
11. As a plan-2 developer, I want a nested `push`/`translate` inside the camera, so that a text block placed relative to the camera composes as in real LÖVE.
12. As a plan-2 developer, I want sprites, rectangles and points partly off the left, right, top and bottom edges under the camera, so that clipping after translation is pinned in a game layout.
13. As a plan-2 developer, I want `points` called with one flat table of many coordinates under the camera, as a ring drawn pixel by pixel, so that the table form holds with a translation.
14. As a plan-2 developer, I want discs drawn as rectangle spans and rings as points, with the game's `rectfill` and `pset` conversions, so that the pixel-by-pixel and span rules of design.md hold in a busy frame.
15. As a plan-2 developer, I want a switch to another canvas, a translation and a colour set there, then a draw back on the game canvas, so that the transform and colour belong to the graphics state, not to a canvas, in a game-like sequence.
16. As a plan-2 developer, I want `clear` on the other canvas while translated, so that `clear` ignoring the transform is pinned in context.
17. As a plan-2 developer, I want a switch to the screen with draws there, then back to the game canvas, so that the window upscale's place in the frame cannot leak into the canvas.
18. As a plan-2 developer, I want a second frame drawn over the first without a `clear`, a message box under its own camera, so that the canvas keeping its pixels across the end of a frame is pinned with images and text.
19. As a plan-2 developer, I want design.md to state the conventions this capture settles, so that I read them in one place after plan 1 is deleted.
20. As a plan-1 developer, I want the program to use only listed functions, so that it runs unchanged in both harnesses.
21. As a plan-1 developer, I want the program not to change the transform at its top level, so that the two harnesses' load order cannot matter.
22. As a plan-1 developer, I want the game's two assets as committed fixtures under the capture directory's `files/`, so that both harnesses reach them through the existing `files` symlink and a scratch `LOVE_CAPTURE_DIR` copy works.
23. As a plan-1 developer, I want the capture to hold many colours and many alpha-0 texels over opaque content, so that a drift in LÖVE or Mesa turns the CI love-capture job red.
24. As a plan-1 developer, I want a one-off randomized comparison of real LÖVE and the fake over many scenes mixing the same calls, recorded in the step note, so that "the fake matches in combination" rests on more than one hand-written frame.
25. As a plan-1 developer, I want the program run in real LÖVE with and without its `setDefaultFilter` line, byte-identical, recorded in the step note, so that step 14's "changes nothing" holds for a game frame.
26. As a plan-1 developer, if the fake does not match, I want the first differing pixel reduced to a minimal program and the fake fixed under `fix-bug`, so that the mismatch becomes a named convention and a test, not a tweak of the capture.
27. As the repository owner, I want the new check to cost one process of about 15 ms, run in parallel, and `make test` measured before and after with design.md's protocol, so that the fast loop stays fast.
28. As the repository owner, I want the CI love-capture job green with the new program and fixtures, its drift checks unchanged, so that the PNG is proven to be what real LÖVE draws.
29. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
30. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

How it was measured: `make love-capture LOVE_CAPTURE_DIR=<scratch>` on
scratch directories holding a copy of the real harness, of `files/`, and
of `game/sprites.png` and `game/font.png` added to that `files/`. The fake
side ran `tmp/love_capture_check` with a scratch copy of the fake harness
whose `files` symlink pointed to the same scratch `files/`. Everything ran
on LÖVE 11.5.0 and Mesa 25.2.8 llvmpipe (LLVM 20.1.2), 4 cores, at commit
`bdd5152` (step 14's binding committed, its capture and docs not yet).
These results are the expected answers. The committed PNG remains the
proof.

- **The draft below matches the fake, pixel for pixel**, all four
  channels of the 19 200 pixels. Its PNG holds 29 distinct colours, all
  opaque; 92 pixels are outside the palette, all from the one deliberate
  tint of a coloured sprite (see "Colour before draw" below). The
  check takes 13–15 ms in the fake (`draw_quads`: 12–14 ms).
- **`setDefaultFilter` changes nothing here.** The draft with and without
  its `setDefaultFilter("nearest", "nearest")` line gives byte-identical
  PNGs in real LÖVE. Two runs of `make love-capture` give byte-identical
  PNGs.
- **60 random scenes match.** A probe program (not committed) seeded 1 to
  60 drew, after `setDefaultFilter` and `clear(0, 0, 0, 1)`, 400 random
  operations each: palette `setColor` or white, `push` + `translate` by
  integers in [−100, 99] × [−80, 79] up to depth 6, `pop`, `points` with a
  table of 1–30 half-integer points from −40 to 200, integer rectangles
  with sizes from −10 to 49 and −8 to 31 partly off the canvas, `draw` of
  random quads up to 24×24 from three images (the sheet with colour 0
  see-through, the font with colour 0 see-through, the sheet with colour 3
  see-through) at positions from −30 to 169 and −20 to 129, switches
  between the harness canvas and another canvas, and screen draws between
  `setCanvas()` and `setCanvas(target)`. All 60 PNGs equal the fake's
  canvas (212 164 non-black pixels in all, 136 distinct colours, tints of
  coloured texels included).
- So no interaction checked here needs a change to the fake.

### Decisions

- **One program, `game_frame`.** One capture check (one process) instead
  of several: the point is the interaction, and the budget is per check.
  Its draw function draws two "frames" in one call, since the harness
  calls it once; LÖVE's per-frame `origin()` stays covered by step 13's
  log test, not by this capture.
- **The game's assets become fixtures.** `game/sprites.png` and
  `game/font.png` are copied byte for byte to `tools/love_capture/files/`
  under the same names, as committed bytes, never regenerated by `make`.
  - Both harnesses already reach `files/` through their `files` symlink,
    and a scratch `LOVE_CAPTURE_DIR` copies `files/`: nothing changes in
    the harnesses, the Makefile or CI, and the CI drift checks (`git diff`
    and "no file outside the commit" under `tools/love_capture/`) keep
    covering them.
  - Rejected: a symlink to `game/` from each harness or from `files/`. A
    scratch copy would carry a dangling relative link, and every change of
    the game's assets by `make asset-export` would silently change a
    capture's input, turning `make test` red until someone reruns
    `make love-capture`.
  - No check keeps the copies equal to `game/`: the capture pins drawing
    conventions on real asset bytes, not the assets themselves, and
    `make test` already guards `game/` against the C data. A later change
    of `game/` leaves this capture valid. (About 54 KB committed.)
- **Colour before draw.** The program sets each colour as `byte / 255`
  from a 16-entry table holding design.md's palette bytes (written into
  the Lua file; no `src/` file is read). It draws images after
  `setColor(1, 1, 1)` except in two places, on purpose:
  - text: white glyph texels under a palette colour, which gives exactly
    the palette byte (and colour 0 gives `000000` for the outline);
  - one 16×8 coloured sprite drawn under colour 12 at the top left: the
    forgotten reset the game must avoid. It is the only source of the 92
    non-palette pixels, kept because it pins the tint carried from
    `setColor` into `draw` on real texels. The game's rule stays design.md's
    ("call `setColor(1, 1, 1)` before drawing an image's own colours").
- **Text through a tinted white glyph image.** design.md's "Text" bullet
  speaks of per-colour letter images. This capture proves the cheaper
  equivalent: one image whose lit cells are white `FF` texels, every other
  texel alpha 0, drawn under `setColor(palette colour)`. design.md then
  allows both, since both give exact palette bytes (`round(c × 255 / 255)`
  is c). The recoloured sprite copy covers the per-colour-copy route.
- **Canvas switches as the game will make them**: the harness canvas is
  the game canvas; `setCanvas()` with draws on the screen stands for the
  window upscale (the upscale call itself is not on the list); a second
  160×120 canvas created at the top level receives a `translate`, a
  `clear` and a colour, which then hold on the game canvas. The translate
  is undone with its opposite before the next section, as the harness
  gives no `origin()`.
- **The transform is changed only inside the draw function**, balanced
  (every `push` has its `pop`, the canvas-switch translation is undone),
  as design.md's capture paragraph requires.
- **`setDefaultFilter("nearest", "nearest")` is the program's first
  line**, at its top level, as the game's `love.load` will call it first
  (H-14-1, H-14-2).
- **No new function, no new fake behaviour.** `love.graphics` keeps step
  14's 13 functions (H-14-5); the fake's C code is unchanged unless chunk 3
  is needed.
- **The randomized probe is not committed.** It took 10 s of real LÖVE for
  60 scenes and would add 60 checks; its result is recorded in the step
  note instead. The step's implementer re-runs it once at the end (story
  24) from the description above.

### Capture program of this step

The draft, from the measurement (`tools/love_capture/game_frame.lua`). If
the committed PNG disagrees with this section, the PNG wins and design.md
follows it.

```lua
local graphics = love.graphics
graphics.setDefaultFilter("nearest", "nearest")

local PALETTE = {
    {0x00, 0x00, 0x00}, {0x1D, 0x2B, 0x53}, {0x7E, 0x25, 0x53}, {0x00, 0x87, 0x51},
    {0xAB, 0x52, 0x36}, {0x5F, 0x57, 0x4F}, {0xC2, 0xC3, 0xC7}, {0xFF, 0xF1, 0xE8},
    {0xFF, 0x00, 0x4D}, {0xFF, 0xA3, 0x00}, {0xFF, 0xEC, 0x27}, {0x00, 0xE4, 0x36},
    {0x29, 0xAD, 0xFF}, {0x83, 0x76, 0x9C}, {0xFF, 0x77, 0xA8}, {0xFF, 0xCC, 0xAA},
}

local function color(c)
    local rgb = PALETTE[c + 1]
    graphics.setColor(rgb[1] / 255, rgb[2] / 255, rgb[3] / 255)
end

local function palette_index(r, g, b)
    for c = 0, 15 do
        local rgb = PALETTE[c + 1]
        if r == rgb[1] / 255 and g == rgb[2] / 255 and b == rgb[3] / 255 then
            return c
        end
    end
    error("not a palette colour")
end

-- swap[c]: another colour, false for see-through, nil to keep c.
local function recolour(source, width, height, swap)
    local data = love.image.newImageData(width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local c = palette_index(source:getPixel(x, y))
            local n = swap[c]
            if n == nil then n = c end
            if n then
                local rgb = PALETTE[n + 1]
                data:setPixel(x, y, rgb[1] / 255, rgb[2] / 255, rgb[3] / 255)
            end
        end
    end
    return graphics.newImage(data)
end

local sprites = love.image.newImageData("files/sprites.png")
local font = love.image.newImageData("files/font.png")
local sheet = recolour(sprites, 128, 128, {[0] = false})
local swapped_sheet
local glyphs = (function()
    local data = love.image.newImageData(48, 30)
    for y = 0, 29 do
        for x = 0, 47 do
            if palette_index(font:getPixel(x, y)) == 7 then
                data:setPixel(x, y, 1, 1, 1)
            end
        end
    end
    return graphics.newImage(data)
end)()

local pattern = (function()
    local data = love.image.newImageData(4, 4)
    local bits = 0xA5A5
    for y = 0, 3 do
        for x = 0, 3 do
            local bit = 15 - (y * 4 + x)
            if math.floor(bits / 2 ^ bit) % 2 == 0 then
                data:setPixel(x, y, 1, 1, 1)
            end
        end
    end
    return graphics.newImage(data)
end)()

local scratch = graphics.newCanvas(160, 120)

local function spr(image, n, x, y, w, h)
    local quad = graphics.newQuad(n % 16 * 8, math.floor(n / 16) * 8, w * 8, h * 8, 128, 128)
    graphics.draw(image, quad, x, y)
end

local function rectfill(x0, y0, x1, y1, c)
    color(c)
    graphics.rectangle("fill", math.min(x0, x1), math.min(y0, y1), math.abs(x1 - x0) + 1, math.abs(y1 - y0) + 1)
end

local function pset(x, y, c)
    color(c)
    graphics.points(x + 0.5, y + 0.5)
end

local function circfill(cx, cy, r, c)
    color(c)
    for dy = -r, r do
        local half = math.floor(math.sqrt(r * r - dy * dy) + 0.5)
        graphics.rectangle("fill", cx - half, cy + dy, 2 * half + 1, 1)
    end
end

local function circ(cx, cy, r, c)
    color(c)
    local coordinates = {}
    for k = 0, 63 do
        local angle = k * math.pi / 32
        local x = cx + math.floor(r * math.cos(angle) + 0.5)
        local y = cy + math.floor(r * math.sin(angle) + 0.5)
        coordinates[#coordinates + 1] = x + 0.5
        coordinates[#coordinates + 1] = y + 0.5
    end
    graphics.points(coordinates)
end

-- The pattern is aligned to the screen: camera_x, camera_y undo the translation.
local function pattern_rectfill(x0, y0, x1, y1, c, camera_x, camera_y)
    color(c)
    local y = y0
    while y <= y1 do
        local qy = (y - camera_y) % 4
        local h = math.min(4 - qy, y1 - y + 1)
        local x = x0
        while x <= x1 do
            local qx = (x - camera_x) % 4
            local w = math.min(4 - qx, x1 - x + 1)
            graphics.draw(pattern, graphics.newQuad(qx, qy, w, h, 4, 4), x, y)
            x = x + w
        end
        y = y + h
    end
end

local function print_text(text, x, y, c)
    color(c)
    for i = 1, #text do
        local code = text:byte(i)
        if code < 32 or code > 126 then code = 127 end
        local cell = code - 32
        graphics.draw(glyphs, graphics.newQuad(cell % 16 * 3, math.floor(cell / 16) * 5, 3, 5, 48, 30), x + (i - 1) * 4, y)
    end
end

local function print_outlined(text, x, y, c)
    for dy = -1, 1 do
        for dx = -1, 1 do
            if dx ~= 0 or dy ~= 0 then print_text(text, x + dx, y + dy, 0) end
        end
    end
    print_text(text, x, y, c)
end

local function frame_one(camera_x, camera_y)
    graphics.clear(0, 0, 0, 1)
    rectfill(0, 0, 159, 7, 1)

    graphics.push()
    graphics.translate(-camera_x, -camera_y)
    rectfill(-20, 10, 140, 70, 1)
    graphics.setColor(1, 1, 1)
    for row = 0, 7 do
        for column = 0, 21 do
            spr(sheet, (row * 22 + column) % 128, -24 + column * 8, 2 + row * 8, 1, 1)
        end
    end
    pattern_rectfill(-15, 30, 61, 49, 12, camera_x, camera_y)
    pattern_rectfill(100, 60, 170, 135, 8, camera_x, camera_y)

    if not swapped_sheet then
        swapped_sheet = recolour(sprites, 128, 128, {[0] = 0, [3] = false, [7] = 8, [6] = 12})
    end
    local planets = {{60, 70, 6, 3}, {95, 88, 7, 9}, {140, 80, 5, 14}, {30, 60, 8, 4}, {190, 95, 6, 11}}
    for index, planet in ipairs(planets) do
        local x, y, r, c = planet[1], planet[2], planet[3], planet[4]
        circfill(x, y, r, c)
        circ(x, y, r + 3, 6)
        graphics.setColor(1, 1, 1)
        spr(index % 2 == 0 and swapped_sheet or sheet, 64 + index * 2, x - 4, y - 4, 2, 2)
        pset(x + r + 6, y, 10)
        pset(x, y - r - 6, 10)
    end
    color(11)
    graphics.points(30.5, 75.5, 31.5, 75.5, 32.5, 76.5, -40.5, 75.5, 200.5, 75.5)

    graphics.push()
    graphics.translate(10, -4)
    print_outlined("SLIPWAYS", 30, 66, 7)
    graphics.pop()
    print_text("x=" .. camera_x, 60, 100, 9)
    graphics.pop()

    rectfill(0, 112, 159, 119, 5)
    print_outlined("CREDITS 0123 abc~", 2, 113, 10)
    graphics.setColor(1, 1, 1)
    spr(sheet, 70, 120, 104, 1, 2)
    spr(sheet, 71, 152, 104, 1, 2)
    spr(swapped_sheet, 0, 140, 110, 2, 2)
    print_text("\127", 100, 2, 8)
    color(12)
    spr(sheet, 16, 4, 0, 2, 1)
end

local function scratch_frame(canvas)
    graphics.setCanvas(scratch)
    graphics.translate(5, 5)
    graphics.clear(1, 0, 0, 1)
    color(14)
    graphics.rectangle("fill", 0, 0, 160, 120)
    graphics.setCanvas(canvas)
    graphics.rectangle("fill", 150, 0, 4, 3)
    graphics.setColor(1, 1, 1)
    spr(sheet, 3, 40, 0, 1, 1)
    graphics.translate(-5, -5)
end

local function frame_two(canvas)
    graphics.setCanvas()
    graphics.setColor(1, 1, 1)
    graphics.draw(sheet, graphics.newQuad(0, 0, 128, 128, 128, 128), 0, 0)
    rectfill(0, 0, 50, 50, 8)
    graphics.setCanvas(canvas)

    graphics.push()
    graphics.translate(3, 0)
    rectfill(20, 84, 80, 98, 0)
    rectfill(21, 85, 79, 97, 13)
    print_outlined("BUILD?", 26, 88, 7)
    graphics.setColor(1, 1, 1)
    spr(sheet, 33, 70, 87, 1, 1)
    graphics.pop()
end

return function(canvas)
    frame_one(37, -9)
    scratch_frame(canvas)
    frame_two(canvas)
end
```

The implementer may tidy names and the one comment to CLAUDE.md's rules
(the comment on `pattern_rectfill` states a trap: the pattern follows the
screen, not the shape), but keeps every interaction listed under
"What it pins". The real-LÖVE PNG of the draft had sha256
`d912b16898881b722604c75e483f954d138bdd950e133aa33dbbf4d4af7a0346`; a
byte-identical program is expected to give it again.

What it pins:

- **Colour before draw**: palette colours left by `rectfill`/`pset`/text
  tint the next draw; white restores texels; one deliberate coloured tint.
- **Translate over points, rectangles and draws**: camera (37, −9) as
  `translate(-37, 9)`, a nested `translate(10, −4)` for a title, a second
  camera `translate(3, 0)` in the second frame; a flat `points` table of
  64 points per ring; points at −40.5 and 200.5 world x, clipped.
- **Transparent texels over existing content**: the see-through sheet over
  the map's rectangle, the pattern's alpha-0 texels over the map and its
  sprites, discs under sprites, outlines over everything, the swapped sheet
  (colour 0 opaque, 3 see-through) over the HUD bar.
- **Clipping after translation** on all four edges (map from world x −24,
  a pattern rectangle to world (170, 135), a planet at world x 190, HUD
  sprites at x 152).
- **Canvas switches**: another canvas translated, cleared and coloured,
  whose translation and colour hold back on the game canvas; draws on the
  screen; the first frame's pixels kept under the second frame.
- **Image builds at load and mid-frame**, from both assets, through
  `getPixel` values compared with `byte / 255` and written with
  `setPixel`.

### Conventions written into `docs/love-port/design.md`

- **"How the Lua game draws"** gains a closing paragraph, **"In
  combination"**: real LÖVE 11.5 and the fake agree when the functions
  are mixed as above (one capture, and 60 random scenes once, at the
  step's commit). Colour, transform and default filter are graphics state:
  set once, they hold across `points`, `rectangle`, `draw`, canvas
  switches and screen draws until changed; `clear` ignores the transform
  and the colour. **The rule for the game**: set the colour before every
  primitive and `setColor(1, 1, 1)` before every image drawn in its own
  colours; undo every translation before leaving the canvas for the
  upscale, or wrap the frame in `push`/`pop`; build see-through and
  recoloured copies with `getPixel` compared with `byte / 255` and
  `setPixel` of palette bytes. Capture: `game_frame`.
- **The "Text" bullet** allows one white glyph image drawn under
  `setColor(palette colour)` beside per-colour letter images: both give
  exact palette bytes (measured in `game_frame`).
- **"Real-LÖVE captures"** names `files/sprites.png` and `files/font.png`
  as byte copies of the game's assets at this step, kept as fixtures, not
  synchronised with `game/`.
- Placement relative to step 14's "Default filter" paragraph: unchanged
  (H-14-2); this step does not touch "Images", "Transforms" or "Default
  filter".

### Modules

- **Capture directory**: one program, its PNG, two fixtures.
- **Makefile, harnesses, CI**: no change. The new pair becomes its own
  `love-capture-check-game_frame` by itself (H-14-3), and `make
  love-capture` runs it with the others.
- **The fake**: no change, unless chunk 3 is needed.

## Implementation Chunks

1. **Fixtures and capture.**
   - Copy the two assets to `files/`; add `game_frame.lua`.
   - `make love-capture`: `git diff` / `git status` under
     `tools/love_capture/` show only the new program, its PNG and the two
     fixtures, every other PNG byte-identical.
   - `make test` passes, `love-capture-check-game_frame` included.
   - Stories 1–18, 20–23, 30.
2. **Proofs, docs, timing.**
   - One-off re-runs in a scratch harness copy: the program without its
     `setDefaultFilter` line (byte-identical PNG), and the 60-scene random
     probe (60 of 60 equal to the fake).
   - design.md as above; the CI run with the love-capture job green; the
     `make test` timing; the step note (Step 3.15).
   - Stories 19, 24, 25, 27–29.
3. **Only if the comparison of chunk 1 or a probe of chunk 2 fails: find
   and fix the fake.**
   - Through `fix-bug`: reduce the first differing pixel to the smallest
     program that still differs, in real LÖVE and in the fake (bisect the
     draw function's sections, then the calls); name the interaction (which
     state carried, which rule).
   - Fix the fake's rule in its module, with a failing test first in the
     module's existing unit or CLI test; if it is a convention, add a
     minimal capture program of its own beside `game_frame` and write the
     convention into design.md's paragraph of that function.
   - Never change `game_frame` to dodge the difference.
   - Story 26.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the capture comparison's verdict, the binary's exit status and output.
  Nothing reaches into the Lua state.
- **The capture comparison is the test**: `love-capture-check-game_frame`
  in `make test`, and every earlier capture still passing. No new unit or
  CLI test: the step adds no fake behaviour (chunk 3 excepted, where the
  fix's module test gets the failing case).
- **Prior art**: `draw_quads` (image draws, clipping), `translate_stack`
  (offsets over all three primitives), `canvas_switch_back` (canvas
  switches), `imagedata_files` (fixtures read through `files/`), step 14's
  `filter_nearest` (H-14-2).
- **Proofs recorded in the step note** (Step 3.15 in plan 1):
  - `make love-capture` writes the new PNG and leaves every other file of
    `tools/love_capture/` byte-identical; a second run changes nothing;
  - the PNG's sha256, its distinct colours and its non-palette pixel
    count;
  - the `setDefaultFilter`-less variant byte-identical in real LÖVE;
  - the 60-scene random probe: 60 of 60 equal, with its totals;
  - the CI run id with the love-capture job green;
  - `make test` before and after, with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 14's last commit,
    H-14-4) and against `2e7215c`; both paired medians ≤ 3 %, or 22
    pooled rounds decide. Record the plain and paired medians, the
    spreads, the rounds, the cores and the new check's own time (about
    15 ms measured; its process loads two PNGs and builds two 128×128
    copies through about 50 000 `getPixel`/`setPixel` calls).
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- Any new function on design.md's list, any change to the fake's
  conventions (chunk 3 excepted, and then only the rule the capture
  proves wrong).
- The window upscale, `origin()` per frame inside one capture (step 13's
  log test covers it), translucent colours or texels, fractional
  positions: all refused or outside the fake already.
- Keeping `files/sprites.png` and `files/font.png` in sync with `game/`.
- Committing the random probe, or any capture beyond `game_frame` unless
  chunk 3 needs a minimal one.
- Capture to palette numbers, the fatal-error catalogue and
  `make love-e2e`: steps 16 to 18.
- The Lua game itself: plans 2–3.
- Audio, always.

## Hypotheses

Plan step 14 (`setDefaultFilter`) is being implemented while this PRD is
written. Its PRD was read, and its first commit (`bdd5152`, the fake's
binding and its log lines) was in the tree when the measurements ran;
none of its later work was read. Before starting, the implementer checks
each item against the committed step 14. When one differs, adapt names,
placement and dependent lines, not the decisions.

- **H-14-1**: the fake accepts `setDefaultFilter("nearest", "nearest")`
  at a capture program's top level and stores nothing (true at
  `bdd5152`: the draft, with the line, passed `tmp/love_capture_check`).
  If step 14 ends with any other behaviour, keep the line only if the
  draft still passes; otherwise delete it and record why.
- **H-14-2**: step 14 commits the capture `filter_nearest` and a "Default
  filter" paragraph in design.md after "Transforms", which allows a
  capture program to call `setDefaultFilter` at its top level and tells
  the game to call it first in `love.load`. This step's design.md text
  sits in "How the Lua game draws" and "Real-LÖVE captures" and does not
  repeat it.
- **H-14-3**: step 14 changes neither harness, the `files/` fixtures, the
  Makefile's capture registration (a top-level `.lua`/`.png` pair is its
  own `love-capture-check-<name>`) nor the CI love-capture job.
- **H-14-4**: step 14's last commit is this step's "before" timing
  baseline, and its step note (Step 3.14) records its timings.
- **H-14-5**: `love.graphics` holds 13 functions after step 14; this step
  changes no key list.
- **H-14-6**: step 14 changes no drawing rule of `points`, `rectangle`,
  `setColor`, `draw`, `clear` or the transform. If it does, the draft and
  the probe are re-run before chunk 1, and a difference goes to chunk 3.

## Further Notes

- The exploration lived in scratch directories passed to
  `make love-capture`, each with a copy of the harness, of `files/` and of
  the two assets; the fake ran through `tmp/love_capture_check` with a
  scratch harness copy. Nothing of it is committed. None of it ran
  `make test` or `make e2e`.
- design.md mentions no 128×128 game area: the game draws on the 160×120
  canvas, and the capture uses all of it.
- The program reads nothing from `src/`: the palette bytes are design.md's
  (`src/p8/palette.c` is only their origin), the font cell rule and the
  sprite numbering are design.md's "The asset PNGs".
- Why a capture that already matches is still worth committing: it is the
  only committed proof of the interactions, and CI reruns it in real LÖVE
  on every push, so a LÖVE, Mesa or fake change that breaks a combination
  turns a job red before plan 4 removes the C reference.
