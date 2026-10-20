---
title: "fake_love accepts setDefaultFilter(\"nearest\", \"nearest\") as the no-op real LÖVE 11.5 proves it to be"
description: "The Lua game needs nearest filtering for its window upscale, but the fake LÖVE has no setDefaultFilter, and nobody has proved in real LÖVE that the call changes nothing the fake draws."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 13, `tmp/fake_love` draws points, rectangles and
image pieces, under `push`/`pop`/`translate`. design.md's function list
also names `love.graphics.setDefaultFilter("nearest", "nearest")`, and the
fake does not have it: a call fails as an unlisted function.

The Lua game of plans 2–3 needs this call. In real LÖVE it draws its
160×120 canvas scaled up to the window at the end of `love.draw()`. Under
LÖVE's default filter, `linear`, that upscale is blurred. Only `nearest`
keeps the pixels sharp. The fake never sees the window. It must still
accept the call, or the game cannot run in it.

design.md's "Images" paragraph already predicts the answer: integer
draws give the same bytes under `linear` and `nearest`, so the call
"changes nothing the fake draws", and fractional positions stay refused.
That prediction rests on probes of step 11. No committed capture
calls `setDefaultFilter`, so nothing on CI checks it. Five questions
were open:

- **Forms.** What does LÖVE accept (one name, a third anisotropy argument,
  other names), and what should the fake accept?
- **Timing.** Does the call change images and canvases that already
  exist, or only those created after it?
- **Canvases.** Does it change canvases as well as images?
- **Bad arguments.** What does LÖVE say for an unknown filter name?
- **Pixels.** Are the captures really the same under `nearest`, whichever
  harness order runs the call?

## Solution

`love.graphics.setDefaultFilter("nearest", "nearest")` joins the fake.
`love.graphics` holds 13 functions from `main.lua` on (H-13-1). The call
checks its arguments and stores nothing: in real LÖVE 11.5 on llvmpipe,
every pixel the fake models is byte-identical under `linear` and
`nearest`. This holds for every committed capture, for 3 000 random
quads, tinted or not, and for images created before or after the call.
The fake accepts exactly the form design.md lists and refuses everything
else, including forms LÖVE accepts, such as one name, an anisotropy
argument, `"linear"` and `"none"`.

One capture program, `filter_nearest`, calls `setDefaultFilter` at its
top level and in its draw function. It then draws images created before
the call, after it, and inside the draw function. It uses high-contrast
checkerboards and stripes, a fixture PNG with a colour key, sub-quads,
1×1 quads, tints, clipping on every side and positions near 2^20. Real
LÖVE pins it on every CI push, and the fake matches it pixel for pixel in
`make test`. Drawn without the calls, real LÖVE gives a byte-identical
PNG. design.md gets a "Default filter" paragraph with the rule for the
game.

## User Stories

1. As a plan-2 developer, I want `love.graphics.setDefaultFilter("nearest", "nearest")` in the fake, so that the game's `love.load` can request sharp scaling for the window and still run in the fake.
2. As a plan-2 developer, I want the call to change nothing the fake draws, so that every capture and every e2e frame stays what real LÖVE draws, with or without the call.
3. As a plan-2 developer, I want the call accepted at any time, any number of times, with a canvas or the screen selected, before or after images and canvases exist, so that the fake never fails where real LÖVE succeeds with the listed form.
4. As a plan-2 developer, I want design.md to tell me to call it once, first thing in `love.load`, before creating the canvas or any image, so that the canvas really is `nearest` when the real game scales it to the window.
5. As a plan-2 developer, I want design.md to say that objects created before the call keep `linear` in real LÖVE, so that I know why the call must come first, even though the fake cannot see it.
6. As a plan-2 developer, I want design.md to say that fractional draw positions stay refused under `nearest`, with the measured reason, so that I do not expect `nearest` to make sub-pixel draws safe.
7. As a plan-2 developer, I want `setDefaultFilter("nearest")` with one name refused, though LÖVE accepts it, so that the game uses the one form that design.md lists and the capture proves.
8. As a plan-2 developer, I want a third argument refused, whether anisotropy or a trailing nil, so that no unmeasured anisotropy value reaches real LÖVE.
9. As a plan-2 developer, I want `"linear"` and `"none"` refused with a message saying that only `"nearest"` is supported, so that a typo or a leftover `"linear"` that would blur the window fails in the fake.
10. As a plan-2 developer, I want an unknown filter name refused with LÖVE's own message, so that the fake's error reads like the one real LÖVE would raise.
11. As a plan-2 developer, I want a non-string argument refused naming the argument and its type, with no number coercion, so that a bug in the call fails where it happens.
12. As a plan-2 developer, I want a refused call to change nothing and an uncaught refusal to be fatal naming the function, so that the error contract is that of every other drawing function.
13. As a plan-2 developer, I want `love.graphics` to stay nil in `conf.lua` and to hold exactly 13 functions from `main.lua` on, so that no function exists without its capture.
14. As a plan-2 developer reading design.md, I want the "Images" paragraph to stop saying "once the fake has it", and a "Default filter" paragraph to hold the form, the measurements, the refusals and the rule for the game, so that I do not need plan 1, which will be deleted.
15. As a plan-1 developer, I want a capture program calling `setDefaultFilter` and drawing images created before, after and inside the draw function, so that real LÖVE pins "nearest draws what the fake draws" on every CI push.
16. As a plan-1 developer, I want that capture to hold texel patterns where any filtering would show, such as alternating colours, transparent texels beside opaque ones, sub-quads and single texels, so that a change in LÖVE or Mesa filtering turns CI red.
17. As a plan-1 developer, I want the capture not to depend on the harness's load order, so that real LÖVE (program loaded in `love.draw`) and the fake (program loaded in `love.load`) draw the same thing.
18. As a plan-1 developer, I want every committed capture re-run once in real LÖVE with `nearest` set in a scratch harness copy, and found byte-identical, recorded in the step note, so that "changes nothing" holds for step 13's `translate_stack` too.
19. As a plan-1 developer, I want each misuse of `setDefaultFilter` checked through the fake's command line with its message, inside an existing log game, so that the error contract is pinned without a new process in `make test`.
20. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
21. As the repository owner, I want the CI love-capture job green with the new program, so that the PNG is proven to be what real LÖVE draws.
22. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
23. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

How it was measured: `make love-capture LOVE_CAPTURE_DIR=<scratch>`, run
on scratch directories holding a copy of the real harness, the `files/`
fixtures and probe programs. The probes wrote their results to a scratch
file and read canvases back with `Canvas:newImageData()`. Everything ran
on LÖVE 11.5.0 and Mesa 25.2.8 llvmpipe (LLVM 20.1.2), at commit
`0b5a98a` (end of step 12). These results are the expected answers of
this step's tests. The committed PNG remains the proof.

- **The default.** `getDefaultFilter()` gives `linear linear 1`.
- **Accepted forms.** Every one below returns nothing, and
  `getDefaultFilter()` shows the effect:
  - `("nearest", "nearest")` sets both to `nearest`, anisotropy 1;
  - `("nearest")` sets both: the magnification filter defaults to the
    minification one, and `("nearest", nil)` does the same;
  - `("linear", "nearest")`, `("nearest", "linear")` and
    `("none", "nearest")` are taken as given (`none` is accepted as a
    minification filter);
  - a third argument is the anisotropy. It is stored as given, with no
    check: `16`, `0`, `-1`, `0.5`, `"4"` (coerced), NaN and infinity are
    all accepted. A nil third argument gives 1, and a fourth argument is
    ignored.
- **Refused forms**, each leaving the default unchanged, also when only
  the second name is bad:
  - `()`: `bad argument #1 to '?' (string expected, got no value)`;
  - `(nil, …)` and a table: `bad argument #1 … (string expected, got
    nil|table)`;
  - an unknown name, case-sensitive (`"bogus"`, `"NEAREST"`, the number
    `1` coerced to `'1'`): `Invalid filter mode '<v>', expected one of:
    'none', 'linear', 'nearest'`.
- **Timing: only objects created after the call.** After
  `setDefaultFilter("nearest", "nearest")`:
  - an Image and a Canvas created earlier still report `linear linear 1`;
  - an Image and a Canvas created later report `nearest nearest 1`;
  - an Image created after a call with anisotropy 16 reports
    `nearest nearest 16`.

  So the call does affect canvases, and it never changes an existing
  object.
- **Pixels under `nearest` equal pixels under `linear`, for everything the
  fake accepts.**
  - All 29 committed captures, re-run in two variants, gave PNGs
    byte-identical to the committed ones:
    - `nearest` set in the harness's `love.load`, before the harness
      canvas and the program exist;
    - `nearest` set on the first line of each program, inside the real
      harness's `love.draw`, after the harness canvas exists.
  - 3 000 random integer draws of random quads from four images (128×128,
    48×30, 7×3, 1024×4; random RGB, 30 % alpha-0 texels), once from
    images created under `linear` and once under `nearest`: 0 differing
    pixels, untinted and tinted `(1, 0.5, 0.25)` alike.
- **Fractional positions stay unsafe under `nearest`.** The same 3 000
  draws shifted by a fraction, compared with the integer draw of the same
  filter:

  | shift | `nearest` | `linear` |
  |---|---|---|
  | 0.5 | 7 249 pixels differ | 19 181 differ |
  | 0.25 | 0 | 19 181 |
  | 1/512 | 0 | 5 231 |
  | 1/1024 | 0 | 0 |
  | −0.5 | 14 945 | 19 200 |

  `nearest` removes the blending of small shifts but still drops or
  doubles texels at half a pixel. The refusal of fractional positions
  (step 11) stays.
- **The draft capture below**, run in real LÖVE once as written and once
  with every `setDefaultFilter` line deleted, gave byte-identical PNGs.
  It has 410 distinct colours, and 2 727 of its pixels are neither of the
  two background colours. The deleted variant run through
  `tmp/love_capture_check` in today's fake matched pixel for pixel.

### Decisions

- **Form: `setDefaultFilter("nearest", "nearest")` only.**
  - The call takes exactly two arguments, both the Lua string
    `"nearest"`.
  - Everything else is refused, though LÖVE accepts most of it:
    - one name: it is the same filter, but not the listed form;
    - a third argument, even 1 or nil: anisotropy is not measured, and
      LÖVE stores any value unchecked;
    - `"linear"` and `"none"`: they would blur the real window, which
      the fake cannot see;
    - mixed pairs;
    - numbers, including LÖVE's coercion of `1` to `'1'`.
- **The fake stores nothing.** A stored filter would have no reader:
  `getDefaultFilter`, `getFilter` and `setFilter` are not on the list,
  and no pixel the fake models depends on the filter (measured above).
  The call checks its arguments and returns nothing. It needs no state,
  no new module and no change to the canvas or image code.
- **No ordering rule.** The call is accepted at any time and any number
  of times: in `love.load`, `love.update` or `love.draw`, with a canvas
  or the screen selected, before or after any Image or Canvas exists. In
  real LÖVE the order matters only for the filter each new object gets,
  and that changes no pixel the fake models. The only object where it
  shows is the window upscale, which is outside the fake. Rejected
  alternative: refusing the call once a Canvas or Image exists. Both
  harnesses create their canvas before loading the program, so a
  capture program could never call `setDefaultFilter`, and the order is
  a rule for the game in design.md instead.
- **Refusals change nothing.** Each is an error starting
  `love.graphics.setDefaultFilter: `. An uncaught one is fatal through
  the existing mechanism, naming the callback and the frame like every
  other drawing function. The checks run in this order, and the first
  failure is reported:
  - the argument count:
    `takes 2 arguments, "nearest" and "nearest", not N`, with the draw
    binding's counting, so a trailing nil counts;
  - for each argument in order:
    - its type: `argument N is a <type>, not a string`, with no number
      coercion;
    - an unknown name: LÖVE's message verbatim, `Invalid filter mode
      '<v>', expected one of: 'none', 'linear', 'nearest'`;
    - a known name other than `nearest`: `argument N is "<v>", only
      "nearest" is supported (the filter is not modelled)`.

  The exact wording of the fake's own messages is the implementer's. The
  tests match the whole log once it is written, and LÖVE's message must
  stay verbatim.
- **Capture programs may call `setDefaultFilter` at their top level.**
  This is the opposite of step 13's rule for the transform (H-13-3), and
  it is safe here:
  - the fake stores nothing, so the fake harness's load order cannot
    matter;
  - in real LÖVE, the call only decides the filter of objects created
    after it, and a program's images are all created after its own
    top-level call in either harness;
  - the harness canvas, created before, stays `linear`, but it is only
    read back, never sampled.

  design.md's capture paragraph needs no new sentence. The "Default
  filter" paragraph says this once.

### Modules

- **Graphics binding** (existing).
  - It gains `setDefaultFilter`, registered like the other functions
    (H-13-5). It reads no upvalue state.
  - `love.graphics` holds 13 functions: step 13's 12 plus
    `setDefaultFilter` (H-13-1).
  - No other module changes: canvas, image, image rules, point, transform
    and host stay as step 13 leaves them.
- **Makefile.** No change. The new capture becomes its own
  `love-capture-check-filter_nearest` by itself (H-13-2), and no unit or
  CLI target is added.

### Capture program of this step

`filter_nearest.lua` uses only functions from steps 6 to 12 plus
`setDefaultFilter`. It uses no `push`, `pop` or `translate`, so it does
not depend on step 13's outcome. It reads `files/rgb_key.png`, step 12's
19×9 RGB fixture with a `tRNS` colour key, so every texel's alpha is 0 or
FF.

The draft, from the measurement:

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

local function checker(x, y)
    if (x + y) % 2 == 0 then
        return 255, 241, 232, (x * 3 + y) % 7 == 0 and 0 or 1
    end
    return 29, 43, 83, 1
end
local function stripes(x, y)
    return (x * 37) % 256, (y * 53 + 40) % 256, x % 2 * 255, (x + 2 * y) % 5 == 0 and 0 or 1
end

local graphics = love.graphics
local before = graphics.newImage(new_data(16, 12, checker))
graphics.setDefaultFilter("nearest", "nearest")
local after = graphics.newImage(new_data(16, 12, checker))
local wide = graphics.newImage(new_data(64, 6, stripes))
local keyed = graphics.newImage(love.image.newImageData("files/rgb_key.png"))

return function(canvas)
    graphics.clear(0.4, 0.1, 0.2, 0)
    graphics.setColor(0, 0.5, 0.25)
    graphics.rectangle("fill", 0, 80, 160, 40)
    graphics.setDefaultFilter("nearest", "nearest")
    local late = graphics.newImage(new_data(16, 12, checker))
    graphics.setColor(1, 1, 1)

    local whole = graphics.newQuad(0, 0, 16, 12, 16, 12)
    local inner = graphics.newQuad(3, 2, 9, 7, 16, 12)
    local texel = graphics.newQuad(5, 4, 1, 1, 16, 12)
    for column, image in ipairs({before, after, late}) do
        local left = (column - 1) * 20
        graphics.draw(image, whole, left, 0)
        graphics.draw(image, inner, left, 14)
        for k = 0, 5 do
            graphics.draw(image, texel, left + k * 2, 24 + k % 2)
        end
        graphics.draw(image, whole, left + 2, 74)
        graphics.draw(image, whole, left - 7 + column, -5)
    end
    graphics.draw(after, whole, -9, 40)
    graphics.draw(after, whole, 153, 40)
    graphics.draw(after, whole, 70, 115)
    graphics.draw(after, whole, 2 ^ 20 - 1, 40)
    graphics.draw(after, whole, -(2 ^ 20 - 1), 40)

    local strip = graphics.newQuad(0, 0, 64, 6, 64, 6)
    local slice = graphics.newQuad(17, 1, 23, 4, 64, 6)
    graphics.draw(wide, strip, 64, 2)
    graphics.draw(wide, slice, 64, 10)
    graphics.draw(wide, strip, 120, 30)
    graphics.draw(keyed, graphics.newQuad(0, 0, 19, 9, 19, 9), 70, 30)
    graphics.draw(keyed, graphics.newQuad(4, 2, 11, 5, 19, 9), 70, 42)

    graphics.setColor(1, 0.5, 0.25)
    graphics.draw(after, whole, 90, 50)
    graphics.draw(before, whole, 110, 50)
    graphics.draw(wide, slice, 64, 90)
    graphics.setColor(41 / 255, 173 / 255, 1)
    graphics.draw(keyed, graphics.newQuad(0, 0, 19, 9, 19, 9), 130, 90)
    graphics.setColor(1, 1, 1)

    graphics.setCanvas()
    graphics.setDefaultFilter("nearest", "nearest")
    graphics.draw(after, whole, 0, 100)
    graphics.setCanvas(canvas)
end
```

What it pins:

- **Three creation times**, drawn side by side with the same quads:
  - `before`, created ahead of the call, so `linear` in real LÖVE;
  - `after`, created at the top level once the call is made;
  - `late`, created inside the draw function after a second call.

  All three must give the same pixels.
- **Texels where filtering would show:**
  - a checkerboard of two palette colours with scattered alpha-0 texels;
  - stripes alternating 0 and FF in blue;
  - a single texel drawn six times;
  - a sub-quad starting inside the image, where `linear` would sample
    its neighbours.
- **The colour-keyed fixture**, whole and through a sub-quad.
- **Tints**, including a palette colour over the keyed image.
- **Clipping** off the left, right, top and bottom, and positions
  ±(2^20 − 1).
- **A call with the screen selected**, then a draw on the screen, which
  changes nothing.

If the committed PNG disagrees with this section, the PNG wins and
design.md follows it.

### Conventions written into `docs/love-port/design.md`

- A **"Default filter"** paragraph after "Transforms" (H-13-3). It
  holds:
  - the only form, `setDefaultFilter("nearest", "nearest")`, accepted at
    any time and storing nothing;
  - the measurements:
    - LÖVE's default is `linear linear 1`;
    - only Images and Canvases created after the call get `nearest`;
    - every capture and 3 000 random integer draws are byte-identical
      under both filters;
    - fractional shifts still differ under `nearest` (0.5 and −0.5 drop
      or double texels), so fractional positions stay refused;
  - the refusals, and why each LÖVE-accepted form is refused;
  - that a capture program may call it at its top level, and why;
  - **the rule for the game**: call `setDefaultFilter("nearest",
    "nearest")` once, as the first graphics call of `love.load`, before
    `newCanvas` and every `newImage`. The window upscale of the canvas is
    then sharp in real LÖVE. The fake cannot check this order. Integer
    draws are unchanged by the call, and fractional draws stay refused;
  - the capture `filter_nearest`.
- **"Images"** changes in two places:
  - "`love.graphics` holds exactly …" names the 13 functions;
  - the sentence "So `setDefaultFilter("nearest", "nearest")`, once the
    fake has it, is a call that changes nothing the fake draws, and
    fractional positions stay refused under `nearest` too." becomes a
    pointer to "Default filter". The step-11 measurement before it stays.
- **The function list's "Drawing" line** keeps
  `setDefaultFilter("nearest", "nearest")` as it is.

## Implementation Chunks

1. **`setDefaultFilter` in the fake.**
   - The binding and its refusals.
   - The six key lists expect 13 names (H-13-1).
   - The misuse lines in the image-draw log game, and their expected log.
   - Stories 1–3, 7–13, 19.
2. **Capture, docs, proof.**
   - The program and its PNG from `make love-capture`; git diff shows
     only the new pair.
   - The comparison passing in `make test`.
   - The one-off re-run of every committed capture under `nearest` in a
     scratch harness copy, with byte-identical results.
   - design.md, the timing, the CI run and the step note.
   - Stories 4–6, 14–18, 20–23.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  - the binary's exit status, stderr and state file;
  - the capture comparison's verdict.

  No test reaches into the Lua state or a module's internals.
- **Graphics binding, CLI test** (existing `test_fake_love_image_draw`,
  about 24 ms).
  - The log game `image_draw_log` gains its lines in its existing
    `love.load`. That adds no process and no target: the filter is
    image state, and this game already logs every image misuse.
  - The game logs, through `pcall`:
    - the accepted form, three times, all succeeding: before any image
      exists, after images exist, and with a canvas selected;
    - each refusal with its message:
      - no argument, one `"nearest"`;
      - `("nearest", "nearest", 1)`, `("nearest", "nearest", nil)`;
      - `("linear", "linear")`, `("nearest", "linear")`,
        `("none", "nearest")`;
      - `("bogus", "nearest")`, `("nearest", "NEAREST")`, both with
        LÖVE's message;
      - `(1, "nearest")`, `({}, "nearest")`, `(nil, "nearest")`,
        `("nearest", true)`;
    - a draw after the refusals, still accepted.
  - The test asserts the whole log, as it does today.
  - Its `love.graphics` key line, like the other five key lists (the
    graphics, points, rectangle and set_color tests, and step 13's
    transform log, H-13-1), expects the 13 names, and `conf graphics nil`
    stays.
- **The capture comparison** is the test of "changes nothing":
  `filter_nearest` passes in `make test`, and every earlier capture still
  passes.
- No new unit test. There is no pure logic to isolate: three string
  comparisons and a count.
- **Prior art:** `test_fake_love_image_draw` and `image_draw_log` (misuse
  log, whole-log assert); the `draw_quads` capture (image draws, clipping,
  ±(2^20 − 1)); `imagedata_files` (fixture read through `files/`).
- **Proofs recorded in the step note** (Step 3.14 in plan 1):
  - `make love-capture` writes the new PNG and leaves every other file of
    `tools/love_capture/` byte-identical;
  - every committed capture, `translate_stack` included, is re-run once
    under `nearest` in a scratch harness copy that calls the filter in
    `love.load`, and its PNG is byte-identical to the committed one (29
    of 29 at `0b5a98a`);
  - the CI run on the pushed commit, with the love-capture job green
    (run id);
  - `make test` before and after, with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 13's last commit,
    H-13-4) and against `2e7215c`. Both paired medians must be ≤ 3 %, or
    22 pooled rounds decide if either exceeds it. Record the plain and
    paired medians, the spreads, the rounds, the cores, and the new
    capture check's own time.
  - The new work is one capture check (about 60 draw calls, a few
    milliseconds, run in parallel) and about 20 extra `pcall` lines in an
    existing fake process. If the gate still fails, design.md rule 5
    applies.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `getDefaultFilter`, `Image:setFilter`, `Image:getFilter`,
  `Canvas:setFilter`, mipmaps, wrap modes and anisotropy.
- Any filter other than `nearest`, and any model of filtering in the
  fake.
- Checking the order of `setDefaultFilter` against `newCanvas` and
  `newImage`. That order is a rule for the game in design.md.
- The window upscale of the canvas, which is outside the fake.
- Fractional draw positions, which stay refused (step 11).
- The combined capture of step 15. Capture to palette numbers and the
  fatal-error catalogue are steps 16 and 17.
- Changing either capture harness.
- Audio, always.

## Hypotheses

Plan step 13 (`push`/`pop`/`translate`) is being implemented while this
PRD is written. Its PRD was read, but none of its code was. Before
starting, the implementer checks each item below against the committed
step 13. When one differs, adapt the names, the placement and the
dependent test lines, not the decisions. This step's capture and its
binding depend on nothing from step 13.

- **H-13-1**: step 13 leaves `love.graphics` with exactly 12 functions:
  the 9 of step 11 plus `push`, `pop` and `translate`. Six places list
  them: the graphics, points, rectangle, set_color and image_draw tests,
  and the new `transform_log` game asserted from
  `test_fake_love_set_color`. This step makes each list 13 names, with
  `setDefaultFilter` sorted after `setColor`.
- **H-13-2**: step 13 keeps the Makefile's capture registration. A
  top-level `.lua` and `.png` pair in `tools/love_capture/` becomes its
  own `love-capture-check-<name>` by itself, and `make love-capture`
  runs only top-level programs. Step 13 adds the `translate_stack` pair.
- **H-13-3**: step 13 adds a "Transforms" paragraph right after "Images"
  in design.md, and makes "Images" name 12 functions. It also adds a
  sentence to "Real-LÖVE captures": a capture program never changes the
  transform at its top level. Both harnesses still load the program the
  same way: the real one inside `love.draw`, after creating its canvas,
  and the fake one in `love.load`, after creating its canvas. This
  step's "Default filter" paragraph goes after "Transforms", and its
  top-level allowance does not contradict step 13's sentence.
- **H-13-4**: step 13's last commit is this step's "before" timing
  baseline, and its note (Step 3.13) records its timings. Step 13 adds
  no CLI target. It extends `test_fake_love_set_color`, not
  `test_fake_love_image_draw`, so the latter is still about 24 ms when
  this step extends it.
- **H-13-5**: step 13 changes the graphics binding's install so that it
  also receives the transform stack. Registration stays one call per
  function, so `setDefaultFilter` is registered the same way as its
  neighbours, with whatever upvalues they carry.
- **H-13-6**: step 13 changes neither the image binding nor `draw`'s
  argument checks, apart from translating the position. With offset 0,
  `filter_nearest` draws exactly what the draft did in the fake at
  `0b5a98a`.

## Further Notes

- The exploration lived in scratch directories passed to
  `make love-capture`, each with a copy of the harness and of `files/`.
  One copy called `setDefaultFilter("nearest", "nearest")` in
  `love.load`, and another prepended the call to every program. Nothing
  of them is committed. None of them ran `make test` or `make e2e`, or
  wrote a LÖVE save directory.
- Why the call matters at all, given that it changes no captured pixel:
  the real game draws its canvas scaled to the window with a call that is
  not on the fake's list (design.md: "the fake reads the canvas and needs
  nothing more"). A canvas created under `linear` blurs that upscale. The
  order rule in design.md is the only guard against it, and plans 2–3
  should keep `setDefaultFilter` as the first graphics call of
  `love.load`.
