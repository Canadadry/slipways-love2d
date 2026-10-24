---
title: "Window layout, and the make test budget for the rest of plan 6"
description: "Nothing says where the 160×120 canvas and the virtual pad go in a window of any size, and the ~1.7 points of make test headroom left against 8c830c6 have not been shared out among the checks steps 6–16 will add."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) runs the game on a portrait iPhone and in a
resizable desktop window. §2 "Layout" fixes the rule: two modes chosen from
the window size every frame (**tall** when `height > 2 * width`, **overlay**
otherwise), the game fitted at maximum size and centred, the pad designed in
a 160×120 reference rectangle and fitted the same way, rectangles never
rounded. Today the game knows one size only: `screen.present` draws canvas 0
at `(0, 0)` and the whole-number `screen.SCALE` (4) into a fixed 640×480
window. No code computes a mode, a fractional scale or a centred rectangle,
so step 7 (the present into the game rectangle) and step 10 (the pad's
element rectangles) have nothing to build on, and the exact values §2 states
for a 390×844 iPhone are not checked anywhere.

A second problem comes first. After step 1.5 the `make test` timing gate
stands at **+1.29 % against the plan's start, 8c830c6**: about **1.7
points** of the 3 % are left for every check steps 6–16 still add (a Lua
test of step 6, the first checks that boot the game at step 7, the
completeness check of step 8, the sheet test of step 9, the pure-Lua tests
of steps 10–13, the six pad screen cases of step 16). If each step spends
what it needs, the last ones find no budget and must rework checks already
committed. The plan's step 6 therefore first measures the costs at `-O2`,
lists every planned check with its estimate, and decides their grouping
before any of them is written.

## Solution

**1. A budget plan for steps 6–16, made first.** The implementer measures,
at the committed `-O2` build:

- one LuaJIT process running a `tests/lua/game/` test the way `make test`
  runs it;
- one short fake run that **boots the game** with `screens` (a replay of a
  few empty frames, `window` 390×844, one or two `screens` frames), split
  into boot, per frame, and per 390×844 screen PNG written and compared;
- a fake run of a tiny program that boots no game (as
  `tests/fixtures/fake_love/image_draw_screens`), for the sheet test;
- the conversion from a check's own time to gate points on this machine
  (step 1.4: one ~80 ms check ≈ 2.5 %; a check's CPU time divided by the
  core count and `make test`'s wall time, checked against that point).

It then lists every check steps 6–16 add, with its estimate and its
grouping, against the headroom left, and **reports the list to the
coordinator, who writes plan §6** (the implementer does not edit §6). The
grouping decided here is binding for later steps; each later step's note
updates the remaining headroom.

Pre-measurement made while writing this PRD (4 cores, `make test` ≈ 0.87 s
wall; indicative, the step re-measures):

| Measured | Wall |
|---|---|
| LuaJIT test process through `sh`, requiring `check` and `p8.vram` | 4–5 ms |
| Fake booting the game, 2 frames, one 390×844 screen | ~46 ms |
| same, 10 frames, one screen | ~70 ms |
| same, 30 frames, one screen | ~89 ms |
| same, 12 frames, six 390×844 screens | ~95–120 ms |
| Fake on `image_draw_screens` (no game, two 390×844 screens) | ~15 ms |

So the game boot costs ~40 ms, a title-screen frame ~1 ms, a 390×844
screen PNG ~5–8 ms; and 1.7 points are worth roughly 55–60 ms of new check
time in all. That does not cover a naive plan (two game-booting screen
targets at ~60 and ~110 ms alone are ~5 points).

**The grouping decided here** (the step confirms it with its own numbers,
or changes it and says why):

| Step | Check | Grouping | Estimate (own time) |
|---|---|---|---|
| 6 | window layout tests | one runner process for all `tests/lua/game/` files (below) | ~1 ms over today's process |
| 7 | present at 640×480 and 390×844 | **one** screen-test target, **one** replay booting the game once: start 390×844, `screens` on a frame there, then `resize` to 640×480 and a second `screens` frame | ~55 ms |
| 8 | completeness (every require and PNG under `game/`) | folded into `lua-guard`, which already reads every `game/` file in one process; no new process | ~1–2 ms |
| 9 | static sheet test | a fake run of a tiny program (no game boot) inside an existing fake C check's process, as step 5 did for `test_fake_love_image_draw` | ~10–15 ms |
| 10–13 | pad elements, stick, touch → mask, show / hide | files in `tests/lua/game/`, run by the same single runner; no new process | ~1–3 ms |
| 14, 15 | wiring, touch replays | `make e2e` only | 0 |
| 16 | six pad screen cases | extra frames and `screens` entries of **step 7's replay** (one boot; at most 16 `screens`); every case but those that must be 390×844 at a small dyadic window (e.g. 200×420 tall, scale 1.25; 320×240 overlay, scale 2), since PNG cost grows with the pixel count | ~25–35 ms |

Total ≈ 95–110 ms, about 2.7–3.2 points: **over the 1.7 points left by
~1–1.5 points.** The step therefore also names where that point comes
from, chosen by measurement among, in order:

1. plan 6's own earlier checks (the plan allows reworking them): the
   per-process and duplicated fake runs of `test_fake_love_window`,
   `test_fake_love_touch`, `test_fake_love_screens` and the
   `test_fake_love_screen` unit test (warn-review already flagged
   `_screen` vs `_screens`): merging two or three of them into one process,
   or dropping fake runs one test of another already covers;
2. cheaper game boots for the screen replay (a replay member that already
   exists and shortens the boot, if the measurement shows one; no fake
   change: its format is frozen after phase 1);
3. the per-check 100 ms budget (design.md rule 2) is respected: if step 7's
   replay with step 16's frames would pass 100 ms, it is split into two
   targets only if the reclaimed headroom pays for the second boot.

Each choice goes in the report with its measured saving. The reclaiming
work itself is done by the step that needs it (step 7 at the latest);
this step only decides and reports it, unless the layout tests below would
already push the gate over 3 %.

**2. One runner for the game's Lua tests.** The `tests/lua/game/` files run
in **one** LuaJIT process: a small runner in that directory runs each
`test_*.lua` in turn (sorted), clearing every `package.loaded` entry the
previous file added (the pattern `tests/lua/test_modules.lua` already
uses), so each file stays per module and isolated. `make test` pays one
LuaJIT start for all of them, today's `test_economy_formulas` included;
steps 10–13 then add tests, not processes. The runner exits 1 when any
file fails, naming the file and the test.

**3. The window layout module.** A new pure-Lua module, required as
`love.window_layout`, computes from a window size (LÖVE units, as
`love.graphics.getDimensions` gives them) one layout value:

- the **mode**: tall or overlay;
- the **game rectangle**: where canvas 0 is drawn, with its scale (window
  units per canvas pixel);
- the **pad rectangle**: where the pad's 160×120 reference rectangle is
  drawn, with its scale (window units per reference unit).

Rules (§2, restated as the module's contract):

- **Fit.** Content of size `cw × ch` fitted into an area `(ax, ay, aw, ah)`
  takes `scale = min(aw / cw, ah / ch)`, `width = cw * scale`,
  `height = ch * scale`, `x = ax + (aw - width) / 2`,
  `y = ay + (ah - height) / 2`. Nothing is rounded.
- **Tall** (`height > 2 * width`): the window is cut into two halves of
  height `height / 2` each (fractional when `height` is odd). The game fits
  the top half `(0, 0, width, height / 2)`; the pad fits the bottom half
  `(0, height / 2, width, height / 2)`.
- **Overlay** (otherwise, `height == 2 * width` included): the game fits the
  whole window; the pad rectangle is exactly the game rectangle.

Expected values (all dyadic, so exact in double precision):

| Window | Mode | Scale | Game x, y, w, h | Pad x, y, w, h |
|---|---|---|---|---|
| 390×844 | tall | 2.4375 | 0, 64.75, 390, 292.5 | 0, 486.75, 390, 292.5 |
| 640×480 | overlay | 4 | 0, 0, 640, 480 | same as game |
| 844×390 | overlay | 3.25 | 162, 0, 520, 390 | same as game |
| 820×1180 | overlay | 5.125 | 0, 282.5, 820, 615 | same as game |
| 390×780 (`h == 2w`) | overlay | 2.4375 | 0, 243.75, 390, 292.5 | same as game |
| 390×781 (`h == 2w + 1`) | tall | 2.4375 | 0, 49, 390, 292.5 | 0, 439.5, 390, 292.5 |

Nothing calls the module in this step: the game, `screen.present`,
`conf.lua` and the fake are unchanged, so the 20 replays keep their
`*.sha256` and `*.end-state` trivially. Step 7 wires the present to the
game rectangle; step 10 places the pad's elements in the pad rectangle.

## User Stories

1. As a phone player holding the iPhone in portrait, I want the game in the top half of the screen, so that my thumbs on the pad never cover it.
2. As a phone player, I want the game as large as the screen width allows, so that 160×120 pixels are readable on a 390-point-wide screen.
3. As a phone player, I want the game below the Dynamic Island (y 64.75 on 390×844), so that no part of it is hidden by the camera cut-out.
4. As a phone player, I want the pad above the home indicator (bottom at y 779.25 on 390×844), so that a swipe on the pad never sends the app to the background.
5. As a phone player, I want the game not to move when the pad shows or hides, so that my eyes never lose the cursor.
6. As a desktop player in the default 640×480 window, I want the game filling the window exactly at scale 4, so that the desktop look is unchanged from today.
7. As a desktop player who resizes the window, I want the game fitted at maximum size and centred, so that no part of it is cut off and the bars are even on both sides.
8. As a desktop player in a very wide window, I want black bars left and right, never a stretched game, so that pixels stay square.
9. As a desktop player in a tall narrow window, I want overlay mode until the window is more than twice as tall as wide, so that the desktop never splits the window by surprise.
10. As an iPad player, I want overlay mode, so that the pad lies over the game as on desktop.
11. As a landscape iPhone player, I want overlay mode with the game centred (x 162 on 844×390), so that the game uses the full height.
12. As a player in overlay mode, I want the pad drawn exactly over the game's rectangle, so that the pad never reaches into the black bars.
13. As a player on any window size, I want fractional scales, so that there are never black bars caused by whole-number scaling.
14. As the implementer of step 7, I want the game rectangle's `x`, `y` and `scale`, so that `screen.present` draws canvas 0 with one `draw(canvas, x, y, 0, scale, scale)`.
15. As the implementer of step 7, I want the rectangle's width and height to be exactly `160 * scale` and `120 * scale`, so that the rectangle is exactly what the present covers, never one ulp off.
16. As the implementer of step 10, I want the pad rectangle with its own scale, so that each element's screen rectangle is `pad.x + ux * pad.scale`, `pad.y + uy * pad.scale`.
17. As the implementer of step 10, I want the pad's reference size exported as constants, so that the element table and the layout agree on 160×120 units.
18. As the implementer of steps 12 and 16, I want the mode in the layout value, so that the pad's hit tests and drawing use the same rectangle the layout chose.
19. As the implementer of a later `resize` screen test, I want the layout recomputed from any size on every call with no cached state, so that a resize changes the rectangles on the very next frame.
20. As a test author, I want the layout pure (no `love` global, no state), so that it is tested under plain LuaJIT in a few milliseconds.
21. As a test author, I want every §2 value checked with exact equality, so that a rounding slipped into the layout fails at once.
22. As a test author, I want the boundary `height == 2 * width` to be overlay and one unit taller to be tall, so that the strict inequality is pinned from both sides.
23. As a test author, I want an odd window height in tall mode to give fractional halves, so that the "never rounded" rule is checked on the halves too.
24. As a maintainer, I want a non-positive or non-numeric size refused with an assert naming the argument, so that a broken `getDimensions` stub fails at the call instead of drawing at infinity.
25. As a maintainer, I want the two rectangles returned as separate tables, so that a caller changing one never changes the other in overlay mode.
26. As a maintainer, I want the module named apart from the existing `game.layout` (the in-cart UI rows), so that a reader never confuses the window layout with the cart's row layout.
27. As a maintainer, I want the module under `game/love/`, beside `love.screen`, so that the LÖVE-side presentation code stays in the directory that draws through LÖVE and depends on nothing in the game.
28. As the coordinator, I want every check steps 6–16 will add listed with a measured or estimated cost against the headroom left versus 8c830c6, so that I can write §6 and no later step discovers the budget is gone.
29. As the coordinator, I want the grouping of those checks decided now (one game boot for all screen cases, Lua tests in one process, step 8 and 9 folded into existing processes), so that each later step knows where its check goes.
30. As the coordinator, I want the shortfall named with where its saving comes from, so that the plan's "no slower than before the plan" holds by construction, not by luck at step 16.
31. As the implementer of step 7, I want the cost of booting the game in the fake measured at `-O2`, so that I know my screen replay's price before writing it.
32. As the implementer of step 16, I want my six cases planned as frames of step 7's replay at small dyadic windows, so that they cost frames and PNGs, not another boot.
33. As the implementer of steps 10–13, I want my Lua tests to join one runner process, so that adding a test file costs no process start.
34. As a test author, I want each game Lua test file run with a fresh `package.loaded`, so that one file's module state never leaks into another's results.
35. As a test author, I want the runner to name the failing file and test, so that one process for all files reports as precisely as one process per file did.
36. As a maintainer, I want the `lua-test-game` help line to say "one process", so that `make help` stays true.
37. As a maintainer, I want the timing gate run against the step's "before" commit, 8dff812 and 8c830c6, with the numbers reported to the coordinator, so that the headroom left after step 6 is known.
38. As a maintainer, I want the 20 e2e replays unchanged, so that the game logic is proven untouched.
39. As a maintainer, I want design.md to state the layout rule once, so that steps 7, 10 and 16 cite one description.

## Implementation Decisions

- **Budget plan first, as a report.** The measurements and the table of
  checks (step, check, grouping, estimate, points) go in the step's final
  report to the coordinator, with the conversion used and the shortfall's
  planned source. The implementer does not edit plan §6; the coordinator
  writes it. Scratch measurement replays are not committed (they live under
  `tmp/` or the scratchpad); the measuring commands are given in the report
  so the next step can rerun them.

- **Grouping (binding for steps 7–16, revisable only by a later step's
  measurement, said in its note):**
  - all `tests/lua/game/` tests in one runner process (decided now,
    built in this step);
  - all game-booting screen cases (step 7's two presents and step 16's six
    cases) in **one replay, one boot**, in one `FAKE_LOVE_CHECKS` target,
    placed early in `test`'s prerequisite list (design.md rule 1: a long
    check goes early); step 16 appends frames and `screens` entries (at most
    16 per replay) and uses small dyadic windows where a case does not need
    390×844;
  - step 8's completeness check inside `lua-guard`;
  - step 9's sheet test as a non-game fake run inside an existing fake C
    check's process;
  - the shortfall (estimated 1–1.5 points) recovered from plan 6's earlier
    checks first (see Solution), by the step that adds the cost.
  The coordinator's example "step 9's sheet test as a `screens` frame of a
  screen replay" was weighed and not taken: the game never draws every
  sprite of the sheet at fixed positions, so it would need a game-side
  debug path; a non-game fake run in an existing process costs ~10–15 ms
  and no game code.

- **The runner.** A small Lua file in `tests/lua/game/` (not matching
  `test_*.lua`), which lists the `test_*.lua` files it is given (the
  Makefile passes the wildcard, so a new file needs no edit), and for each:
  snapshots `package.loaded`, runs the file, and removes every entry the
  file added. `check.run` stops calling `os.exit` per file; it returns the
  failure count (or raises), and the runner exits 1 when any file failed,
  each failure printed as `<file>: <test>: <error>`. The `lua-run-%`
  pattern and `LUA_GAME_TEST_RUNS` are replaced by one target (the
  existing `lua-test-game` name, now a direct prerequisite of `test`),
  whose recipe runs `luajit` once. Its dependencies stay the test files,
  `check.lua` and `LUA_GUARD_FILES`.

- **Module and place.** `love.window_layout`, in the `love/` directory
  beside `love.screen`. `game.layout` already exists (the cart's row and
  tooltip layout), so "layout" alone is taken; `window_layout` says what is
  laid out. It goes in `love/` rather than `game/` because its users are the
  present (`love.screen`, step 7) and the pad's drawing and hit tests, all
  on the LÖVE side of the canvas; `love/` modules depend only on `p8/`, and
  this one requires at most `p8.vram` (for the canvas size). It calls no
  `love.*` function: the caller passes the size.

- **Interface.**
  - `window_layout.compute(width, height)` → a new table
    `{ mode = ..., game = rectangle, pad = rectangle }`, where each
    rectangle is `{ x = ..., y = ..., width = ..., height = ..., scale = ... }`.
    Every call returns fresh tables; the module holds no state.
  - `window_layout.MODE_TALL = "tall"`, `window_layout.MODE_OVERLAY =
    "overlay"`: constants in the codebase's `UPPER_CASE` style, with string
    values so a failed `expect` prints a readable mode.
  - `window_layout.PAD_WIDTH = 160`, `window_layout.PAD_HEIGHT = 120`: the
    pad's reference rectangle (§2). They equal `vram.SCREEN_WIDTH/HEIGHT`
    but are their own constants: the pad's design units are not the canvas.
    The game rectangle fits `vram.SCREEN_WIDTH × vram.SCREEN_HEIGHT`.
  - Names follow CLAUDE.md: no module prefix in exports (`compute`, not
    `compute_window_layout`), full words for fields (`width`, `height`,
    `scale`, never `w`, `h`, `s`).

- **Arithmetic.** In double precision, in this order, for each rectangle:
  `scale = min(area_width / content_width, area_height / content_height)`,
  then `width = content_width * scale`, `height = content_height * scale`,
  then `x = area_x + (area_width - width) / 2`,
  `y = area_y + (area_height - height) / 2`. Width and height are derived
  from the scale, never copied from the area: the rectangle must equal the
  extent `draw(..., scale, scale)` covers. In tall mode the half height is
  `height / 2`, unrounded, and the bottom half starts at `height / 2`.

- **Mode test.** `height > 2 * width` → tall, else overlay. Recomputed on
  every call (§2: "recomputed every frame"); no hysteresis.

- **Overlay pad.** The pad rectangle has the same five values as the game
  rectangle, in a separate table (story 25). Because the pad reference and
  the canvas are both 160×120, the two scales are always equal in both
  modes; the module still computes the pad's own fit, so a future change of
  the pad's reference size needs no rewrite.

- **Validation.** `compute` asserts that `width` and `height` are numbers
  greater than 0, with a message naming the module, the argument and its
  value. Integers are not required (real LÖVE with `highdpi` may scale
  units; the fake gives integers 1–4096).

- **Units.** LÖVE units, as `love.graphics.getDimensions` returns them;
  `highdpi` (step 7's `conf.lua`) is not this module's concern.

- **design.md.** A short paragraph is added under "How the Lua game draws":
  the two modes, the fit rule, "never rounded", the overlay pad equal to the
  game rectangle, and that `love.window_layout` computes them. It does not
  yet say the present uses it (step 7 says that). The `make test` section
  (rule 1's list of what runs as its own process) is updated for the single
  game Lua test process. The fake's contract does not change.

- **Timing gate for this step.** With the runner, the step's net change to
  `make test` is one process fewer per future game test file and ~1 ms of
  new tests in an existing process; the gate is expected flat or slightly
  better. It runs anyway (design.md rule 4, 11 rounds, 22 if needed)
  against the "before" commit, 8dff812 and 8c830c6, and its plain and
  paired medians, spreads, round count, core count and the runner's own
  time go in the report, with the headroom left versus 8c830c6.

- **Hypotheses on step 5 — all confirmed** against the committed code
  (commits f7e32f5…e95bbfc) and plan 6's "Step 1.5" note:
  - **H-5-1 (confirmed)**: step 5 added no Lua module to `game/` (no
    `game/` file changed since 8f4c7f6), so no `love.window_layout`;
    `screen.present` still draws at `(0, 0)` and `screen.SCALE` 4, and
    `conf.lua` is still 640×480, not resizable.
  - **H-5-2 (confirmed)**: no new `make test` process (its screen cases
    run inside `test_fake_love_image_draw`); the `tests/lua/game/` rule is
    unchanged. The only Makefile changes are `-O2` in the default
    `CFLAGS` and object lists.
  - **H-5-3 (confirmed)**: step 5 passed its gate after `-O2` and
    recorded +1.29 % against 8c830c6, so ~1.7 points are left; this is the
    budget the plan of chunk 1 shares out.
  - **H-5-4 (confirmed)**: `vram.SCREEN_WIDTH/HEIGHT` are 160×120 and
    `p8.vram` loads under plain LuaJIT.
  - **H-5-5 (confirmed)**: `lua-guard`, `lua-test-modules` and
    `make lua-names` are unchanged and load every `game/` module under
    plain LuaJIT.

## Implementation Chunks

1. **Budget plan (no committed code).** Measure, at `-O2`, a game Lua
   test process, a game-booting fake run with `screens` (boot, per frame,
   per 390×844 screen), a non-game fake run with screens, and the
   points-per-ms conversion; produce the table of every check of steps
   6–16 with estimate, grouping and points against the headroom left
   versus 8c830c6, with the shortfall's source. Kept for the final report.
2. **One runner for `tests/lua/game/`.** The runner, `check.run`
   returning instead of exiting, the Makefile's single target, design.md's
   rule-1 wording; `test_economy_formulas` passes through it, and a
   deliberately failing scratch file (not committed) shows the file and test
   named and exit 1.
3. **`love.window_layout` and its tests** (TDD), plus the design.md
   layout paragraph.
4. **Gate.** The timing gate of design.md rule 4 against "before",
   8dff812 and 8c830c6; `make test-all` green; numbers into the report.

## Testing Decisions

- **What a good test is here:** it calls `window_layout.compute` with a
  window size and compares the returned mode and the ten rectangle values
  with exact `==` against the values above. It checks results, never how
  they are computed (no test of a private fit function). Every expected
  value in the table is dyadic, so exact equality is meaningful; no
  tolerance (§1).

- **Tested module:** `love.window_layout`, in one new file named after it
  in `tests/lua/game/`, using `check.expect` like `test_economy_formulas`,
  written first (TDD), run by the new runner.

- **Cases** (the plan's list, plus the boundary's other side):
  1. 390×844: tall, the exact values of §2 (game y 64.75, pad y 486.75,
     390×292.5, scale 2.4375), and the pad's bottom 779.25.
  2. 640×480: overlay, scale 4, the whole window, pad equal to game.
  3. 844×390: overlay, scale 3.25, x 162, height-limited.
  4. 820×1180: overlay (1180 < 1640), scale 5.125, y 282.5, width-limited.
  5. 390×780 (`height == 2 * width`): overlay, y 243.75.
  6. 390×781: tall, halves of 390.5, game y 49, pad y 439.5.
  7. Overlay: the pad table is a different table from the game table with
     equal values.
  8. Two calls with different sizes: the second result does not depend on
     the first (no state).
  9. `compute(0, 480)`, `compute(640, -1)` and `compute("640", 480)` raise
     an error naming the argument.

- **The runner** is checked by `make test` itself (both files pass
  through it) and once by hand with a failing scratch file; no committed
  self-test (it would be another process).

- **Prior art:** `tests/lua/game/test_economy_formulas.lua` (pure module,
  `check.lua` helpers); `tests/lua/test_modules.lua` (resetting
  `package.loaded` between modules in one process).

- **No screen test, no fake C test, no e2e change** in this step: the
  module has no caller yet. `make e2e` is not re-timed (no per-frame
  change; §2 lists steps 4, 5, 7, 14–16 only).

- **Check of the step:** the budget table is in the report; the layout
  tests pass in `make test` through the runner; the 20 replays unchanged
  (`make e2e` green); `lua-guard`, `lua-test-modules` and `make lua-names`
  green; design.md states the layout and the single game Lua test process;
  the timing gate passes and its numbers are in the report.

## Out of Scope

- Writing plan §6 (the coordinator does, from the report).
- Building any check of steps 7–16, and the reclaiming of earlier checks
  the budget plan assigns to a later step.
- Using the layout: `screen.present` drawing into the game rectangle and
  `conf.lua`'s `resizable`, `highdpi`, 640×480 (step 7).
- Calling `love.graphics.getDimensions` or handling `love.resize` in
  `main.lua` (step 7 and later).
- The pad's element table and element rectangles, the 44-point minimum
  (step 10).
- Touch hit tests, the stick, show / hide (steps 11–13).
- Safe-area APIs (§2: the halves make them unnecessary), iPad- or
  landscape-specific layouts, rotation handling beyond the mode rule.
- Any fake LÖVE change (its format is frozen after phase 1).

## Further Notes

- Why derive width from the scale: for a window like 391×845 the scale is
  `391 / 160`, and `160 * (391 / 160)` may differ from 391 by one ulp. The
  rectangle must match the present's real extent, which is
  `160 * scale`; the tests stick to dyadic sizes so they stay exact, and the
  contract (width = content × scale) is what step 7 relies on.
- The pre-measurement's 1 ms per title-screen frame matches step 1.4's
  1.542 ms per e2e frame before `-O2`; step 7 re-measures the e2e
  per-frame cost anyway.
- The largest `make test` check today is `test_love_e2e` (~270 ms own
  time), well over rule 2's 100 ms budget; it predates plan 6 and is not
  this plan's to rework, but the budget report may mention it as the
  biggest lever if plan 6's own checks cannot cover the shortfall.
