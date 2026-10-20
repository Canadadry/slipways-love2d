---
title: "fake_love fills love.graphics.rectangle exactly as real LÖVE 11.5 on Mesa does"
description: "The Lua game will draw its spans, bars and boxes with rectangle(\"fill\", …), but the fake LÖVE has no rectangle and nobody has measured which pixels a rectangle covers in real LÖVE 11.5."
status: needs-triage
---

## Problem Statement

After plan step 7, `tmp/fake_love` can create, select and clear a canvas
and light single pixels with `love.graphics.points`, and `make test`
compares every capture program run in the fake with the PNG real LÖVE 11.5
drew. The Lua game of plans 2–3 draws its filled shapes, its circle and
line spans and its boxes with `love.graphics.rectangle("fill", …)`
(design.md, "How the Lua game draws"), and the fake has no `rectangle`. A
game that calls it fails.

Nothing says which pixels a rectangle covers either. A PICO-8
`rectfill(x0, y0, x1, y1)` is inclusive on both ends; LÖVE's
`rectangle("fill", x, y, w, h)` is two GPU triangles that Mesa's llvmpipe
rasterizes with a pixel-centre test, a fill rule for edges that fall
exactly on a centre, and the same single-precision projection and 1/256
sub-pixel snap that step 7 found for points. If the fake guessed ("floor
both ends", "round", "inclusive"), every bar, panel and span of the e2e
frames could be one pixel too wide or too narrow, and nobody would know
whether the game or the fake was wrong.

## Solution

The fake gains `love.graphics.rectangle` in the one form the game needs,
`rectangle("fill", x, y, w, h)` with five arguments, drawing with the
current colour (opaque white until step 9 adds `setColor`). The pixels it
fills are those real LÖVE 11.5 on Mesa llvmpipe fills, computed by
reproducing the measured pipeline — LÖVE's float arguments and float
addition for the far edges, step 7's projection, viewport and snap, then a
pixel-centre test with the top-left fill rule — not by a rounding rule
that only looks right. Off-canvas parts are clipped; zero sizes draw
nothing; negative sizes extend the other way. Any other mode, any extra
argument, any non-number, non-finite or out-of-range value is an error
naming `love.graphics.rectangle`.

Six capture programs pin the convention against real LÖVE: integer
rectangles at every column and row, a sub-pixel grid of edges, every
column and every row with edges at the fractions where single precision
decides, off-canvas and far coordinates, and the call forms over a
transparent background. Their PNGs come from `make love-capture`;
`make test` compares each with the fake. The convention goes into
`docs/love-port/design.md`, with the rule the game must follow: a PICO-8
`rectfill(x0, y0, x1, y1)` with integer corners is
`rectangle("fill", min(x0, x1), min(y0, y1), |x1 − x0| + 1, |y1 − y0| + 1)`.

## User Stories

1. As a plan-2 developer, I want `love.graphics.rectangle("fill", x, y, w, h)` in the fake, so that the Lua game can draw its spans, bars and boxes.
2. As a plan-2 developer, I want a rectangle with integer `x, y, w, h` to fill exactly columns x … x + w − 1 and rows y … y + h − 1, at every position of the 160×120 canvas, so that PICO-8's inclusive `rectfill` maps onto it with one `+ 1`.
3. As a plan-2 developer, I want `rectangle("fill", x, y, 1, 1)` with integers to fill exactly pixel (x, y), at the four corners too, so that a one-pixel rectangle is a safe way to address a pixel.
4. As a plan-2 developer, I want a negative width or height to fill the same pixels as the rectangle spanning the same edges with a positive size, as LÖVE does, so that the game need not normalize corners.
5. As a plan-2 developer, I want a zero width or height to fill nothing and raise no error, as in LÖVE, so that an empty span needs no special case.
6. As a plan-2 developer, I want fractional edges to cover a pixel exactly when real LÖVE does — a pixel is filled when its centre lies inside, the left and top edges inclusive, the right and bottom edges exclusive, after the single-precision pipeline and the 1/256 snap — so that no float coordinate the game computes can differ.
7. As a plan-2 developer, I want edges within a few single-precision steps of a pixel centre to behave exactly as in real LÖVE, including the position-dependent edges at c + 0.5 + 1/512, so that the fake is right even where only the float pipeline decides.
8. As a plan-2 developer, I want the far edges `x + w` and `y + h` computed as LÖVE does, by a single-precision addition of the converted arguments, so that a width the game computes with more precision than a float lands like in LÖVE.
9. As a plan-2 developer, I want a rectangle partly off the canvas (negative coordinates, past 160 or 120) to fill only its on-canvas part, so that a panel sliding in from an edge is drawn like LÖVE draws it.
10. As a plan-2 developer, I want a rectangle entirely off the canvas, or one whose edges reach up to a million pixels away, to fill exactly what LÖVE fills and raise no error, so that clipping in the game is optional.
11. As a plan-2 developer, I want a rectangle to draw in opaque white until `setColor` exists, as LÖVE's default colour is, so that the step's captures need no colour function.
12. As a plan-2 developer, I want a rectangle over a transparent pixel to make it opaque white, and the transparent pixels around it to keep their colour and alpha 0, so that transparency matches LÖVE byte for byte.
13. As a plan-2 developer, I want overlapping or repeated rectangles to leave each pixel as one opaque fill, so that drawing a span twice is harmless.
14. As a plan-2 developer, I want rectangles drawn while no canvas is selected to leave every canvas untouched, as in LÖVE where they go to the screen, so that the canvas the fake reads is right.
15. As a plan-2 developer, I want `"line"`, any other mode string, and a mode that is not a string to raise an error naming `love.graphics.rectangle`, so that an outline (which real LÖVE antialiases) never reaches the game unnoticed.
16. As a plan-2 developer, I want a sixth argument (the rounded-corner `rx`, `ry`, `segments`, even 0 or nil) to raise an error, so that the fake never draws a rounded rectangle it cannot reproduce.
17. As a plan-2 developer, I want a missing argument, a string, boolean, nil or table where a number belongs, to raise an error naming `love.graphics.rectangle` and the argument, so that the fake is stricter than LÖVE's silent string coercion.
18. As a plan-2 developer, I want NaN and infinite values to raise an error, so that a division by zero in the game is reported instead of drawing nothing.
19. As a plan-2 developer, I want any of `x, y, w, h` of magnitude 2^20 or more to raise an error, with the same bound as `points`, so that the fake never accepts a rectangle near the range where llvmpipe's clipping breaks down.
20. As a plan-2 developer, I want a refused call to draw nothing, so that an error leaves the canvas as it was.
21. As a plan-2 developer, I want `love.graphics` to hold exactly `clear`, `newCanvas`, `points`, `rectangle` and `setCanvas` after this step, so that no function exists without its capture.
22. As a plan-1 developer, I want a capture program of integer rectangles (corners, every column, every row, negative and zero sizes), so that the basic convention is settled by real LÖVE.
23. As a plan-1 developer, I want a capture program sweeping low and high edge fractions in isolated cells, with positive and negative sizes, so that the fill rule and the snap are settled by real LÖVE.
24. As a plan-1 developer, I want capture programs placing edges at every column and every row at the fractions where single precision decides, plus far edges that only a float addition gets right, so that the fake's pipeline is proven at every position, not at a few.
25. As a plan-1 developer, I want a capture program for off-canvas, negative and far coordinates near and away from each edge, so that clipping is settled by real LÖVE.
26. As a plan-1 developer, I want a capture program for overlaps, duplicates, a large batch, refused calls, a transparent background and rectangles drawn with no canvas selected, so that each form is proven in one place.
27. As a plan-1 developer, I want each new capture compared with the fake in `make test` as its own parallel check, so that the fast loop proves the convention without getting slower.
28. As a plan-1 developer, I want the span rule tested directly in a pure-C unit test with values taken from the captures, so that a compiler fusing two float operations, a double-precision addition or a truncating division is caught with a precise message.
29. As a plan-1 developer, I want the points rule and the rectangle rule to share one implementation of the snapped window coordinate, so that a fix to the pipeline cannot reach one and miss the other.
30. As a plan-1 developer, I want each misuse of `rectangle` checked through the fake's command line with its message, so that the error contract is pinned.
31. As a plans-2–3 reader, I want the rectangle convention in `docs/love-port/design.md`, including the `rectfill` mapping and "pass integers", so that I do not need plan 1, which will be deleted.
32. As the repository owner, I want `make test` measured before and after this step with design.md's protocol and kept within its gate, so that the fast loop stays fast.
33. As the repository owner, I want the CI love-capture job green with the new programs, so that the PNGs are proven to be what real LÖVE draws.
34. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
35. As the repository owner, I want nothing of this step to include or link anything from `src/`, so that plan 4 can delete `src/`.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture`, pointed at scratch directories holding a copy of the
real harness, on LÖVE 11.5.0 with Mesa 25.2.8 llvmpipe (LLVM 20.1.2),
gave these results. They are the expected answers of this step's
programs; the committed PNGs remain the proof.

- **Default state**: `getColor()` is `1 1 1 1`. A filled rectangle writes
  `FFFFFFFF`, also over pixels cleared to `33669900` (alpha 0); the
  pixels around keep `33669900`.
- **Integers**: `rectangle("fill", 10, 10, 5, 3)` fills columns 10–14,
  rows 10–12; `(25, 10, −5, −3)` fills 20–24 × 7–9; a zero width or height
  fills nothing; every integer edge at every column and row behaves the
  same (left/top inclusive, right/bottom exclusive).
- **Fractions**: `(20.5, 10.5, 5, 3)` fills 20–24 × 10–12 (an edge on a
  pixel centre: the low edge includes it, the high edge excludes it);
  `(30.25, 10.25, 5, 3)` fills 30–34 × 10–12; `(40.75, 10.75, 5, 3)`
  fills 41–45 × 11–13; `(70, 10, 0.4, 0.4)` fills nothing;
  `(75.3, 10.3, 0.4, 0.4)` fills (75, 10).
- **The pipeline is step 7's, then a pixel-centre test.** This model,
  prototyped in Python with numpy and checked against every captured
  rectangle — 6 144 random rectangles with edges concentrated near pixel
  centres and sub-pixel ties, positive and negative sizes; full sweeps of
  low and high edges at c + 0.5 + δ over every column and every row for
  15 values of δ; the six final drafts below — gives zero differing
  pixels:

  ```
  // prototype, per axis; size is 160 for x (with w), 120 for y (with h)
  a    = (float) x                    // LÖVE's binding converts each argument
  b    = a + (float) w                // the far edge: a float addition
  A, B = snap(a, size), snap(b, size) // step 7's pipeline up to lrintf(win * 256)
  lo, hi = min(A, B), max(A, B)
  pixel i is filled iff lo <= 256 * i + 128 < hi   // its centre, top-left rule
  ```

  `snap` is exactly step 7's per-axis computation: `v * (2f / size)`,
  then `+ (−1)`, each rounded to float, then `fmaf(ndc, size / 2,
  size / 2)`, then `lrintf(win * 256)`. Every alternative is refuted by
  the drafts: fusing the projection (68 + 103 wrong pixels on the column
  and row sweeps), splitting the viewport (48 + 79), adding `x + w` in
  double precision (93 + 27), rounding the snap's ties up (565 + 212),
  the bottom-right fill rule (750 on the sub-pixel draft, thousands on
  the sweeps). Ties-to-even and ties-down in the snap cannot be told
  apart, as for points.
- **Position dependence**: an edge at c + 0.5 + 1/512 snaps onto the
  centre or past it depending on c: for size 160 the low edge includes c
  for c = 0, 1, 4, 5, 6, 9 … and excludes it for c = 2, 3, 7, 8, 12 …
  (27 of 160 columns excluded); for size 120, c = 0, 3, 4, 7 … include
  and c = 1, 2, 5, 6 … exclude (71 of 120). Edges within ±1/1024 or
  ±3·2^-17 of a centre all snap onto it at every position; 1/256 past it
  is past at every position.
- **Float addition**: with `x = c + 0.3` and `w = 0.2 + 1/512 + 10^-6`
  the far edge lands within a hair of the tie; LÖVE's float sum fills
  nothing at 61 of the 160 columns where a double sum would fill column
  c (e.g. c = 16, 21, 23, 26); on rows, 27 of 120 (e.g. 17, 22, 23, 29).
- **Off-canvas**: parts off the canvas are clipped exactly: `(−5, 2, 10,
  2)` fills 0–4, `(155, 2, 10, 2)` fills 155–159; `(−0.5, y, 1, 1)` fills
  nothing, `(−0.49, y, 1, 1)` fills column 0, `(159.5, y, 1, 1)` fills
  159, `(159.51, y, 1, 1)` nothing; likewise for rows at −0.5, −0.49,
  119.5, 119.51.
- **Far values**: a rectangle with one edge on the canvas and the other
  up to 2^127 away (either side, either axis) fills exactly what the
  model with ideal clipping predicts. A rectangle whose two edges are
  both far outside, on opposite sides, fills the whole row up to
  ±2^31 (2^33 for asymmetric spans); beyond that llvmpipe's clipper
  fills only the first half of the canvas (columns 0–79, rows 0–59).
  `(−10^6, y, 10^6 − 5, 1)` and `(10^6, y, 5, 1)` fill nothing.
- **NaN and infinities** are accepted and draw nothing visible.
- **Forms**: `"line"` draws an antialiased outline (`93939393`
  pixels); any other mode string (`"full"`, `"FILL"`) and a number fail
  with `Invalid draw mode '…', expected one of: 'line', 'fill'`; a table
  mode fails with `bad argument #1 to '?' (string expected, got table)`.
  A missing or nil coordinate, a boolean or table fail with
  `bad argument #K to '?' (number expected, got …)`; numeric strings are
  coerced. A sixth argument `rx` (with `ry`, `segments`) draws a rounded
  rectangle, except `rx = ry = 0` and `rx = 0` alone, which draw the
  plain one. A refused call draws nothing.
- **No canvas**: rectangles drawn after `setCanvas()` do not reach the
  harness canvas.

### Modules

- **Point rule** (step 7's pure-C module, H-7-1, extended; it keeps its
  name). It gains the rectangle's per-axis question: given an edge
  coordinate and a signed extent (doubles) and the canvas size along that
  axis, which pixels from first to last (or none) are filled, clipped to
  `0 … size − 1`. It encodes the prototype above, the float conversion of
  each argument and the float addition each in its own statement.
  - The snapped window coordinate (`lrintf(win * 256)`, the steps before
    it unchanged) is split out of the point landing into one internal
    function that both the point landing and the span use; the point
    rule's answers do not change, and its unit test must stay green
    untouched.
  - The pixel range is derived from `lo` and `hi` with floor division:
    the snapped values are negative for edges left of or above the
    canvas, and C's truncating division rounds them toward zero, which
    can turn an empty range left of the canvas, such as `(−1.6, 1)`, into
    pixel 0. The unit test pins it.
  - Acceptance reuses the point rule's predicate (finite, magnitude below
    2^20) for each of `x, y, w, h`. Every accepted rectangle has its far
    edge below 2^21, so the snapped values stay below 2^30 and fit a
    `long`, and both edges stay far inside the range llvmpipe clips
    correctly (2^31).
- **Canvas store** (H-7-2, extended): a fill of an inclusive pixel range
  of the current target with the current colour, with the same contract
  as the one-pixel plot: no target, no change; the range must lie on the
  canvas (assert); writing replaces the four bytes; the store's existing
  opaque-colour contract applies unchanged until step 9.
- **Graphics binding** (H-7-3, extended) gains `rectangle`, so
  `love.graphics` holds exactly `clear`, `newCanvas`, `points`,
  `rectangle`, `setCanvas`. The one accepted form is exactly five
  arguments: the string `"fill"` and four Lua numbers (type number, not a
  numeric string). The binding checks everything first (count, mode, then
  each number's type, finiteness and range), then asks the span for each
  axis and fills the range when both are non-empty. Every refusal is a
  Lua error starting `love.graphics.rectangle: ` and naming the problem:
  `takes 5 arguments, not N` (also for a sixth argument, nil included),
  `argument 1 is a <type>, not a string`, `mode '<mode>' is not
  supported, only 'fill'` (also for `"line"`), `argument K is a <type>,
  not a number`, `argument K is not finite`, `argument K is out of range
  (|v| >= 2^20)`. The exact wording is the implementer's; the tests
  match the function name, the index or mode, and the key word.
- **Nothing else changes**: the host, the comparison command, the
  harnesses, the Makefile's capture registration (a `.lua` and its
  `.png` in `tools/love_capture/` become a `make test` check by
  themselves), the `src/` guard (the point rule is already in the fake's
  plain-C source list).

### Capture programs of this step

Each is a `tools/love_capture/<name>.lua` returning `function(canvas)`,
using only `rectangle`, `clear` and `setCanvas` (and `pcall`/`assert`);
the canvas starts transparent black, every filled pixel is `FFFFFFFF`,
every other pixel `00000000` unless said. Rectangle lists are loops in
the program, not literal lists. Each draft below was captured and run
through the prototype: zero differing pixels; the counts are the drafts'.

| program | draws | expected |
|---|---|---|
| `rectangle_integers` | 1×1 at the four corners; (10, 10, 5, 3); (25, 10, −5, −3); (30, 10, 0, 3), (35, 10, 3, 0), (40, 10, 0, 0); (45, 10, 2, 2) and (46, 11, 2, 2) overlapping; for c = 0 … 159, (c, 20 + 2(c mod 4), 1, 1 + c mod 2); for r = 0 … 119, (60 + 2(r mod 4), r, 1 + r mod 2, 1) | 459 pixels: corners, 10–14 × 10–12, 20–24 × 7–9, the 7-pixel overlap pair, and the two staircases (which cross at columns 60–67, rows 20–27) |
| `rectangle_subpixel` | 20 fractions F = {0, 1e-7, 1e-6, 1e-4, 1/1024, 3/1024, 1/256, 1/128, 1/4, 0.5 − 1/256, 0.5 − 3/1024, 0.5 − 1/1024, 0.5 − 1e-6, 0.5, 0.5 + 1e-6, 0.5 + 1/1024, 0.5 + 3/1024, 0.5 + 1/256, 3/4, 1 − 1e-6}; for each (low, high) in F × F, rectangle n in its own 5×5 cell at (5(n mod 32), 5⌊n/32⌋): even n from (cell + 1 + low) with size 2 + high − low on both axes, odd n from (cell + 3 + high) with the negated size | 1 728 pixels, per the model; the bottom-right rule would be 750 pixels off |
| `rectangle_columns` | 14 deltas δ ∈ {0, ±2^-17, 3·2^-17, ±1/1024, −1/512, 1/512 − 2^-17, 1/512, 1/512 + 2^-17, ±1/256, 0.5 − 2^-17, −0.5 + 2^-17}; for each, 4 rows by phase p = 0 … 3 of low edges (c + 0.5 + δ, row, 1.75, 1) and 4 rows of high edges (c − 1.25, row, 1.25 + 0.5 + δ, 1), c = p, p + 4, … ≤ 161; then 4 rows of (c + 0.3, row, 0.2 + 1/512 + 1e-6, 1) and 4 rows of (c + 0.1, row, 0.4 + 1/512 + 1.6e-6, 1) | 7 023 pixels, per the model |
| `rectangle_rows` | the same with the axes swapped, c ≤ 121, on columns 0–119 | 5 327 pixels, per the model |
| `rectangle_offcanvas` | with F = 2^20 − 1: (−5, 2, 10, 2), (155, 2, 10, 2), (20, −5, 2, 8), (30, 115, 2, 10); fully off (−10, 10, 5, 5), (160, 10, 5, 5), (40, −10, 5, 5), (40, 120, 5, 5); (−0.5, 20, 1, 1), (−0.49, 22, 1, 1), (159.5, 24, 1, 1), (159.51, 26, 1, 1) and their row counterparts at columns 50–56; (−F, 30, F + 10, 1), (150, 32, F, 1), (−F, 34, 2F, 1), (F, 36, −F − 5, 1), (−10^6, 38, 10^6 − 5, 1), (10^6, 40, 5, 1) and their column counterparts at columns 80–90 | 636 pixels: 0–4 and 155–159 on rows 2–3; 20–21 on rows 0–2; 30–31 on rows 115–119; (0, 22), (159, 24), (52, 0), (54, 119); 0–9 on row 30; 150–159 on row 32; rows 34 and 36 entirely; column 80 rows 0–9, column 82 rows 110–119, columns 84 and 86 entirely |
| `rectangle_forms` | `clear(0.2, 0.4, 0.6, 0)`; (10, 10, 20, 10) and (20, 15, 20, 10) overlapping; (50, 10, 4, 4) twice; `assert(not pcall(rectangle, "full", 60, 10, 5, 5))`; `assert(not pcall(rectangle, "fill", 60, 20, 5))`; 1 000 rectangles (4(i mod 40), 60 + 2(⌊i/40⌋ mod 25), 3, 1); `setCanvas()`, (0, 30, 160, 20), `setCanvas(canvas)` | 3 366 `FFFFFFFF`, 15 834 `33669900` |

The two refusals of `rectangle_forms` are ones real LÖVE makes too, so
the program runs in both. The plan's edge cases: off-canvas and negative
coordinates are `rectangle_offcanvas`; transparent pixels are
`rectangle_forms`; nested translate does not exist before step 13, which
captures `translate` over `rectangle` (step 15 combines them). If a PNG
disagrees with this table, the PNG wins and design.md follows it.

### Conventions written into `docs/love-port/design.md`

A "Rectangles" paragraph after step 7's "Points" paragraph (H-7-5):

- the one form accepted, every refusal, the error prefix;
- the colour: the current colour, opaque white until `setColor`;
- the coverage rule in words: each argument converted to single
  precision, the far edges `x + w` and `y + h` added in single precision,
  each edge through the same projection, viewport and 1/256 snap as a
  point, then a pixel is filled when its centre lies between the edges,
  low edges inclusive, high edges exclusive (LÖVE's canvas top-left
  rule); the per-axis prototype;
- **the rule for the game**: integer rectangles are exact at every
  position; `rectfill(x0, y0, x1, y1)` is `rectangle("fill", min(x0,
  x1), min(y0, y1), |x1 − x0| + 1, |y1 − y0| + 1)`; negative sizes fill
  the same pixels, zero sizes nothing; edges within 1/512 of a pixel
  centre are position-dependent and the game must not produce them;
- off-canvas parts are clipped; the shared 2^20 bound and why;
- rectangles with no canvas selected touch nothing the fake models.

The function list line `rectangle("fill", …)` gains its exact form.

### Decisions taken without the plan

- **Reproduce the pipeline, not a rounding rule.** "Fill
  ⌈x − 0.5⌉ … ⌈x + w − 0.5⌉ − 1" matches every integer and most
  fractions, but not the float sum family nor the 1/512 ties; the plan
  wants pixel-for-pixel conventions, and the shared snap costs nothing.
- **One snap for points and rectangles.** Step 7's point landing is the
  same fill rule applied to a 1×1 square centred on the snapped
  coordinate (256n < fix ≤ 256n + 256 is lo ≤ 256n + 128 < hi with
  lo = fix − 128, hi = fix + 128), so the two rules must agree on the
  snap; splitting it out keeps them from drifting.
- **"fill" only, five arguments only.** `"line"` is antialiased in real
  LÖVE and off the design's list; rounded corners are polygons the fake
  cannot reproduce; a sixth argument is refused even when LÖVE would
  draw the plain rectangle (0, nil), so that no call depends on it.
- **Stricter than LÖVE on types and values**, as `points`: no string
  coercion, no NaN, no infinity, no value at or beyond 2^20.
- **Zero sizes accepted**: an empty span is a natural result of clipping
  in the game, and LÖVE accepts it.
- **Check everything, then draw.** A refused call draws nothing.
- **Misuse is tested in one fixture game** logging each misuse through
  `pcall` and `debug_state()`, plus one uncaught misuse for the fatal
  path, as step 7 did for `points`: one or two processes, inside the
  100 ms budget.

## Implementation Chunks

1. **Span rule.** Split the snap out of the point landing; add the
   rectangle span with its acceptance; extend the point rule's unit test
   (or a sibling in the same unit target) with the values below. Stories
   2–10, 18, 19 (rule side), 28, 29.
2. **`rectangle` in the fake.** The store's range fill, the binding's
   `rectangle` with its refusals, the CLI test with its fixture games and
   the graphics CLI test's name list. Stories 1, 11–17, 20, 21, 30.
3. **Captures, docs, proof.** The six programs and their PNGs from
   `make love-capture` (git diff shows only the new files), the six
   comparisons passing in `make test`, design.md, the timing, the CI
   run, the step note. Stories 22–27, 31–35.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the rule's answers, the store's bytes, the binary's exit status,
  stderr and state file, the comparison's verdict. No test reaches into
  the Lua state or the store's layout.
- **Span rule, unit test** (step 7's pure-C point rule test, H-7-6,
  extended; its existing cases unchanged). Values from the captures,
  written as (start, extent) → first … last or none:
  - size 160: (10, 5) → 10 … 14; (25, −5) → 20 … 24; (10, 0) → none;
    (20.5, 5) → 20 … 24; (30.25, 5) → 30 … 34; (40.75, 5) → 41 … 45;
    (70, 0.4) → none; (75.3, 0.4) → 75 … 75; (0, 1) → 0 … 0;
    (159, 1) → 159 … 159;
  - clipping, size 160: (−5, 10) → 0 … 4; (155, 10) → 155 … 159;
    (−0.5, 1) → none; (−0.49, 1) → 0 … 0; (−1.6, 1) → none (the floor
    division case); (159.5, 1) → 159 … 159; (159.51, 1) → none;
    (−(2^20 − 1), 2^20 − 1 + 10) → 0 … 9; (150, 2^20 − 1) → 150 … 159;
    (2^20 − 1, −(2^20 − 1) − 5) → 0 … 159; (10^6, 5) → none;
  - size 120: (119, 1) → 119 … 119; (119.5, 1) → 119 … 119;
    (119.51, 1) → none;
  - the tie at c + 0.5 + 1/512 as a low edge with extent 1.75: size 160,
    c = 0 → starts at 0, 2 → 3, 5 → 5, 7 → 8; size 120, c = 0 → 0,
    1 → 2, 3 → 3; each failing assert says which rounding was probably
    fused or split;
  - the float addition: size 160, (16.3, 0.2 + 1/512 + 1e-6) → none and
    (1.3, same) → 1 … 1; size 120, (17.3, same) → none; a failing assert
    says the far edge was probably added in double precision;
  - acceptance is step 7's predicate, already tested.
- **Canvas store** (step 6's unit test, as extended by step 7): a range
  fill writes `FFFFFFFF` over exactly the range of the target, over a
  transparent coloured pixel too, leaves the other canvas untouched, and
  with no target changes nothing.
- **`rectangle`, CLI test** (new, its own target in `FAKE_LOVE_CHECKS`,
  through `tmp/fake_love` and the shared CLI helper, a fixture game and
  a one-frame replay, as step 7's points CLI test):
  - a game whose `love.load` logs the sorted names of `love.graphics`
    (`clear newCanvas points rectangle setCanvas`), then, in `pcall`,
    each misuse with its message: `rectangle("line", 1, 2, 3, 4)`,
    `rectangle("full", …)`, `rectangle(5, 1, 2, 3, 4)`,
    `rectangle({"fill"}, …)`, `rectangle("fill", 1, 2, 3)`,
    `rectangle("fill", 1, 2, 3, 4, 0)`, `rectangle("fill", 1, 2, 3, 4,
    nil)`, `rectangle("fill", "1", 2, 3, 4)`, `rectangle("fill", 1, nil,
    3, 4)`, `rectangle("fill", 1, 2, true, 4)`, `rectangle("fill", 0/0,
    2, 3, 4)`, `rectangle("fill", 1, 2, 3, 1/0)`, `rectangle("fill",
    2^20, 2, 3, 4)`, `rectangle("fill", 1, 2, -2^20, 4)`; then
    `rectangle("fill", 1, 2, 0, 4)` and a valid call with no canvas
    selected, logged as successes; the log is compared byte for byte;
    the step 6 graphics CLI test's expected name list gains `rectangle`;
  - a game calling `rectangle("line", …)` uncaught in `love.draw` exits 1
    naming `love.graphics.rectangle` and `f0`;
  - "a refused call draws nothing" for a refusal real LÖVE does not make
    (NaN width): one fixture program, run by the comparison command
    against a fully transparent PNG written to `tmp/` by the test PNG
    encoder, calls `pcall(rectangle, "fill", 10, 10, 0/0, 5)` and must
    leave the canvas transparent. It joins the rectangle CLI test target.
- **The capture comparisons** are the tests of the convention: each of
  the six programs passes in `make test`.
- **Prior art:** step 7's point rule unit test and points CLI test with
  its log, uncaught and refused fixtures; step 6's canvas store test and
  graphics CLI test; `test_fake_love_joystick` for misuse logged through
  `pcall` and `debug_state()`.
- **Proofs recorded in the step note** (Step 3.8 in plan 1):
  - `make love-capture` writes the six new PNGs and leaves every other
    file of `tools/love_capture/` byte-identical;
  - the CI run on the pushed commit with the love-capture job green
    (run id);
  - `make test` before and after with design.md's protocol: 11 rotated
    paired rounds against the step's "before" (step 7's last commit,
    H-7-7) and against `2e7215c`, both medians of paired ratios ≤ 3 %
    (22 pooled rounds if either exceeds it); plain and paired medians,
    spreads, rounds, core count, and the slowest new check's own time
    (budget 100 ms); a check over budget is split before the step closes.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- `setColor`, colours other than the default white, blending of
  non-opaque colours: step 9.
- `rectangle("line", …)`, rounded rectangles, `polygon`, `line`: not on
  the design's list.
- `push`/`pop`/`translate` and rectangles under a transform: step 13
  (LÖVE transforms the four corners on the CPU in single precision
  before the projection; step 13 settles it with its capture).
- Images, quads, `draw`, `setDefaultFilter`: steps 10–14.
- Drawing to the screen: not modelled; rectangles with no canvas are
  accepted and dropped.
- Named fatal errors for unlisted functions: step 17. The capture file:
  step 16.
- Audio, always.

## Hypotheses

Plan step 7 is being implemented while this PRD is written. Its point
rule, store plot and `points` binding are committed (2ffb05e, e92fa98);
its captures, its design.md paragraph and its step note are not. Each
item below is checked by the implementer against the committed step 7
before starting; when one differs, adapt the names, not the decisions.

- **H-7-1**: the fake has a pure-C point rule module in its plain-C
  source list with an acceptance predicate (finite, |v| < 2^20) and a
  per-axis landing function computing step 7's pipeline, each rounding
  in its own statement, the viewport through `fmaf`, with a compile-time
  `FLT_EVAL_METHOD == 0` assert. This step splits the snapped value out
  of the landing function and adds the span beside it.
- **H-7-2**: the canvas store holds the current colour, opaque white,
  and a one-pixel plot that replaces four bytes, does nothing with no
  target and asserts the pixel is on the canvas. This step adds a range
  fill with the same contract.
- **H-7-3**: the graphics binding installs `love.graphics` holding
  exactly `clear`, `newCanvas`, `points`, `setCanvas`, its errors
  prefixed with the full function name, and `points` checks every
  argument before drawing. This step adds `rectangle` the same way.
- **H-7-4**: step 7 has its own points CLI test target in
  `FAKE_LOVE_CHECKS` (a log fixture through `pcall` and `debug_state()`,
  an uncaught fixture, a refused-call fixture compared against a
  transparent PNG from the test PNG encoder); this step adds a sibling
  target rather than growing it.
- **H-7-5**: step 7 ends with its six `points_*` captures and PNGs
  committed and a "Points" paragraph in `docs/love-port/design.md` after
  "Canvas and clear"; this step's "Rectangles" paragraph follows it, in
  the same shape.
- **H-7-6**: the point rule has a pure-C unit test in the unit list,
  linked with the rule only; this step's span cases join it.
- **H-7-7**: step 7's last commit is this step's "before" timing
  baseline, and its note (Step 3.7) records its timings.

## Further Notes

- The exploration programs lived in scratch directories passed to
  `make love-capture` with a copy of the harness; nothing of them is
  committed. The six drafts of the table were captured and checked
  against the prototype (programs run in LuaJIT with a recording stub of
  `love.graphics`, then the model): zero differing pixels.
- Real LÖVE's binding converts `x, y, w, h` to float before
  `Graphics::rectangle`, which builds the corners `x + w`, `y + h` in
  float; the 4-vertex polygon is drawn as a triangle fan, so the shared
  diagonal never shows with the top-left rule.
- llvmpipe clips rectangles correctly far beyond the 2^20 bound (one
  edge up to 2^127, both edges up to 2^31); the bound is the fake's, for
  consistency with `points` and to keep every snapped value in a `long`.
- Six new checks of one LuaJIT start and at most about 5 000 calls each
  run in parallel on the cores the unit tests leave idle; if the gate
  fails, design.md rule 5 applies.

## Decision after chunk 1 (guideline)

The binding keeps the PRD bound: each of x, y, w, h passes
`point_accepts_coordinate` (finite, |v| < 2^20). `rectangle_offcanvas` changes
only its out-of-bound widths, with H = 2^19: (−F, F+10) becomes
(−(2^20−20), 2^20−10); (−F, 2F) becomes (−H, 2H); (F, −F−5) becomes
(H, −H−5); same for the row counterparts. Expected pixels and the 636 total
are unchanged. The span function's own contract (start accepted, finite
extent < 2^21) and its unit values stay as committed in chunk 1.
