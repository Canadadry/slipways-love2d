---
title: "fake_love draws images through newImage, newQuad and draw exactly as real LÖVE 11.5 does at integer positions"
description: "The Lua game draws its sprites, letters and fill patterns as quads of images, but the fake LÖVE cannot turn an ImageData into a drawable image, and nobody has measured how real LÖVE's default linear filter, its alpha blending and the setColor tint act on such a draw."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 10, `tmp/fake_love` can draw points and filled
rectangles in any opaque colour, and build `ImageData` pixel by pixel
(H-10-1). Nothing built in an ImageData reaches a canvas yet. The Lua game
of plans 2–3 draws most of its frame that way (design.md, "How the Lua game
draws"): sprites and the map are quads of the sprite sheet or of its
recoloured copies, letters are quads of per-colour letter images whose
background is transparent, fill patterns are images drawn under the shape.
It needs `love.graphics.newImage(imagedata)`, `newQuad(x, y, w, h, sw, sh)`
and `draw(image, quad, x, y)`. The fake has none of them: a game that calls
them fails.

Nobody has measured what real LÖVE does with them either, and three things
could make the fake disagree with LÖVE without anyone noticing:

- **The default filter is linear.** In real LÖVE 11.5,
  `getDefaultFilter()` returns `linear linear 1`, and a new Image is
  `linear`, wrap `clamp`, one mipmap level. The game only switches to
  `nearest` in step 14. A linear filter can blend neighbouring texels into
  colours that are not in the palette.
- **Blending.** The blend mode is `alpha`/`alphamultiply`. Design.md says
  see-through pixels come from images, so a texel of alpha 0 must leave
  the canvas exactly as it was, and a texel of alpha 1 must replace it
  exactly. Nothing proves either.
- **Tint.** `setColor` multiplies every texel drawn after it. If the fake
  ignored the current colour, a game that forgot `setColor(1, 1, 1)` before
  a sprite would pass in the fake and draw tinted sprites in LÖVE.

Measured below in real LÖVE 11.5 on Mesa llvmpipe: at integer positions,
with quads built on the image's own size, a draw copies texels exactly,
with **byte-identical results under the linear and the nearest filter**.
An alpha-0 texel leaves all four bytes of the canvas pixel untouched,
transparent canvas pixels included. An alpha-1 texel writes its bytes with
alpha `FF`. The tint gives each channel `round(colour × texel / 255)`, for
all 65 536 pairs. Fractional positions are where the filter matters: from
1/512 of a pixel on, the linear filter blends texels, and the nearest
filter drops or doubles texel columns near half a pixel. Translucent
texels blend with llvmpipe's 8-bit arithmetic.

## Solution

`love.graphics` gains exactly three functions, and the fake gains two new
object types with no methods:

- `love.graphics.newImage(data)` takes exactly one ImageData. It returns
  a new Image holding a **copy** of the ImageData's size and bytes at the
  time of the call: later `setPixel` calls on the ImageData do not change
  the Image, as in LÖVE. It refuses an ImageData that holds an alpha byte
  other than `00` and `FF`, naming the first such pixel. The Image is freed
  by Lua's garbage collector.
- `love.graphics.newQuad(x, y, w, h, sw, sh)` takes exactly six Lua
  numbers, all integers: `sw` and `sh` (the reference size) from 1 to
  1024, `x` and `y` from 0, `w` and `h` from 1, the quad inside the
  reference size (`x + w ≤ sw`, `y + h ≤ sh`). It returns a Quad.
- `love.graphics.draw(image, quad, x, y)` takes exactly an Image, a Quad
  and two integer Lua numbers of magnitude below 2^20, and the quad's
  reference size must equal the image's size. Each texel (`quad.x + i`,
  `quad.y + j`) goes to canvas pixel (`x + i`, `y + j`). Pixels off the
  canvas are clipped silently. An alpha-`00` texel leaves the canvas pixel's
  four bytes as they were. An alpha-`FF` texel writes, per channel,
  `round(c × t / 255)` (c the current colour's byte, t the texel's byte)
  and alpha `FF`. With white, the default colour, that is the texel itself.
  With the screen selected, nothing the fake models changes.
- Every refusal is an error starting with the function's full name, and
  a refused call changes nothing.

The fake does not model texture filtering. It needs no model: every draw
it accepts is one where real LÖVE gives the same bytes under either filter.
Every draw where the filter would matter is refused: a fractional position,
a fractional quad, a quad scaled by a reference size other than the
image's, or a translucent texel. Step 14 adds
`setDefaultFilter("nearest", "nearest")` as a call that changes nothing the
fake draws (see Implementation Decisions).

Two capture programs pin all of this in real LÖVE on every CI push, and
`make test` compares them with the fake. The conventions go into
`docs/love-port/design.md` as an "Images" paragraph, with the rule the
game must follow.

## User Stories

1. As a plan-2 developer, I want `love.graphics.newImage(data)` in the fake, so that the sprite sheet, its recoloured copies, letter images and pattern images can be drawn.
2. As a plan-2 developer, I want `newImage` to copy the ImageData's bytes when it is called, as LÖVE does, so that I can keep editing an ImageData without changing an Image already made from it.
3. As a plan-2 developer, I want `love.graphics.newQuad(x, y, w, h, sw, sh)` in the fake, so that I can name a sprite, a map tile, a letter cell or a pattern as a piece of an image.
4. As a plan-2 developer, I want `love.graphics.draw(image, quad, x, y)` in the fake, so that a piece of an image reaches the canvas.
5. As a plan-2 developer, I want a draw at integer coordinates to copy every texel of the quad to the canvas pixel at the same offset, so that a sprite lands where PICO-8's `spr` puts it.
6. As a plan-2 developer, I want a texel of alpha 0 to leave the canvas pixel's four bytes untouched, over opaque and over transparent pixels alike, so that see-through sprite pixels and letter backgrounds work as in LÖVE.
7. As a plan-2 developer, I want a texel of alpha 1 to replace the canvas pixel with its colour and alpha `FF`, over any pixel, so that opaque sprite pixels are exact.
8. As a plan-2 developer, I want a draw to clip silently at every canvas edge, with negative positions too, so that a sprite partly off the screen draws its visible part and raises no error, as in LÖVE.
9. As a plan-2 developer, I want a draw entirely off the canvas, even far off (up to just below 2^20), to draw nothing and raise no error, so that off-screen entities cost nothing.
10. As a plan-2 developer, I want later draws to cover earlier ones where their texels are opaque and to show them through where they are transparent, so that layered drawing composes as in LÖVE.
11. As a plan-2 developer, I want the current `setColor` colour to tint a draw the way LÖVE tints it (`round(c × t / 255)` per channel), so that a draw after a coloured shape is tinted in the fake exactly when it is tinted in LÖVE.
12. As a plan-2 developer, I want the default colour, opaque white, to draw an image's own bytes, so that `setColor(1, 1, 1)` before a draw gives the sprite unchanged.
13. As a plan-2 developer, I want a palette colour tinting a white (`FF`) texel to give exactly that palette colour, so that I may draw one white mask image in any palette colour if that turns out simpler.
14. As a plan-2 developer, I want the tint never to touch an alpha-0 texel, so that a tinted sprite still lets the background through.
15. As a plan-2 developer, I want a draw with the screen selected to change nothing the fake models, as `points` and `rectangle` do, so that drawing before `setCanvas` never corrupts a canvas.
16. As a plan-2 developer, I want one Quad to be reusable with any image of its reference size and at any number of positions, so that one quad per sprite serves every recoloured copy of the sheet.
17. As a plan-2 developer, I want images of any size from 1×1 to 1024×1024, odd sizes such as the 3×5 letter cell and the 48×30 font included, to draw the same way, so that no size is special.
18. As a plan-2 developer, I want `newImage` to refuse an ImageData holding a translucent alpha (neither 0 nor 1), naming the pixel, so that a colour LÖVE would blend into a non-palette colour is reported where the image is made.
19. As a plan-2 developer, I want `draw` to refuse a fractional position, so that a 16.16 fixed-point coordinate passed without `math.floor` is reported instead of blended by LÖVE's linear filter.
20. As a plan-2 developer, I want `newQuad` to refuse fractional, negative, empty or out-of-image values and reference sizes outside 1–1024, so that a quad that would sample outside its image, sample between texels, or draw nothing by mistake is reported.
21. As a plan-2 developer, I want `draw` to refuse a quad whose reference size differs from the image's size, so that a quad built for the sprite sheet cannot silently scale the font image.
22. As a plan-2 developer, I want `draw(image, x, y)` without a quad, any extra argument (rotation, scale, origin, shear) and a trailing nil refused, so that the fake implements exactly one way to draw.
23. As a plan-2 developer, I want `draw` to refuse an ImageData, a Canvas, a table or a string as its first argument, and anything but a Quad as its second, so that the misplaced object is named.
24. As a plan-2 developer, I want `newImage` to refuse a file name, a settings table, a Canvas, an Image and any count but one, so that every image comes from an ImageData the game built.
25. As a plan-2 developer, I want non-numbers (no string coercion), NaN and infinities refused wherever a number belongs, so that the fake stays as strict as `rectangle` and `setColor`.
26. As a plan-2 developer, I want a refused call to change nothing, neither the canvas nor any object, so that an error caught by `pcall` leaves the state as it was.
27. As a plan-2 developer, I want Images and Quads to hold no method, so that `getWidth`, `setFilter`, `setViewport` or `replacePixels` cannot slip into the game unmeasured.
28. As a plan-2 developer, I want unreachable Images and Quads freed by the garbage collector, with no count limit, so that a game making images lazily never runs out.
29. As a plan-2 developer, I want `love.graphics` to hold exactly `clear`, `draw`, `newCanvas`, `newImage`, `newQuad`, `points`, `rectangle`, `setCanvas` and `setColor` from `main.lua` on, and to stay nil in `conf.lua`, so that no function exists without its capture.
30. As a plan-2 developer reading design.md, I want an "Images" paragraph with the forms, the copy rule, the blending, the tint, the refusals and the rule for the game, so that I do not need plan 1, which will be deleted.
31. As a plan-2 developer reading design.md, I want to learn that LÖVE's default filter is linear and why integer draws are still exact, so that I do not depend on step 14's `nearest` for correctness, and know that fractional positions are what it would change.
32. As a plan-1 developer of step 14, I want this step to state that the fake does not model the filter and why, so that step 14 only adds `setDefaultFilter("nearest", "nearest")` with its capture and changes nothing the fake draws.
33. As a plan-1 developer, I want a capture program drawing quads of several images (alpha-0 texels over transparent and opaque content, partly and wholly off-canvas, negative and far positions, reused quads, odd sizes, copy-after-`newImage`, a draw with the screen selected, a tinted draw), so that real LÖVE settles the copy, clip, alpha and copy-on-create rules on every CI push.
34. As a plan-1 developer, I want a capture program tinting a texel ramp with 120 colours on all three channels, including the 24 pairs where llvmpipe's 8-bit multiply and exact rounding disagree, so that real LÖVE settles the tint rule.
35. As a plan-1 developer, I want each new capture compared with the fake in `make test` as its own parallel check, so that the fast loop proves the conventions without getting slower.
36. As a plan-1 developer, I want the draw rule (clip, alpha, tint) and the quad and position rules tested directly in pure C modules, with values from the real-LÖVE measurements, so that a floor-instead-of-round tint or an off-by-one clip is caught with a precise message.
37. As a plan-1 developer, I want each misuse checked through the fake's command line with its message, so that the error contract is pinned, including the refusals real LÖVE does not make.
38. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
39. As the repository owner, I want the CI love-capture job green with the new programs, so that the PNGs are proven to be what real LÖVE draws.
40. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
41. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture LOVE_CAPTURE_DIR=<scratch>`, with a copy of the real
harness and probe programs that printed to standard output or wrote raw
canvas bytes to a scratch file. Run on LÖVE 11.5.0 with Mesa 25.2.8
llvmpipe (LLVM 20.1.2, 256 bits). These results are the expected answers
of this step's tests; the committed PNGs remain the proof.

- **Defaults.** `getDefaultFilter()` → `linear linear 1`. A new Image:
  `:type()` `Image`, filter `linear linear`, wrap `clamp clamp`, 1 mipmap,
  format `rgba8`. Blend mode `alpha alphamultiply`, colour `1 1 1 1`,
  `isGammaCorrect()` false.
- **Copy on create.** An ImageData changed by `setPixel` after `newImage`
  leaves the Image as it was.
- **Integer draws: an exact copy, under both filters.** 8 images of random
  bytes with 30 % alpha-0 texels (128×128, 48×30, 3×5, 7×13, 1×1, 160×120,
  255×3, 1024×4), each with 302 quads of random integer size and origin
  inside the image, plus the whole image and the last texel. They were
  drawn at random integer positions from −50 to 200 (x) and −50 to 160 (y)
  over a canvas cleared to `33669900`. Against a model that copies alpha-1
  texels and skips alpha-0 ones: **0 differing pixels** for every image,
  under `linear` and under `nearest` alike.
- **Fractional positions: where the filter matters.** A 7×13 image drawn
  at 20 or 101 plus an offset, on each axis:
  - offsets up to 1/1024 (and down to −1/1024) give the integer result
    under both filters;
  - under `linear`, from 1/512 on, 24 to 104 of the 91 pixels differ from
    both neighbouring integer positions (blended colours);
  - under `nearest`, offsets up to 0.49 give the integer result and 0.51
    the next one, but around 0.5 (0.5 − 10^-6, 0.5, 0.5 + 1/512, and −0.5
    at some positions) neither neighbour matches: texel columns are
    dropped or doubled.
- **Alpha.** For texel alpha a from 0 to 255 over 8 destinations
  (`00000000`, `000000FF`, `FFFFFFFF`, `33669900`, `336699FF`,
  `C80D8080`, `1D2B53FF`, `FF004DFF`), with white colour:
  - a = 0 leaves all four destination bytes, alpha-0 RGB included;
  - a = 255 writes the texel with alpha `FF`;
  - in between, every channel matches
    `min(255, mul(s, a) + mul(d, 255 − a))`, and alpha matches
    `min(255, a + mul(da, 255 − a))`, where
    `mul(p, q) = (p·q + (p·q >> 8) + 128) >> 8` is llvmpipe's 8-bit
    multiply. 0 mismatches over 8 × 65 536 texels. Exact rounding fails
    on 72 per destination. Recorded for a future need: this step refuses
    translucent texels.
- **Tint.** All 65 536 pairs of colour byte c (from `setColor(c/255)`)
  and opaque texel byte t, per channel: `round(c × t / 255)`, with no tie
  possible because 255 is odd. Measured mismatches: 0. Floor gives 95 310
  mismatches over the three channels, single-precision truncation the
  same, and llvmpipe's `mul` 24 pairs (the 8-bit multiply is only used
  by the blender). Alpha stays `FF`. A tint over alpha-0 texels leaves
  the destination as it was.
- **Forms (real LÖVE's own refusals and acceptances).**
  - `newQuad(0, 0, 0, 2, 4, 4)`, `newQuad(0, 0, −1, 2, 4, 4)` and
    `newQuad(0.5, 0, 2, 2, 4, 4)` are accepted;
  - `newQuad(0, 0, 2, 2)` fails with `bad argument #5 … (number
    expected, got no value)`;
  - `draw(image, quad)` (no position) and `draw(image, 0, 0)` (no quad)
    are accepted;
  - `draw(imagedata, quad, 0, 0)` fails with `Texture expected, got
    ImageData`;
  - `newImage()` fails with `ImageData expected, got no value`.
- **The two drafts below**, captured in real LÖVE: byte-identical PNGs
  under the default filter and under `setDefaultFilter("nearest",
  "nearest")` set before the program's top level. Compared with a LuaJIT
  model of this PRD's conventions (step 10's conversions, copy on create,
  integer draw, clip, alpha 0/255, `round(c × t / 255)`), **0 differing
  pixels** on both. With a floor tint, 304 pixels of `draw_quads` and
  14 070 of `draw_tint` differ. With llvmpipe's `mul` as the tint, 43
  pixels of `draw_tint` differ.

### Modules

- **Canvas store** (existing pure module) gains one operation and one
  conversion:
  - the tint conversion: a colour byte and a texel byte in, a byte out,
    `round(c × t / 255)` computed in integers (`(2·c·t + 255) / 510`). It is
    a fourth colour rule, after `clear` (float, ties to even), `setColor`
    (float, truncated) and `setPixel` (double, half up, H-10-1). Its
    one-line contract names the trap: not the floor of the product, and
    not llvmpipe's 8-bit multiply, which only the blender uses.
  - draw texels: given a row-major RGBA8 source with its width, a source
    rectangle (column, row, width, height) asserted inside it, and an
    integer destination (x, y), it walks the rectangle. A texel whose
    destination lies off the canvas is skipped. An alpha-`00` texel
    leaves the pixel. An alpha-`FF` texel writes its tinted RGB under the
    current colour, with alpha `FF`. With the screen as target, it does
    nothing. It asserts that every texel walked is `00` or `FF` in alpha
    (the binding refused the rest at `newImage`). It clips
    arithmetically, so a destination of ±(2^20 − 1) costs no loop over
    off-canvas texels. The current colour is the one `canvas_set_color`
    keeps, opaque by its own assertion.
- **Image and quad rules** (new pure C module, no Lua). It holds the
  checks that do not touch Lua:
  - whether a draw position is accepted: finite, an integer, magnitude
    below 2^20 (the bound of `points` and `rectangle`, reused, not
    copied);
  - whether quad values are accepted: six finite integers, reference
    sides 1 to 1024 (the image data store's maximum side, H-10-2), origin
    ≥ 0, size ≥ 1, inside the reference size;
  - the first pixel of an RGBA8 buffer whose alpha is neither `00` nor
    `FF`, or none.

  One header, plain functions, no state. The Image itself is the
  binding's userdata (size and copied bytes), so this module stores
  nothing.
- **Graphics binding** (existing) gains `newImage`, `newQuad` and `draw`,
  and two metatables, `Image` and `Quad`, whose `__index` is empty: no
  methods.
  - **Image userdata.** One full userdata holding width, height and the
    copied bytes, freed by the garbage collector, with no count limit.
  - **Quad userdata.** One full userdata holding the six integers.
  - `newImage`: exactly one argument. Step 10's accessor (H-10-2) gives
    the ImageData's size and bytes, or NULL when the value is not an
    ImageData; the refusal message names the type given. A translucent
    alpha is refused naming the pixel and its alpha byte. Otherwise the
    bytes are copied.
  - `newQuad`: exactly six Lua numbers, then the quad rules.
  - `draw`: exactly four arguments, checked in order: an Image (the
    message for an ImageData or a Canvas says it is not an Image), a
    Quad, two Lua numbers accepted as positions, and the quad's reference
    size equal to the image's. Then it calls the canvas store's draw
    texels.
  - Errors start `love.graphics.newImage: `, `love.graphics.newQuad: `,
    `love.graphics.draw: `. Each message names the argument index or the
    count and the problem: `is a <type>, not …`, `is not finite`, `is
    not an integer`, `is out of range`, `takes N arguments, not M`,
    `reference size … differs from the image's …`, `pixel (x, y) has
    alpha N`. The exact wording is the implementer's. The tests match the
    function name, the index or count, and the key word.
  - The header's contract lists the nine functions.
- **Host, harnesses, comparison command, image data store, image
  binding**: unchanged. The graphics binding is already installed after
  `conf.lua`.
- **Makefile**: the new pure module joins the fake's host sources and so
  both `tmp/fake_love` and `tmp/love_capture_check`. Its unit test joins
  the unit list and `FAKE_SIDE_BINARIES`. The CLI test joins
  `FAKE_LOVE_CHECKS` and `FAKE_SIDE_BINARIES`. A `.lua` and `.png` pair in
  `tools/love_capture/` becomes a check by itself (H-10-6).

### Capture programs of this step

Both use only listed functions from this and earlier steps
(`newImageData`, `setPixel`, `newImage`, `newQuad`, `draw`, `setColor`,
`rectangle`, `clear`, `setCanvas`). They build their images with
`setPixel(x, y, byte / 255, …, 0 or 1)`, which stores exactly the byte
(H-10-1). Both drafts were captured in real LÖVE and match the model
(above). The layout is the decision, and every case stays.

`draw_quads` (draft, captured):

```lua
local function new_data(width, height, texel)
    local data = love.image.newImageData(width, height)
    for y = 0, height - 1 do
        for x = 0, width - 1 do
            local r, g, b, a = texel(x, y)
            data:setPixel(x, y, r / 255, g / 255, b / 255, a)
        end
    end
    return data
end

local sheet_data = new_data(128, 128, function(x, y)
    return (x * 37 + y * 11) % 256, (x * 5 + y * 29 + 100) % 256, (x * x + y) % 256, (x * 7 + y * 3) % 5 == 0 and 0 or 1
end)
local sheet = love.graphics.newImage(sheet_data)
sheet_data:setPixel(0, 0, 1, 1, 1, 1)
sheet_data:setPixel(1, 0, 0, 0, 0, 1)
local other = love.graphics.newImage(new_data(128, 128, function(x, y)
    return (x * 3) % 256, (y * 2) % 256, 200, (x + y) % 4 == 0 and 0 or 1
end))
local cell = love.graphics.newImage(new_data(3, 5, function(x, y)
    return 255, 241, 232, (x + y) % 2
end))
local font = love.graphics.newImage(new_data(48, 30, function(x, y)
    return x * 5, y * 8, 77, (x * y) % 3 == 0 and 1 or 0
end))
local strip = love.graphics.newImage(new_data(1024, 4, function(x, y)
    return x % 256, math.floor(x / 4), y * 60, x % 7 == 0 and 0 or 1
end))

return function(canvas)
    local graphics = love.graphics
    graphics.clear(0.2, 0.4, 0.6, 0)
    graphics.setColor(1, 0.5, 0)
    graphics.rectangle("fill", 0, 60, 160, 60)
    graphics.setColor(1, 1, 1)

    local corner = graphics.newQuad(0, 0, 16, 16, 128, 128)
    graphics.draw(sheet, corner, 0, 0)
    graphics.draw(sheet, graphics.newQuad(112, 112, 16, 16, 128, 128), 20, 0)
    graphics.draw(sheet, graphics.newQuad(0, 0, 128, 128, 128, 128), -120, 40)
    local texel = graphics.newQuad(5, 7, 1, 1, 128, 128)
    for k = 0, 7 do
        graphics.draw(sheet, texel, 40 + k, 0)
        graphics.draw(sheet, graphics.newQuad(5 + k, 7, 1, 1, 128, 128), 40 + k, 1)
    end
    local block = graphics.newQuad(30, 40, 24, 9, 128, 128)
    graphics.draw(sheet, block, 150, 20)
    graphics.draw(sheet, block, 60, 115)
    graphics.draw(sheet, block, 70, -5)
    graphics.draw(sheet, block, -24, 30)
    graphics.draw(sheet, block, 160, 30)
    graphics.draw(sheet, block, 40, 120)
    graphics.draw(sheet, block, 2 ^ 20 - 1, 0)
    graphics.draw(sheet, block, -(2 ^ 20 - 1), 0)
    graphics.draw(sheet, block, -0, 100)
    graphics.draw(other, block, 80, 70)
    graphics.draw(other, corner, 136, 60)
    graphics.draw(sheet, corner, 136, 60)
    graphics.draw(sheet, corner, 136, 60)
    local whole_cell = graphics.newQuad(0, 0, 3, 5, 3, 5)
    for k = 0, 5 do
        graphics.draw(cell, whole_cell, 90 + k * 2, 80 + k % 2)
        graphics.draw(cell, whole_cell, 90 + k * 2, 20 + k % 2)
    end
    graphics.draw(font, graphics.newQuad(45, 25, 3, 5, 48, 30), 130, 90)
    graphics.draw(font, graphics.newQuad(0, 0, 48, 30, 48, 30), 100, 30)
    graphics.draw(strip, graphics.newQuad(0, 0, 1024, 4, 1024, 4), -1000, 50)
    graphics.draw(strip, graphics.newQuad(0, 0, 1024, 4, 1024, 4), -1000, 56)

    graphics.setCanvas()
    graphics.draw(sheet, corner, 100, 0)
    graphics.setCanvas(canvas)

    graphics.setColor(0.5, 1, 0.25)
    graphics.draw(sheet, corner, 120, 40)
    graphics.draw(sheet, corner, 120, 100)
    graphics.setColor(1, 1, 1)
end
```

What it pins, with values read from the real PNG:

- **Copy on create.** Texel (0, 0) of the sheet is alpha 0 when
  `newImage` runs and is set opaque white afterwards: canvas (0, 0)
  stays `33669900`. Texel (1, 0) is set black afterwards, yet canvas
  (1, 0) shows the original `256901FF`.
- **Transparent texels.** Alpha-0 texels over the transparent top half
  keep `33669900`, RGB included. Over the opaque bottom half they keep
  `FF7F00FF`.
- **Clipping.**
  - the whole sheet at (−120, 40) shows its last 8 columns;
  - the 1024-wide strip at (−1000, 50) shows texels 1000–1023 in
    columns 0–23 (canvas (23, 50) is `FFFF00FF`, (24, 50) the
    background);
  - quads partly off the right edge, the bottom edge and the top edge
    show their visible part;
  - (−24, 30), (160, 30), (40, 120) and ±(2^20 − 1) draw nothing;
  - `−0` draws at column 0.
- **Composition.** Repeated and overlapping draws show the last opaque
  texel. Two images share one quad.
- **Odd sizes.** The 3×5 cell and the 48×30 image, with the last cell
  `(45, 25, 3, 5)`.
- **Screen.** The draw with the screen selected leaves (100–115, 0–15)
  as background.
- **Tint.** The tinted corner (colour bytes `7F FF 3F`) differs from
  floor on 304 pixels.

The edge cases the plan names are all here: off-canvas, negative
coordinates and transparent pixels. Nested translate is step 13's.

`draw_tint` (draft, captured):

```lua
local data = love.image.newImageData(160, 1)
for x = 0, 159 do
    data:setPixel(x, 0, x / 255, (x + 96) / 255, (255 - x) / 255)
end
local ramp = love.graphics.newImage(data)

return function(canvas)
    local graphics = love.graphics
    local row = graphics.newQuad(0, 0, 160, 1, 160, 1)
    for y = 0, 119 do
        graphics.setColor(y / 255, (120 + y) / 255, (255 - y) / 255)
        graphics.draw(ramp, row, 0, y)
    end
end
```

Pixel (x, y) holds `round(y·x/255)`, `round((120+y)(x+96)/255)`,
`round((255−y)(255−x)/255)`, and alpha `FF`: 57 600 pairs, every one of
llvmpipe's 24 `mul` disagreements among them.

Refusals real LÖVE does not make cannot be captured: the CLI test pins
them. If a PNG disagrees with this section, the PNG wins and design.md
follows it.

### Conventions written into `docs/love-port/design.md`

An "Images" paragraph after step 10's "Image data" paragraph (H-10-4):

- the forms, every refusal and the error prefixes; a refused call changes
  nothing; Images and Quads have no methods and are garbage collected;
  `love.graphics` holds exactly the nine functions;
- `newImage` copies the ImageData at the call; a translucent alpha is
  refused;
- a draw copies texel (`qx + i`, `qy + j`) to pixel (`x + i`, `y + j`),
  clipped; alpha 0 leaves the pixel's four bytes; alpha 1 writes
  `round(c × t / 255)` per channel, alpha `FF`; with the screen selected
  it touches nothing modelled;
- **the filter**: LÖVE's default is `linear`. Integer draws of quads on
  the image's own size are byte-identical under `linear` and `nearest`
  (measured). Fractions from 1/512 of a pixel on blend under `linear` and
  drop or double columns near 0.5 under `nearest`. Hence the refusals;
  the fake does not model filtering;
- the measured blend formula for translucent texels, as the reason for
  the refusal, in one sentence;
- **the rule for the game**:
  - build images with alpha exactly 0 or 1;
  - call `setColor(1, 1, 1)` before drawing an image's own colours. A
    palette colour tinting an `FF` texel gives exactly that colour, while
    any other tint gives non-palette bytes;
  - pass integer positions (floor fixed-point coordinates first);
  - build each quad with integers, inside the image, on the image's own
    size;
- the captures: `draw_quads`, `draw_tint`.

The function list's "Drawing" line gains the exact forms:
`newImage(imagedata)`, `newQuad(x, y, w, h, sw, sh)`,
`draw(image, quad, x, y)`.

### Decisions taken without the plan

- **Do not model the filter; refuse where it would matter.** Measured:
  every accepted draw is identical under both filters. A model of
  llvmpipe's bilinear sampling would be large, unprovable over its whole
  input space, and useless to a game that must draw palette colours.
  - **Step 14 then**: `setDefaultFilter("nearest", "nearest")` becomes
    the one accepted form, recorded as state the fake does not use, with
    a capture showing integer draws unchanged. Whether `nearest` lets
    some fractional positions in is step 14's question.
  - **Recommendation recorded here**: keep the refusal. Under `nearest`,
    a fraction near 0.5 still drops texel columns, and an integer is what
    the game should pass anyway.
- **Positions: integers only, magnitude below 2^20.** The same bound as
  `points` and `rectangle`, for the same reason (llvmpipe's fixed point).
  Step 13's `translate` decides whether the integer check applies before
  or after the transform; until then the position is used as given.
- **Quads stricter than LÖVE.** LÖVE accepts empty, negative and
  fractional quads, and reference sizes other than the image's (which
  scale the texture). All are refused: a sprite is a whole number of
  texels of its own image. A wrong quad in the game is a bug, and the
  fake reports it where LÖVE would draw something plausible.
- **Translucent texels refused at `newImage`, not modelled.** The
  formula is measured and recorded, but the game draws palette colours
  only. A translucent texel always blends into a non-palette byte, which
  step 17 makes fatal anyway. The image is the place where the bad byte
  was made, so the refusal names it there. This matches `setColor`
  refusing alpha ≠ 1.
- **Model the tint, do not refuse it.** It is measured on all 65 536
  pairs and costs one integer expression. Refusing a non-white colour
  would forbid the white-mask use, which is exact. The game's rule says
  when a tint is safe.
- **One draw form.** `draw(image, quad, x, y)` with exactly four
  arguments: no quad-less form, no transform arguments, and no Canvas as
  a drawable. The game's list names this form only.
- **Copy at `newImage`**, as LÖVE does: it is measured, and it makes an
  Image independent of its ImageData's later life and of its garbage
  collection.
- **No methods on Image or Quad.** `getWidth`, `setFilter`, `setWrap`,
  `replacePixels`, `setViewport`, `getViewport`, `type` and `release`
  are not on the design's list.
- **Check everything, then draw.** A refused call changes nothing.
- **Misuse tested in one fixture game** logging each misuse through
  `pcall` and `debug_state()`, plus one uncaught misuse for the fatal
  path, as steps 7 to 10 did.

## Implementation Chunks

1. **Pure rules.**
   - The canvas store's tint conversion and draw texels.
   - The image and quad rules module (position, quad, first translucent
     pixel).
   - Their unit tests, with the values below.
   - Stories 5–14, 17, 36.
2. **`newImage`, `newQuad`, `draw` in the fake.**
   - The graphics binding's three functions and two metatables, through
     step 10's accessor.
   - The CLI test target with its fixtures (log, uncaught, capture
     fixtures drawn and compared).
   - The graphics names test updated to the nine functions.
   - Stories 1–4, 15, 16, 18–29, 37.
3. **Captures, docs, proof.**
   - The two programs and their PNGs from `make love-capture` (git diff
     shows only the new files).
   - The two comparisons passing in `make test`.
   - design.md, the timing, the CI run, the step note.
   - Stories 30–35, 38–41.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the store's bytes, the rule functions' answers, the binary's exit
  status, stderr and state file, the comparison's verdict. No test
  reaches into the Lua state or the userdata layout.
- **Canvas store, unit test** (existing test, extended):
  - the tint, values from the measurements:
    - `(255, t)` → t and `(c, 255)` → c for all bytes;
    - `(0, t)` → 0;
    - `(1, 128)` → 1, where floor gives 0 (a failing assert says the
      tint probably floors);
    - `(152, 229)` → 137 and `(253, 191)` → 190, where llvmpipe's
      8-bit multiply gives 136 and 189 (a failing assert says it
      probably uses the blender's multiply);
    - `(127, 37)` → 18; `(7, 233)` → 6;
    - an exhaustive loop against `floor(c·t/255 + 0.5)` in double.
  - draw texels, on a store with a selected canvas:
    - a 3×2 source with alpha-0 and alpha-255 texels over a canvas
      cleared to `33669900` writes exactly the opaque texels and leaves
      the rest `33669900`;
    - a tinted draw writes the tinted bytes with alpha `FF`;
    - a source rectangle inside a larger source reads the right texels;
    - destinations at (−2, −1), (158, 118), (−3, 0) with width 3,
      (160, 0), (0, 120) and ±(2^20 − 1) clip exactly;
    - with the screen as target, nothing changes.
- **Image and quad rules, unit test** (new, in the unit list):
  - positions: 0, −0, ±(2^20 − 1) accepted; 0.5, 1/1024, 2^20, NaN and
    ±infinity refused;
  - quads:
    - (0, 0, 1, 1, 1, 1) and (0, 0, 1024, 1024, 1024, 1024) accepted;
    - (45, 25, 3, 5, 48, 30) accepted, (46, 25, 3, 5, 48, 30) refused;
    - zero or negative size, negative origin, fractional values, and
      reference sides 0 or 1025 refused;
  - first translucent pixel: none for a buffer of `00` and `FF` alphas;
    the index of the first `80`, `01` or `FE`.
- **`newImage`, `newQuad`, `draw`, CLI test** (new, its own target in
  `FAKE_LOVE_CHECKS`, through `tmp/fake_love` and the shared CLI helper,
  as steps 7 to 10's CLI tests):
  - a `conf.lua` logging `tostring(love.graphics)` (`nil`);
  - a `main.lua` logging `type` of an Image and a Quad (`userdata`), and
    `getWidth` and `setViewport` on them as `nil`;
  - in `love.load`, each misuse in `pcall` with its message:
    - `newImage()`, `newImage("sprites.png")`, `newImage(data, {})`,
      `newImage(canvas)`, `newImage(image)`, `newImage({})`;
    - `newImage` of an ImageData holding alpha 0.5 at (2, 1), whose
      message names (2, 1) and 128;
    - `newQuad` with 4, 5 and 7 arguments, `"0"` as x, a NaN, an
      infinity, 0.5 as x, 0 and −1 as w, −1 as y, 0 and 1025 as sw,
      `x + w` above sw;
    - `draw` with 3 and 5 arguments, a trailing nil, an ImageData, a
      Canvas, a table and a string as the drawable, a table as the quad,
      the image as the quad, x 0.5, y 1/1024, x 2^20, a NaN, `"3"` as x,
      a 128-reference quad on a 48×30 image;
  - then an accepted call of each, logged `ok`;
  - the log is compared byte for byte;
  - a game calling `draw(image, quad, 0.5, 0)` uncaught in `love.draw`
    exits 1 naming `love.draw f0`, `love.graphics.draw` and `integer`;
  - two capture fixtures run through `tmp/love_capture_check` against
    PNGs the test writes, as `rectangle_draws`/`rectangle_refused` do:
    - "draws": a 4×3 image with two alpha-0 texels drawn at (−1, −1),
      (10, 10) and (158, 118); exact pixels expected;
    - "refused": refused calls in `pcall`, a draw with the screen
      selected, and an ImageData changed after `newImage`; the canvas is
      expected transparent, or holding the original texels.
- **The capture comparisons** are the tests of the conventions: both
  programs pass in `make test`, and every earlier capture still passes.
- **Prior art:**
  - step 6's canvas store test;
  - `test_fake_love_graphics` (names, nil in `conf.lua`);
  - `test_fake_love_rectangle` (log, uncaught, capture fixtures with
    written PNGs);
  - `test_fake_love_set_color`;
  - step 10's CLI test for userdata misuse (H-10-7).
- **Proofs recorded in the step note** (Step 3.11 in plan 1):
  - `make love-capture` writes the two new PNGs and leaves every other
    file of `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green (run
    id);
  - `make test` before and after, with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 10's last commit,
    H-10-5) and against `2e7215c`. Both medians of paired ratios must be
    ≤ 3 % (22 pooled rounds if either exceeds it). Record the plain and
    paired medians, the spreads, the rounds, the core count, and the
    slowest new check's own time. The budget is 100 ms. `draw_quads`
    makes about 37 000 `setPixel` calls at load and is the likely
    slowest. A check over budget is split before the step closes.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `newImageData(file)` from a PNG: step 12.
- `push`, `pop`, `translate`, and how a transform moves a draw: step 13.
- `setDefaultFilter`: step 14, which this PRD only advises.
- The combined program (colour before draw, translate, canvas switches):
  step 15.
- Translucent texels and translucent colours (refused; the blend formula
  is recorded only), drawing a Canvas, `draw` without a quad or with
  transforms, `newImage` from a file or with settings, every Image and
  Quad method, `SpriteBatch`, `replacePixels`, mipmaps, wrap modes.
- The fatal "unlisted function" and "foreign pixel": step 17.
- Audio, always.

## Hypotheses

Plan step 10 (`newImageData`/`setPixel`/`getPixel`) is being implemented
while this PRD is written. Its pure image data store is committed; its
binding, its captures, its design.md paragraph and its step note are not.
Before starting, the implementer checks each item below against the
committed step 10. When one differs, adapt the names, not the decisions.

- **H-10-1**: `ImageData:setPixel` stores each component as
  `floor(clamp(c) × 255 + 0.5)` in double and any alpha as given, and a
  new ImageData is `00000000`. So `setPixel(x, y, byte / 255, …, 0 or 1)`
  stores exactly those bytes with alpha `00` or `FF`, as both capture
  programs and every fixture assume. The real-LÖVE drafts confirm it
  independently.
- **H-10-2**: step 10 provides a C accessor that takes a Lua stack index
  and returns the ImageData's width, height and row-major RGBA8 bytes,
  or NULL when the value is not an ImageData. The userdata owns them, and
  they stay valid while it is reachable. Sides are integers from 1 to
  1024 (`IMAGE_DATA_MAX_SIDE`), and the quad rule reuses that maximum.
  `newImage` copies through the accessor and never reads the binding's
  layout.
- **H-10-3**: the ImageData metatable holds exactly `setPixel` and
  `getPixel`, and an ImageData is a full userdata that the accessor tells
  apart from an Image, a Quad and a Canvas. `newImage` and `draw` name it
  in their refusals.
- **H-10-4**: step 10 ends with an "Image data" paragraph in
  `docs/love-port/design.md` after "Colour", and with the "Images" line of
  the function list holding the exact forms. This step's "Images"
  paragraph follows "Image data" and adds the fourth colour rule.
- **H-10-5**: step 10's last commit is this step's "before" timing
  baseline, and its note (Step 3.10) records its timings.
- **H-10-6**: step 10 leaves the capture harnesses, `love_capture_check`
  and the Makefile's capture registration unchanged. A `.lua` and its
  `.png` in `tools/love_capture/` therefore become a `make test` check by
  themselves, and step 10's `imagedata_*` captures pass.
- **H-10-7**: step 10's CLI test is its own target in `FAKE_LOVE_CHECKS`
  and its binary is in `FAKE_SIDE_BINARIES`; this step adds siblings the
  same way. Its binding is installed after `conf.lua`, like
  `love.graphics`, so both are nil in `conf.lua`.
- **H-10-8**: step 10's binding style holds: errors are prefixed with the
  full function name, every argument is checked before acting,
  non-finite numbers and strings are refused, and a refused call changes
  nothing. This step follows it.
- **H-10-9**: step 10 changes neither the canvas store nor the graphics
  binding. The current colour stays opaque by `canvas_set_color`'s
  assertion, and `love.graphics` holds the six functions of step 9 before
  this step adds three.

## Further Notes

- The exploration programs lived in scratch directories passed to
  `make love-capture` with a copy of the harness; nothing of them is
  committed. Raw canvas bytes were fitted with Python and numpy. The
  drafts were compared with a LuaJIT model of this PRD's conventions,
  and with the floor and 8-bit-multiply tints to count what each program
  discriminates.
- Four colour rules now coexist, each measured:
  - `clear`: float, ties to even;
  - `setColor`: float, truncated;
  - `setPixel`: double, half up;
  - the draw tint: exact rounding of `c·t/255`.

  All of them keep palette bytes unchanged when used as the game's rules
  say.
- Translucent blending, if ever needed: `min(255, mul(s, a) + mul(d,
  255 − a))` per channel and `min(255, a + mul(da, 255 − a))` for alpha,
  with `mul(p, q) = (p·q + (p·q >> 8) + 128) >> 8`. It is measured with a
  white colour only; a tinted translucent texel is unmeasured.
- Two new capture checks and one CLI check each start one LuaJIT and run
  in parallel on the cores the unit tests leave idle. If the gate fails,
  design.md rule 5 applies.
