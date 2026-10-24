---
title: "Fake LÖVE: scaled quad draws onto the screen, alpha blending, and pad_sheet"
description: "The fake LÖVE cannot draw a sprite stretched by a fractional scale onto the screen, refuses every Image with a translucent texel, and cannot swap the pad's sheet, so no screen test of the virtual pad can be written."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) draws the virtual pad on the screen, after the
present, as sprites of `game/pad.png` cut by quads and **stretched into
fractional layout rectangles** (on a 390×844 iPhone the pad rectangle is
390×292.5 at y 486.75, scale 2.4375). Its screen tests compare whole
screens pixel for pixel, with tests running a flat-colour sheet
(`tests/fixtures/pad_flat.png`) in place of the real art. After step 4
(screen framebuffer, present at any offset and scale, `screens`), the fake
LÖVE (`tools/fake_love/`) still cannot support one of those tests:

- `draw(image, quad, x, y, 0, sx, sy)` does not exist: `draw` takes 4 or 6
  arguments, and the 4-argument quad draw takes integer positions and no
  scale;
- a quad draw with the screen selected "changes nothing the fake models":
  the screen framebuffer receives only the present, so a sprite never shows
  on a screen PNG;
- `newImage` refuses an ImageData holding any alpha other than `00` and
  `FF`, so a sprite with antialiased edges (alpha between 0 and 1) cannot
  even become an Image, and nothing defines how such a texel lands on the
  screen;
- a replay cannot say "use this sheet as `pad.png`": the only file a replay
  can put on the fake disk is `bindings.cfg`.

The plan lets this step choose between modelling translucent sprites on the
screen and proving the sprites opaque; that choice must be written in
`docs/love-port/design.md`.

## Solution

**Decision: the refusal is lifted for images drawn on the screen.** A
sprite at a 2.4375 scale on a phone looks far better with antialiased
edges, the step's title already asks for alpha blending, and the blend
arithmetic is already measured and written in design.md (llvmpipe's 8-bit
blender). The game's own assets (`sprites.png`, `font.png`) are RGB and
drawn only onto canvases, where nothing changes.

- **`newImage` accepts any alpha.** An Image remembers its first
  translucent texel (alpha neither `00` nor `FF`). Such an Image is drawn
  onto the screen only: a draw of it onto a canvas is refused at the call,
  naming the texel, because blending on a canvas is not modelled (the same
  reason a translucent colour is refused there since step 1).
- **The scaled quad draw** `draw(image, quad, x, y, 0, sx, sy)`, onto the
  screen only: any finite position of magnitude below 2^20 (fractions
  included), a rotation equal to 0, and two scales, each finite, greater
  than 0 and at most 1024, **equal or not** (a sprite is stretched into its
  rectangle). It requires the screen selected and a zero transform offset;
  any colour is accepted.
- **The 4-argument quad draw onto the screen** now reaches the screen too,
  at its translated integer position, as the scaled draw with scales 1 and
  1. Onto a canvas it is unchanged.
- **The rasterisation rule** of design.md's "The screen" paragraph (step 4)
  is extended from the present to every screen draw: destination rectangle
  `x … x + qw·sx` by `y … y + qh·sy`, a pixel belongs when its centre lies
  inside, half-open; it samples quad texel `(qx + floor((cx − x)/sx), qy +
  floor((cy − y)/sy))`, nearest.
- **Alpha blending, LÖVE's default `"alpha"` mode.** Each texel is first
  tinted by the current colour, all four channels, with the existing tint
  rule `round(c × t / 255)`; the tinted source `s` with alpha `a` then
  lands on screen pixel `d` with llvmpipe's arithmetic, already in
  design.md: per colour channel `min(255, mul(s, a) + mul(d, 255 − a))`,
  `mul(p, q) = (p·q + (p·q >> 8) + 128) >> 8`; the screen's alpha stays
  `FF`. With `a = FF` this writes `s`, with `a = 00` it leaves `d`, so the
  present of step 4 (opaque or transparent texels, white colour) gives the
  same bytes through the same code.
- **The present keeps refusing a translucent canvas** (premultiplied canvas
  content is a trap the plan does not need); only its message changes, since
  it no longer points at "step 5".
- **`"pad_sheet": "<file>"`**, a new optional top-level replay member: a
  `.png` file name next to the replay, read before `love.load` like
  `bindings`, and served on the fake disk as `pad.png`, in place of the game
  directory's `pad.png`. Without it, the game directory's file (the real
  sheet from step 9 on) is what the game loads.

The game does not change: it draws nothing on the screen but the present,
loads no `pad.png` yet, and no e2e replay carries `pad_sheet` or
`screens`, so the 20 replays keep their `*.sha256` and `*.end-state`.

## User Stories

1. As a plan-6 implementer, I want `draw(image, quad, x, y, 0, sx, sy)` onto the screen, so that each pad sprite can be stretched into its layout rectangle.
2. As a plan-6 implementer, I want fractional positions in the scaled draw, so that a sprite lands at y = 486.75 on a 390×844 window without rounding.
3. As a plan-6 implementer, I want fractional scales such as 2.4375 in the scaled draw, so that the pad scales with the game.
4. As a plan-6 implementer, I want unequal `sx` and `sy`, so that a sprite whose aspect differs from its rectangle's is stretched to fill it.
5. As a plan-6 implementer, I want a scale below 1, so that a large sprite can be drawn into a small rectangle.
6. As a plan-6 implementer, I want a sprite partly off the window clipped silently, so that the layout never has to clamp its rectangles.
7. As a plan-6 implementer, I want any colour accepted for a screen draw, translucent included, so that the pad could be tinted or faded later without a fake change.
8. As a plan-6 implementer, I want `newImage` to accept an ImageData with translucent alpha, so that `pad.png` can have antialiased edges.
9. As an artist, I want translucent pixels of `pad.png` blended onto the screen, so that the pad's edges look smooth over the game.
10. As a test author, I want the blend formula stated in design.md with integer arithmetic, so that my test computes every expected byte exactly, with no tolerance.
11. As a test author, I want the screen's alpha to stay `FF` after any blend, so that an expected frame is opaque everywhere.
12. As a test author, I want the tint applied to all four channels before the blend, so that a translucent colour over an opaque texel blends like a translucent texel.
13. As a test author, I want an alpha-`FF` source to write its bytes and an alpha-`00` source to leave the pixel, so that opaque sprites need no blend arithmetic in my expected frame.
14. As a test author, I want the scaled draw to sample the quad's own texels only (never a neighbour outside the quad), so that a sprite cut from a sheet never bleeds its neighbour.
15. As a test author, I want the half-open centre rule for scaled quads to be the present's rule, so that two abutting sprites never overlap and never leave a gap.
16. As a test author, I want later screen draws to lie over earlier ones, the present included, so that the pad drawn after the present covers the game.
17. As a test author, I want the 4-argument quad draw with the screen selected to reach the screen, so that every screen draw the game makes is visible in a screen PNG.
18. As a replay author, I want `"pad_sheet": "<file>"` next to the replay, so that a screen test runs with `pad_flat.png` and keeps passing when the real art changes.
19. As a replay author, I want `pad_sheet` served as `pad.png` through every reader of the fake disk (`newImageData`, `filesystem.read`, `getInfo`), so that the game cannot tell the sheet was swapped.
20. As a replay author, I want `pad_sheet` to shadow the game directory's `pad.png`, so that the real sheet never leaks into a flat-sheet test.
21. As a replay author, I want a replay without `pad_sheet` to load the game directory's `pad.png`, so that the default is the real art.
22. As a replay author, I want a malformed `pad_sheet` (not a string, not a `.png` name of 5–64 characters `A-Za-z0-9_.-`, starting with `.`, given twice) refused before `love.load` with the file, the byte offset and the problem, so that a typo never runs.
23. As a replay author, I want a `pad_sheet` file that is missing, unreadable or over 1 MiB refused before `love.load`, naming the path, with no output left, so that a run never uses a half-read sheet.
24. As a maintainer, I want a draw onto a canvas of an Image holding a translucent texel refused at the call, naming the texel and its bytes, so that the canvas never receives bytes the fake does not model.
25. As a maintainer, I want that refusal decided from a flag the Image computed once at `newImage`, so that the game's thousands of canvas draws per frame pay no scan.
26. As a maintainer, I want the scaled draw refused with a canvas selected, so that a scaled quad never reaches a canvas, where nearest filtering would drop or double texels.
27. As a maintainer, I want the scaled draw refused under a non-zero transform offset, as the present is, so that the pad's rectangle is exactly the layout's.
28. As a maintainer, I want the scaled draw refused for a rotation other than 0, a non-finite, zero, negative or over-1024 scale, a non-finite or far position, a non-Image, a non-Quad and a quad of another reference size, each with a message naming the argument, so that every mistake is caught at its line.
29. As a maintainer, I want a Canvas passed with seven arguments refused as "not an Image", so that the present keeps its single six-argument form.
30. As a maintainer, I want the argument-count message of `draw` to list the three forms, so that the error tells the game author what is accepted.
31. As a maintainer, I want the present's translucent-canvas refusal kept, with a message that no longer mentions a future step, so that the contract stays true after this step.
32. As a maintainer, I want every screen draw checked on every frame, rasterised only on frames `screens` lists, so that `make e2e` pays nothing for the raster.
33. As a maintainer, I want the screen module's single draw entry point to serve both the present and the quad draws, so that one rule, one tested piece of code, decides every screen pixel.
34. As a maintainer, I want the present's bytes unchanged by the generalised draw, so that step 4's screen tests pass untouched.
35. As a maintainer, I want the 20 replays and their end states unchanged, so that the game logic is proven untouched.
36. As a maintainer, I want design.md's Images, Image files, present, screen, Colour, replay members, run steps and function list paragraphs updated in this step, so that the contract stays the single description of the fake.
37. As a maintainer, I want design.md's asset rule rewritten ("an Image holding a translucent texel is drawn only onto the screen"), so that step 9's sheet rules follow from the contract.
38. As a maintainer, I want the step to add no `make test` process and to pass design.md's timing gate against its "before" commit, `8dff812` and the plan's start `8c830c6`, with the medians recorded in plan 6 §6, so that `make test` is no slower than before the plan.
39. As a maintainer, I want `make e2e`'s median of 5 runs measured beside the "before" commit, with the projection of 720 frames × 1.542 ms leaving at least 1 s under 9 s (or, in a slow session, a paired difference of at most 2 %), so that the 9-second limit is proven.
40. As the implementer of step 9, I want the sheets free to hold translucent pixels, so that step 9 adds no opacity check.
41. As the implementer of step 16, I want the fake to rasterise exactly what the pad will draw (scaled quads after the present, any colour), so that step 16 adds no fake change.

## Implementation Decisions

- **Opaque or translucent: translucent, lifted on the screen.** Recorded in
  design.md. Consequence for the plan: step 9's check gains no opacity
  test, and the asset rules of design.md change here, not at step 9.

- **Screen module** (step 4's, H-4-1): `screen_draw_canvas` is replaced
  by one draw function taking a source (RGBA8 pixels and size), a source
  rectangle (`qx`, `qy`, `qw`, `qh`, integers inside the source), a
  position `x`, `y`, two scales `sx`, `sy` (doubles) and a tint (four
  bytes). It does nothing on an unrecorded frame. The rule, in double
  precision: `x1 = x + qw · sx`, `y1 = y + qh · sy`; pixel `(i, j)`, centre
  `cx = i + 0.5`, `cy = j + 0.5`, belongs when `x <= cx < x1` and
  `y <= cy < y1`; its texel is `(qx + clamp(floor((cx − x) / sx), 0, qw −
  1), qy + clamp(floor((cy − y) / sy), 0, qh − 1))`; the tinted source is
  `s_k = round(c_k × t_k / 255)` for the four channels (the existing tint
  function); the pixel's colour channels become `min(255, mul(s_k, s_a) +
  mul(d_k, 255 − s_a))`, its alpha stays `FF`. Column and row tables are
  computed once per draw (O(width + height)); a source alpha `FF` may be
  copied and `00` skipped as shortcuts, which give the same bytes
  (`mul(p, 255) = p`, `mul(p, 0) = 0` for every byte). Step 4's assert that
  every texel is `00` or `FF` goes. The present calls it with the whole
  canvas, `sx = sy = s` and a white tint. The `mul` function is the one
  formula of design.md; if the code base has none yet, the screen module
  owns it.

- **Images (graphics module).**
  - `newImage(imagedata)` no longer refuses a translucent alpha; the Image
    keeps, beside its copied bytes, the index of its first translucent texel
    in row order (or none), found once with the existing translucent-pixel
    finder.
  - `draw(image, quad, x, y)` (4 arguments): unchanged checks; then, with a
    canvas selected, after the translucent-colour refusal, an Image holding
    a translucent texel is refused:
    `love.graphics.draw: the image's texel (<x>, <y>) is <RRGGBBAA>, translucent (an Image with translucent texels is drawn only onto the screen: blending on a canvas is not modelled)`.
    With the screen selected, it calls the screen module at the translated
    integer position with scales 1, 1 and the current colour.
  - `draw(image, quad, x, y, 0, sx, sy)` (7 arguments), checked in this
    order: argument 1 an Image (`argument 1 is a Canvas, not an Image` for a
    Canvas), argument 2 a Quad, arguments 3–7 numbers; 3 and 4 by the
    present's position rule (H-4-3); 5 equal to 0 with the present's
    rotation message; 6 and 7 by the present's scale rule (H-4-3), each on
    its own (unequal scales accepted); the quad's reference size equal to
    the image's; then the state: a canvas selected →
    `love.graphics.draw: a canvas is selected, a scaled Image is drawn only onto the screen (scaled draws onto a canvas are not modelled)`;
    a non-zero offset →
    `love.graphics.draw: the offset is (<dx>, <dy>), not (0, 0) (a scaled draw onto the screen is not translated)`.
    Any colour. Then the screen module draws it.
  - The count message becomes
    `love.graphics.draw: takes 4 arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7, an Image, a Quad, x, y, 0, sx and sy, not <n>`.
  - The present keeps its checks; its translucent-canvas message ends
    `(a translucent canvas is not modelled on the screen)` instead of
    naming step 5 (H-4-4).
  - Points, rectangles and `clear` with the screen selected still model
    nothing.

- **Replay reader.** New optional top-level member `pad_sheet`, in the
  start-state member table beside `bindings`: a string naming a `.png` file
  of 5–64 characters `A-Za-z0-9_.-`, not starting with `.`, given at most
  once; refused with the file, byte offset and
  `"pad_sheet" <value> is not a .png file name of 5-64 characters A-Za-z0-9_.- next to the replay`
  or `"pad_sheet" given twice`. A query returns the name, NULL when absent.
  The PNG is not decoded here: a bad PNG fails where the game loads it, as
  any file.

- **Fake command.** The disk builder reads the `pad_sheet` file next to the
  replay, as it reads `bindings` (shared helper, its own limit
  1 048 576 bytes), and puts its bytes in memory as `pad.png`, which shadows
  the game directory's file for `read`, `getInfo` and `newImageData`, as
  `bindings.cfg` and `save.bin` do. Refusals, before `love.load`, exit 1,
  no capture, state or PNG:
  `fake_love: <replay>: "pad_sheet": <path> cannot be read` and
  `fake_love: <replay>: "pad_sheet": <path> is over 1048576 bytes`.

- **No change** to `game/`, `tools/lua_guard.lua` (no new function name),
  `tools/e2e_limits.sh`, `love_capture_check` and the capture programs, or
  the 20 replays.

- **design.md** (`docs/love-port/design.md`), same step:
  - Images: the 7-argument form; the 4-argument form onto the screen; the
    translucent-image refusal moved from `newImage` to the canvas draw; the
    rule for the game rewritten: images drawn onto a canvas have alpha
    exactly 0 or 1, images drawn only onto the screen may hold any alpha;
  - Image files: "a translucent alpha loads … but `newImage` refuses it"
    and the asset rule ("make no Image from an ImageData holding a
    translucent alpha") rewritten; the pad sheets may be RGBA with any
    alpha;
  - The screen (step 4's paragraph): every screen draw, the quad rule with
    `sx`, `sy` and the quad origin, the tint and blend formula with the
    screen alpha staying `FF`, the decision "translucent sprites are
    modelled on the screen, not proven opaque";
  - Colour: "Onto the screen, any colour is accepted and the draw models
    nothing" becomes "… and the draw is tinted and blended (see The
    screen)";
  - The present: the reworded translucent refusal;
  - replay members and run step 2: `pad_sheet` served as `pad.png`;
  - the function list: `draw(image, quad, x, y, 0, sx, sy)` onto the screen
    only.

- **Timing and cost (plan §2).**
  - **No new `make test` target.** The plan's start commit `8c830c6` is
    now the binding baseline. The gates so far passed at +2.78 % and
    +2.49 % against their own "before" commits, and the total is measured
    against that start. So the step adds no process: its CLI cases go
    into the existing `test_fake_love_image_draw` (measured ~27–31 ms,
    three fake runs). It gains two short runs, a screen-draw replay and a
    missing-`pad_sheet` refusal, and stays near 50 ms, well under the
    100 ms budget. The other checks are extended in place: the screen unit
    test (in-process, a few ms), the replay reader's test and the updated
    logs. `test_fake_love_screens` (~70–107 ms) and `test_fake_love_files`
    (~100–112 ms) are already at the budget and get no case.
  - **Sum before the gate** (plan §2): the step records the new checks'
    own added times, about 20–25 ms of process time in total (≈ 0.7 % of
    `make test`), before running the gate.
  - **Timing gate**: design.md's protocol (rule 4), 11 rounds (22 pooled
    if needed), against three baselines: this step's "before" commit
    `8f4c7f6`, design.md's `8dff812`, and the plan's start `8c830c6`. Each
    paired median must be ≤ 3 %. Medians, spread, rounds, cores and the
    extended check's own time go in plan 6 §6. If any baseline fails,
    the step first drops the missing-`pad_sheet` run: the file read is the
    helper `bindings` shares, and `test_fake_love_files` already covers its
    refusal. Then, as the plan says, it reworks or merges checks, its own
    or earlier plan checks (for example merging `test_fake_love_screen`'s
    process into another unit binary's run), before it closes.
  - **`make e2e` time**: in the same session, `8f4c7f6` in its own
    worktree under `tmp/`, one warm-up each, then 5 alternating timed runs
    per commit (wall, and user + sys). Recorded in §6: both medians and
    the per-run differences. Gate: this step's median + 720 × 1.542 ms
    (1.110 s) must leave at least 1 s under 9 s, i.e. median ≤ 6.89 s
    (step 4: 6.756 s, a 0.13 s margin). **Slow session**: if `8f4c7f6`'s
    own projection already exceeds the limit in that session, the step is
    judged on the median of the per-run differences from "before", which
    must be ≤ 2 %. The note records both numbers and flags the budget for
    step 7. The new e2e work is one flag test per canvas quad draw (the
    Image's remembered translucent texel, no scan) and the 7-argument
    dispatch. The game draws nothing new and no replay lists `screens`.
    No raster, scan or allocation is added on an e2e frame, so a
    measurable cost is a bug, fixed before the step closes.

## Implementation Chunks

1. **Screen module generalised.** The quad, two-scale, tinted and blended
   draw, with its unit tests (below); the present switched to it. Step 4's
   screen tests and `make e2e` unchanged.
2. **Images on the screen.** `newImage` lifted with the remembered texel,
   the canvas refusal, the 4-argument screen path, the 7-argument form, the
   new messages; `image_draw_log` and `image_files_log` expectations
   updated. `make e2e` and `make end-state` 20/20.
3. **`pad_sheet`.** Replay member and its reader tests; the disk builder
   serving `pad.png`; its refusals.
4. **Screen draw cases in `test_fake_love_image_draw`.** The fixture
   game, the committed sheet PNGs, the replay, the expected frames built by
   the test, and the missing-`pad_sheet` refusal run.
5. **Contract and measurements.** design.md; timing gate; `make e2e`
   medians and projection recorded in plan 6 §6.

## Testing Decisions

- A good test drives a module through its interface and compares exact
  bytes; no tolerance; expected frames written before the code (TDD),
  computed by the test with straight per-pixel loops over the formula of
  design.md, plus named landmark pixels that do not depend on the formula.
- **Screen module unit test** (step 4's, extended, no new target): a source
  whose texel `(i, j)` has a unique opaque colour; cases: a quad of a
  larger source stretched by a fractional, non-dyadic, unequal scale at a
  fractional position (only the quad's texels appear, column and row
  landmarks at the half-open ties, e.g. `sx = 2.4375` with a texel edge at
  a pixel centre); a scale below 1 on one axis only; a quad clipped on the
  left and the bottom; a source with alphas `01`, `40`, `7F`, `80`, `C0`,
  `FE` over a known backdrop, each expected byte computed by the `mul`
  formula and a few written out as literals; a tint with alpha `80` over an
  opaque texel; alpha `00` leaves, `FF` writes; the present case of step 4
  giving identical bytes through the new entry point; the screen alpha `FF`
  after every blend.
- **Screen draw cases in the existing `test_fake_love_image_draw`** (no
  new target, no new binary; see Timing and cost). Two runs are added to
  that test. The first uses a new fixture game directory with its own
  `pad.png` (opaque, in colours absent from the sheet). The game creates
  canvas 0 cleared to a palette colour, an image from a generated
  ImageData of unique opaque texels, and
  `newImage(newImageData("pad.png"))`. One replay, `window` 390×844,
  `pad_sheet` naming a committed sheet next to it (a small RGBA PNG holding
  the six translucent alphas above, alpha `00` and `FF`, made once with an
  outside tool and committed), `screens` listing every frame:
  - f0: the generated image's quad `(1, 1, 3, 2)` at (10.25, 7.6),
    `sx` 2.4375, `sy` 1.7, and its whole quad at (−3.5, 20.5), scales 0.75
    and 3 — the fractional stretch, unequal scales, clipping;
  - f1: the present of canvas 0 at (0, 64.75) scale 2.4375, then the sheet's
    sprites stretched over it by 10 and 10 at (5.5, 70.5), then one sprite
    under `setColor(0.5, 1, 0.25, 0.5)`, then a 4-argument draw under
    `translate(3, 4)` — the sheet served in place of `pad.png` (its bytes,
    never the game directory's), blending over the present, the tint, the
    integer screen path;
  The test decodes each PNG with `png_file_load` and compares all four
  channels with the frame it builds. The second run is a refusal: a
  replay whose `pad_sheet` file is missing (exit 1, the message, no
  capture, state or PNG). Both runs reuse the test's existing paths and
  helpers. Prior art: `test_fake_love_screens` (H-4-6) for the PNG
  comparison, `test_fake_love_files` for the `bindings` refusal.
- **Updated tests**: `test_fake_love_image_draw`'s expected log (the
  translucent `newImage` now `ok`; new lines for a translucent Image drawn
  onto a canvas, the 7-argument form's refusals one by one, a Canvas with
  seven arguments, the new count message, the present's reworded
  translucent message); `test_fake_love_image`'s `image of rgba` line now
  succeeds; the replay reader's test (`pad_sheet` accepted, queried, and
  each refusal).
- **Regression**: `make e2e` 20/20 and `make end-state` 20/20; step 4's
  screen tests unchanged; the capture programs unchanged.

## Step 4 outcome (hypotheses, now confirmed)

Step 4 is committed (`0c8a5f2` … `8f4c7f6`, plan 6 §6 "Step 1.4"). Each
hypothesis was checked against the committed code and note.

- **H-4-1, confirmed**: the screen module exists with `screen_create`,
  `screen_free`, `screen_begin_frame(width, height, recorded)` (`000000FF`,
  no buffer until a frame is recorded), `screen_draw_canvas(source, x, y,
  scale)` (it asserts every texel is `00` or `FF`) and `screen_read_pixels`.
  This step replaces `screen_draw_canvas` with the generalised draw and
  keeps the rest.
- **H-4-2, confirmed**: design.md "The screen" states the half-open centre
  rule and the clamped floor texel, in double precision. It is extended in
  place.
- **H-4-3, confirmed**: `image_rules_check_present_position` and
  `image_rules_check_present_scale` exist. The scaled quad draw reuses
  them. Their names say "present", so they may be renamed to a
  screen-draw name if the implementer finds that clearer. The 4-argument
  draw keeps `image_rules_check_position`.
- **H-4-4, confirmed**: the present refuses a translucent canvas with
  `… translucent (blending onto the screen is not modelled before plan 6 step 5)`.
  The new ending is `(a translucent canvas is not modelled on the screen)`.
- **H-4-5, confirmed**: `screens` (at most 16, ascending),
  `<capture>.screen-f<N>.png` removed on a later failure, `png_file_write`
  and `png_file_load` in the fake.
- **H-4-6, confirmed**: there is the unit test `test_fake_love_screen`
  (~22–33 ms) and the CLI check `test_fake_love_screens` (~70–107 ms, at the
  100 ms budget, so it is **not extended**). Its PNG decode and compare
  helpers are local to that test. This step moves them into the shared CLI
  helpers if a second test needs them; otherwise it copies nothing.
- **H-4-7, confirmed**: `8f4c7f6` is this step's "before" commit. The
  per-frame cost is 1.542 ms and the touch-replay cap is 720 frames. The
  projection is 7.87 s, which leaves 1.13 s under 9 s: 0.13 s of margin.
- **H-4-8, confirmed**: `graphics_install` takes the screen (an upvalue)
  and the host passes `record_screen` in the frame input. The 4-argument
  draw with the screen selected passes its checks, then
  `canvas_draw_texels` changes nothing.
- **H-4-9, confirmed**: no e2e replay changed and none carries `screens`.
- **H-4-10, confirmed**: the disk builder (`initial_save`, `bindings`) and
  the start-state member table (now with `screens`) are where `pad_sheet`
  joins.

## Out of Scope

- Blending on a canvas, translucent colours or images onto a canvas.
- Translucent canvases in the present; the present's other rules.
- Blend modes other than `"alpha"`, `setBlendMode`, rotation, origin and
  shear arguments, a transform offset under a scaled draw.
- Points, rectangles and `clear` onto the screen.
- `game/pad.png`, its quad table and `pad_flat.png` (step 9); the layout
  (steps 6, 10); any drawing of the pad (step 16); any game change.
- A real-LÖVE capture of scaled quads or blending (the plan verifies its
  extensions by fake tests only).
- Linear filtering: the fake models nearest only (the game sets it).

## Further Notes

- Decided without a guideline agent: the plan offers the choice, and the
  step title (alpha blending), the look on a phone and the already-measured
  blend formula settle it. Choices this PRD made that the plan does not
  dictate: translucency lifted at `newImage` and refused at the canvas draw
  from a per-Image flag; unequal scales accepted; the 4-argument draw
  rasterised on the screen; a zero offset required for the scaled draw; the
  present's translucency refusal kept; `pad_sheet` served in memory as
  `pad.png` with a 1 MiB limit and the bindings name rules with `.png`; no
  new `make test` process: the CLI cases join `test_fake_love_image_draw`,
  the cheapest image check, because `test_fake_love_screens` and
  `test_fake_love_files` are at the 100 ms budget.
- Why the screen's alpha stays `FF`: LÖVE's `"alpha"` mode gives alpha
  `a + mul(d_a, 255 − a)`, and with `d_a = FF`, `mul(255, q) = q` for every
  byte `q`, so the sum is 255.
- Real LÖVE tints in the shader in floats and converts to 8 bits before the
  blender; the fake's `round(c × t / 255)` then `mul` is the plan's
  contract, verified by fake tests only.
