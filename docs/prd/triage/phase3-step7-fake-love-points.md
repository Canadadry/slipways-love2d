---
title: "fake_love draws love.graphics.points exactly where real LÖVE 11.5 on Mesa does"
description: "The Lua game will draw circles and lines pixel by pixel with love.graphics.points, but the fake LÖVE has no points and nobody knows which pixel a point lights in real LÖVE 11.5."
status: needs-triage
---

## Problem Statement

After plan step 6, `tmp/fake_love` can create a canvas, select it and
clear it, and `make test` compares every capture program run in the fake
with the PNG real LÖVE 11.5 drew. The Lua game of plans 2–3 draws its
circles and lines pixel by pixel with `love.graphics.points` (design.md,
"How the Lua game draws"), and the fake has no `points`. A game that calls
it fails.

Nothing says which pixel a point lights, either. `points(x, y)` is not
"pixel (x, y)": real LÖVE sends a GL point of size 1 through the vertex
pipeline, Mesa's llvmpipe snaps it to a sub-pixel grid in single precision,
and a point at an integer coordinate lands on the pixel up and left of it.
If the fake guessed (floor, round, `x + 0.5`), every e2e frame of plans 2–3
could be off by one pixel along a circle, and nobody would know whether
the game or the fake was wrong.

## Solution

The fake gains `love.graphics.points`, in the two forms the game needs (a
flat argument list of coordinates, or one flat table of them), drawing
with the current colour, which stays opaque white until step 9 adds
`setColor`. A point lights the pixel real LÖVE 11.5 on Mesa llvmpipe
lights, computed by reproducing the float pipeline measured below, not by
a rounding rule that only looks right. Points off the canvas draw
nothing; coordinates the real pipeline would mangle, non-finite numbers
and every other form are errors naming `love.graphics.points`.

Six capture programs pin the convention against real LÖVE: pixel centres
and integers, a sub-pixel grid, every column and every row at the
fractions where single precision decides, off-canvas and negative
coordinates, and the call forms over a transparent background. Their PNGs
come from `make love-capture`; `make test` compares each with the fake.
The convention goes into `docs/love-port/design.md`, with the rule the
game must follow: to light pixel (x, y), pass `x + 0.5, y + 0.5`.

## User Stories

1. As a plan-2 developer, I want `love.graphics.points` in the fake, so that the Lua game can draw its circles and lines pixel by pixel.
2. As a plan-2 developer, I want `points(x + 0.5, y + 0.5)` to light exactly pixel (x, y) for every pixel of the 160×120 canvas, so that I have one safe way to address a pixel.
3. As a plan-2 developer, I want `points(x, y)` with integer coordinates to light pixel (x − 1, y − 1), as real LÖVE does, so that a game passing integers sees in the fake the same shift it would see in LÖVE.
4. As a plan-2 developer, I want `points(0, y)` and `points(x, 0)` to draw nothing, as in real LÖVE, so that the edge behaviour of integer coordinates matches.
5. As a plan-2 developer, I want any coordinate to land where real LÖVE 11.5 on Mesa lands it, including fractions within a few single-precision steps of a sub-pixel boundary, so that no float coordinate the game computes can differ.
6. As a plan-2 developer, I want points entirely off the canvas (negative, past 160 or 120, up to a million away) to draw nothing and raise no error, so that a circle partly off screen is drawn like LÖVE draws it.
7. As a plan-2 developer, I want points just past the right or bottom edge (160 + 1/1024, 120 + 1/1024) to light the last column or row, as LÖVE does, so that edge pixels match.
8. As a plan-2 developer, I want `points(x1, y1, x2, y2, ...)` with any even number of numbers to draw each point, so that I can draw a span in one call.
9. As a plan-2 developer, I want `points({x1, y1, x2, y2, ...})` with one flat table to draw the same points, so that I can batch a whole circle in one call.
10. As a plan-2 developer, I want `points()` and `points({})` to draw nothing and succeed, as in LÖVE, so that an empty batch needs no special case.
11. As a plan-2 developer, I want ten thousand points in one call to draw all of them, so that a large batch is not truncated.
12. As a plan-2 developer, I want a point drawn twice on the same pixel to leave it as once, so that overlapping spans are harmless.
13. As a plan-2 developer, I want points to draw in opaque white until `setColor` exists, as LÖVE's default colour is, so that the step's captures need no colour function.
14. As a plan-2 developer, I want a point over a transparent pixel to make it opaque white, and the transparent pixels around it to keep their colour and alpha 0, so that transparency matches LÖVE byte for byte.
15. As a plan-2 developer, I want points drawn while no canvas is selected to leave every canvas untouched, as in LÖVE where they go to the screen, so that the canvas the fake reads is right.
16. As a plan-2 developer, I want an odd number of coordinates to raise an error naming `love.graphics.points`, as LÖVE does, so that a forgotten coordinate is caught.
17. As a plan-2 developer, I want a string, boolean, nil or table where a coordinate belongs to raise an error naming `love.graphics.points` and the argument or element, so that the fake is stricter than LÖVE's silent string coercion.
18. As a plan-2 developer, I want NaN and infinite coordinates to raise an error, so that a division by zero in the game is reported instead of drawing nothing.
19. As a plan-2 developer, I want a coordinate of magnitude 2^20 or more to raise an error, so that the fake never accepts a point that real LÖVE's fixed-point overflow would wrap back onto the canvas.
20. As a plan-2 developer, I want the table-of-pairs and per-point-colour forms (`{{x, y}, {x, y, r, g, b, a}}`) to raise an error, so that colour only ever comes from the current colour.
21. As a plan-2 developer, I want a table followed by more arguments, or a table with a hole, to raise an error, so that half-built batches are caught.
22. As a plan-2 developer, I want a call that raises an error to draw none of its points, so that an error leaves the canvas as it was.
23. As a plan-2 developer, I want `love.graphics` to hold exactly `newCanvas`, `setCanvas`, `clear` and `points` after this step, so that no function exists without its capture.
24. As a plan-1 developer, I want a capture program for pixel centres and integer coordinates, so that the basic landing rule is settled by real LÖVE.
25. As a plan-1 developer, I want a capture program sweeping sixteenths and tiny offsets in both axes in isolated cells, so that the snapping rule is settled by real LÖVE.
26. As a plan-1 developer, I want capture programs placing points at every column and every row with the fractions where single precision decides, so that the fake's float pipeline is proven at every position, not at a few.
27. As a plan-1 developer, I want a capture program for off-canvas and negative coordinates near and far from each edge, so that clipping is settled by real LÖVE.
28. As a plan-1 developer, I want a capture program for the accepted call forms, empty calls, duplicates, a large batch, a transparent background and points drawn with no canvas selected, so that each form is proven in one place.
29. As a plan-1 developer, I want each new capture compared with the fake in `make test` as its own parallel check, so that the fast loop proves the convention without getting slower.
30. As a plan-1 developer, I want the landing rule tested directly in a pure-C unit test with values taken from the captures, so that a compiler fusing two float operations is caught with a precise message.
31. As a plan-1 developer, I want each misuse of `points` checked through the fake's command line with its message, so that the error contract is pinned.
32. As a plans-2–3 reader, I want the points convention in `docs/love-port/design.md`, including "pass `x + 0.5, y + 0.5`", so that I do not need plan 1, which will be deleted.
33. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
34. As the repository owner, I want the CI love-capture job green with the new programs, so that the PNGs are proven to be what real LÖVE draws.
35. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
36. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture`, pointed at scratch directories holding a copy of the
harness (calling `program(canvas)`, as step 6 makes it, H-6-4), on LÖVE
11.5.0 with Mesa 25.2.8 llvmpipe, gave these results. They are the
expected answers of this step's programs; the committed PNGs remain the
proof.

- **Default state**: `getColor()` is `1 1 1 1`, `getPointSize()` is 1.
  A point lights exactly one pixel, `FFFFFFFF`, also over a pixel cleared
  to `33669900` (alpha 0); the neighbours keep `33669900`.
- **Landing**: `points(x + 0.5, y + 0.5)` lights (x, y) at the four
  corners and inside. `points(10, 10)` lights (9, 9); `points(160, 60)`
  lights (159, 59); `points(0, 30)` and `points(30, 0)` light nothing.
  In general a point lights column n when n < x′ ≤ n + 1, where x′ is the
  coordinate snapped to 1/256 pixel after a single-precision pipeline;
  the same for rows. A fraction of 1/1024 behaves like 0 (lights n − 1),
  3/1024 and 1/256 like any positive fraction (light n).
- **The pipeline is single precision and position-dependent at the
  boundary.** At c + 1/512 (exactly half a sub-pixel step), whether
  column c or c − 1 lights depends on c: for size 160, c = 0, 1, 5, 6,
  10, 11, 15, 20, … light c, the others c − 1; rows differ from columns
  (size 120: 0 lights nothing, 1 lights 0, 2 lights 2, 70 lights 70). A
  round-to-1/256 in double precision is wrong on 232 of 2 120 random
  points concentrated near those boundaries. The following model,
  prototyped in Python with numpy and checked against every captured
  point (2 067 + 1 547 at 13 fractions over every column and row,
  1 119 + 839 at c + 1/512 ± up to 3 single-precision steps, 2 120
  random points, the final six programs below): zero differences.

  ```
  // prototype of the per-axis rule; size is 160 for x, 120 for y
  v    = (float) coordinate
  s    = 2.0f / (float) size           // the ortho projection's scale
  p    = v * s                         // rounded to float
  ndc  = p + (-1.0f)                   // rounded to float, NOT fused with the product
  win  = fmaf(ndc, size / 2, size / 2) // the viewport: fused, one rounding
  fix  = lrintf(win * 256.0f)          // 8 sub-pixel bits, ties to even
  pixel = floor((fix - 1) / 256)       // lands iff 0 <= pixel < size
  ```

  Fusing the projection's product and sum (or not fusing the viewport)
  gives 18 to 116 wrong pixels per sweep; ties-to-even and ties-down in
  the snap cannot be told apart by any column, ties-up can (264 wrong).
- **Off-canvas**: negative coordinates, `-1/1024`, 160 + 3/1024, 160.4,
  120 + 3/1024, 120.4 and ±10^6 light nothing; 159.99, 160, 160 + 1/1024
  light column 159, likewise row 119.
- **Overflow**: llvmpipe does not clip a point before its fixed-point
  conversion. Coordinates of 2^24 + c, 2^31, 2^32, ±3.4·10^38 wrap onto
  the canvas (column 0, row 0 or (159, 0) were lit in the probes);
  everything below 2^23 in magnitude that was tried stayed off. NaN,
  ±10^39 and ±10^300 (infinite in single precision) light nothing.
- **Forms**: `points(x1, y1, …)`, `points({x1, y1, …})` and
  `points({{x1, y1}, …})` draw the same pixels; `{{x, y, r, g, b, a}}`
  colours each point (`{0, 1, 0, 0.5}` over transparent black gave
  `007F007F`, a step 9 matter). `points()` and `points({})` succeed and
  draw nothing. An odd count, also in a table or a table followed by
  more numbers, fails with `Number of vertex components must be a
  multiple of two` and draws none of the call's points; a boolean, nil or table argument fails with `bad
  argument #1 to '?' (number expected, got …)`; strings are coerced; NaN
  and infinities are accepted and draw nothing visible.
- **No canvas**: points drawn after `setCanvas()` do not reach the
  harness canvas.

### Modules

- **Point rule** (new deep module, pure C, no Lua, in the fake's sources).
  It answers one question per axis: given a coordinate (a double) and the
  canvas size along that axis, which pixel index the point lands on, or
  that it lands outside `0 … size − 1`. It encodes the pipeline above,
  each rounding in its own statement and the viewport step through
  `fmaf`, so that no compiler can fuse or skip a rounding: GCC's
  `-std=c99` already disables contraction, clang only contracts within
  one expression; the unit test catches either. It also asserts at
  compile time that `FLT_EVAL_METHOD` is 0. It also answers whether a
  coordinate is accepted: finite and of magnitude below 2^20 (1 048 576).
  The bound keeps every accepted coordinate eight times below llvmpipe's
  first wrap (2^23) and far above anything the game draws; step 13,
  which adds `translate`, applies the bound and the rule to the
  translated coordinate. Rectangles (step 8) may add their own edge rule
  here.
- **Canvas store** (step 6's module, H-6-1, extended):
  - a current colour of four bytes, `FFFFFFFF` when the store is created
    (LÖVE's default white); step 9 adds the setter. This step has no way
    to change it;
  - plot one pixel of the current target with the current colour; with
    no target it does nothing (the screen is not modelled, as step 6's
    `clear`, H-6-11). Plotting replaces the four bytes, which is what
    LÖVE's alpha blending gives for an opaque colour; blending a
    non-opaque colour is step 9's decision, and the store asserts the
    colour is opaque until then (assert with a message naming step 9's
    blending). The pixel indexes must be inside the canvas (assert).
- **Graphics binding** (step 6's module, H-6-2, extended) gains `points`,
  so `love.graphics` holds exactly `clear`, `newCanvas`, `points`,
  `setCanvas`. Accepted forms:
  - `points(x1, y1, …, xn, yn)`: zero or an even number of arguments,
    each a Lua number (type number, not a numeric string);
  - `points(t)`: exactly one argument, a table whose length (the `#`
    operator, read raw) is even, elements 1 … #t all numbers.

  The binding first checks every coordinate (form, type, accepted by the
  point rule), then plots each point that lands, in order. Any refusal is
  a Lua error starting `love.graphics.points: ` and naming the problem:
  `odd number of coordinates (N)`, `argument K is a <type>, not a
  number`, `element K is a <type>, not a number` (this also catches a
  pairs table, whose elements are tables, and a hole, which reads as
  nil), `a table must be the only argument`, `argument K is not finite`
  (or `element K`), `argument K is out of range (|v| >= 2^20)` (or
  `element K`). The exact wording is the implementer's; the tests match
  the function name, the index and the key word.
- **Nothing else changes**: the host, the comparison command, the
  harnesses, the Makefile rules. A capture program added to
  `tools/love_capture/` with its PNG becomes a `make test` check by
  itself (H-6-3). The point rule joins the fake's plain-C source list,
  so every binary linking the canvas store or the binding links it
  (H-6-8), and the `src/` guard covers it (H-6-5).

### Capture programs of this step

Each is a `tools/love_capture/<name>.lua` returning `function(canvas)`
(H-6-4), using only `points`, `clear` and `setCanvas`; the canvas starts
transparent black (step 6), every lit pixel is `FFFFFFFF`, every other
pixel `00000000` unless said. The point lists are written as loops in
the program, not as literal lists; the expected counts were obtained
from the drafts of these programs run through `make love-capture`.

| program | draws | expected |
|---|---|---|
| `points_landing` | half-integers at the four corners and (80.5, 60.5); integers (10, 10), (20, 10), (160, 60), (80, 120), (0, 30), (30, 0) | 9 pixels: (0, 0), (159, 0), (0, 119), (159, 119), (80, 60), (9, 9), (19, 9), (159, 59), (79, 119) |
| `points_subpixel` | 26 × 26 points, x and y fractions from {k/16, k = 0 … 15} ∪ {1e-7, 1e-6, 1e-4, 1/1024, 3/1024, 1/256, 1/128, 0.5 ± 1e-6, 1 − 1e-6}, point n in its own 3×3 cell at base (3(n mod 53) + 1, 3⌊n/53⌋ + 1) | 676 pixels: per axis, base − 1 when the fraction is 0, 1e-7, 1e-6, 1e-4 or 1/1024, base otherwise |
| `points_columns` | 19 fractions {0, 1/1024, 3/1024, 1/256, 1/4, 1/2, 3/4, 1 − 1/256, 1 − 3/1024, 1 − 1/1024, 1 − 1/512, 1/2 + 1/512} ∪ {1/512 + k·2^-17, k = −3 … 3}; for each, three rows (phases 0, 1, 2), row r at y = r + 0.5 holding x = c + fraction for c = phase, phase + 3, … ≤ 160 | 3 039 pixels, per the model; the 1/512 rows show the position-dependent pattern |
| `points_rows` | the same with the axes swapped: column r at x = r + 0.5, y = c + fraction, c ≤ 120 | 2 279 pixels, per the model |
| `points_offcanvas` | (0, 0), (−0.5, 5), (−1, 10), (−0.25, 15), (−0.75, 20), (−1/1024, 22.5); (159.5, 25.5), (160, 30.5), (159.99, 35.5), (160 + 1/1024, 37.5), (160 + 3/1024, 39.5), (160.4, 40.5); (5.5, −0.5), (10.5, −0.25), (15.5, −0.75); (20.5, 119.5), (25.5, 120), (30.5, 119.99), (35.5, 120 + 1/1024), (40.5, 120 + 3/1024), (45.5, 120.4); ±10^6 and ±(2^20 − 1) on each axis with the other on the canvas, (10^6, 10^6), (−(2^20 − 1), −(2^20 − 1)) | 8 pixels: (159, 25), (159, 30), (159, 35), (159, 37), (20, 119), (25, 119), (30, 119), (35, 119) |
| `points_forms` | `clear(0.2, 0.4, 0.6, 0)`; three points as arguments on row 2.5, three as a flat table on row 4.5 (x = 10.5, 12.5, 14.5); `assert(not pcall(points, 30.5, 4.5, 32.5))` (odd count: refused, draws nothing); `points()`; `points({})`; (20.5, 2.5) twice in one call; a 20 000-number table, i = 0 … 9 999 at (i mod 160 + 0.5, 60 + ⌊i/160⌋ mod 50 + 0.5); `setCanvas()`, (30.5, 2.5), `setCanvas(canvas)` | 8 007 `FFFFFFFF`: (10, 2), (12, 2), (14, 2), (10, 4), (12, 4), (14, 4), (20, 2), rows 60–109 entirely; 11 193 `33669900` |

The exact 1/512 tie (k = 0) is only in `points_columns` and
`points_rows`, where every position is compared; the other programs
avoid it so that their expected pixels can be stated in prose. If a PNG
disagrees with this table, the PNG wins and design.md follows it.

The plan's edge cases: off-canvas and negative coordinates are
`points_offcanvas`; transparent pixels are `points_forms`; nested
translate does not exist before step 13, which adds its capture of
`translate` over `points` (step 15 combines them).

### Conventions written into `docs/love-port/design.md`

A "Points" paragraph after step 6's "Canvas and clear" paragraph (H-6-6):

- the forms accepted and every refusal, with the error prefix;
- the colour: the current colour, opaque white until `setColor`;
- the landing rule in words: a point lights column n when n < x′ ≤ n + 1,
  x′ being the coordinate through LÖVE's single-precision projection and
  Mesa's viewport, snapped to 1/256 pixel; the per-axis prototype above,
  with "each rounding separate, the viewport fused";
- **the rule for the game**: to light pixel (x, y), call
  `points(x + 0.5, y + 0.5)`; integer coordinates light (x − 1, y − 1);
  fractions within 1/512 of an integer are position-dependent and the
  game must not produce them;
- off-canvas points draw nothing; the 2^20 bound and why (llvmpipe's
  fixed-point wrap at 2^23);
- points with no canvas selected touch nothing the fake models.

The function list line for `points` gains its two forms.

### Decisions taken without the plan

- **Reproduce the pipeline, not a rounding rule.** A simpler rule
  ("pixel = ceil(x) − 1") matches every coordinate the game should pass,
  but not 232 of 2 120 random coordinates; the plan wants pixel-for-pixel
  conventions from real LÖVE, and the float model costs a few operations.
- **Two forms only.** The pairs form and the per-point-colour form are
  refused: the game sets one colour before drawing (design.md), and a
  per-point colour would bypass step 9's colour checks.
- **Stricter than LÖVE on types and values.** No string coercion, no
  NaN, no infinity, no coordinate at or beyond 2^20: each is either a
  game bug or a point real LÖVE could wrap onto the canvas.
- **`points()` and `points({})` are accepted** as in LÖVE: an empty
  batch is a natural result of clipping in the game, and refusing it
  would force a special case.
- **Check everything, then draw.** A refused call draws nothing.
- **The colour lives in the canvas store**, with the pixels it is written
  to; `setColor`, `rectangle` and `draw` (steps 8–11) read it there.
- **Misuse is tested in one fixture game**, which calls each misuse
  through `pcall` and returns the messages from `debug_state()`, plus
  one uncaught misuse for the fatal path: one or two processes instead
  of one per case, inside the 100 ms budget.

## Implementation Chunks

1. **Point rule.** The pure-C module and its unit test, in the unit
   list. Stories 2–7, 18, 19 (rule side), 30.
2. **`points` in the fake.** The store's colour and plot, the binding's
   `points` with its forms and refusals, the CLI test with its fixture
   games. Stories 1, 8–17, 20–23, 31.
3. **Captures, docs, proof.** The six programs and their PNGs from
   `make love-capture` (git diff shows only the new files), the six
   comparisons passing in `make test`, design.md, the timing, the CI
   run, the step note. Stories 24–29, 32–36.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the rule's answers, the store's bytes, the binary's exit status,
  stderr and state file, the comparison's verdict. No test reaches into
  the Lua state or the store's layout.
- **Point rule, unit test** (new, pure C, linked with the rule only,
  joins the unit list like the pad test). Values from the captures:
  - size 160: 0.5 → 0, 159.5 → 159, 10 → 9, 160 → 159, 0 → off,
    1/1024 → off, 3/1024 → 0, 159.99 → 159, 160 + 1/1024 → 159,
    160 + 3/1024 → off, 160.4 → off, −0.25 → off, ±10^6 → off;
  - size 120: 119.5 → 119, 120 → 119, 120 + 1/1024 → 119,
    120 + 3/1024 → off;
  - the tie at c + 1/512: size 160, c = 0 → 0, 1 → 1, 2 → 1, 5 → 5;
    size 120, c = 0 → off, 1 → 0, 2 → 2, 70 → 70 (70 is the value
    where both wrong fusions give 69); each failing assert says which
    rounding was probably fused;
  - accepted: 0, −(2^20 − 1), 2^20 − 1; refused: 2^20, −2^20, NaN,
    ±infinity.
- **Canvas store** (step 6's unit test, extended): a plot lights one
  pixel `FFFFFFFF` of the target and leaves the others and the other
  canvas untouched; a plot over a transparent coloured pixel gives
  `FFFFFFFF`; a plot with no target changes no canvas.
- **`points`, CLI test** (new, its own target in `FAKE_LOVE_CHECKS`,
  through `tmp/fake_love` and the shared CLI helper, fixture games and a
  one-frame replay):
  - a game whose `love.load` logs the sorted names of `love.graphics`
    (`clear newCanvas points setCanvas`), then, in `pcall`, each misuse
    with its message: `points(1, 2, 3)`, `points({1, 2, 3})`,
    `points({1, 2}, 3, 4)`, `points("5", "50")`, `points(true, 1)`,
    `points(nil, 1)`, `points({{1, 2}})`, `points({1, nil, 3, 4})`,
    `points(0/0, 1)`, `points(1/0, 1)`, `points(2^20, 1)`,
    `points(1, -2^20)`; then `points()`, `points({})` and a valid call
    with no canvas selected, logged as successes; the log is compared
    byte for byte;
  - a game drawing an odd count uncaught in `love.draw` exits 1 naming
    `love.graphics.points` and `f0`;
  - "a refused call draws nothing": for an odd count it is proven by
    `points_forms` against real LÖVE; for a refusal real LÖVE does not
    make (a valid point then NaN), one fixture program, run by the
    comparison command against a fully transparent PNG written to
    `tmp/` by step 6's test PNG encoder (H-6-7), calls
    `pcall(points, 10.5, 10.5, 0/0, 1)` and must leave the canvas
    transparent. This case joins the points CLI test target.
- **The capture comparisons** are the tests of the convention: each of
  the six programs passes in `make test`.
- **Prior art:** `test_fake_love_pad` for a pure-C fake module;
  `test_fake_love_joystick` and `test_fake_love_files` for fixture games
  logging misuse through `pcall` and `debug_state()`; step 6's canvas
  store test and comparison CLI test (H-6-7).
- **Proofs recorded in the step note:**
  - `make love-capture` writes the six new PNGs and leaves every other
    file of `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green
    (run id);
  - `make test` before and after with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 6's last commit,
    H-6-9) and against `2e7215c`, both medians of paired ratios ≤ 3 %
    (22 pooled rounds if either exceeds it); plain and paired medians,
    spreads, rounds, core count, and the slowest new check's own time
    (budget 100 ms); a check over budget is split before the step closes.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `setColor`, colours other than the default white, blending of
  non-opaque colours, and per-point colours: step 9.
- `setPointSize`, `setPointStyle`, `getColor`, `getPointSize`: not on the
  design's list.
- `rectangle`, images, quads, `draw`, `setDefaultFilter`: steps 8–14.
- `push`/`pop`/`translate` and points under a transform: step 13 (the
  rule's input becomes the transformed coordinate there).
- Drawing to the screen: not modelled; points with no canvas are
  accepted and dropped.
- Named fatal errors for unlisted functions: step 17. The capture file:
  step 16.
- Audio, always.

## Hypotheses

Plan step 6 is being implemented while this PRD is written. Everything
this step assumes about its outcome is listed here; the implementer
checks each against the committed step 6 before starting, and adapts the
names (not the decisions) when one differs.

- **H-6-1**: a pure-C canvas store module owns each canvas's RGBA8
  pixels (160×120, row-major), the current target (a canvas or none) and
  the byte conversion of a colour component; it has a unit test. This
  step adds the current colour and a one-pixel plot to it.
- **H-6-2**: a graphics binding module installs `love.graphics` holding
  exactly `newCanvas`, `setCanvas` and `clear`, after `love.conf` and
  before `main.lua`, as C closures over the store; its errors are Lua
  errors starting with the full function name
  (`love.graphics.clear: …`). This step adds `points` there.
- **H-6-3**: the comparison command (`tmp/love_capture_check <harness
  dir> <program.lua> <program.png>`) runs one capture program in the
  fake and compares all four bytes of every pixel; the Makefile turns
  each stem present in `tools/love_capture/` into its own check in
  `FAKE_LOVE_CHECKS` from a pattern, so adding a `.lua` and its `.png`
  is all a new check needs. If registration is a list instead, the six
  names are added to it.
- **H-6-4**: both harnesses create the 160×120 canvas before loading the
  program, select it, call `program(canvas)`, then unselect it; the
  canvas is transparent black when the program starts.
- **H-6-5**: the nothing-from-`src/` guard runs in `make test` over the
  objects of every fake-side binary and every source under
  `tools/fake_love/`; a new module there is covered by being in the
  fake's source list.
- **H-6-6**: step 6 wrote a "Canvas and clear" paragraph in
  `docs/love-port/design.md`; this step's "Points" paragraph follows it.
- **H-6-7**: step 6 has a CLI test of the graphics binding through
  `tmp/fake_love` and fixture games, and a comparison CLI test with a
  small RGBA PNG encoder in the fake-side test helpers writing expected
  PNGs into `tmp/`. This step reuses the encoder for its one
  comparison fixture and adds its own CLI test target rather than
  growing step 6's.
- **H-6-8**: the Makefile has a plain-C source list for the fake and a
  LuaJIT-flagged list for the bindings; the comparison binary and
  `tmp/fake_love` link every module of the plain-C list they need. The
  point rule joins the plain-C list. Both binaries link `libm` (LuaJIT
  needs it); if not, `-lm` is added for `fmaf` and `lrintf`.
- **H-6-9**: step 6's last code commit is this step's "before" timing
  baseline, and step 6's note records its timings.
- **H-6-10**: `make love-capture` still runs every `.lua` of
  `LOVE_CAPTURE_DIR` through the real harness in its `harness/`
  directory; overriding `LOVE_CAPTURE_DIR` on the command line, as done
  for this PRD's exploration, keeps working.
- **H-6-11**: with no canvas selected, step 6's store makes drawing a
  no-op (its `clear` does nothing); `points` follows it.

## Further Notes

- The exploration programs lived in scratch directories passed to
  `make love-capture` with a copy of the harness; nothing of them is
  committed. The six drafts of the table were captured and checked
  against the model: zero differing pixels.
- The model is the composition of LÖVE's orthographic projection
  (`2/size`, `−1`, in single precision; the transform is the identity
  here) and llvmpipe's viewport and 8-bit sub-pixel snap; it was found
  by testing four fusion variants and three tie rules against the
  captures, and only one variant has zero differences. If a future
  Mesa changes it, the CI love-capture job turns red on the PNGs.
- `points({{x, y, r, g, b, a}})` produced `007F007F` for
  green at alpha 0.5 over transparent black: alpha blending rounds 127.5
  down there, unlike `clear`'s conversion (0.5 → 128). Step 9 should
  capture it before deciding blending.
- Six new checks of one LuaJIT start and at most 10 000 calls each run
  in parallel on the cores the unit tests leave idle; if the gate fails,
  design.md rule 5 applies.
