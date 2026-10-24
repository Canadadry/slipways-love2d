---
title: "Fake LÖVE: the screen framebuffer, the present at any offset and scale, and screen PNGs"
description: "The fake LÖVE models nothing on the screen and accepts only an integer present, so no test can check where a fractionally scaled, centred game lands in a 390x844 window."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) shows the 160×120 game on a portrait iPhone,
scaled by a fractional factor (2.4375 on a 390×844 window) and centred at
fractional positions (y 64.75 to 357.25), with nearest filtering. Every
visual requirement of the plan is a test comparing a whole screen with an
expected frame, pixel for pixel. Today the fake LÖVE (`tools/fake_love/`)
cannot support one of them:

- the present `draw(canvas, x, y, 0, s, s)` is refused unless `x` and `y`
  are integers and `s` is an integer from 1 to 16, so the plan's
  `screen.present` at (0, 64.75) and scale 2.4375 fails at the call;
- every draw onto the screen "changes nothing the fake models": there is no
  screen to look at, so nothing proves where the game's pixels land;
- a replay cannot ask for a frame's screen, and the fake has no PNG writer
  (the only one is a test helper);
- design.md states no rule for where a scaled texel lands, so no test can
  compute an expected frame, and at 2.4375 the texel edges fall exactly on
  pixel centres (x = 19.5 is the edge between texels 7 and 8), where an
  unstated rule is a coin toss.

The screen raster is also the main risk to `make e2e`'s 9-second limit: a
640×480 window is 307 200 pixels, and the 20 replays play 4 072 frames.

## Solution

The fake gains a **screen framebuffer**, a widened present, and a replay
field that writes chosen frames' screens as PNG files:

- **The present** takes any finite position of magnitude below 2^20,
  fractions included, and any equal finite scale greater than 0 and at most
  1024. It still requires the screen selected, an opaque white colour, a
  zero transform offset and rotation 0, and it now refuses a canvas holding
  a translucent pixel (blending arrives with step 5).
- **The rasterisation rule** is written once in design.md: a screen pixel
  belongs to a present when its centre lies inside the destination
  rectangle, half-open on each axis (`x0 <= cx < x1`), and takes the texel
  that centre maps to (nearest, the same half-open floor). An alpha-`FF`
  texel writes its bytes, an alpha-`00` texel leaves the pixel. A later
  present lies over an earlier one.
- **The screen** is an RGBA framebuffer of the window size, opaque black at
  the start of every frame, after that frame's resize. In step 4 only the
  present reaches it (step 5 adds the quad draws).
- **`"screens": [frame numbers]`**, a new optional top-level replay member:
  after each listed frame, the fake writes the whole screen as an 8-bit RGBA
  PNG named `<capture>.screen-f<N>.png`.
- **The raster stays cheap by being lazy**: the screen is rasterised only on
  the frames `screens` lists. On every other frame each screen draw is
  checked exactly as on a listed frame, and nothing is rasterised. Since the
  screen is black at every frame start and only `screens` reads it, skipping
  the raster on unlisted frames changes nothing anyone can observe. No e2e
  replay carries `screens`, so `make e2e` pays no raster at all; it pays
  only the present's translucency scan (19 200 alpha bytes per present).

The game does not change: it still presents at (0, 0) and scale
`screen.SCALE`, so the 20 replays keep their `*.sha256` and `*.end-state`.

## User Stories

1. As a plan-6 implementer, I want the present to accept a fractional position, so that `screen.present` can centre the game at y = 64.75 on a 390×844 window.
2. As a plan-6 implementer, I want the present to accept a fractional scale such as 2.4375, so that the game fills the width of any window with no black bars.
3. As a plan-6 implementer, I want the present to accept a scale below 1, so that a window smaller than 160×120 still shows the whole game.
4. As a plan-6 implementer, I want the present to accept a negative or off-window position, so that a partly off-screen present is clipped silently, not refused.
5. As a plan-6 implementer, I want a replay to list frames in `"screens"`, so that a test can get the whole screen of exactly those frames.
6. As a plan-6 implementer, I want each listed frame's screen written as a PNG next to the capture, named after the capture and the frame, so that a test finds it without guessing.
7. As a plan-6 implementer, I want the screen to be opaque black at the start of every frame, so that an expected frame is black wherever nothing was drawn.
8. As a plan-6 implementer, I want the screen to take the window size in force on that frame, after its resize, so that a test of the step-16 resize case sees the new size.
9. As a test author, I want design.md to state the rasterisation rule exactly, arithmetic included, so that my test can compute the expected frame on its own.
10. As a test author, I want a pixel whose centre lies exactly on an edge of the destination to belong to exactly one side (half-open), so that two abutting draws never both cover a pixel and never leave a gap.
11. As a test author, I want a pixel whose centre lies exactly on an edge between two texels to take the texel to the right (or below), so that the 390×844 present is deterministic where texel edges land on pixel centres.
12. As a test author, I want an alpha-`00` texel of the presented canvas to leave the screen pixel, so that a present of a partly transparent canvas over another present is exact.
13. As a test author, I want a later present to lie over an earlier one, so that overlapping draws give a defined frame.
14. As a test author, I want the PNG to be RGBA8 of the window size, row 0 at the top, so that I can decode it and compare all four channels with no tolerance.
15. As a test author, I want a frame not listed in `screens` to write no PNG, so that the files present are exactly the ones asked for.
16. As a maintainer, I want every check of the present to run on every frame whether or not the frame is listed, so that a replay's success never depends on its `screens`.
17. As a maintainer, I want the raster to run only on listed frames, so that the 20 e2e replays, which list none, pay no raster.
18. As a maintainer, I want no framebuffer allocated in a run that lists no frame, so that `make e2e` and `love_capture_check` use no extra memory.
19. As a maintainer, I want a present of a canvas holding a translucent pixel refused at the call, naming the pixel, so that a texel the fake would have to blend never reaches the screen silently before step 5 models blending.
20. As a maintainer, I want the present to keep refusing a selected canvas, a non-white colour, a non-zero transform offset, a rotation, unequal scales and a non-Canvas, so that what the screen shows is exactly what the rule says.
21. As a maintainer, I want a zero, negative, infinite or NaN scale, and a scale above 1024, refused with a message naming the argument, so that a typo never draws a mirrored or empty game.
22. As a maintainer, I want a position of magnitude 2^20 or more, NaN or infinite refused as today, so that the destination rectangle stays well defined in double precision.
23. As a replay author, I want a malformed `screens` (not an array, a non-integer, a frame outside the replay, numbers not strictly ascending, more than 16 entries, the member given twice) refused before `love.load` with the file, the byte offset and the problem, so that a typo never runs.
24. As a replay author, I want `screens` to count frames the way `frames` does, touch-only and empty frames included, so that a frame number means the same thing everywhere.
25. As a replay author, I want a run that fails after writing some screen PNGs to remove them, with the capture and the state file, so that a failed run leaves no output, as every refusal of the fake does.
26. As a replay author, I want a run that fails on a listed frame to write no PNG for it, so that a PNG always shows a frame that completed.
27. As the game, I want the present at (0, 0) and scale 4 to keep working byte for byte, so that the 20 replays keep their hashes.
28. As a maintainer, I want the fake's PNG writer to be the one the C tests already use, moved into the fake's PNG module, so that the repository has one writer, not two.
29. As a maintainer, I want the PNG writer to report a failure to write instead of asserting, so that the fake prints `cannot write` like for the capture.
30. As a maintainer, I want the screen raster built as its own C module with a small interface (begin a frame, draw a canvas, read the pixels), so that the rule is unit-tested without starting the fake.
31. As a maintainer, I want a fake C test that runs a replay with `screens` and compares each PNG with an expected frame the test builds itself, so that the plan's verification is automatic and exact.
32. As a maintainer, I want that test to cover the 390×844 present at scale 2.4375 with landmark pixels named (column 18 is texel 7, column 19 is texel 8; row 64 is black, row 65 is texel row 0; row 74 is texel row 4; row 356 is texel row 119, row 357 black), so that the half-open rule is proven at the tie, not just on average.
33. As a maintainer, I want that test to cover a non-dyadic fractional scale at a fractional, partly off-window offset, so that clipping and the floor on inexact values are exercised.
34. As a maintainer, I want that test to cover a resize between two listed frames, a listed frame with no present, two overlapping presents and an alpha-`00` texel, so that the frame start, the size and the order are all checked.
35. As a maintainer, I want design.md's present paragraph, a new screen paragraph with the rule, the replay members paragraph, the run steps and the function list updated in the same step, so that the contract stays the single description of the fake.
36. As a maintainer, I want the new `make test` checks to pass design.md's timing gate with their medians recorded in plan 6 §6, so that `make test` is no slower.
37. As a maintainer, I want `make e2e`'s median of 5 runs measured beside the "before" commit and recorded in §6, with the per-frame cost and the touch replays' frame cap, so that steps 5, 7, 15 and 16 can prove they stay under 9 s.
38. As the implementer of step 15, I want a cap on the touch replays' frames (twins included) fixed now, so that I know how many frames I may write before re-measuring.
39. As the implementer of step 5, I want the screen module to take the quad draws later through the same frame start, clipping and texel rule, so that step 5 only adds the scaled quad and blending.
40. As the implementer of step 7, I want the present's new range to cover every window size the replay format allows (1 to 4096 per side), so that the layout's scale never meets a refusal.

## Implementation Decisions

- **PNG writer.** The writer of the C tests (8-bit RGBA, stored deflate
  blocks) moves into the fake's PNG module, beside the reader, as a write
  function taking a path, row-major RGBA8 pixels and the size, returning
  false when the file cannot be created or written (no assert). The test
  helper file goes; the five tests that used it link the PNG module and
  assert on its result. Byte output unchanged, so their fixtures stay valid.

- **New C module `screen`** (pure C, no Lua), owned by the host:
  - create / free;
  - begin a frame: the window size and whether this frame is recorded. A
    recorded frame (re)allocates the buffer when the size changed and fills
    it with `000000FF`; returns false only when memory runs out. An
    unrecorded frame touches no buffer, and none is ever allocated in a run
    that records nothing;
  - draw a canvas: an RGBA8 source of its size, position `x`, `y`, scale
    `s` (doubles). Does nothing on an unrecorded frame. Asserts every texel
    it reads is alpha `00` or `FF` (the present refuses translucency
    first);
  - read: the pixels and size of the current frame, NULL when it is not
    recorded.
  - The rule, in double precision, for a source `W`×`H`:
    `x1 = x + W * s`, `y1 = y + H * s`; screen pixel `(i, j)` has centre
    `cx = i + 0.5`, `cy = j + 0.5` and belongs to the draw when
    `x <= cx < x1` and `y <= cy < y1`; its texel is column
    `floor((cx - x) / s)` and row `floor((cy - y) / s)`, each clamped to
    `0 … W-1` / `0 … H-1` (the clamp only guards rounding). Alpha `FF`
    writes the texel's four bytes; alpha `00` leaves the pixel. Pixels off
    the window do not exist (clipping). The implementation computes the
    column and row tables once per draw (O(width + height)), then copies;
    it must give the same bytes as the per-pixel rule.
  - Real LÖVE rasterises in single precision on the GPU, with a top-left
    rule that is also half-open; the fake's double rule is the plan's
    contract, verified by fake tests only (design.md's function-list
    sentence for plan 6), not by a real-LÖVE capture.

- **Present (graphics module).** `draw(canvas, x, y, 0, s, s)`:
  - argument checks in today's order, with two new rules in the image-rules
    module: a present position is finite and of magnitude below 2^20
    (fractions accepted; the quad draw position keeps its integer rule); a
    present scale is finite, greater than 0 and at most 1024 (the integer
    1–16 scale rule goes). Messages: `argument <n> is not finite`,
    `argument <n> is out of range (|v| >= 2^20)`,
    `argument <n> is out of range (s greater than 0, at most 1024)`. The
    unequal-scales message prints the scales with `%g`;
  - then, as today: screen selected, white `FFFFFFFF`, zero transform
    offset (the transform offset stays refused; "any offset" in the plan is
    the position argument);
  - then, new: the canvas's first translucent pixel (alpha neither `00` nor
    `FF`) in row order is refused:
    `love.graphics.draw: the canvas's pixel (<x>, <y>) is <RRGGBBAA>, translucent (blending onto the screen is not modelled before plan 6 step 5)`.
    The scan reads the 19 200 alpha bytes of the canvas at every present,
    recorded frame or not;
  - then the screen module draws it (a no-op on an unrecorded frame).
  - `graphics_install` takes the screen (not owned).
  - Points, rectangles, `clear` and quad draws with the screen selected
    still model nothing; the screen framebuffer receives only the present
    in this step.

- **Host.** The frame-input struct (H-3-1) gains one boolean, "record this
  frame's screen"; zero means not recorded, which `love_capture_check`
  passes. The host begins the screen's frame right after the frame's
  resize, before any callback of the frame (H-3-4), with the window size
  then in force. A new host query returns the current frame's screen pixels
  and size, NULL when not recorded.

- **Replay reader.** New optional top-level member `screens`, read with
  the other start-state members (H-3-2): an array (possibly empty) of at
  most 16 integers, each from 0 to frame count − 1, strictly ascending; any
  other shape refused with the file, the byte offset and the problem
  (`"screens" is <kind>, not an array of frame numbers`,
  `"screens" entry <v> is not an integer`,
  `"screens" entry <n> is not a frame of this replay (0-<last>)`,
  `"screens" entry <n> is not after <previous>`,
  `"screens" lists <n> frames, more than 16`, `"screens" given twice`). A
  query answers whether a frame is listed.

- **Fake command.** The frame input reader (H-3-3) sets the record flag
  from the replay. After a frame's capture append succeeds (canvas 0
  checked), a recorded frame's screen is written to
  `<capture path>.screen-f<N>.png`, `N` in plain decimal; a write failure
  is `fake_love: <path>: cannot write`, exit 1. The fake remembers each
  PNG it wrote and removes them all, with the capture and the state file,
  when the run fails at any later point. It does not delete PNGs of frames
  not listed (tests remove their own files before a run).

- **No change** to `game/`, `tools/lua_guard.lua`, the function list's
  names (only the present's accepted form widens), `tools/e2e_limits.sh`,
  or the 20 replays.

- **design.md** (`docs/love-port/design.md`), same step:
  - "The present": the new ranges, the translucency refusal, and that it
    draws onto the screen framebuffer per "The screen"; the rule for the
    game stays `(0, 0)` and `screen.SCALE` until step 7 rewrites it;
  - a new **"The screen"** paragraph beside it: the framebuffer (window
    size, `000000FF` at each frame start after the resize), the
    rasterisation rule word for word as above, alpha `00`/`FF`, later over
    earlier, clipping, that only the present reaches it in this step, that
    it is rasterised only on frames listed in `screens` while every check
    runs on every frame, and the PNG format and name;
  - the run steps at the top of "The fake LÖVE": the PNG written after a
    listed frame's capture, removed on failure;
  - the replay members paragraph: `screens` and its rules, and that no e2e
    replay carries it;
  - the function list: `draw(canvas, x, y, 0, s, s)` onto the screen only,
    with the new ranges;
  - the Images paragraph's sentence "six arguments are taken as the
    present below, which refuses an Image" stays; the quad draw's integer
    rule stays.

- **Timing and cost (plan §2).**
  - **Timing gate** for the new `make test` checks (the screen module's
    unit test and the screens CLI check): design.md's protocol, 11 rounds,
    both baselines (the "before" commit, H-3-5, and `8dff812`), ≤ 3 %;
    medians, spread, rounds, cores and the slowest new check's own time
    recorded in plan 6 §6. Each check stays under 100 ms.
  - **`make e2e` time**: in the same session, the "before" commit in its own
    worktree under `tmp/`, one warm-up each, then 5 alternating timed runs of
    `make e2e` per commit (`time`, wall and user + sys). Recorded: both
    wall medians.
  - **Per-frame cost**, measured once here and reused by steps 5, 7, 15
    and 16: the median over the 5 runs of this step's (user + sys) time of
    `make e2e`, divided by the 20 replays' frame count (4 072, H-3-6) and by
    the core count. Rationale: `make e2e` runs one job per replay on every
    core, longest first, so short added replays are paid as CPU spread over
    the cores; the measure includes start-up and make's own work, which
    overestimates the per-frame cost. The wall time divided by the frame
    count is recorded beside it for information.
  - **Touch-replay frame cap**, estimated once here and recorded in §6 as
    the cap step 15 keeps: **720 frames**, touch replays and their pad
    twins together, i.e. 360 per side: title to first slipway 180 (the
    `make e2e` limit without an exception; `pad_title` reaches a game in
    about 22 frames), aim pan on a diagonal 60, Back held to end the year
    60 (a 30-frame hold ends a year), show / hide by a gamepad button /
    show again 60.
  - **Gate**: this step's `make e2e` wall median + 720 × per-frame cost
    must be ≤ 8 s. The raster adds nothing to an e2e frame (no `screens`);
    the only new e2e work is the translucency scan. If the gate fails, the
    step first checks the scan's share (median with the scan disabled
    locally, not committed); if the scan costs more than 1 % of the median,
    the canvas store keeps a per-canvas "may hold a translucent pixel" flag,
    set by a translucent `clear` and cleared when a scan finds none, so an
    opaque canvas is never scanned; if the gate still fails, the cost is
    not this step's and the step records the numbers in §6 and stops for
    the plan's rework rule.

## Implementation Chunks

1. **PNG writer in the fake.** Move the writer into the PNG module with a
   boolean result; switch the five tests; a round-trip unit test (write,
   then load: same size and bytes, a 1×1 and a 390×844 image) in the PNG
   module's existing test. All tests green, outputs unchanged.
2. **Screen module.** The rule and its unit test (below). Not wired.
3. **Present widened and wired.** Image-rules position and scale rules
   (their unit test updated), the translucency refusal, `graphics_install`
   with the screen, the host owning the screen and beginning each frame;
   the `image_draw_log` fixture's present cases updated (half y, fractional
   scale and seventeen now accepted; zero, mirrored, above 1024 and a
   translucent canvas refused). `make e2e` and `make end-state` 20/20.
4. **`screens` and the PNGs.** Replay member and its reader tests; record
   flag in the frame input; the fake writing and removing PNGs; the screens
   CLI check with its fixture game and replays.
5. **Contract and measurements.** design.md; timing gate; `make e2e`
   medians, per-frame cost, cap and projection recorded in plan 6 §6.

## Testing Decisions

- A good test drives a module through its interface and compares exact
  bytes: a source, a draw and the resulting pixels for the screen module; a
  replay and a fixture game, then the decoded PNG, for the fake. No test
  reads the host's or the screen's internals, and no comparison has a
  tolerance. Expected frames are written before the code (TDD).
- **Screen module unit test** (new `test_fake_love_screen`, in
  `UNIT_TESTS`): sources whose texel `(i, j)` has a unique opaque colour, so
  every expected pixel names its texel. Cases: an unrecorded frame reads
  NULL and a draw is a no-op; a recorded frame starts `000000FF` at its
  size; scale 1 at integer position (identity); 160×120 at (0, 64.75) scale
  2.4375 in 390×844 with the landmarks of user story 32; a centre exactly
  on the right/bottom edge excluded and on the left/top edge included; a
  negative fractional position clipped; a scale below 1 (texels skipped);
  alpha `00` texels leave a previous draw; a later draw over an earlier
  one; a size change between recorded frames. Expected pixels are computed
  in the test by the per-pixel formula of design.md (straight loops over
  every pixel, not the module's tables), plus the named landmark asserts,
  which do not depend on the formula. Prior art: the canvas and point unit
  tests.
- **Screens CLI check** (new `test_fake_love_screens`, its own target in
  `FAKE_LOVE_CHECKS`, its binary in the fake-side lists), with a fixture
  game that creates canvas 0 (cleared to palette colour 0, so the frame
  check passes), a canvas 1 painted from an ImageData whose texel `(i, j)`
  is `(i, j, 0x80, FF)`, and a canvas 2 transparent but for a few opaque
  texels; it presents per a per-frame table. One replay, `window`
  390×844, `screens` listing:
  - f0: canvas 1 at (0, 64.75), scale 2.4375 — the plan's tall present;
  - f1 not listed (a present, no PNG must exist);
  - f2 with `resize` 640×480: canvas 1 at (−10.5, 7.25), scale 3.1;
  - f3: no present — all black;
  - f4: canvas 1 at (0, 0) scale 1, then canvas 2 at (100.5, 50.5) scale
    0.75 over it (alpha `00` leaves canvas 1's pixels, later over earlier).
  The test removes stale files, runs the fake once, decodes each PNG with
  the PNG module's loader and compares all four channels with the expected
  frame it builds (same per-pixel formula and landmarks), and asserts no
  PNG for f1. Refusal runs: each malformed `screens` of user story 23 (exit
  1, message, no capture, state or PNG); a game that fails at f2 after f0
  was written (no PNG left); a translucent canvas presented (the message
  with the pixel). Prior art: `test_fake_love_window`, `test_fake_love_set_color`
  (PNG comparison through the CLI helpers).
- **Updated tests**: the image-rules unit test (new position and scale
  verdicts), `test_fake_love_image_draw`'s expected log, the replay
  reader's test (`screens` accepted, queried, and each refusal), the PNG
  module's test (round trip).
- **Regression**: `make e2e` 20/20 match and `make end-state` 20/20; the
  capture programs unchanged (`love_capture_check` records no screen).
- **Timing gate** and **`make e2e` measurement** as decided above, recorded
  in plan 6 §6.

## Hypotheses on step 3 (not done yet)

- **H-3-1**: step 3 kept `host_frame` taking one frame-input struct and
  only added fields to it (the frame's touches and mouse); this step adds
  one boolean, and a zero-initialised struct, as `love_capture_check`
  passes, still means "nothing held, no touch, not recorded".
- **H-3-2**: step 3 kept the replay reader's table of optional top-level
  members (`initial_save`, `bindings`, `attract_after`, `window`) with
  "given twice" refused and other top-level members ignored, and the frame
  count as the number of objects in `frames` (touch-only and `{}` frames
  included); `screens` joins that table and is checked against that count.
- **H-3-3**: step 3 extended the fake command's per-frame input reader for
  touches and the mouse without changing the run loop's shape (frame, then
  capture append); this step adds the record flag there and the PNG write
  after the capture append.
- **H-3-4**: step 3's frame order is: resize and `love.resize`, then the
  touches, mouse and pad list made current, the pad callbacks,
  `love.update`, the transform reset, `love.draw`, the "Canvas is active"
  check. The screen's frame begins right after the resize, before any
  callback.
- **H-3-5**: step 3's final commit is this step's "before" commit, for the
  timing gate and the `make e2e` medians; its `test_fake_love_touch` check
  is in that baseline.
- **H-3-6**: step 3 changed no e2e replay and no game code; the 20
  replays still total 4 072 frames, counted as objects of `frames`.
- **H-3-7**: step 3 updated design.md's function list (touch and mouse),
  the replay members paragraph (four frame keys) and added a touch-and-mouse
  paragraph; this step edits those sections in place and does not touch the
  touch paragraph.
- **H-3-8**: after step 3, `tools/e2e_limits.sh` counts frames by objects
  of `frames` and does not refuse unknown top-level members, so it needs no
  change for `screens` (which no e2e replay carries).

## Out of Scope

- Quad draws onto the screen, any scaled quad, alpha blending of
  translucent texels, `pad_sheet`, the `newImage` translucency refusal
  (step 5).
- Points, rectangles and `clear` onto the screen: not modelled, now or
  later in this plan.
- The transform offset in the present, rotation, unequal scales.
- Any game change: `screen.present` at the layout's rectangle, `conf.lua`
  (step 7); the layout module (step 6).
- A real-LÖVE capture of the fractional present (the plan verifies its
  extensions by fake tests only).
- A check refusing `screens` in `tests/e2e/`.
- `highdpi`: one LÖVE unit is one screen pixel.

## Further Notes

- Decided without a guideline agent: the plan and the code settled every
  question. Choices this PRD made that the plan does not dictate: the lazy
  raster (only listed frames), the PNG name `<capture>.screen-f<N>.png`,
  the 16-entry and strictly ascending `screens`, the scale range
  (0, 1024], the translucency refusal at the present (rather than blending
  before step 5), double-precision arithmetic in the rule, the writer moved
  into the fake's PNG module, the screen module unit-tested on its own,
  the CPU-based per-frame cost and the 720-frame cap.
- On a 390×844 window at scale 2.4375 = 39/16 every quantity is exact in
  binary, so the landmarks are exact ties: texel edge `k` lies at
  `2.4375 k`, a pixel centre for `k ≡ 8 (mod 16)` on x (19.5 for k = 8), and
  `64.75 + 2.4375 k` is a pixel centre for `k ≡ 4 (mod 16)` on y (74.5 for
  k = 4).
- With a nearest filter set by `setDefaultFilter`, a GPU samples the texel
  under the pixel centre and includes centres on the top-left edges only,
  which is why the half-open floor rule is the natural contract.
