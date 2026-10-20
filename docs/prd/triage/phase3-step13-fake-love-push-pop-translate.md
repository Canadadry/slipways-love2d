---
title: "fake_love moves drawing with push, pop and translate exactly as real LÖVE 11.5 does"
description: "The Lua game needs a camera offset, the PICO-8 camera, but the fake LÖVE has no push, pop or translate, and nobody has measured how real LÖVE composes a translation with points, rectangles and image draws."
status: needs-triage
---

## Problem Statement

After plan steps 6 to 11, and step 12 under way, `tmp/fake_love` draws
points, rectangles and image pieces at the coordinates the game passes.
The C game draws most of the screen through a camera: `scroll.c` and
`message_box.c` set a PICO-8 `camera(x, y)` offset, always an integer
(floored fixed-point), and every later draw is shifted by it. The Lua game
of plans 2–3 will do this with `love.graphics.push`, `translate` and
`pop`, the three functions design.md's list already names. The fake has
none of them: a call fails as an unlisted function.

Real LÖVE does not simply add the offset to each coordinate in double
precision. It keeps the transform as a single-precision matrix, applies
it to each vertex on the CPU in single precision, then runs the vertices
through the projection pipeline that steps 7 and 8 modelled. Near the
1/512 ties of those steps, this order of roundings decides which pixel
lights. A fake that adds in double would light another pixel than real
LÖVE for some accepted coordinates, and nobody would notice: the e2e
replays compare the fake with the C game, never with real LÖVE.

Six other questions were open:

- **Fractional translations.** Does `translate(0.5, 0.5)` then
  `points(x, y)` light what `points(x + 0.5, y + 0.5)` lights? And what does
  a fractional offset do to rectangles and image draws?
- **The stack.** How deep can `push` go? What does `pop` do with nothing
  pushed? What do `push("all")` and `push("transform")` save?
- **Frames.** Is the transform reset each frame, and is the stack?
- **Canvases.** Is a translation made before `setCanvas` still in force
  after it?
- **`clear`.** Does a translation move or shrink it?
- **Bad arguments.** What does LÖVE accept that the game must not use?

## Solution

`love.graphics.push()`, `love.graphics.pop()` and
`love.graphics.translate(dx, dy)` join the fake, so `love.graphics`
holds 12 functions from `main.lua` on. The fake keeps a stack of integer
offsets:

- `translate` adds to the current offset;
- `push` saves a copy of it;
- `pop` brings the saved offset back.

`points`, `rectangle` and `draw` apply the current offset exactly as real
LÖVE 11.5 does on Mesa llvmpipe: in single precision, before the
projection and snapping rules of steps 7 and 8. `clear` ignores it. As in
LÖVE, the current offset goes back to zero right before each frame's draw
step, while the stack carries over from frame to frame.

The fake accepts only what the game needs and what was measured to be
exact:

- `push()` and `pop()` with no arguments;
- `translate` with two integers;
- an offset that stays below 2^20 in magnitude.

It refuses fractional translations. Real LÖVE draws them, but a
fractional offset turns integer rectangles into pixel-centre ties and
integer image draws into blended colours outside the palette (measured).
The game's camera is an integer anyway.

One capture program, `translate_stack`, pins all of this in real LÖVE on
every CI push. It covers:

- `clear` under a translation;
- the stack limit of 128 and `pop` with nothing pushed;
- the float composition at eight measured tie cases;
- a translation made while the screen is selected;
- nested and negative translations over points, rectangles and draws,
  off the canvas on every side;
- offsets near 2^20.

The fake must match it pixel for pixel in `make test`. design.md gets a
"Transforms" paragraph with the rule for the game.

## User Stories

1. As a plan-2 developer, I want `love.graphics.translate(dx, dy)` in the fake, so that I can port the PICO-8 `camera(x, y)` as a translation by `(-x, -y)`.
2. As a plan-2 developer, I want `love.graphics.push()` and `love.graphics.pop()` in the fake, so that I can draw the world under the camera and the HUD without it in the same frame.
3. As a plan-2 developer, I want `points`, `rectangle` and `draw` to land where real LÖVE 11.5 lands them under a translation, pixel for pixel, so that the fake shows what the player sees.
4. As a plan-2 developer, I want an integer translation to move a point, a rectangle or an image draw by exactly that many pixels, so that `points(x + 0.5, y + 0.5)`, integer rectangles and integer draws keep their step 7, 8 and 11 rules under a camera.
5. As a plan-2 developer, I want the fake to compose a translation with a fractional point or rectangle coordinate in single precision, as LÖVE does, so that even a coordinate near a 1/512 tie lights the pixel LÖVE lights.
6. As a plan-2 developer, I want nested translations to add up, so that a panel drawn inside a scrolled view is offset by both.
7. As a plan-2 developer, I want `pop` to restore the offset of the matching `push`, so that the HUD after `pop` is drawn untranslated.
8. As a plan-2 developer, I want `push` and `pop` to leave the current colour and the selected canvas alone, as LÖVE's default push does, so that a colour set inside a push stays set after the pop.
9. As a plan-2 developer, I want the offset to stay in force across `setCanvas` switches, including a switch to the screen, as in LÖVE, so that I can set the camera before choosing the target.
10. As a plan-2 developer, I want `clear` to fill the whole canvas whatever the offset, as in LÖVE, so that clearing under a camera never leaves a border.
11. As a plan-2 developer, I want the current offset reset to zero before every draw step, as LÖVE's `love.run` does with `origin()`, so that a camera left set in one frame does not shift the next frame's drawing.
12. As a plan-2 developer, I want the stack kept across frames, as in LÖVE, so that a `push` without a `pop` fails in the fake at the same call as in LÖVE.
13. As a plan-2 developer, I want the 129th nested `push` refused with LÖVE's message, so that a missing `pop` shows up after 128 leaks, exactly as in LÖVE.
14. As a plan-2 developer, I want a `pop` with nothing pushed refused with LÖVE's message, so that an extra `pop` shows up at once.
15. As a plan-2 developer, I want a fractional translation refused naming the argument, so that I never shift rectangles onto pixel-centre ties or blend image texels into colours outside the palette.
16. As a plan-2 developer, I want a translation with a string, a NaN, an infinity, a missing or an extra argument refused, though LÖVE accepts several of them, so that a bug in the camera code fails where it happens.
17. As a plan-2 developer, I want `push("all")`, `push("transform")`, `push(nil)` and `pop` with arguments refused, so that the fake implements exactly the form whose behaviour is measured and modelled.
18. As a plan-2 developer, I want a translation whose total offset would reach 2^20 refused and the offset left unchanged, so that every composed coordinate stays exact and far from llvmpipe's 2^23 wrap-around.
19. As a plan-2 developer, I want a point, rectangle or draw whose translated position reaches 2^20 refused, with "after translate" in the message, so that the argument rules of steps 7, 8 and 11 still hold once the offset is applied.
20. As a plan-2 developer, I want a refused `push`, `pop` or `translate` to change nothing, so that an error caught by `pcall` leaves the stack and the offset as they were.
21. As a plan-2 developer, I want `push`, `pop` and `translate` to work from `love.load` and `love.update` too, with LÖVE's timing (an offset set in `love.load` holds in the first `love.update`, an offset set in `love.draw` holds in the next `love.update`), so that the fake never differs from LÖVE when a frame starts.
22. As a plan-2 developer, I want `love.graphics` to stay nil in `conf.lua` and to hold exactly 12 functions from `main.lua` on, so that no function exists without its capture.
23. As a plan-2 developer reading design.md, I want a "Transforms" paragraph with the forms, the composition, the stack, the frame timing, the refusals and the rule for the game, so that I do not need plan 1, which will be deleted.
24. As a plan-1 developer, I want a capture program drawing under nested, negative and large translations, off the canvas on every side, so that real LÖVE pins the composition on every CI push.
25. As a plan-1 developer, I want that capture to include the measured cases where a double sum lights another pixel, so that a fake adding in double fails `make test` at once.
26. As a plan-1 developer, I want that capture to show LÖVE's stack depth and its empty-`pop` failure as pixels, so that the limit of 128 is checked against real LÖVE on every push.
27. As a plan-1 developer, I want the stack rules tested directly in a pure C module, so that the depth limit, the restore and the offset bound are checked in milliseconds.
28. As a plan-1 developer, I want the float composition tested directly in the point module with the measured tie cases, so that a double sum fails with a precise message.
29. As a plan-1 developer, I want each misuse checked through the fake's command line with its message, and the frame timing checked over three frames, so that the error contract and the per-frame reset are pinned.
30. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
31. As the repository owner, I want the CI love-capture job green with the new program, so that the PNG is proven to be what real LÖVE draws.
32. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
33. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

How it was measured: `make love-capture LOVE_CAPTURE_DIR=<scratch>`, with
a copy of the real harness and probe programs. The probes wrote their
results to a scratch file and read canvases back with
`Canvas:newImageData()`. One probe ran its own copy of the harness over
four frames. Candidate cases were found by a scratch C search that used
the fake's single-precision pipeline from steps 7 and 8. Everything ran
on LÖVE 11.5.0 and Mesa 25.2.8 llvmpipe (LLVM 20.1.2). These results are
the expected answers of this step's tests. The committed PNG remains the
proof.

- **Composition with points: single precision.** The translated
  coordinate is `(float) x + (float) dx`, added in single precision. It
  then goes through the projection, viewport and 1/256 snap of the Points
  rule, unchanged.
  - Sixty cases were found where this float sum and a double sum
    `(float) (x + dx)` light different columns: translations 37, −91,
    100, 3 and −150, and also 0.5, 0.25, 0.1, 1/3 and −0.7. All lie
    within 1/512 of an integer once translated.
  - Real LÖVE matched the float sum in **60 of 60** cases and the double
    sum in 0.
  - `love.graphics.transformPoint` agrees: `translate(0.1, 0.2)` gives
    `0.10000000149011612`, the float.
- **Composition with rectangles: the far edge is `(x + w) + dx`.** LÖVE
  adds `x + w` in single precision, then translates each vertex:
  - near edge `(float) x + (float) dx`;
  - far edge `((float) x + (float) w) + (float) dx`;
  - each sum in single precision, then the span rule of step 8.

  Twenty-four cases were found where `(x + dx) + w` fills another span,
  including a negative width and a clipped span. Real LÖVE matched the
  rule above in **24 of 24** and the other order in 0.
- **`translate(0.5, 0.5)` then `points(x, y)` with integer x and y lights
  exactly what `points(x + 0.5, y + 0.5)` lights.** On a grid from −3 to
  162 by −3 to 122: 7 680 pixels lit, 0 differences. These sums are
  exact in float, so the float rule above predicts it.
- **Integer translations are exact sums everywhere.** These were drawn
  both ways, once under the translations and once with the offsets added
  by hand, and gave 0 differing pixels each time:
  - nested `push`/`translate`/`pop` over points (2 561 lit);
  - over rectangles (516 lit);
  - over image draws, negative and clipped on each side;
  - offsets up to 2^20 − 28 cancelled by coordinates near −2^20.

  The whole draft capture below was also drawn both ways: its two PNGs
  are byte-identical.
- **Fractional translations break the rules of the other steps.**
  - `translate(0.5, 0.5)` with integer rectangles puts every edge on a
    pixel centre, the tie that step 8 tells the game to avoid.
  - Image draws under a fractional offset, compared with the integer
    draw:
    - 0.5 differs on 120 pixels, with blended bytes (`36 32 64` where the
      texel is `80 64 128`);
    - 0.25 differs on 120 pixels;
    - −0.5 differs on 130 pixels;
    - 1/512 differs on 37 pixels (`127` instead of `128`);
    - 1/1024 gives the same pixels.
- **The stack.**
  - 128 nested `push` calls succeed and the 129th fails with
    `Maximum stack depth reached (more pushes than pops?)`.
  - A `pop` with nothing pushed fails with `Minimum stack depth reached
    (more pops than pushes?)`.
  - `push()`, `push(nil)`, `push("transform")` and `push("all")` are
    accepted. Any other string or a number fails with `Invalid graphics
    stack type '<v>', expected one of: 'all', 'transform'`.
  - Extra arguments to `pop` are ignored.
- **What the default push saves: the transform only.**
  - After `setColor(red)`, `push()`, `setColor(green)`,
    `setCanvas(other)`, `translate(5, 5)` and `pop()`: the colour is
    green, the canvas is `other`, and the offset is back to 0.
  - With `push("all")`, the colour comes back to red and the canvas to
    the one selected before.
- **Arguments.**
  - `translate()` and `translate(1)` fail with `bad argument #… (number
    expected, got no value)`, and `translate(1, nil)` fails the same way.
  - `translate("1", "2")` is accepted, through string coercion.
  - `translate(1, 2, 3)` is accepted and ignores the third argument.
  - `translate(0/0, 0)` and `translate(1/0, 0)` are accepted and turn
    the transform into NaN or infinity.
- **Canvases.**
  - An offset set before `setCanvas(other)` is still in force after it,
    after `setCanvas()` and after switching back. The transform belongs
    to the graphics state, not to a canvas.
  - `clear` under `translate(50, 70)` fills the whole canvas: 0
    differing pixels from an untranslated `clear`.
- **Frames.** The probe harness did `translate(7, 9)` and `push()` in
  `love.load`, then `push()` and `translate(2, 3)` in every `love.draw`,
  and `translate(1, 1)` in every `love.update`. It saw:
  - in the first `love.update`, an offset of (7, 9) and a depth of 1;
  - at the start of every `love.draw`, an offset of (0, 0), with the depth
    growing 1, 2, 3, 4;
  - in every later `love.update`, an offset of (2, 3), the one left by
    the previous `love.draw`.

  So LÖVE's `love.run` resets the current transform (`origin()`) right
  before the draw step, and never touches the stack. A frame ending with
  a non-empty stack raises no error. The leaked pushes add up until the
  129th fails, which the probe hit after 4 frames.

### Decisions

- **Forms: `push()`, `pop()`, `translate(dx, dy)` only.**
  - `push` and `pop` take no argument. Anything passed is refused,
    including `"transform"`, `"all"` and a trailing nil.
  - `push("all")` would also have to save and restore the colour and the
    canvas, which the game does not need. `push("transform")` is the same
    as `push()` and adds nothing.
  - `translate` takes exactly two Lua numbers. Strings are not coerced.
    A third argument, NaN and the infinities are refused, though LÖVE
    accepts them.
- **Offsets are integers.** Each of `dx` and `dy` must be an integer of
  magnitude below 2^20. A fraction is refused, because LÖVE turns integer
  rectangles into pixel-centre ties and integer draws into blended texels
  (measured above). The game's camera is a floored integer, as in the C
  game. A program that wants half-pixel points keeps step 7's rule,
  `points(x + 0.5, y + 0.5)`, and never uses `translate(0.5, 0.5)`.
- **The offset stays below 2^20.** A `translate` that would bring the
  current offset to 2^20 or more in magnitude, on either axis, is refused
  and changes nothing. All offsets are then exact in single precision,
  and every sum stays exact or rounds as LÖVE rounds it.
- **The composition is LÖVE's, in single precision.** The offset is
  applied the same way in all three draw functions:
  - `points`: each coordinate becomes `(float) x + (float) offset`;
  - `rectangle`: the near edge becomes `(float) x + (float) offset`, and
    the far edge `((float) x + (float) w) + (float) offset`;
  - `draw`: the position becomes `x + offset`, an exact integer sum.

  Every sum is a separate single-precision rounding. The result then
  goes through the unchanged rules of steps 7, 8 and 11. With offset 0,
  every result is the same as before this step.
- **Translated positions keep the 2^20 bound.**
  - The argument checks of `points`, `rectangle` and `draw` stay as they
    are, with the same messages. A coordinate, x, y, w and h are each
    below 2^20.
  - Then the translated position is checked against 2^20 as well:
    - each `points` coordinate;
    - `rectangle`'s near edge on each axis (its far edge then stays below
      3 × 2^20, far from the 2^31 limit of step 8);
    - `draw`'s x and y.

    A failure gives `argument N is out of range after translate (|v| >=
    2^20)`, or `element N …` for the table form of `points`.
  - `points` still checks every point before drawing any.
- **The stack holds 128 saved offsets.** Like LÖVE:
  - the 129th nested `push` is refused with LÖVE's message;
  - a `pop` at depth 0 is refused with LÖVE's message;
  - `push` saves a copy of the current offset, and `pop` makes the saved
    offset current again;
  - neither touches the colour or the canvas.
- **Frames, as LÖVE's `love.run`.**
  - The run starts at offset 0 with an empty stack.
  - Right before each frame's draw step, the current offset goes back to
    0 and the stack is left as it is. This happens whether or not the
    game defines `love.draw`, and whatever is selected.
  - The offset set in `love.load` holds through the first `love.update`,
    and the one left by `love.draw` holds through the next
    `love.update`.
  - No check at the end of a frame: a leaked `push` fails at the same
    call as in LÖVE, the 129th nesting. This is faithful, not stricter,
    because a replay of more than 128 frames already exposes a leak of
    one push per frame.
- **The screen and `clear`.**
  - With the screen selected, `push`, `pop` and `translate` still change
    the state, so a translation made before `setCanvas(canvas)` applies to
    the canvas.
  - `clear` ignores the offset and fills the whole selected canvas.
- **Capture programs must not change the transform at their top level.**
  The real harness loads a program inside `love.draw`, after LÖVE's
  `origin()`, while the fake harness loads it in `love.load`, before the
  fake's reset. A translation at the top level would therefore hold in
  real LÖVE and be reset in the fake. The harnesses stay as they are, and
  design.md's capture paragraph states the rule.
- **Messages**, prefixed `love.graphics.push: `, `love.graphics.pop: ` or
  `love.graphics.translate: `:
  - push and pop: `takes no arguments, not N` (push's message names
    `push("all")` as unsupported), `Maximum stack depth reached (more
    pushes than pops?)`, `Minimum stack depth reached (more pops than
    pushes?)`;
  - translate: `takes 2 numbers, dx and dy, not N arguments`, `argument N
    is a <type>, not a number`, `argument N is not finite`, `argument N is
    not an integer`, `argument N is out of range (|v| >= 2^20)`, and `the
    offset would reach 2^20` naming the axis and the value;
  - points, rectangle and draw: the new `out of range after translate`
    message above.

  The exact wording is the implementer's. Tests match the prefix and the
  key words, and LÖVE's two stack messages verbatim.

### Modules

- **Transform** (new, pure C, no Lua).
  - A fixed-size stack of integer offsets: the current offset and up to
    128 saved ones. It allocates nothing.
  - Its operations:
    - initialise, giving offset 0 and depth 0;
    - push, which fails at depth 128;
    - pop, which fails at depth 0;
    - translate by two integers, which fails when either total would
      reach 2^20;
    - reset the current offset, leaving the stack;
    - read the current offset;
    - read the depth.
  - A failed operation changes nothing. Checking an argument for finite,
    integer and range stays with the binding, through the step 11
    position rule (the image rules' position check), which is already the
    rule "an integer of magnitude below 2^20".
- **Point** (existing). The pixel finder and the span finder take the
  axis offset and compose in single precision as decided above. The
  offset is an integer below 2^20. The span's input asserts are
  re-stated for translated edges. A predicate tells whether a coordinate
  plus an offset is still accepted. Callers pass 0 where no transform
  applies, and the results then do not change.
- **Graphics binding** (existing).
  - It gains `push`, `pop` and `translate`, reaching the transform stack
    the way it reaches the canvas store.
  - `points`, `rectangle` and `draw` read the current offset, check the
    translated positions, then call the point module or the canvas's
    texel copy with composed values.
  - `love.graphics` holds 12 functions.
- **Host** (existing).
  - It owns the transform stack, which lives as long as the host, and
    passes it to the graphics binding.
  - In each frame, it resets the current offset after `love.update` and
    before the draw step, whether or not `love.draw` exists.
- **Canvas** (existing): unchanged. It receives composed integer
  positions.
- **Makefile.**
  - The transform module joins the fake's host sources.
  - Its unit test joins the unit list and `FAKE_SIDE_BINARIES`.
  - The new capture check needs no rule (H-12-2).
  - No new CLI target.

### Capture program of this step

`translate_stack` uses only functions from steps 6 to 11 plus the three
new ones. It does not change the transform at its top level. Every point
and rectangle in it follows design.md's rules, except the eight measured
tie cases, which are there to catch a double sum. It was captured in real
LÖVE, and the result was byte-identical to the same drawing with the
offsets added by hand.

- The tie rows hold the expected columns: rows 100 to 103 light columns
  2, 3, 3 and 3. A double sum lights 3, 4, 2 and 4.
- Rows 105 to 108 fill columns 69–150, 16–45, 7–17 and 0–47. The other
  order of sums fills 69–149, 17–45, 6–17 and 0–48.
- Row 118 shows a 128-pixel bar for the depth, and the 4-pixel marker at
  column 140 for the refused empty `pop`.
- 621 pixels differ from the background.

The draft, from the measurement:

```lua
local data = love.image.newImageData(6, 4)
for y = 0, 3 do
    for x = 0, 5 do
        data:setPixel(x, y, (40 + 30 * x) / 255, (60 * y) / 255, 200 / 255, (x + y) % 4 == 0 and 0 or 1)
    end
end
local image = love.graphics.newImage(data)
local whole = love.graphics.newQuad(0, 0, 6, 4, 6, 4)
local part = love.graphics.newQuad(2, 1, 3, 2, 6, 4)

-- {translate x, point x}: the float sum lights another column than the double sum.
local point_cases = {
    {37, -33.998045563697815},
    {-91, 95.001953840255737},
    {3, 0.0019544362994565745},
    {100, -95.998046159744263},
}
-- {translate x, x, w}: the far edge is (x + w) + translate, not (x + translate) + w.
local rectangle_cases = {
    {37, 32.157488867645952, 81.344477964683847},
    {37, 9.4861900185299888, -29.984235350377077},
    {3, 15.092026511109584, -11.590072818471622},
    {-91, 20.130568780484534, 119.37138868644311},
}

return function(canvas)
    local graphics = love.graphics
    graphics.push()
    graphics.translate(50, 40)
    graphics.clear(0.1, 0.2, 0.3, 1)
    graphics.pop()

    local pushed = 0
    while pcall(graphics.push) do
        pushed = pushed + 1
        if pushed > 200 then break end
    end
    for _ = 1, pushed do
        graphics.pop()
    end
    graphics.setColor(1, 1, 0)
    graphics.rectangle("fill", 0, 118, pushed, 1)
    if not pcall(graphics.pop) then
        graphics.rectangle("fill", 140, 118, 4, 1)
    end

    graphics.setColor(1, 1, 1)
    for row, case in ipairs(point_cases) do
        graphics.push()
        graphics.translate(case[1], 0)
        graphics.points(case[2], 99 + row + 0.5)
        graphics.pop()
    end
    for row, case in ipairs(rectangle_cases) do
        graphics.push()
        graphics.translate(case[1], 0)
        graphics.rectangle("fill", case[2], 104 + row, case[3], 1)
        graphics.pop()
    end

    graphics.setCanvas()
    graphics.translate(3, 3)
    graphics.setCanvas(canvas)

    graphics.setColor(1, 0.5, 0)
    for k = -6, 40 do
        graphics.points(k * 4 + 0.5, k % 7 - 5 + 0.5, k * 4 + 1.5, 20 + 0.5)
    end
    graphics.points(-3.5, 10.5, 156.5, 10.5, 157.5, 11.5, 10.5, -3.5, 10.5, 117.5)

    graphics.push()
    graphics.translate(20, 0)
    graphics.setColor(0, 1, 0.5)
    graphics.rectangle("fill", 0, 25, 10, 4)
    graphics.push()
    graphics.translate(-40, 10)
    graphics.rectangle("fill", 0, 25, 30, 3)
    graphics.rectangle("fill", 170, 25, 10, 3)
    graphics.pop()
    graphics.rectangle("fill", 15, 25, 5, 8)
    graphics.pop()
    graphics.rectangle("fill", 0, 45, 4, 4)

    graphics.setColor(1, 1, 1)
    graphics.push()
    graphics.translate(-30, 50)
    graphics.draw(image, whole, 40, 0)
    graphics.draw(image, part, 50, 0)
    graphics.draw(image, whole, 25, 2)
    graphics.draw(image, whole, 60, -54)
    graphics.translate(2 ^ 20 - 1, -(2 ^ 20 - 1))
    graphics.draw(image, whole, -(2 ^ 20 - 1) + 70, 2 ^ 20 - 1)
    graphics.points(-(2 ^ 20 - 1) + 80.5, 2 ^ 20 - 1 + 0.5)
    graphics.rectangle("fill", -(2 ^ 20 - 1) + 85, 2 ^ 20 - 1, 3, 2)
    graphics.pop()
    graphics.draw(image, whole, 153, 113)
    graphics.draw(image, part, 70, 80)
end
```

What it pins:

- **`clear` under a translation**: the whole canvas becomes `1A334C`.
- **The stack**: depth 128 (the bar), and the refused `pop` (the marker).
  The fake must refuse the 129th push without changing anything, or the
  bar comes out wrong.
- **The float composition** for points and for the rectangle's far edge
  (rows 100 to 108).
- **A translation made with the screen selected** applies after
  `setCanvas(canvas)`.
- **Nested and negative offsets**, restored by `pop`, over points,
  rectangles and both quads.
- **Clipping** off the left, right, top and bottom.
- **An offset of 2^20 − 28** cancelled by coordinates near −2^20. It
  stays within the 2^20 bound, since the offset is 2^20 − 28.
- **Colour after `pop`**: the colour set inside a push stays set.

If the PNG disagrees with this section, the PNG wins and design.md
follows it.

### Conventions written into `docs/love-port/design.md`

- A **"Transforms"** paragraph after "Images" (and after step 12's
  "Image files", H-12-4). It holds:
  - the three forms;
  - the integer offsets and the 2^20 bound on the offset and on
    translated positions;
  - the single-precision composition, with the rectangle's far-edge
    order;
  - the stack (128, LÖVE's messages, offset only, colour and canvas
    untouched);
  - the frame timing (reset before the draw step, stack kept, no
    end-of-frame check);
  - the screen and `clear`;
  - the refusals and why: fractions make ties and blended texels, and
    `push("all")`, coercion and NaN are not modelled;
  - **the rule for the game**:
    - port `camera(x, y)` as `push()`, `translate(-x, -y)`, …, `pop()`,
      with the integer camera;
    - keep `points(x + 0.5, y + 0.5)`, integer rectangles and integer
      draws under it;
    - never translate by a fraction;
    - pair every `push` with a `pop` in the same frame;
  - the capture `translate_stack`.
- The function list's "Drawing" line reads `push()`, `pop()`,
  `translate(dx, dy)` with integer offsets.
- "Images" changes in two places:
  - "`love.graphics` holds exactly …" now names the 12 functions;
  - `draw`'s "two integers of magnitude below 2^20" adds "also once
    translated".
- "Real-LÖVE captures" gains one sentence: a capture program never
  changes the transform at its top level, and why.

## Implementation Chunks

1. **The transform stack and the float composition.**
   - The pure transform module and its unit test.
   - The point module's offset parameter and its accepted predicate, with
     the measured tie cases in the point unit test.
   - Every existing caller passes 0, so nothing visible changes yet.
   - Stories 5, 13, 14, 18, 20, 27, 28.
2. **`push`, `pop` and `translate` in the fake.**
   - The three bindings.
   - The composed checks and positions in `points`, `rectangle` and
     `draw`.
   - The host owning the stack and resetting it before the draw step.
   - The five key lists updated.
   - The CLI log game and its test.
   - Stories 1–4, 6–12, 15–17, 19, 21, 22, 29.
3. **Capture, docs, proof.**
   - The program and its PNG from `make love-capture` (git diff shows only
     the new pair).
   - The comparison passing in `make test`.
   - design.md, the timing, the CI run, the step note.
   - Stories 23–26, 30–33.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the module's returned offsets and verdicts, the pixel or span a
  coordinate gives, the binary's exit status, stderr and state file, the
  capture comparison's verdict. No test reaches into the Lua state or a
  module's internals.
- **Transform, unit test** (new, in the unit list, about 1 ms).
  - Translations add.
  - A push keeps the offset, and a translate after it is undone by the
    pop.
  - Nested pushes restore in order.
  - The 128th push is accepted and the 129th refused with the state
    unchanged.
  - A pop at depth 0 is refused.
  - A translate reaching 2^20 on either axis is refused with the offset
    unchanged, and 2^20 − 1 is accepted.
  - A reset zeroes the current offset and keeps the depth and the saved
    offsets: a pop after a reset brings back the saved offset.
- **Point, unit test** (existing).
  - With offset 0, every existing case is unchanged.
  - The four point cases and four rectangle cases of the capture give
    LÖVE's columns and spans. A failing assert says a double sum or the
    `(x + dx) + w` order was probably used.
  - Integer offsets move integer-plus-half points and integer spans by
    exactly the offset, on both axes.
  - The accepted predicate refuses a translated coordinate at 2^20.
- **Graphics binding, CLI test.**
  - The five tests that list `love.graphics`'s keys (graphics, points,
    rectangle, set_color, image_draw) expect the 12 names.
  - A new fixture game, `transform_log`, runs from the existing
    `test_fake_love_set_color` target. That target is the lightest CLI
    check (about 16 ms), and the colour and the transform are the same
    kind of drawing state, kept across frames and canvases. No new CLI
    target is added, to keep `make test` within its gate.
  - The game logs through `pcall` and `debug_state()`:
    - each misuse with its message:
      - `push(nil)`, `push("all")`, `push("transform")`, `pop(1)`;
      - `translate()`, `translate(1)`, `translate(1, 2, 3)`,
        `translate("1", "2")`, `translate(1, nil)`;
      - `translate(0.5, 0)`, `translate(0, 0/0)`, `translate(1/0, 0)`,
        `translate(2^20, 0)`;
      - a translate reaching a total of 2^20;
      - `points`, `rectangle` and `draw` out of range only after
        translate, including the table form of `points`;
    - the 129th push and an empty pop, with LÖVE's messages;
    - the frame timing over `three_frames.json`, probed through the offset
      bound. A `pcall` of `translate(2^20 − v, 0)`, undone when it
      succeeds, tells whether the current offset is below v. Each frame
      logs:
      - the offset seen in `love.update`: what `love.load` left first,
        then what the previous `love.draw` left;
      - the offset seen at the start of `love.draw`: 0;
      - the free depth, falling by one per frame because each
        `love.draw` leaks one `push`.
    - a translate made with the screen selected, still in force after
      `setCanvas`;
    - `love.graphics` keys exactly the 12 names, and nil in `conf.lua`.
  - The test asserts the whole log, as the existing log tests do.
- **The capture comparison** is the test of the composition:
  `translate_stack` passes in `make test`, and every earlier capture still
  passes, with offset 0 everywhere.
- **Prior art:**
  - `test_fake_love_set_color` and `set_color_log` (log game, whole-log
    assert);
  - `test_fake_love_image_draw` (misuse log of the draw functions);
  - `test_fake_love_point` (pure pipeline cases with measured values);
  - `test_fake_love_image_rules` (pure rules module with verdicts).
- **Proofs recorded in the step note** (Step 3.13 in plan 1):
  - `make love-capture` writes the new PNG and leaves every other file of
    `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green (run
    id);
  - `make test` before and after, with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 12's last commit,
    H-12-3) and against `2e7215c`. Both paired medians must be ≤ 3 %
    (22 pooled rounds if either exceeds it). Record the plain and paired
    medians, spreads, rounds, cores, and the slowest new check's own
    time.
  - The new work is small:
    - one unit binary (about 1 ms);
    - one capture check (about 200 draw calls, a few milliseconds);
    - one more fake process inside an existing CLI target (a few
      milliseconds).

    If the gate still fails, design.md rule 5 applies.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `push("all")`, `push("transform")`, `origin`, `scale`, `rotate`,
  `shear`, `applyTransform`, `replaceTransform`, `transformPoint`,
  `inverseTransformPoint`, `getStackDepth` and Transform objects.
- Fractional translations, and translations through NaN or infinity.
- Any check at the end of a frame on the stack depth.
- `setDefaultFilter`: step 14. The combined capture of step 15.
- The capture itself (canvas to palette numbers) and the fatal-error
  catalogue: steps 16 and 17.
- Changing either capture harness.
- Audio, always.

## Hypotheses

Plan step 12 (`newImageData(file)` from a PNG) is being implemented while
this PRD is written. Its PRD was read, and nothing of its code is. Before
starting, the implementer checks each item below against the committed
step 12. When one differs, adapt the names, the placement and the one
dependent test line, not the decisions. This step's capture and
composition depend on nothing from step 12.

- **H-12-1**: step 12 leaves the graphics binding, the point module, the
  canvas module and the host's frame loop unchanged. `love.graphics`
  still holds the 9 functions of step 11, and the five key-list tests
  still expect them.
- **H-12-2**: step 12 keeps the Makefile's capture registration: a
  `.lua` and `.png` pair at the top level of `tools/love_capture/`
  becomes its own `love-capture-check-<name>` by itself, and
  `make love-capture` runs only top-level programs. Its `files` fixture
  directory and the two `files` symlinks do not affect this step. A
  scratch `LOVE_CAPTURE_DIR` copy used to re-run all captures must copy
  `files` too.
- **H-12-3**: step 12's last commit is this step's "before" timing
  baseline, and its note (Step 3.12) records its timings. Its paired
  ratio against `2e7215c` is not known here. Step 11 ended at +2.24 %,
  so this step adds no new CLI target.
- **H-12-4**: step 12 adds an "Image files" paragraph between "Image
  data" and "Images" in design.md, edits the function list's "Images"
  line, and adds one sentence about fixtures to "Real-LÖVE captures".
  This step's "Transforms" paragraph goes after "Images", and its
  capture-paragraph sentence goes after step 12's.
- **H-12-5**: step 12 adds its own unit target for the PNG decoder and
  extends `test_fake_love_image`, not `test_fake_love_set_color`. So
  `test_fake_love_set_color` is still about 16 ms when this step extends
  it.
- **H-12-6**: step 12 changes neither harness's Lua. The real harness
  still loads the program inside `love.draw`, and the fake harness in
  `love.load`, which is what the top-level rule above relies on.
- **H-12-7**: step 12 changes the host only to pass the disk to the
  image binding. The install order and the graphics binding's install
  signature are as step 11 left them, so adding the transform stack to
  that signature is this step's only change there.

## Further Notes

- The exploration files lived in a scratch directory passed to
  `make love-capture`, with a copy of the harness. Nothing of them is
  committed. The multi-frame probe used its own harness copy in a second
  scratch directory. Neither ran `make test` or `make e2e`, and neither
  wrote a LÖVE save directory.
- How the tie cases were found: a scratch C search over coordinates
  near `n + 1/512` (columns) and near pixel-centre ties (rectangle far
  edges), with integer and fractional offsets. It kept the cases where
  the float and double sums, or the two far-edge orders, give different
  pixels. Real LÖVE matched the float rules in all 84. The eight in the
  capture use integer offsets only, since the fake refuses fractions.
- Why LÖVE behaves this way: LÖVE 11.5 applies the transform on the CPU
  to each vertex in single precision (`x' = m0·x + m4·y + m12`, where
  `m0 = 1` and `m4 = 0` are exact), and builds the rectangle's corners as
  `x + w` in single precision before transforming them. The measurement,
  not this reading of LÖVE's source, is what the step relies on.
