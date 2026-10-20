---
title: "fake_love stores pixels in love.image ImageData, written by setPixel and read back by getPixel exactly as real LÖVE 11.5 does"
description: "The Lua game builds its pattern images and recoloured sprite sheets pixel by pixel in ImageData, but the fake LÖVE has no love.image and nobody has measured how real LÖVE stores a setPixel component or what getPixel returns."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 9, `tmp/fake_love` can draw points and filled
rectangles in any opaque colour on its canvases. The Lua game of plans 2–3
also builds images of its own (design.md, "How the Lua game draws"): fill
pattern images, patterned disks built pixel by pixel, sprite sheets
recoloured per palette combination, per-colour letter images with a
transparent background. It builds all of them in `ImageData` through
`love.image.newImageData(w, h)` and `ImageData:setPixel`, and reads source
pixels back with `ImageData:getPixel`. The fake has no `love.image`: a game
that calls it fails.

Nobody has measured what real LÖVE does with these functions either, and
guessing from the two colour rules already found would be wrong three
ways. Measured below in real LÖVE 11.5: `setPixel` **rounds half up in
double precision** — `setPixel(…, 0.5)` stores 128 where `setColor(0.5)`
draws 127, and `setPixel(…, 2.5/255)` stores 3 where `clear(2.5/255)`
gives 2. `getPixel` returns `byte / 255` computed in double, the exact
value the Lua expression `byte / 255` gives, not the single-precision
value a C++ `Colorf` would hold. Coordinates are **floored**, not
truncated: `setPixel(-0.5, 0, …)` is out of range, `setPixel(0.99, 0, …)`
writes pixel 0. Without these measured, a recoloured sprite sheet could be
one byte off on some colours, or a game comparing `r == byte / 255` could
work in the fake and fail in LÖVE, and nobody would know which side was
wrong.

## Solution

The fake gains `love.image`, holding exactly `newImageData`, and the
`ImageData` object with exactly `setPixel` and `getPixel`:

- `love.image.newImageData(w, h)` with integer sizes from 1 to 1024
  returns a new ImageData, transparent black (`00000000`) everywhere,
  freed by Lua's garbage collector when unreachable.
- `data:setPixel(x, y, r, g, b)` and `data:setPixel(x, y, r, g, b, a)`
  (alpha 1 when omitted) store four bytes at pixel
  (`floor(x)`, `floor(y)`); each component becomes a byte as real LÖVE
  does: clamped to [0, 1], times 255 plus 0.5 in double precision,
  floored. Any alpha is stored as given (0 keeps the RGB bytes, no
  premultiplication); the ImageData is storage, nothing is blended.
- `data:getPixel(x, y)` returns the four bytes of pixel
  (`floor(x)`, `floor(y)`) as four numbers `byte / 255`, computed in
  double precision.
- A floored coordinate outside the ImageData (NaN and infinities
  included) is an error in both methods, as in LÖVE. The table form, a
  string, a non-finite component, a wrong count, a fractional or
  out-of-bounds size and the file form (step 12) are errors naming the
  function; a refused call changes nothing.

Nothing reaches the canvas in this step: an ImageData is only drawn once
step 11 gives the fake `newImage` and `draw`. The plan's verification is a
test game logging `getPixel`'s returned values against values measured in
real LÖVE. Two capture programs also pin the conventions in real LÖVE on
every CI push, through functions earlier steps already provide: they read
each pixel back with `getPixel` and paint it on the canvas with
`setColor` and a 1×1 `rectangle`, and paint the outcome of exact
comparisons and refused calls as coloured marks. The conventions go into
`docs/love-port/design.md` with the rule the game must follow.

## User Stories

1. As a plan-2 developer, I want `love.image.newImageData(w, h)` in the fake, so that the Lua game can build pattern images, disks and recoloured sprite sheets.
2. As a plan-2 developer, I want a new ImageData to be transparent black everywhere, as in LÖVE, so that pixels the game never sets are see-through.
3. As a plan-2 developer, I want `data:setPixel(x, y, r, g, b, a)` to store the four components of one pixel, so that I can build an image pixel by pixel.
4. As a plan-2 developer, I want `data:setPixel(x, y, r, g, b)` to store alpha 1, as LÖVE does, so that opaque pixels need no fourth component.
5. As a plan-2 developer, I want `setPixel(…, byte / 255, …)` to store exactly that byte on every channel, for all 256 bytes, so that the 16 palette colours reach the image unchanged.
6. As a plan-2 developer, I want each component converted as real LÖVE converts it — clamped to [0, 1], times 255 plus 0.5 in double precision, floored — so that a component the game computes another way lands on the same byte as in LÖVE.
7. As a plan-2 developer, I want `design.md` to warn that `setPixel(0.5)` stores 128, `setColor(0.5)` draws 127 and `clear(2.5/255)` gives 2 where `setPixel(2.5/255)` stores 3, so that I never assume the three colour entry points round alike.
8. As a plan-2 developer, I want components below 0 or above 1 clamped as LÖVE clamps them, so that `setPixel(2, -1, 1e30)` stores `FF 00 FF`.
9. As a plan-2 developer, I want `data:getPixel(x, y)` to return four numbers, each exactly `byte / 255` as the Lua expression computes it, so that `r == 7 / 255` holds in the fake exactly when it holds in LÖVE.
10. As a plan-2 developer, I want a pixel read with `getPixel` and written back with `setPixel` to keep its four bytes, so that copying and recolouring an image never drifts.
11. As a plan-2 developer, I want a value from `getPixel` passed straight to `setColor` to draw the stored byte, so that the game may colour a shape from an image pixel.
12. As a plan-2 developer, I want an alpha of 0 to keep the RGB bytes as given, as in LÖVE, so that a transparent pixel still reads back its colour.
13. As a plan-2 developer, I want translucent alphas (0.5) stored as bytes like the other components, as in LÖVE, so that the ImageData is plain storage and step 11 alone decides how a translucent texel draws.
14. As a plan-2 developer, I want fractional coordinates floored in both methods, as in LÖVE, so that a fixed-point coordinate addresses the same pixel in the fake and in LÖVE.
15. As a plan-2 developer, I want a coordinate whose floor is outside the ImageData (−1, −0.5, −1e-300, the width, the height, NaN, ±infinity, 2^31, 2^32 + 3) to raise an error naming the method, as LÖVE raises one, so that an off-image write is reported, never wrapped onto another pixel.
16. As a plan-2 developer, I want two ImageData objects to be independent, so that writing one never changes another.
17. As a plan-2 developer, I want overwriting a pixel to replace its four bytes, so that the last `setPixel` wins as in LÖVE.
18. As a plan-2 developer, I want the table form `setPixel(x, y, {r, g, b, a})` to raise an error, so that the fake implements exactly one way to pass a pixel.
19. As a plan-2 developer, I want fewer than 3 or more than 4 components to raise an error, so that a forgotten component (LÖVE refuses it) and an extra argument (LÖVE ignores it) are both reported.
20. As a plan-2 developer, I want a string, boolean, nil or table where a number belongs to raise an error naming the argument, so that the fake stays as strict as `setColor` and `rectangle`.
21. As a plan-2 developer, I want NaN and infinite components to raise an error, so that a division by zero in the game is reported instead of storing 0 (LÖVE's NaN) or a clamp.
22. As a plan-2 developer, I want `getPixel` to take exactly two numbers, so that a misplaced argument is reported.
23. As a plan-2 developer, I want calling a method with a dot instead of a colon to raise an error saying so, so that the mistake is named.
24. As a plan-2 developer, I want `newImageData` to refuse a size that is not an integer from 1 to 1024, a string, a pixel format or encoded data, so that a size computed by mistake is reported instead of floored or allocated huge.
25. As a plan-2 developer, I want `newImageData("file.png")` refused with an error naming the function until step 12 adds the file form, so that no half-implemented form exists.
26. As a plan-2 developer, I want a refused `setPixel` to leave the ImageData unchanged, so that an error caught by `pcall` does not corrupt the image.
27. As a plan-2 developer, I want `love.image` to be nil while `conf.lua` runs and to hold exactly `newImageData` afterwards, as the other modules do, so that no function exists without its capture.
28. As a plan-2 developer, I want an ImageData to hold exactly `setPixel` and `getPixel`, so that `getWidth`, `mapPixel`, `paste` or `encode` cannot slip into the game unmeasured.
29. As a plan-2 developer, I want unreachable ImageData objects freed by the garbage collector, as in LÖVE, so that a game building images every frame does not exhaust memory.
30. As a plan-2 developer, I want ImageData to exist from `love.load` on and to persist across frames like any Lua value, so that images built once are kept.
31. As a plan-1 developer, I want a test game logging `getPixel`'s returned values, with seventeen significant digits, for fresh, set, clamped, floored and refused pixels, compared byte for byte with values measured in real LÖVE, so that the step is verified as the plan says, before anything reaches the canvas.
32. As a plan-1 developer, I want a capture program storing every byte at offsets where half-up rounding, ties-to-even rounding, truncation and single precision disagree, read back with `getPixel` and painted on the canvas, so that real LÖVE settles the conversion on every CI push.
33. As a plan-1 developer, I want a capture program painting the fresh ImageData, floored coordinates, the default alpha, clamping, alpha 0, overwrites, independence, `getPixel`'s exactness and the out-of-range refusals LÖVE also makes, so that the forms are proven in one place.
34. As a plan-1 developer, I want each new capture compared with the fake in `make test` as its own parallel check, so that the fast loop proves the conventions without getting slower.
35. As a plan-1 developer, I want the conversion, the read-back value and the coordinate rule tested directly in a unit test of a pure C module, with values taken from the real-LÖVE measurements, so that a float conversion or a truncation is caught with a precise message.
36. As a plan-1 developer, I want each misuse checked through the fake's command line with its message, so that the error contract is pinned, including the refusals real LÖVE does not make.
37. As a plan-2 developer reading design.md, I want an "Image data" paragraph with the forms, the three-way colour warning and the rule for the game, so that I do not need plan 1, which will be deleted.
38. As a plan-1 developer of step 11, I want a C accessor giving an ImageData's size and RGBA8 bytes from its Lua value, so that `newImage` can copy them without reaching into the binding.
39. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
40. As the repository owner, I want the CI love-capture job green with the new programs, so that the PNGs are proven to be what real LÖVE draws.
41. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
42. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture LOVE_CAPTURE_DIR=<scratch>`, with a copy of the real
harness and probe programs printing to standard output or painting the
canvas, on LÖVE 11.5.0 with Mesa 25.2.8 llvmpipe (LLVM 20.1.2), gave these
results. They are the expected answers of this step's tests; the
committed PNGs remain the proof.

- **Objects.** `newImageData(4, 3)` is a `userdata`, `:type()`
  `ImageData`, format `rgba8`, every pixel `0 0 0 0`. `love.image` holds
  `isCompressed`, `newCompressedData`, `newCubeFaces`, `newImageData`.
- **Conversion: half up, in double.** 19 046 components stored and read
  back (every byte with 21 offsets from −0.5 to +0.999, including
  0.5 ± 10^-5 … 10^-15; ±8 double steps and ±8 float steps around every
  `(b + 0.5)/255`; 5 000 random values): every byte equals
  `floor(clamp01(c) × 255 + 0.5)` computed in double. Wrong models: the
  same in single precision on 2 905, ties to even (`clear`'s rule, in
  float) on 752, float truncation (`setColor`'s rule) on 9 983. A fused
  multiply-add cannot be told apart from the separate product and sum on
  any measured value. Examples: `0.5` → 128, `0.3` → 77, `2.5/255` → 3,
  `1.5/255` → 2, `0.5/255` → 1, `2.49/255` → 2, `2.51/255` → 3,
  `4.5/255` → 5, `(5 − 10^-6)/255` → 5, `(0.5 − 10^-9)/255` → 0 (single
  precision gives 1), `(1.5 − 10^-9)/255` → 1 (single gives 2); every
  `(b + 0.5)/255` rounds up to b + 1. Clamp: `(2, −1, 1e30, −1e30)` stores
  `FF 00 FF 00`; ±infinity clamp; NaN stores 0.
- **Read-back: `byte / 255` in double.** For all 256 bytes, each of the
  four values `getPixel` returns equals the Lua expression `byte / 255`
  (`1` gives `0.0039215686274509803`, not the float
  `0.0039215688593685627`). Hence `setPixel(getPixel(…))` keeps the
  bytes, and `setColor` of a `getPixel` value draws the stored byte
  (`setColor`'s float truncation of `byte / 255` gives the byte).
- **Forms.** `setPixel(x, y, r, g, b)` stores alpha `FF`. The table form
  `setPixel(x, y, {r, g, b[, a]})` works. A seventh argument is ignored.
  A string component fails with `bad red color component argument to
  ImageData:setPixel (expected number)`, a missing or boolean one with
  `bad blue color component …`, a string coordinate with `bad argument
  #1 to ImageData:setPixel (expected number)`. `getPixel()` and
  `getPixel(0)` fail (`bad argument #1/#2 … (expected number)`), as does
  the dot call `data.getPixel(0, 0)`.
- **Coordinates: floored.** `setPixel(0.99, 0, …)` writes pixel 0,
  `3.9` pixel 3, `4 − 10^-9` on a width of 4 pixel 3, `getPixel(2.9,
  1.99)` reads (2, 1), `getPixel(255.999, 0)` reads 255. Out of range in
  both methods (`Attempt to set out-of-range pixel!`, `… get …`): −1,
  −0.5, −1e-300, the width or height itself, NaN, ±infinity, 2^31,
  2^32 + 3, −2^32 + 3, 1e300. `−0` is pixel 0.
- **Sizes.** `newImageData(0, 3)` and `(−1, 3)` fail with `Invalid image
  size.`; `(4)` fails; `(1.5, 2.7)` gives 1×2; `("4", "3")` is coerced;
  `(100000, 100000)` succeeds (no limit); a format argument `"rgba8"` is
  accepted.
- These match LÖVE 11.5's LuaJIT FFI wrapper for ImageData (its error
  texts, double arithmetic and `floor` of coordinates), which is a
  consistent explanation, not a fact the fake relies on.

### Modules

- **Image data store** (new pure C module, no Lua, beside the canvas
  store): the pixel buffer of one ImageData and its rules.
  - the size limits: integer sides from 1 to 1024 (`IMAGE_DATA_MAX_SIDE`
    or similar);
  - an initialiser over caller-owned memory (the binding provides it
    inside a Lua userdata) setting the size and all bytes to 0, and the
    byte count a size needs;
  - the component conversion of `setPixel`: a double in, a byte out,
    `floor(clamp(c) × 255 + 0.5)` in double; asserts the component is
    not NaN. The header states the rule in one line and contrasts it with
    the canvas store's two conversions (`clear` rounds ties to even in
    float, `setColor` truncates in float, H-9-5): mixing the three up is
    the trap this step found. No helper is shared with either.
  - the read-back value of a byte: `byte / 255.0` in double;
  - pixel lookup from two doubles: floor each, and report whether the
    floored pixel is inside (NaN and infinities are outside; the
    comparison is done in double before any integer conversion, so 2^32 +
    3 never wraps);
  - write four bytes / read four bytes at an inside pixel (asserted).
- **Image binding** (new, Lua side, installed by the host after
  `conf.lua` like the other modules): sets `love.image` holding exactly
  `newImageData`, and the `ImageData` metatable whose `__index` holds
  exactly `setPixel` and `getPixel`. An ImageData is one full userdata
  holding the store's header and bytes, so Lua's garbage collector frees
  it; there is no count limit.
  - `newImageData`: exactly two Lua numbers, each an integer from 1 to
    1024; anything else (another count, a string — the file form of step
    12 included —, a fractional, zero, negative or too large size) is an
    error starting `love.image.newImageData: `.
  - `setPixel`: self an ImageData (else an error saying to call it with a
    colon), then 5 or 6 more arguments, all Lua numbers (no string
    coercion; a table third argument is reported as such: the table form
    is not supported), the pixel inside, the components finite. Checks
    everything first, then converts and stores; alpha 1 when omitted.
    Errors start `ImageData:setPixel: `.
  - `getPixel`: self an ImageData, exactly 2 more Lua numbers, the pixel
    inside; returns four numbers. Errors start `ImageData:getPixel: `.
  - an out-of-range pixel's message says `out-of-range pixel` and gives
    the coordinates as passed and the ImageData's size; other messages
    name the argument index or the count and the problem (`is a <type>,
    not a number`, `is not finite`, `takes …, not N arguments`). The exact
    wording is the implementer's; the tests match the function name, the
    index or count, and the key word.
  - a C accessor for step 11: given a Lua stack index, the ImageData's
    width, height and RGBA8 bytes, or NULL when the value is not an
    ImageData. Owned by the userdata, valid while it is reachable.
- **Host**: installs the image binding after `conf.lua`, with the other
  modules, so `love.image` is nil in `conf.lua`. Nothing else changes in
  the host, the canvas store, the graphics binding, the point rule, the
  comparison command or the harnesses.
- **Makefile**: the pure module joins the fake's host sources (so
  `tmp/fake_love` and `tmp/love_capture_check` both get it), the binding
  the LuaJIT-flagged sources; the new unit test and CLI test binaries
  join `FAKE_SIDE_BINARIES` for the `src/` guard; the CLI test joins
  `FAKE_LOVE_CHECKS` as its own target; the unit test joins the unit
  list. A `.lua` and `.png` pair in `tools/love_capture/` becomes a check
  by itself (H-9-7).

### Capture programs of this step

The plan says this step is verified by `getPixel`'s returned values in a
test game, since no ImageData reaches the canvas before step 11. The
phase's rule still asks for a capture program per step, and design.md for
real-LÖVE proof of every convention, so this step does both. The capture
programs use only listed functions from this and earlier steps
(`newImageData`, `setPixel`, `getPixel`, `setColor`, `rectangle`,
`clear`) and `pcall`/`select`: values become pixels through
`setColor(v)` and `rectangle("fill", x, y, 1, 1)` (H-9-1). Both drafts
below were captured in real LÖVE and compared with a model of this PRD's
conventions run in LuaJIT: zero differing pixels.

`imagedata_rounding` (draft, captured; offsets are the decision):

```lua
return function(canvas)
    local graphics = love.graphics
    local data = love.image.newImageData(160, 60)
    local offsets = {-0.5, -1e-6, 0, 1e-6, 0.25, 0.49, 0.5 - 1e-9, 0.5, 0.5 + 1e-9, 0.51, 0.75, 0.999}
    for k = 0, 9599 do
        local byte, family = k % 256, math.floor(k / 256)
        local function component(channel)
            return (byte + offsets[(family + 3 * channel) % #offsets + 1]) / 255
        end
        data:setPixel(k % 160, math.floor(k / 160), component(0), component(1), component(2), component(3))
    end
    for y = 0, 59 do
        for x = 0, 159 do
            local r, g, b, a = data:getPixel(x, y)
            graphics.setColor(r, g, b)
            graphics.rectangle("fill", x, y, 1, 1)
            graphics.setColor(a, a, a)
            graphics.rectangle("fill", x, y + 60, 1, 1)
        end
    end
end
```

Expected: all 19 200 pixels opaque; rows 0–59 the stored RGB, rows 60–119
the stored alpha as grey. Against the real PNG, single-precision half-up
is wrong on 3 315 pixels, `clear`'s ties-to-even rule on 5 562,
`setColor`'s truncation on 14 788; double half-up on none. It cannot see
whether `getPixel` returns `byte / 255` or its float (both draw the same
byte): `imagedata_forms` covers that.

`imagedata_forms` (draft captured; layout may move, every case stays):

| region | draws | real LÖVE |
|---|---|---|
| background | `clear(0.2, 0.4, 0.6, 0)` | `33669900` wherever nothing is drawn |
| (0, 0) 8×8 RGB, (8, 0) 8×8 alpha | a fresh 8×8 ImageData shown pixel by pixel: `setColor(r, g, b)` at the RGB block, `setColor(a, a, a)` at the alpha block | `000000FF` in both blocks |
| (16, 0) | `select("#", fresh:getPixel(7, 7))` as grey `n/255` | `040404FF` |
| (0, 10) 16×16 RGB, (16, 10) alpha | a 16×16 ImageData after `setPixel(2.7, 1.2, 1, 0, 0)`, `(0.999, 15.999, 0, 1, 0, 1)`, `(−0, 3, 0, 0, 1, 1)`, `(15.5, 0, 2, −1, 1e30, −1e30)`, `(4, 4, 0.2, 0.4, 0.6, 0)`, `(5, 5, 1, 1, 1, 1)` then `(5, 5, 0.1, 0.2, 0.3, 0.5)`, `(6, 6, 0.5, 0.5, 0.5, 0.5)`, and pixel (7, 7) set from `getPixel(2.9, 1.99)` | (2, 1) `FF0000`/alpha `FF`; (0, 15) `00FF00`/`FF`; (0, 3) `0000FF`/`FF`; (15, 0) `FF00FF`/alpha `00`; (4, 4) `336699`/alpha `00`; (5, 5) `1A334D`/alpha `80`; (6, 6) `808080`/alpha `80`; (7, 7) `FF0000`/`FF`; every other pixel `000000`/`00` |
| (32, 10) and (64, 10) | a second 16×16 ImageData with only `setPixel(2, 1, 0, 1, 1, 1)`, then the first again | (34, 11) `00FFFFFF`, the first unchanged at (66, 11) `FF0000FF` |
| (100, 10) 16×16 marks, (100, 30) 16×16 RGB | 256 bytes stored as `byte / 255` on all channels; a mark per byte: `setColor(1, 0, 0.5)` when all four `getPixel` values equal `byte / 255`, else `setColor(0, 1, 0.5)` | 256 marks `FF007FFF` (a float read-back gives 254 `00FF7FFF`); the grey ramp `000000FF` … `FFFFFFFF` |
| rows 50–53, columns 0–11 | for each of `−1, −0.5, −1e-300, 16, 16.5, 1/0, −1/0, 0/0, 2^31, 2^32 + 3, −2^32 + 3, 1e300`, the mark of `pcall` of `setPixel` with it as x (row 50) and as y (row 51), `getPixel` likewise (rows 52, 53) | 48 marks `00FF7FFF` (refused) |
| row 52, columns 20–24 | `getPixel(v, v)` for `0, −0, 0.5, 15, 15.999` | 5 marks `FF007FFF` (accepted) |
| row 55, columns 0–4 | `pcall` of `newImageData(0, 4)`, `newImageData(4, −1)`, `setPixel(0, 0, 1, 0)`, `getPixel(0)`, `setPixel(0, 0, 1, 0, true)` | 5 marks `00FF7FFF` |
| (0, 60) | the first ImageData again, after all the refused calls | identical to (0, 10): refused calls changed nothing |

The plan's edge cases: off-image and negative coordinates are the
out-of-range rows; transparent pixels are the fresh ImageData, alpha 0
keeping its RGB, and the harness background kept where nothing is drawn;
nested translate does not apply (an ImageData has no transform; step 13
captures `translate`). Refusals real LÖVE does not make (table form,
extra argument, string sizes, fractional sizes, sizes over 1024, NaN and
infinite components, format argument) cannot be captured: the CLI test
pins them. If a PNG disagrees with this table, the PNG wins and design.md
follows it.

### Conventions written into `docs/love-port/design.md`

An "Image data" paragraph after step 9's "Colour" paragraph (H-9-4):

- the forms accepted, every refusal, the error prefixes, a refused call
  changing nothing; `love.image` nil in `conf.lua`, then exactly
  `newImageData`; an ImageData holds exactly `setPixel` and `getPixel`;
  sizes 1 to 1024; garbage collected; no file form until step 12;
- a new ImageData is `00000000` everywhere; coordinates are floored, a
  floored pixel outside is an error in both methods;
- the conversion: clamped to [0, 1], times 255 plus 0.5 in **double**,
  floored — half up: `0.5` stores 128 (`setColor` draws 127),
  `2.5/255` stores 3 (`clear` gives 2); any alpha is stored, alpha 0
  keeps the RGB; nothing is blended in an ImageData;
- `getPixel` returns `byte / 255` in double, equal to the Lua expression;
- **the rule for the game**: write palette bytes as `byte / 255`; read a
  byte back as `math.floor(v * 255 + 0.5)` or compare with `byte / 255`;
  a `getPixel` value may go straight to `setPixel` or `setColor`;
  use integer coordinates (fractions are floored, as in LÖVE);
- the captures: `imagedata_rounding`, `imagedata_forms`.

The function list's "Images" line gains the exact forms:
`love.image.newImageData(w, h)`, `ImageData:setPixel(x, y, r, g, b[, a])`,
`ImageData:getPixel(x, y)`.

### Decisions taken without the plan

- **A capture as well as the test game.** The plan names the test game;
  the phase rule and design.md ask for real-LÖVE proof. Encoding values
  through `setColor` and 1×1 rectangles is enough to pin the stored
  bytes, and exactness marks pin the read-back value; both captures rerun
  in real LÖVE on every CI push. The test game stays the plan's
  verification and covers what captures cannot: the fake-only refusals
  and seventeen-digit values.
- **Floor coordinates like LÖVE, do not refuse fractions.** The Lua game
  computes with 16.16 fixed-point numbers; a fractional coordinate means
  the same pixel in the fake and in LÖVE, and the capture proves it.
  Refusing would make the fake disagree with LÖVE on code that works.
- **Sizes stricter than LÖVE.** Integers from 1 to 1024 only: a size is
  an allocation, a fraction or a string there is a bug, and LÖVE's
  unbounded sizes would let a mistake allocate gigabytes. The game's
  largest image is the 128×128 sprite sheet. A larger need is a design.md
  decision.
- **Components: clamp like LÖVE, refuse non-finite**, as `clear` and
  `setColor` do (H-9-3); LÖVE's NaN-stores-0 is not reproduced.
- **Any finite alpha is stored**, unlike `setColor`, which refuses
  translucency: an ImageData blends nothing, so storing 0.5 is exact.
  Drawing a translucent texel is step 11's decision.
- **Two forms of `setPixel` only, one of `getPixel`.** No table form, no
  extra argument, no string coercion: one way to pass a pixel, as for
  `setColor`.
- **Own conversion, own module.** The setPixel rule lives in the image
  data store, not beside the canvas store's two rules, with its own
  one-line contract; no shared helper.
- **Userdata owns the bytes.** Garbage collection, no count limit, no
  global registry; step 11 reads them through the accessor. The canvas
  store's 16-canvas limit does not apply.
- **No `getWidth`, `getHeight`, `getDimensions`, `mapPixel`, `paste`,
  `encode`, `getFormat`, `type`.** Not on the design's list; the game
  keeps sizes itself.
- **Check everything, then write.** A refused call changes nothing.
- **Misuse tested in one fixture game** logging each misuse through
  `pcall` and `debug_state()`, plus one uncaught misuse for the fatal
  path, as steps 7 to 9 did.

## Implementation Chunks

1. **Image data store.** The pure module (sizes, initialiser, setPixel
   conversion, read-back value, floored lookup, byte access) and its unit
   test with the values below. Stories 2, 5–10, 12–17, 35.
2. **`love.image` in the fake.** The binding with its refusals and the
   step-11 accessor, the host installing it, the CLI test target with its
   fixtures (log, uncaught). Stories 1, 3, 4, 11 (read-back side),
   18–31, 36, 38.
3. **Captures, docs, proof.** The two programs and their PNGs from
   `make love-capture` (git diff shows only the new files), the two
   comparisons passing in `make test`, design.md, the timing, the CI run,
   the step note. Stories 11, 32–34, 37, 39–42.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the store's results and bytes, the binary's exit status, stderr and
  state file, the comparison's verdict. No test reaches into the Lua
  state or the userdata layout.
- **Image data store, unit test** (new, in the unit list; it does not
  run the fake):
  - conversion, values from the measurements: `b/255` → b for all 256
    bytes; `0.5` → 128; `0.3` → 77; `2.5/255` → 3 (a failing assert says
    the conversion probably rounds ties to even, as `clear`); `1.5/255` →
    2; `0.5/255` → 1; `2.49/255` → 2; `2.51/255` → 3; `4.5/255` → 5;
    `(5 − 1e-6)/255` → 5; `(0.5 − 1e-9)/255` → 0 and `(1.5 − 1e-9)/255`
    → 1 (a failing assert says the conversion probably runs in single
    precision); `0.5` → 128 against `setColor`'s 127 (a failing assert
    says it probably truncates); `2` → 255, `−1` → 0, `1e30` → 255,
    `−1e30` → 0;
  - read-back: byte b gives exactly `b / 255.0` for all 256 bytes, and
    the conversion of that value gives b back;
  - lookup: on a 4×3 buffer, (0.99, 0) → (0, 0), (3.9, 2.999) → (3, 2),
    (−0.0, 0) inside; (−0.5, 0), (−1e-300, 0), (4, 0), (0, 3), NaN,
    ±infinity, 2^31, 2^32 + 3 outside;
  - a new buffer is all 0; a write then a read gives the four bytes; two
    buffers are independent; the size limits accept 1 and 1024 and refuse
    0 and 1025.
- **`love.image`, CLI test** (new, its own target in `FAKE_LOVE_CHECKS`,
  through `tmp/fake_love` and the shared CLI helper, a fixture game and a
  one-frame or three-frame replay, as steps 7 to 9's CLI tests). This is
  the plan's "verified by `getPixel`'s returned values in a test game":
  - a `conf.lua` logging `tostring(love.image)` (`nil`); a `main.lua`
    logging the sorted names of `love.image` (`newImageData`), of an
    ImageData's `__index` (`getPixel setPixel`) and `type(data)`
    (`userdata`);
  - in `love.load`, each line `string.format("%.17g")` of the four
    values, as measured: a fresh pixel `0 0 0 0` and the count 4;
    `setPixel(0, 0, 1/255, 0.5, 2.5/255, 0.3)` →
    `0.0039215686274509803 0.50196078431372548 0.011764705882352941
    0.30196078431372547`, with `r == 1/255` logged `true`;
    `setPixel(1, 0, 2, −1, 1e30)` → `1 0 1 1`; `setPixel(2.9, 1.5, 0.2,
    0.4, 0.6, 0)` read at `(2, 1)` → `0.20000000000000001
    0.40000000000000002 0.59999999999999998 0`; `(0.5 − 1e-9)/255` → `0`;
    `getPixel(0.999, 0.5)` equal to `getPixel(0, 0)`;
    `newImageData(1024, 1024)` ok;
  - then, in `pcall`, each misuse with its message: `newImageData()`,
    `(4)`, `(4, 3, "rgba8")`, `("4", 3)`, `("sprites.png")`, `(0, 3)`,
    `(3, −1)`, `(1.5, 2)`, `(1025, 1)`, `(1, 1025)`, `(0/0, 1)`;
    `data.setPixel(0, 0, 1, 1, 1)` (dot call), `setPixel(0, 0, 1, 0)`,
    `setPixel(0, 0, 1, 0, 0, 1, 0)`, `setPixel(0, 0, {1, 0, 0})`,
    `setPixel(0, 0, "1", 0, 0)`, `setPixel("0", 0, 1, 0, 0)`,
    `setPixel(0, 0, 1, nil, 0)`, `setPixel(0, 0, 1, 0, true)`,
    `setPixel(0, 0, 0/0, 0, 0)`, `setPixel(0, 0, 1, 1/0, 0)`,
    `setPixel(0, 0, 1, 0, 0, 0/0)`, out of range at `(3, 0)`, `(0, 2)`,
    `(−1, 0)`, `(−0.5, 0)`, `(0/0, 0)`, `(0, 1/0)`; `data.getPixel(0, 0)`,
    `getPixel(0)`, `getPixel(0, 0, 0)`, `getPixel("0", 0)`, `getPixel(3,
    0)`, `getPixel(0, −0.5)`, `getPixel(0/0, 0)`; then pixel (0, 0) read
    again, unchanged by the refusals; the log compared byte for byte;
  - a game calling `data:setPixel(160, 0, 1, 1, 1)` on a 160×120
    ImageData uncaught in `love.draw` exits 1 naming `love.draw f0`,
    `ImageData:setPixel` and `out-of-range`.
- **The capture comparisons** are the tests of the conventions: both
  programs pass in `make test`, and every earlier capture still passes.
- **Prior art:** step 6's canvas store test (conversion values) and
  `test_fake_love_graphics` (names, nil in `conf.lua`); steps 7 to 9's CLI
  tests with their log and uncaught fixtures (`set_color_log`,
  `rectangle_log`); `test_fake_love_joystick` for a userdata with a
  metatable, the colon-call error and misuse logged through `pcall` and
  `debug_state()`.
- **Proofs recorded in the step note** (Step 3.10 in plan 1):
  - `make love-capture` writes the two new PNGs and leaves every other
    file of `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green
    (run id);
  - `make test` before and after with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 9's last commit,
    H-9-6) and against `2e7215c`, both medians of paired ratios ≤ 3 %
    (22 pooled rounds if either exceeds it); plain and paired medians,
    spreads, rounds, core count, and the slowest new check's own time
    (budget 100 ms; `imagedata_rounding` makes 9 600 `setPixel` and
    `getPixel` and 19 200 `setColor` and `rectangle` calls and is the
    likely slowest); a check over budget is split before the step closes.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `newImage`, `newQuad`, `draw`, and how an ImageData's pixels (translucent
  texels included) and the current colour reach the canvas: step 11.
- `love.image.newImageData(file)` from a PNG: step 12.
- Every other `ImageData` method and `love.image` function (`getWidth`,
  `mapPixel`, `paste`, `encode`, `newCompressedData`, …), pixel formats
  other than `rgba8`, the table form of `setPixel`: not on the design's
  list.
- `translate` and the transform stack: step 13 (an ImageData has none).
- The fatal "unlisted function": step 17.
- Audio, always.

## Hypotheses

Plan step 9 (`setColor`) is being implemented while this PRD is written.
Its store colour and conversion (224e988) and its `setColor` binding with
its CLI test (4d62537) are committed; its captures, its design.md
paragraph and its step note are not. Each item below is checked by the
implementer against the committed step 9 before starting; when one
differs, adapt the names, not the decisions.

- **H-9-1**: `setColor(r, g, b)` converts each component as real LÖVE
  does (float, clamp, × 255 in float, truncate), so `setColor(v, v, v)`
  with `v = byte / 255` in double draws exactly the byte, `setColor(1, 0,
  0.5)` draws `FF007FFF`, and a 1×1 integer `rectangle("fill", …)` paints
  exactly one pixel in the current colour, opaque. Both capture programs
  rely on it to show values; the real-LÖVE drafts above confirm it
  independently.
- **H-9-2**: `love.graphics` holds exactly `clear`, `newCanvas`,
  `points`, `rectangle`, `setCanvas`, `setColor`; this step does not
  change it, and no test lists the `love` table's own keys, so no name
  list changes.
- **H-9-3**: step 9's binding style holds: errors prefixed with the full
  function name, every argument checked before acting, non-finite numbers
  and strings refused, a refused call changing nothing. This step follows
  it.
- **H-9-4**: step 9 ends with a "Colour" paragraph in
  `docs/love-port/design.md` after "Rectangles", stating `setColor`'s
  truncation against `clear`'s rounding; this step's "Image data"
  paragraph follows it and adds the third rule.
- **H-9-5**: the canvas store names its two conversions apart (`clear`'s
  and the drawing colour's, as committed in 224e988); the image data
  store's conversion is a third, separate function, and its header
  contrasts it with both.
- **H-9-6**: step 9's last commit is this step's "before" timing
  baseline, and its note (Step 3.9) records its timings.
- **H-9-7**: step 9 leaves the capture harnesses, `love_capture_check`
  and the Makefile's capture registration unchanged, so a `.lua` and its
  `.png` in `tools/love_capture/` become a `make test` check by
  themselves, and step 9's `color_*` captures pass.
- **H-9-8**: step 9's CLI test is its own target in `FAKE_LOVE_CHECKS`
  and binary in `FAKE_SIDE_BINARIES` (as committed in 4d62537); this
  step adds siblings the same way.

## Further Notes

- The exploration programs lived in scratch directories passed to
  `make love-capture` with a copy of the harness; nothing of them is
  committed. The two drafts were compared with a LuaJIT model of this
  PRD's conventions (and of the wrong models, to count what each program
  discriminates) with Python and numpy: zero differing pixels for the
  chosen model.
- Three colour rules now coexist, each measured: `clear` (float, ties to
  even), `setColor` (float, truncation), `setPixel` (double, half up).
  All three map `byte / 255` to the byte, which is why the game's rule
  "pass `byte / 255`" holds everywhere; they differ only on values the
  game must not produce.
- Two new checks of one LuaJIT start each run in parallel on the cores
  the unit tests leave idle; if the gate fails, design.md rule 5 applies.
