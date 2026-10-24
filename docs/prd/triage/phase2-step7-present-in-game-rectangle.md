---
title: "Present the canvas into the game rectangle, resizable window, and the screen-test budget"
description: "The game still draws its 160×120 canvas at (0, 0) and scale 4 into a fixed 640×480 window, so a phone or a resized window shows it misplaced; and the first make test check that boots the game does not fit the ~0.7 point of timing headroom left."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) runs the game on a portrait iPhone and in a
resizable desktop window. Step 6 added `love.window_layout`, the pure module
that says where the canvas goes in a window of any size, but nothing calls
it: `screen.present` still draws canvas 0 at `(0, 0)` with the whole-number
`screen.SCALE` (4), and `conf.lua` asks for a fixed, non-resizable 640×480
window. On a 390×844 iPhone that draws the canvas as a 640×480 block from
the top-left corner, cut off on the right, over the Dynamic Island; on a
resized desktop window the game stays in the corner at its old size.

Nothing checks the whole screen of the real game either. The fake's own
tests (steps 1.4, 1.5) check the present on fixture programs; no
`make test` check boots `game/`. The plan's screen tests (this step's two,
step 16's six pad cases) are the first, and booting the game is the most
expensive thing a check does (~43 ms, about 22 ms of it LuaJIT parsing
~950 KB of game source). The timing gate against the plan's start commit,
8c830c6, stands at **+2.32 %** after step 6 (which already had to pull a
reclaim forward to pass): **about 0.7 point is left**, while this step's
check alone costs ~2.1–2.3 points and the plan's later checks ~1 point
more. The reclaims step 6 named, (b) and (c), give ~1.2 points: not enough.
This step must make its check as cheap as the plan allows, find the rest
within §2 ("reworks or merges checks, its own or earlier plan checks"), and
fix how step 16 groups its cases.

## Solution

**1. The present draws into the game rectangle.** Every frame,
`screen.present` reads the window size with
`love.graphics.getDimensions()`, asks `window_layout.compute` for the
layout, and draws canvas 0 with one `draw(canvas, game.x, game.y, 0,
game.scale, game.scale)`, unrounded. At 640×480 that is exactly today's
draw `(0, 0)`, scale 4, so the 20 replays (no `window` member, so 640×480)
draw the same screen and keep every `*.sha256` and `*.end-state`. At
390×844 the canvas lands at `(0, 64.75)`, scale 2.4375, 390×292.5, in the
top half; in a wider desktop window it is centred with black bars.

**2. `conf.lua`.** `t.window.resizable = true`, `t.window.highdpi = true`,
start size 640×480. `screen.SCALE` disappears (it no longer describes the
present); the start size is the canvas size times a start scale of 4 kept
in `conf.lua` itself.

**3. One screen test of the real game, as cheap as the plan allows.** A
new fake check boots `game/` once on a two-frame fixture replay: frame 0 at
390×844 (tall mode), frame 1 a `resize` to 640×480 (overlay mode), both
listed in `screens` (the plan's step 7 text requires these two sizes, so
no smaller window replaces them). For each listed frame the test builds
the expected whole screen itself from that frame of the run's own capture
(palette numbers turned into RGB with a palette table of the test's own),
at the rectangle §2 states, and compares the PNG exactly, all four
channels, every pixel. The 640×480 frame is filled directly, every screen
pixel `(i, j)` taking texel `(i / 4, j / 4)` (integer scale: no
rasteriser needed); the 390×844 frame goes through the shared
expected-screen helper, the only one with a fractional scale. The expected
rectangles are literal numbers, not computed by the Lua module, so the
test checks both the layout and the present.

**4. The budget, closed in this step.** The step makes the ordered
reclaims below, each measured, then passes the timing gate against three
baselines, with margin for every check the plan still adds.

Measurements (4 cores, `-O2`; step 2.6's note, plus a prototype made while
writing this PRD):

| Measured | Cost |
|---|---|
| game boot in the fake | ~43 ms, + ~1.6 ms per frame |
| a 390×844 screen PNG, written / compared | ~4.7 / ~5.4 ms |
| a 640×480 screen PNG, written / compared | ~5 / ~9.5 ms |
| this step's check (estimate) | ~67–73 ms ≈ 2.1–2.3 points |
| 1 point of the gate | ≈ 32 ms of a check's own time |
| `test_e2e_limits` own time, today | ~735–767 ms, 349 execs (170 `grep`, 95 `sh`, 32 `awk`, 49 `rm`/`mkdir`) |
| same, with the per-file `grep`s folded into the script's one `awk` (prototype) | ~400–440 ms, 179 execs, every assertion passing |
| `make test` wall, ee5e66a vs that prototype (11 paired rounds) | 935 → 821 ms, **paired median −10.3 %** |
| 27 `sh -c "rm -rf … && mkdir -p …"` (the test's resets and removals) | ~170 ms |
| 32 bare runs of the script through `sh -c` | ~120 ms |

`test_e2e_limits` is on `make test`'s critical path (its ~0.75 s is most of
the ~0.9 s wall), which is why its saving shows almost one for one on the
wall time; the 32 ms-per-point ratio, valid for a check run beside others,
does not apply to it. `tools/e2e_limits.sh` was changed by plan 6 (step
1.3, chunk 2: frames counted as objects of `frames`), so it is an earlier
plan check in §2's sense.

## User Stories

1. As a phone player holding the iPhone in portrait, I want the game drawn in the top half at 390×292.5 from y 64.75, so that nothing is hidden by the Dynamic Island and the bottom half is free for the pad.
2. As a phone player, I want the game as wide as the screen, so that the 160×120 pixels are as large as the width allows.
3. As a phone player, I want no rounding of the game rectangle, so that the game is exactly centred in its half.
4. As a desktop player, I want the window to open at 640×480, so that the game looks as it always did on first launch.
5. As a desktop player, I want to resize the window, so that I can make the game larger or fit it beside other windows.
6. As a desktop player who widens the window, I want black bars left and right and square pixels, so that the game is never stretched.
7. As a desktop player who makes the window taller, I want the game centred vertically, so that the bars are even.
8. As a desktop player who resizes on any frame, I want the very next frame drawn at the new size, so that there is no frame drawn at the old layout.
9. As a desktop player on a high-density display, I want `highdpi` on, so that the game is drawn at the display's real resolution in LÖVE units.
10. As a phone player, I want `highdpi` on, so that Love2D Studio gives the game the full Retina resolution.
11. As a player in the default window, I want the present unchanged at 640×480, so that the desktop look is pixel-identical to before the plan.
12. As a maintainer, I want the 20 replays to keep their `*.sha256` and `*.end-state`, so that the game logic is proven untouched.
13. As a maintainer, I want one check that boots the real game and compares the whole screen at 390×844, so that a wrong layout or a wrong present on the phone fails `make test`.
14. As a maintainer, I want the same check to compare the whole screen at 640×480 after a `resize`, so that both modes and the resize path are checked by the game itself.
15. As a test author, I want the expected screen built from the run's own capture, so that the test does not depend on what the title screen draws and survives any change of the game's content.
16. As a test author, I want the expected rectangles written as literal numbers from §2, so that the test checks the layout module rather than repeating it.
17. As a test author, I want exact comparison on all four channels of every pixel, so that a one-pixel shift or a rounding fails, never passes by tolerance.
18. As a test author, I want the test to refuse a captured frame that is entirely colour 0, so that a black canvas cannot make a misplaced present pass.
19. As a test author, I want the test to use its own 16-colour table, so that a palette bug in the fake's capture is not copied into the expected frame.
20. As a test author, I want the test written before the present changes, so that it fails first at 390×844 (the canvas at `(0, 0)` scale 4) and passes after.
21. As a test author, I want the check to remove stale screen PNGs before its run, so that a PNG from an earlier run never passes for a new one.
22. As a test author, I want the 640×480 expected frame filled directly at scale 4, so that the costliest comparison of the check builds its frame in one pass.
23. As the implementer of step 16, I want this step's replay to be the one my six cases extend, so that the whole plan pays one game boot for all its screen tests.
24. As the implementer of step 16, I want the replay to live next to where `pad_flat.png` will be, so that its `pad_sheet` member resolves without moving files.
25. As the implementer of step 16, I want a written frame plan (windows, frames, which ones are in `screens`), so that I add frames to a budget already measured.
26. As the implementer of step 16, I want small dyadic windows for the cases that do not need the phone size, so that the screen PNGs I add cost little and stay exact.
27. As the coordinator, I want this check's own time measured, so that §6 records the real price of a game boot in `make test`.
28. As the coordinator, I want the cost of every screen test the plan adds projected from a measured scratch replay, so that step 16 does not find the budget gone.
29. As the coordinator, I want the timing gate passed against ee5e66a, 8dff812 and 8c830c6 with at least one point of margin against 8c830c6, so that steps 8–16 start with room for their checks.
30. As the coordinator, I want an ordered list of reclaims, each with its measured saving, so that the step stops reclaiming as soon as the gate has its margin and nothing is reworked for nothing.
31. As the coordinator, I want reclaims (b) and (c) from step 6's plan applied, so that the plan's own checks stay within design.md's per-check budget and do not repeat each other.
32. As a maintainer, I want `tools/e2e_limits.sh` to read each replay once, in its one `awk`, so that `make test` and `make e2e` stop paying a `grep` per replay per key.
33. As a maintainer, I want every message of `tools/e2e_limits.sh` unchanged in wording, so that a failing replay is reported as before.
34. As a maintainer, I want `test_e2e_limits`'s assertions unchanged while it becomes cheaper, so that the guard is tested exactly as much as before.
35. As a maintainer, I want the test's work-directory resets done by C calls rather than shells, so that a reset does not cost three processes.
36. As the coordinator, I want `make e2e` re-timed and its per-frame cost re-measured at `-O2`, so that step 15's touch-replay cap is projected with a current number.
37. As a maintainer, I want design.md's present rule to say "the game rectangle of `love.window_layout`", so that the doc no longer states `(0, 0)` and `screen.SCALE`.
38. As a maintainer, I want design.md to name the new check among the screen checks, so that a reader finds where the game's screen is tested.
39. As a maintainer, I want `conf.lua`'s fields checked by a pure-Lua test, so that a field the fake ignores is still verified.
40. As a maintainer, I want no `love.resize` handler added, so that the layout has one source, the window size read every frame.
41. As a maintainer, I want the `love.graphics` call to stay in `love/`, so that `lua-guard`'s rule that only `love/` draws keeps holding.
42. As a maintainer, I want the per-check budget of 100 ms respected with step 16's frames projected, so that the check never has to be split later.
43. As a phone tester at step 8, I want a known risk about canvas density written down, so that a blurry or gappy game on the phone is diagnosed quickly.

## Implementation Decisions

- **Hypotheses on step 6, checked against the committed code (ee5e66a)
  and plan §6's "Step 2.6" note:**
  - **H-6-1 (holds)**: `love.window_layout.compute(width, height)` returns
    `{ mode, game, pad }`, each rectangle `{ x, y, width, height, scale }`,
    unrounded, `MODE_TALL` / `MODE_OVERLAY`; 390×844 → game `(0, 64.75)`,
    scale 2.4375; 640×480 → `(0, 0)`, scale 4.
  - **H-6-2 (holds)**: all `tests/lua/game/` files run in one LuaJIT
    process through the runner of that directory (target `lua-test-game`);
    each file ends with `return check.run({...})`, and a new file needs no
    Makefile edit.
  - **H-6-3 (holds)**: §6 binds one replay, one boot, one target for this
    step's two presents and step 16's cases (small windows: 160×322 or
    200×420 tall, 160×120 or 320×240 overlay); step 8 inside `lua-guard`;
    step 9 inside an existing fake check, window ≤ 256×128; steps 10–13 in
    the runner. Reclaims (b) and (c) are assigned to this step.
  - **H-6-4 (fails)**: the headroom is not ~1.7 points. Step 6's first gate
    failed (+3.34 %); reclaim (a) (`test_fake_love_touch`'s misuse runs
    folded, ~−34 ms) was pulled into step 6, which then passed at
    **+2.32 % against 8c830c6: ~0.7 point left.** The budget below is built
    on that number.
  - **H-6-5 (holds)**: design.md has the layout paragraph under "How the Lua
    game draws", not yet saying the present uses it.
  - **H-6-6 (holds)**: `screen.present`, `screen.SCALE`, `conf.lua`,
    `main.lua` and the fake are as at c67b0b4.
  - **H-6-7**: the "before" commit for the gate and `make e2e` is
    **ee5e66a**.

- **`love.screen`.** `present()` keeps its name and takes no argument: it
  reads `love.graphics.getDimensions()` (a `love.graphics` call, allowed
  only under `love/` by `lua-guard`, so it cannot move to `main.lua`),
  calls `window_layout.compute` on the size, and draws canvas 0 at the game
  rectangle's `x`, `y` and `scale`. Its other rules are unchanged: last in
  `love.draw`, screen selected, `setColor(1, 1, 1)` before, outside any
  `push`. It returns nothing; step 16 decides whether the pad needs the
  layout it used. `screen.SCALE` is removed (its only users were the
  present and `conf.lua`).

- **No `love.resize` handler.** The layout is recomputed from
  `getDimensions` on every present (§2: "recomputed every frame"), so a
  resize takes effect on its own frame. A cached layout updated by
  `love.resize` was rejected: two sources of truth.

- **`conf.lua`.** Width and height are `vram.SCREEN_WIDTH` and
  `vram.SCREEN_HEIGHT` times a local `START_SCALE = 4`; `resizable = true`;
  `highdpi = true`; the other fields unchanged. It no longer requires
  `love.screen`. No `minwidth`/`minheight` (not in the plan). The fake
  ignores `t.window`, so the 20 replays still run at the 640×480 default.

- **The screen-test check.** A new C test of the fake's kind,
  `test_game_screens`, in `UNIT_TESTS` and `FAKE_LOVE_CHECKS`, built like
  `test_fake_love_screens` with `fake_love_cli`, `expected_screen` and the
  PNG reader. It runs the fake on `game` with its fixture replay
  `game_screens.json`, at the top of the fixtures directory (where step 9
  puts `pad_flat.png`, so step 16's `pad_sheet` resolves next to it):
  `{"input": "pad", "window": [390, 844], "screens": [0, 1], "frames": [{}, {"resize": [640, 480]}]}`.
  It needs exit 0; the game's state output is written but not compared.
  The test's frame list (window size, game rectangle, listed frame) is a
  table step 16 extends. The check is placed so `-j` starts it among the
  first jobs (design.md rule 1); its position is settled by the gate's
  measurement (once `test_e2e_limits` is shorter, the critical path
  changes).

- **Expected frame.** Black `000000FF` at the window size; canvas texel
  `(i, j)` is the RGB of palette number `n(i, j)` of that capture frame
  (low nibble = even x), alpha `FF`. At 640×480, overlay scale 4 at
  `(0, 0)`, the frame is filled directly, pixel `(i, j)` from texel
  `(i / 4, j / 4)` (integer division), which is what the rule of "The
  screen" gives at an integer scale and zero offset; at 390×844 it is drawn
  with `expected_screen_draw` (whole canvas, white tint, `(0, 64.75)`,
  2.4375). The test also asserts that each frame holds a texel other than
  colour 0 and that a pixel of each black bar is black (rows 64 and 357 at
  390×844). Edge-row checks of the half-open rule are not repeated: the
  fake's step-1.4 tests own that rule.

- **`conf.lua` test.** A runner file in `tests/lua/game/` loads
  `game/conf.lua` with a stub global `love = {}`, calls `love.conf(t)` on
  `{ window = {}, modules = {} }`, checks width 640, height 480,
  `resizable` and `highdpi` true, restores the global, and ends with
  `return check.run({...})`. ~0 ms inside the runner.

- **Budget: the ordered reclaims.** Done in this order; each one's saving
  measured (own time of the changed check, then a quick paired
  `make test` comparison) before the next. Items 1–4 are done whatever
  the numbers; items 5–7 only while the gate's margin against 8c830c6 is
  under one point (that is, while it is above +2 %).
  1. **This step's check as cheap as allowed:** two frames, one boot, the
     640×480 frame filled directly, nothing else in the replay
     (estimate: the check at ~65–70 ms instead of ~67–73).
  2. **(b) `test_fake_love_screens`** (step 6's plan): its frames 2–4 at
     about 200×150, a tiny replay for its failing run, the
     malformed-`screens` CLI refusal dropped (the replay reader's unit test
     asserts each message) — ≈ −25–30 ms, ~0.85 point.
  3. **(c) `test_fake_love_window`** (step 6's reserve): its misuse cases
     in one pcall-logged run, compared whole, as step 6 did for
     `test_fake_love_touch`; the `window_invalid` CLI refusal dropped when
     the reader's unit test asserts its message — ≈ −11 ms, ~0.35 point.
  4. **(d) `tools/e2e_limits.sh` reads each replay once.** The per-replay
     `grep` for the `"input": "pad"` header and the four `grep`s for the
     old mouse keys move into the script's single `awk`, which already
     reads every replay line by line: it marks per file whether a line
     matches each pattern (same regular expressions, same line-based
     semantics as `grep -q`) and reports in `END`, with the same message
     texts. Per replay that is five processes fewer, in `make test` (via
     `test_e2e_limits`, 32 runs) and in `make e2e` (20 replays).
     **Measured with a prototype: `test_e2e_limits` 750 → ~430 ms, every
     assertion passing; `make test` paired median −10.3 % against
     ee5e66a over 11 rounds; `make e2e` 20/20.** The stderr order changes
     (header and mouse messages now come with the others, after the
     companion-file checks); nothing depends on that order.
  5. **(e) `test_e2e_limits`'s resets in C**: the work directory emptied
     and recreated with `opendir`/`remove`/`mkdir`, and its five single
     `rm` calls replaced by `remove`, instead of 27 `system` shells —
     ≈ −150 ms of its own time (27 such shells measured at ~170 ms).
  6. **(f) `test_e2e_limits` runs the script without the outer shell**
     (`fork`/`exec` of `sh tools/e2e_limits.sh` with stdout and stderr to
     the log): one process fewer per run, 32 runs — ≈ −60 ms.
  7. **(g) `test_love_e2e`** (~255 ms own time, pre-plan): made cheaper
     without losing a case. A reserve only; not expected to be needed.
  The game boot is not a lever: most of it is parsing game source, and
  caching bytecode would change the fake, frozen after phase 1.

- **Expected outcome.** From +2.32 %, the step's check adds ~2.1 points,
  (b) and (c) take ~1.2 back and (d) ~10 (measured), so the gate should
  land near −8 % against 8c830c6. The projection adds steps 8–16's
  ~1.0–1.1 points (step 8 ~1–2 ms, step 9 ~8–10 ms, steps 10–13 ~2 ms,
  step 16 ~17 ms per §6): still far under 3 %. If (d)'s measured effect is
  much smaller in the real gate than in the prototype, items 5–7 follow in
  order until the margin is one point.

- **Step 16's projection.** The step runs the check's fake command on a
  scratch extension of the replay shaped as the plan below (not committed;
  under `tmp/`), with its screens compared by the same code, and reports
  its own time, which must stay under design.md's 100 ms per check.

- **Step 16's grouping (for §6, confirmed or changed by the projection).**
  Step 16 appends frames to `game_screens.json` and rows to the test's
  frame table, and adds `"pad_sheet": "pad_flat.png"`; no new target, no
  second boot, at most 16 `screens`, frames kept to the minimum (~1.6 ms
  each). Its cases use small dyadic windows from §6's list: **200×420**
  (tall: 420 > 400; scale 1.25; game `(0, 30)` 200×150; pad `(0, 240)`
  200×150) and **320×240** (overlay, scale 2, `(0, 0)`); 160×322 and
  160×120 are the cheaper alternatives if a 44-unit button still reads
  there. Frame plan:

  | Frame | Window | Input | Screen case |
  |---|---|---|---|
  | 0 | 390×844 | none | step 7 tall present; step 16 "hidden" (pad hidden at start) |
  | 1 | resize 640×480 | none | step 7 overlay present |
  | 2 | resize 200×420 | touch 1 in the pad half | "shown in tall mode" (the revealing touch presses nothing) |
  | 3 | 200×420 | none (lift) | — |
  | 4 | 200×420 | touch 2 on A | "a button pressed" |
  | 5 | 200×420 | none (lift) | — |
  | 6 | 200×420 | touch 3 in the stick zone | — (the base appears) |
  | 7 | 200×420 | touch 3 moved on a diagonal | "the stick pushed on a diagonal" |
  | 8 | resize 320×240 | none | "after a `resize`" and "shown in overlay mode" |

  Six PNGs (two big, four small), nine frames; ~+17 ms, the whole check
  ~85–90 ms. A frame serves two cases only when its state is exactly
  both; if step 16 needs distinct frames, it adds them at a small window.
  Windows, frames and hit positions are the step-16 implementer's to
  adjust; the one-replay, one-boot, small-window rule is binding.

- **`make e2e` time.** Median of 5 runs of `make e2e`, beside ee5e66a in
  the same session; the per-frame cost re-measured as step 1.4 defined it
  (user + sys of `make e2e` / the 20 replays' frame count / 4 cores) at
  `-O2`. Projection: median + 720 × the new per-frame cost, which must
  leave at least 1 s under 9 s; in a slow session, §2's paired-difference
  rule. Reclaim (d) also shortens `make e2e` (100 `grep`s fewer); the
  present's extra work per frame (one `getDimensions`, one `compute`,
  three small tables) is expected to be negligible.

- **Timing gate.** design.md rule 4: 11 rounds (22 if needed) against
  ee5e66a, 8dff812 and 8c830c6, each paired median ≤ 3 %; the step's own
  target is ≤ +2 % against 8c830c6. Plain and paired medians, spreads,
  round count, core count, the new check's own time and each reclaim's
  saving go into the report, with the headroom left against 8c830c6.

- **design.md.**
  - "The present": the closing rule becomes: present canvas 0 once per
    frame, last in `love.draw`, with the screen selected, after
    `setColor(1, 1, 1)`, outside any `push`, at the game rectangle's
    position and scale that `love.window_layout` computes from
    `getDimensions()` (`screen.present()`); the "plan 6 step 7 rewrites
    this rule" remark goes.
  - "The LÖVE functions the game may use": the same rule restated there
    loses `(0, 0)` and `screen.SCALE`.
  - "How the Lua game draws": the layout paragraph gains that the present
    uses the game rectangle every frame, with no `love.resize` handler,
    and that `conf.lua` opens a resizable, `highdpi` 640×480 window, which
    the fake ignores.
  - "The screen": the checks list gains `test_game_screens`.
  - "`make e2e`": `tools/e2e_limits.sh`'s description stays true (it
    still checks the same limits); only if the text names `grep` is it
    reworded.
  - The fake's contract does not change.

- **The report to the coordinator** (the implementer does not edit plan
  §6): the check's own time, each reclaim's measured saving, the gate
  numbers, the step-16 projection, the `make e2e` median and per-frame cost
  with the step-15 projection, and step 16's grouping as above, confirmed
  or changed.

## Implementation Chunks

1. **The failing screen test.** The fixture replay and `test_game_screens`
   with its frame table, palette table, direct 640×480 fill and helper-drawn
   390×844 frame; the target. It fails at frame 0 (today's present at
   `(0, 0)` scale 4) and passes at frame 1. Measure its own time.
2. **The present and `conf.lua`.** `screen.present` through the layout,
   `screen.SCALE` removed, `conf.lua`'s fields and its runner test; the
   screen test green; `make e2e` and `make end-state` 20/20; `lua-guard`,
   `lua-test-modules`, `make lua-names` green; design.md's present rule.
3. **Reclaims (b) and (c)** in `test_fake_love_screens` and
   `test_fake_love_window`, each saving measured; their assertions kept or
   moved, never lost.
4. **Reclaim (d)**: `tools/e2e_limits.sh`'s `grep`s into its `awk`;
   `test_e2e_limits` unchanged and green; `make e2e` 20/20; measured.
5. **Gate and report.** The gate against three baselines; items (e)–(g)
   only if the margin against 8c830c6 is under one point; the step-16
   projection; `make e2e` median and per-frame cost; the report.

## Testing Decisions

- **A good test here** checks what a player would see: the whole screen
  of a frame of the real game, compared exactly with a frame the test
  builds from the rule of design.md's "The screen" and the rectangles of
  §2. It does not stub `love.graphics`, inspect `screen.present`'s calls,
  or call `love.window_layout`.
- **Tested:** the game's present through `test_game_screens` (390×844
  tall, 640×480 overlay after a `resize`); `conf.lua` by a runner test;
  the layout module by step 6's tests; the 20 replays by `make e2e` and
  `make end-state`.
- **Reclaims keep their coverage:** `test_e2e_limits` is not edited by
  reclaim (d) (it is the oracle that the script's behaviour is unchanged);
  (e) and (f) change only how it runs, not what it asserts; in (b) and (c)
  every dropped run's assertions are already made elsewhere (named in the
  commit) or moved into a surviving run.
- **Not tested:** `highdpi` and `resizable` behaviour in real LÖVE (no
  window may be opened, and the fake does not model them); the phone trial
  of step 8 is where they are seen.
- **TDD order:** the screen test first, red at 390×844; then the present.
- **Prior art:** `tests/test_fake_love_screens.c` and
  `tests/test_fake_love_image_draw.c` (screen PNGs, stale-PNG removal,
  `expected_screen_draw` / `expected_screen_assert_file`);
  `tests/test_fake_love_capture.c` (a test-owned palette table);
  `tests/test_fake_love_touch.c` after step 6 (misuses in one pcall-logged
  run, for (c)); `tests/lua/game/test_economy_formulas.lua` (runner file).
- **Check of the step:** the screen test passes; the 20 replays unchanged
  (`make e2e`, `make end-state`); `make test-all` green; design.md states
  the new present rule; the gate passes against ee5e66a, 8dff812 and
  8c830c6, at or under +2 % against 8c830c6; the numbers and step 16's
  grouping are in the report.

## Out of Scope

- Writing plan §6 (the coordinator does, from the report).
- Any fake LÖVE change (frozen after phase 1), including a `newCanvas`
  settings table or bytecode caching.
- The pad: its sheet, element table, drawing, touches, show / hide
  (steps 9–16); this step only plans step 16's frames.
- `make love-file` and the completeness check (step 8).
- A `love.resize` handler, minimum window sizes, safe-area handling.
- Smaller windows than 640×480 and 390×844 for this step's two frames (the
  plan's step text names those sizes).
- The "nothing in these plans targets iOS" line of design.md.
- Real-LÖVE captures of the scaled present.

## Further Notes

- **Risk for the phone trial (step 8), not verifiable here.** In LÖVE 11,
  `newCanvas(w, h)` takes the window's DPI scale as its default
  `dpiscale`; with `highdpi = true` on a Retina screen (iPhone: 3) the
  160×120 canvas may become 480×360 pixels of 160×120 units. Rectangles
  and quads land on whole units and look the same, but `points` and the
  present's sampling may differ from the fake. If the phone shows gaps or
  blur, the fix is `newCanvas(160, 120, { dpiscale = 1 })`, a
  function-list extension decided in its own step. Desktop Linux (DPI
  scale 1) is unaffected.
- Real LÖVE clears the screen to black before `love.draw`, as the fake
  starts each frame black, so the bars are black in both.
- Why the expected frame comes from the capture: the capture is pinned by
  the e2e hashes elsewhere, the title's content is not this test's concern,
  and a later change of the title (or step 16's button press starting a
  game) needs no edit of the test.
- The (d) prototype: the five `grep` checks removed from the per-replay
  loop; in the `reading == "json"` branch, per line,
  `if ($0 ~ /"input": *"pad"/) padded[FILENAME] = 1` and the same for
  `"mouse_x"`, `"mouse_y"`, `"mouse_buttons"`, `"dpad"`; in `END`, per
  given replay, the two messages of the old `grep`s with the same text.
  It ran in throwaway worktrees of ee5e66a (removed), timed with
  `date +%s%N`, 11 rounds alternating which worktree went first.
