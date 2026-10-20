---
title: "fake_love colours points and rectangles with love.graphics.setColor exactly as real LÖVE 11.5 on Mesa does"
description: "The Lua game picks each shape's colour with setColor, but the fake LÖVE draws everything opaque white and nobody has measured how real LÖVE turns a setColor component into a pixel byte."
status: needs-triage
---

## Problem Statement

After plan steps 7 and 8, `tmp/fake_love` lights pixels with
`love.graphics.points` and fills rectangles with
`love.graphics.rectangle("fill", …)`, always in opaque white, LÖVE's
default colour. The Lua game of plans 2–3 draws every flat shape in one of
the 16 PICO-8 colours, which it passes to `love.graphics.setColor` as
`byte / 255` (design.md, "How the Lua game draws"). The fake has no
`setColor`: a game that calls it fails, and a game that does not draws
everything white.

Nothing says how real LÖVE turns a colour component into a pixel byte
either. Step 6 measured `clear`: Mesa rounds `component × 255` to the
nearest byte, ties to even. Guessing that `setColor` does the same would be
wrong: measured below, real LÖVE **truncates** a `setColor` component, so
`setColor(0.5, 0.5, 0.5)` draws `7F7F7F` where `clear(0.5, 0.5, 0.5)` gives
`808080`, and a component one float step below `128/255` draws `7F`, not
`80`. Translucent colours blend with the pixels under them, which the game
never needs and the fake cannot reproduce byte for byte without modelling
llvmpipe's blender. Without a measured convention, every coloured span of
the e2e frames could be one byte off, and nobody would know whether the
game or the fake was wrong.

## Solution

The fake gains `love.graphics.setColor` in the forms the game needs,
`setColor(r, g, b)` and `setColor(r, g, b, 1)`. The colour it sets is the
current colour of every later `points` and `rectangle`, for the rest of the
run: across frames, canvas switches, `setCanvas()` and `clear`, from
`love.load` on. Each component becomes a byte as real LÖVE 11.5 does: to
single precision, clamped to [0, 1], times 255 in single precision,
**truncated**. A pixel drawn in the current colour takes its four bytes,
alpha `FF`, also over a transparent pixel. A translucent colour (an alpha
other than 1), the table form, a string, a non-finite number or a wrong
count is an error naming `love.graphics.setColor`, and a refused call
leaves the current colour as it was.

Three capture programs pin the convention against real LÖVE: every byte
of every channel at the offsets where truncation and rounding disagree,
drawn with both `rectangle` and `points`; components one or a few float
steps below and above `byte/255`; and the colour's state (default, clamp,
persistence across canvases and `clear`, refused calls real LÖVE also
refuses, opaque colours over a transparent background). Their PNGs come
from `make love-capture`; `make test` compares each with the fake. The
convention goes into `docs/love-port/design.md` with the rule the game must
follow: pass each palette byte as the Lua expression `byte / 255`, never a
value computed another way, and never an alpha.

## User Stories

1. As a plan-2 developer, I want `love.graphics.setColor(r, g, b)` in the fake, so that the Lua game can draw each shape in its PICO-8 colour.
2. As a plan-2 developer, I want `setColor(r, g, b, 1)` accepted too, so that code that spells the alpha out still runs.
3. As a plan-2 developer, I want `setColor(byte / 255, …)` to draw exactly that byte on each channel, for all 256 bytes, so that the 16 palette entries reach the canvas unchanged.
4. As a plan-2 developer, I want each component converted as real LÖVE converts it — to float, clamped to [0, 1], times 255 in float, truncated — so that a component the game computes differently (0.5, a product, a value one float step below `byte/255`) lands on the same byte as in LÖVE.
5. As a plan-2 developer, I want `design.md` to warn that `setColor(0.5)` gives 127 while `clear(0.5)` gives 128, so that I never assume both functions round alike.
6. As a plan-2 developer, I want components below 0 or above 1 clamped as LÖVE clamps them, as `clear` already does, so that the two colour entry points accept the same range.
7. As a plan-2 developer, I want the current colour to be opaque white (`FFFFFFFF`) until the first `setColor`, as in LÖVE, so that steps 7 and 8's captures and fixtures stay right.
8. As a plan-2 developer, I want `points` to light each pixel in the current colour, replacing its four bytes, so that a pixel-by-pixel circle has the colour the game chose.
9. As a plan-2 developer, I want `rectangle("fill", …)` to fill in the current colour, replacing the four bytes of each covered pixel, so that spans, bars and panels have the colour the game chose.
10. As a plan-2 developer, I want a coloured point or rectangle over a transparent pixel to make it opaque in the current colour, with no trace of the old RGB, so that transparency matches LÖVE byte for byte.
11. As a plan-2 developer, I want a coloured shape over another to replace it, so that drawing order decides the colour as in LÖVE.
12. As a plan-2 developer, I want the colour set in `love.load`, before any canvas exists, to apply to the first `love.draw`, so that setup code can choose a colour.
13. As a plan-2 developer, I want the colour to persist across frames, as in LÖVE whose loop never resets it, so that a colour set in one frame is still current in the next.
14. As a plan-2 developer, I want the colour to survive `setCanvas(other)`, `setCanvas()` and `setCanvas(canvas)`, so that the colour is graphics state, not canvas state, as in LÖVE.
15. As a plan-2 developer, I want `setColor` called while no canvas is selected to set the colour for later draws, so that the order of `setColor` and `setCanvas` does not matter.
16. As a plan-2 developer, I want `clear` to ignore the current colour and leave it unchanged, as in LÖVE, so that clearing a canvas does not recolour later shapes.
17. As a plan-2 developer, I want an alpha other than 1 (0, 0.5, 2) to raise an error naming `love.graphics.setColor`, so that no translucent colour, which real LÖVE blends with the pixels below, reaches a fake that does not blend.
18. As a plan-2 developer, I want the table form `setColor({r, g, b})` to raise an error, so that the fake implements exactly one way to pass a colour.
19. As a plan-2 developer, I want fewer than 3 or more than 4 arguments to raise an error, so that a forgotten component (which LÖVE refuses) and an extra one (which LÖVE silently ignores) are both reported.
20. As a plan-2 developer, I want a string, boolean, nil or table where a number belongs to raise an error naming the argument, so that the fake is stricter than LÖVE's string coercion.
21. As a plan-2 developer, I want NaN and infinite components to raise an error, so that a division by zero in the game is reported instead of drawing an undefined byte.
22. As a plan-2 developer, I want a refused `setColor` to leave the current colour unchanged, so that an error caught by `pcall` does not recolour later shapes.
23. As a plan-2 developer, I want `love.graphics` to hold exactly `clear`, `newCanvas`, `points`, `rectangle`, `setCanvas` and `setColor` after this step, so that no function exists without its capture; `getColor` stays absent.
24. As a plan-1 developer, I want a capture program drawing every byte value on every channel, at offsets where truncation, rounding and double precision disagree, with both `rectangle` and `points`, so that the conversion is settled by real LÖVE for both shapes.
25. As a plan-1 developer, I want a capture program with components a few float steps around `byte/255`, so that the single-precision conversion is proven, not just the truncation.
26. As a plan-1 developer, I want a capture program for the default colour's replacement, clamping, persistence across canvases, `clear`, refusals LÖVE also makes and transparent backgrounds, so that the colour's state is proven in one place.
27. As a plan-1 developer, I want each new capture compared with the fake in `make test` as its own parallel check, so that the fast loop proves the convention without getting slower.
28. As a plan-1 developer, I want the component conversion tested directly in the canvas store's unit test with values taken from the captures, so that a rounding conversion, a double-precision product or a missing clamp is caught with a precise message.
29. As a plan-1 developer, I want each misuse of `setColor` checked through the fake's command line with its message, so that the error contract is pinned.
30. As a plan-1 developer, I want a refused translucent `setColor` proven to leave the colour unchanged on the canvas, although no real-LÖVE capture can show it, so that the fake-only refusal is pinned too.
31. As a plans-2–3 reader, I want the colour convention in `docs/love-port/design.md`, including "pass `byte / 255`, never an alpha", so that I do not need plan 1, which will be deleted.
32. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
33. As the repository owner, I want the CI love-capture job green with the new programs, so that the PNGs are proven to be what real LÖVE draws.
34. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
35. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture`, pointed at scratch directories holding a copy of the
real harness (one copy changed to draw only on the fourth frame), on LÖVE
11.5.0 with Mesa 25.2.8 llvmpipe (LLVM 20.1.2), gave these results. They
are the expected answers of this step's programs; the committed PNGs remain
the proof.

- **Stored state**: `getColor()` is `1 1 1 1` by default.
  `setColor(0.25, 0.5, 0.75)` reads back `0.25 0.5 0.75 1` (alpha
  defaults to 1); `setColor(2, −1, 0.3, 1.5)` reads back
  `1 0 0.30000001192093 1`: each component is stored as a float, clamped
  to [0, 1] when set. `setColor(1/0, −1/0, 0.5)` reads back `1 0 0.5 1`;
  NaN is stored as NaN. The table form takes `{r, g, b[, a]}` and copies
  it (changing the table afterwards changes nothing); `{}` fails. Numeric
  strings are coerced. A fifth argument and arguments after a table are
  ignored. Fewer than 3 numbers fail with
  `bad argument #K to '?' (number expected, got no value)`; a boolean with
  `… got boolean`. A refused call leaves the colour unchanged.
- **Conversion: truncation.** 28 800 components drawn as 1×1 rectangles
  and as points: `(b + d)/255` for every byte b and twelve offsets d
  (0, ±0.5, 0.25, 0.49, 0.51, 0.4999, 0.5001, 0.5 ± 10^-5, 0.5 ± 10^-6),
  float ties of `× 255`, and random values. Every pixel equals
  `floor(clampf((float) c) × 255f)`; ties-to-even rounding (`clear`'s
  rule) is wrong on 14 066 of them, rounding half up on 14 702. A second
  set of 7 407 components, ±12 float steps around every `b/255` and
  double values just below it, separates the single-precision conversion
  from a double one: truncating the double product is wrong on 502;
  truncating the float product in float or in double cannot be told
  apart. Consequences: `byte/255` (computed in double, then converted)
  always gives the byte; `0.5` gives 127 (`clear` gives 128); `0.3` gives
  76; `128/255 × (1 − 2^-30)`, one float step below, gives 127; `1/255`
  one float step below gives 0.
- **Points and rectangles agree**: the same 9 600 colours drawn with
  `rectangle("fill", x, y, 1, 1)` and with `points(x + 0.5, y + 60.5)`
  give identical bytes.
- **Opaque colours replace**: alpha is always `FF` on every pixel drawn
  with alpha 1; a coloured pixel over `33669900`, over `80 40 BF 00`, over
  an opaque pixel or over another shape takes the colour's bytes exactly.
- **Translucent colours blend** with LÖVE's default blend mode
  ("alpha", "alphamultiply": source RGB times alpha plus destination RGB
  times 1 − alpha; alpha a + dst_a(1 − a)), also over a transparent
  pixel, whose RGB then shows through: `setColor(1, 128/255, 0, 128/255)`
  over `33669900` gives `99 73 4C 80`, over `204060FF` gives
  `90 60 30 FF`; alpha 0 changes nothing; a half-alpha white drawn twice
  differs from once (`CCD8E5BF` vs `99B2CC7F` over `33669900`). The bytes
  come from llvmpipe's 8-bit blender; the fake does not model them.
- **Persistence**: a colour set in `love.load`, before any canvas, is
  current in `love.draw`; a colour set in the first frame's `love.draw` is
  still current in the fourth frame (LÖVE's loop calls `origin`, `clear`
  and `present`, none of which touches the colour). A colour set while
  another canvas or the screen is selected is current after
  `setCanvas(canvas)`. `clear(…)` ignores the colour and leaves it as it
  was.

### Modules

- **Canvas store** (step 6's module, extended by steps 7 and 8, H-8-2):
  - a second component conversion beside `clear`'s, for drawing colours:
    a double in, a byte out, as measured (float, clamp, × 255 in float,
    truncate); it asserts the component is not NaN, as `clear`'s does.
    `clear`'s conversion is unchanged. The two names must say which is
    which; the header states each one's rule in one line, since mixing
    them up is the trap this step found.
  - a setter for the current colour taking four bytes; it asserts the
    alpha byte is `FF` with a message saying `setColor` refuses
    translucent colours because blending is not modelled. The colour is
    store state: nothing resets it — not selecting a canvas, not `clear`,
    not a new frame. Its initial value stays opaque white.
  - the one-pixel plot and step 8's range fill keep their contract and
    their opaque-colour assert unchanged; they already write the current
    colour (H-8-2).
- **Graphics binding** (H-8-3) gains `setColor`, so `love.graphics` holds
  exactly `clear`, `newCanvas`, `points`, `rectangle`, `setCanvas`,
  `setColor`. Accepted: exactly 3 Lua numbers, or exactly 4 whose fourth
  equals 1. The binding checks everything first (count, then each
  argument's type, then finiteness, then the alpha), then converts the
  three components and sets the colour. Every refusal is a Lua error
  starting `love.graphics.setColor: ` and naming the problem:
  `takes 3 numbers or 4 with alpha 1, not N arguments`,
  `argument K is a <type>, not a number` (a table first argument is
  reported as such: the table form is not supported), `argument K is not
  finite`, `alpha is <v>, only 1 is supported (translucent colours are not
  modelled)`. The exact wording is the implementer's; the tests match the
  function name, the index or count, and the key word.
- **Nothing else changes**: `points` and `rectangle` (they already draw the
  current colour), the point rule, the host, the comparison command, the
  harnesses, the Makefile's capture registration (a `.lua` and its `.png`
  in `tools/love_capture/` become a `make test` check by themselves), the
  `src/` guard.

### Capture programs of this step

Each is a `tools/love_capture/<name>.lua` returning `function(canvas)`,
using only `setColor`, `rectangle`, `points`, `clear`, `newCanvas` and
`setCanvas` (and `pcall`/`assert`). Value lists are loops in the program,
with at most one short table of offsets or factors. Each draft below was
captured and checked against the model: zero differing pixels.

| program | draws | expected |
|---|---|---|
| `color_bytes` | `offsets = {−0.5, −1e-6, 0, 1e-6, 0.25, 0.49, 0.5, 0.51, 0.75, 0.999}`; for k = 0 … 9 599, byte = k mod 256, family = ⌊k/256⌋: `setColor(byte/255, (byte + 0.5)/255, (byte + offsets[family mod 10 + 1])/255)`, then `rectangle("fill", k mod 160, ⌊k/160⌋, 1, 1)` and `points(k mod 160 + 0.5, ⌊k/160⌋ + 60.5)` | all 19 200 pixels opaque, per the model; rows 60–119 equal rows 0–59; red is the byte everywhere; green is the byte (rounding would add one: 8 530 channel values differ between truncation and ties-to-even over the program); blue −0.5 and −1e-6 give byte − 1 (clamped at 0), the others the byte (255 + 0.999 clamps to 255); truncating the double would be wrong on 692 values |
| `color_precision` | `factors = {1 − 2^-30, 1 − 2^-26, 1 − 2^-25, 1 − 2^-24, 1 − 2^-23, 1 − 2^-22, 1 − 2^-21, 1 + 2^-24, 1 + 2^-22}`; for k = 0 … 2 294, byte = k mod 255 + 1, `value = byte/255 × factors[⌊k/255⌋ + 1]`, `setColor(value, value, value)`, `rectangle("fill", k mod 160, ⌊k/160⌋, 1, 1)` | 2 295 grey pixels per the model, 1 372 of them one below their byte (e.g. byte 128 × (1 − 2^-30) gives `7F7F7F`, byte 1 × (1 − 2^-30) gives `000000`, byte 7 × (1 − 2^-30) gives `070707` where a double truncation gives 6); truncating the double would be wrong on 413; the other 16 905 pixels `00000000` |
| `color_state` | top level: `other = newCanvas(160, 120)`, `setColor(1, 0.5, 0.25)`; in the draw function: `clear(0.5, 0.25, 0.75, 0)`; 10×10 rectangles at (0, 0) with the top-level colour, (10, 0) with `setColor(0.5, 0.25, 0.75, 1)`, (20, 0) with `(2, −1, 0.3)`, (30, 0) with `(1e30, −1e30, 1, 1)`, (40, 0) with `(0, 0, 0)`, (50, 0) with `(1, 1, 1)`; palette colour 12 `(41/255, 173/255, 1)` at (0, 20, 30, 20), colour 8 `(1, 0, 77/255)` at (10, 30, 30, 20) over it, colour 10 `(1, 236/255, 39/255)` as points (5.5 + i, 25.5) and (5.5 + i, 45.5) for i = 0 … 29 over both; colour 11 set, `setCanvas(other)`, fill `other` whole, colour 13 `(131/255, 118/255, 156/255)` set while `other` is selected, `setCanvas()`, a full-canvas rectangle (to the screen), `setCanvas(canvas)`, rectangle (60, 20, 10, 10); `setCanvas(other)`, `clear(0, 0, 0, 1)`, `setCanvas(canvas)`, rectangle (70, 20, 10, 10); `assert(not pcall(setColor, 1, 0))`, `assert(not pcall(setColor))`, rectangle (80, 20, 10, 10), `points(85.5, 35.5)` | `FF7F3FFF` ×100, `7F3FBFFF` ×100 (clear gave `80 40 BF`), `FF004CFF` ×100, `FF00FFFF` ×100, `000000FF` ×100, `FFFFFFFF` ×100, `29ADFFFF` ×375, `FF004DFF` ×575, `FFEC27FF` ×60, `83769CFF` ×301, the other 17 289 pixels `8040BF00` |

The plan's edge cases: transparent pixels are `color_state`'s background
(alpha 0, RGB `8040BF` replaced by every opaque draw); off-canvas and
negative coordinates do not interact with the colour and stay covered by
steps 7 and 8's captures, now drawn in the current colour; nested
translate does not exist before step 13, which captures `translate` over
coloured shapes (step 15 combines colour, `translate` and `draw`).
Translucent colours cannot be captured: real LÖVE blends them and the fake
refuses them; the CLI test pins the refusal. If a PNG disagrees with this
table, the PNG wins and design.md follows it.

### Conventions written into `docs/love-port/design.md`

A "Colour" paragraph after step 8's "Rectangles" paragraph (H-8-6):

- the forms accepted, every refusal, the error prefix, a refused call
  leaving the colour unchanged;
- the current colour: opaque white until the first `setColor`, then the
  last one set, for the whole run: across frames, canvases, `setCanvas()`
  and `clear`, from `love.load` on; `clear` ignores it;
- the conversion: to single precision, clamped to [0, 1], times 255 in
  single precision, **truncated** — unlike `clear`, which rounds (`0.5`
  gives 127 here, 128 there); a pixel drawn takes the colour's four bytes,
  alpha `FF`, over any pixel, transparent included;
- **the rule for the game**: pass each palette byte as the Lua expression
  `byte / 255`, which always gives the byte; a component computed any
  other way can lose a whole byte (one float step below `128/255` draws
  127); never pass an alpha other than 1: translucent colours blend in
  real LÖVE and are refused by the fake;
- the captures: `color_bytes`, `color_precision`, `color_state`.

The "Points" and "Rectangles" paragraphs replace "opaque white until
`setColor` exists" with "the current colour (see Colour)". The function
list's `setColor` gains its exact forms: `setColor(r, g, b)` or
`setColor(r, g, b, 1)`.

### Decisions taken without the plan

- **Translucent colours refused, not blended.** The game draws only the
  16 opaque palette colours (design.md); its see-through pixels come from
  images (step 11), never from a colour's alpha. Reproducing llvmpipe's
  8-bit blender byte for byte would need its own captures and model for
  a case the game never reaches; refusing keeps the fake exact. Adding
  blending later is a design.md decision with its captures.
- **Truncation, measured, beside `clear`'s rounding.** Two conversions in
  the store, named apart, each with its one-line rule; no shared helper
  that could make one look like the other.
- **Clamp like LÖVE**, as `clear` does: components out of [0, 1] are
  accepted and clamped; non-finite ones are refused, as for `clear`.
- **Two forms only.** No table form, no fifth argument, no string
  coercion: stricter than LÖVE, one way to pass a colour. A plan-2 need
  for the table form is a design.md decision with a capture.
- **Any opaque colour accepted, not only the 16.** The capture programs
  need arbitrary colours; step 17 decides where "a colour that is not one
  of the 16" becomes fatal, and must keep the capture comparison able to
  run this step's programs.
- **No `getColor`.** It is not on the design's list; the game keeps its
  own current colour if it needs one.
- **Check everything, then set.** A refused call changes nothing.
- **Persistence across frames is a decision, not a capture**: the capture
  harness draws one frame; the four-frame scratch run above is the
  evidence, and the fake keeps the colour in the store for the run.
  Step 16's capture of every frame will see it on the game.
- **Misuse tested in one fixture game** logging each misuse through
  `pcall` and `debug_state()`, plus one uncaught misuse for the fatal
  path, as steps 7 and 8 did.

## Implementation Chunks

1. **Store colour.** The drawing-colour conversion and the colour setter
   with its assert; extend the canvas store's unit test with the values
   below. Stories 3–7, 10, 11, 14, 16 (store side), 28.
2. **`setColor` in the fake.** The binding with its refusals; the new CLI
   test target with its fixtures (log, uncaught, refused translucent
   colour against an encoded PNG); the existing name lists gain
   `setColor`. Stories 1, 2, 8, 9, 12, 13, 15, 17–23, 29, 30.
3. **Captures, docs, proof.** The three programs and their PNGs from
   `make love-capture` (git diff shows only the new files), the three
   comparisons passing in `make test`, design.md, the timing, the CI run,
   the step note. Stories 24–27, 31–35.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the store's conversion results and bytes, the binary's exit status,
  stderr and state file, the comparison's verdict. No test reaches into
  the Lua state or the store's layout.
- **Canvas store, unit test** (step 6's test, as extended by steps 7 and
  8, H-8-2; its existing cases unchanged):
  - drawing conversion, values from the captures: `b/255` → b for
    b = 0, 1, 7, 128, 255; `0.5` → 127; `(2 + 0.5)/255` → 2; `0.3` → 76;
    `(5 − 0.5)/255` → 4; `(5 − 1e-6)/255` → 4; `(255 + 0.999)/255` → 255;
    `2` → 255; `−1` → 0; `128/255 × (1 − 2^-30)` → 127;
    `1/255 × (1 − 2^-30)` → 0; `7/255 × (1 − 2^-30)` → 7 (a failing
    assert says the product was probably taken in double precision);
    `0.5` → 127 against `clear`'s 128 (a failing assert says the drawing
    conversion probably rounds);
  - after setting a colour, a plot and a range fill write its four bytes,
    over a transparent coloured pixel too; the colour survives selecting
    another canvas, the screen, and a clear; a new store still plots
    `FFFFFFFF`.
- **`setColor`, CLI test** (new, its own target in `FAKE_LOVE_CHECKS`,
  through `tmp/fake_love` and the shared CLI helper, a fixture game and a
  one-frame replay, as steps 7 and 8's CLI tests):
  - a game whose `love.load` logs the sorted names of `love.graphics`
    (`clear newCanvas points rectangle setCanvas setColor`), then, in
    `pcall`, each misuse with its message: `setColor()`,
    `setColor(1, 0)`, `setColor(1, 0, 0, 1, 0)`, `setColor({1, 0, 0})`,
    `setColor("1", 0, 0)`, `setColor(1, nil, 0)`, `setColor(1, 0, true)`,
    `setColor(0/0, 0, 0)`, `setColor(1, 1/0, 0)`, `setColor(1, 0, 0, 0)`,
    `setColor(1, 0, 0, 0.5)`, `setColor(1, 0, 0, 2)`,
    `setColor(1, 0, 0, nil)`; then `setColor(2, −1, 0.3)` and
    `setColor(1, 0, 0, 1)` logged as successes; the log is compared byte
    for byte;
  - a game calling `setColor(1, 0, 0, 0.5)` uncaught in `love.draw` exits 1
    naming `love.graphics.setColor` and `f0`;
  - "a refused call keeps the colour", for refusals real LÖVE does not
    make: one fixture program, run by the comparison command against a PNG
    written to `tmp/` by the test PNG encoder (a red 10×10 square on
    transparent black), calls `setColor(1, 0, 0)`, then
    `pcall(setColor, 0, 1, 0, 0.5)` and `pcall(setColor, {0, 1, 0})`, then
    `rectangle("fill", 0, 0, 10, 10)`; it joins the `setColor` CLI target.
  - every test that lists `love.graphics`'s names (step 6's graphics CLI
    test, and steps 7 and 8's logs if they list them, H-8-5) gains
    `setColor`.
- **The capture comparisons** are the tests of the convention: each of
  the three programs passes in `make test`, and steps 7 and 8's captures
  still pass (they never call `setColor`).
- **Prior art:** step 6's canvas store test (`clear`'s conversion values)
  and graphics CLI test; steps 7 and 8's CLI tests with their log,
  uncaught and refused fixtures; `test_fake_love_joystick` for misuse
  logged through `pcall` and `debug_state()`.
- **Proofs recorded in the step note** (Step 3.9 in plan 1):
  - `make love-capture` writes the three new PNGs and leaves every other
    file of `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green
    (run id);
  - `make test` before and after with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 8's last commit,
    H-8-7) and against `2e7215c`, both medians of paired ratios ≤ 3 %
    (22 pooled rounds if either exceeds it); plain and paired medians,
    spreads, rounds, core count, and the slowest new check's own time
    (budget 100 ms; `color_bytes` makes 9 600 `setColor`, `rectangle` and
    `points` calls each and is the likely slowest); a check over budget is
    split before the step closes.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- Blending: translucent colours, `setBlendMode`, premultiplied alpha.
  Refused, not modelled.
- `getColor`, `setBackgroundColor`, the table form of `setColor`: not on
  the design's list.
- The colour's effect on `draw(image, quad, x, y)` (LÖVE multiplies image
  texels by the current colour): step 11 measures it with its own capture;
  this step's conversion is only proven for `points` and `rectangle`.
- `push`/`pop`/`translate`, and whether `push` saves the colour (only
  `push("all")` does in LÖVE; the game uses the plain form): step 13.
- The fatal "colour that is not one of the 16": step 17.
- Drawing to the screen: not modelled.
- Audio, always.

## Hypotheses

Plan step 8 is being implemented while this PRD is written. Its span rule
is committed (73daf5c, `point_find_span`); its store range fill, its
`rectangle` binding, its captures, its design.md paragraph and its step
note are not. Each item below is checked by the implementer against the
committed step 8 before starting; when one differs, adapt the names, not
the decisions.

- **H-8-1**: `rectangle("fill", x, y, w, h)` with integers fills exactly
  columns x … x + w − 1 and rows y … y + h − 1, so a 1×1 rectangle at
  integer (x, y) fills pixel (x, y). This step's programs rely on it (the
  real-LÖVE drafts above confirm it independently: their 1×1 rectangles
  match points at (x + 0.5, y + 0.5) pixel for pixel).
- **H-8-2**: the canvas store holds the current colour, initially opaque
  white, and both the one-pixel plot and step 8's range fill write it,
  replacing four bytes, doing nothing with no target, asserting the
  range is on the canvas and that the colour is opaque. This step adds
  only the setter and the drawing conversion; neither drawing function
  changes.
- **H-8-3**: the graphics binding installs `love.graphics` holding exactly
  `clear`, `newCanvas`, `points`, `rectangle`, `setCanvas`, its errors
  prefixed with the full function name, and checks every argument before
  acting. This step adds `setColor` the same way.
- **H-8-4**: step 8's `rectangle` refuses anything but `"fill"` and five
  arguments, so this step's programs only use that form.
- **H-8-5**: step 8 has its own rectangle CLI test target in
  `FAKE_LOVE_CHECKS` (log fixture, uncaught fixture, refused-call fixture
  compared against an encoded PNG), and the step 6 graphics CLI test's
  name list holds `rectangle`; this step adds a sibling target and adds
  `setColor` to every name list.
- **H-8-6**: step 8 ends with its six `rectangle_*` captures and PNGs
  committed and a "Rectangles" paragraph in `docs/love-port/design.md`
  after "Points", saying the colour is opaque white until `setColor`; this
  step's "Colour" paragraph follows it and both paragraphs point to it.
- **H-8-7**: step 8's last commit is this step's "before" timing baseline,
  and its note (Step 3.8) records its timings.

## Further Notes

- The exploration programs lived in scratch directories passed to
  `make love-capture` with a copy of the harness; nothing of them is
  committed. The three drafts of the table were captured and checked
  against the model in Python with numpy: zero differing pixels.
- The measured truncation is consistent with LÖVE 11.5 storing the colour
  as clamped floats and handing it to the GPU as 8-bit vertex colours
  converted by a C++ cast (`(uint8)(c * 255.0f)`); the shader's
  `byte/255` then reaches the 8-bit canvas unchanged. `clear` goes
  through `glClearColor`, which Mesa rounds: hence the two rules.
- The fake models only alpha 1, so llvmpipe's blend arithmetic (the
  `99 73 4C 80` and `90 60 30 FF` examples above) is recorded here as a
  finding, not as a convention.
- Three new checks of one LuaJIT start each and at most about 30 000 calls
  run in parallel on the cores the unit tests leave idle; if the gate
  fails, design.md rule 5 applies.
