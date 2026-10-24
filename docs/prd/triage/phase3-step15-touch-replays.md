---
title: "Touch e2e replays, each equal to its pad twin by hash"
description: "The virtual pad reaches the game since step 14, but no committed replay plays it, so a regression in touches, the reveal rule or the gamepad hide would go unseen by make e2e."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) plays the game on a portrait iPhone with a
virtual pad. Steps 10 to 13 built the pad as pure modules and step 14
wires them into `main.lua`: the virtual mask is OR-ed with the real
gamepad's in `love.update`, before `input_step.run`. The Lua tests prove
each module and their composition, but `make e2e` (the source of truth,
`CLAUDE.md`) plays only the 20 gamepad replays of `tests/e2e/`, none of
which carries a touch, a click or a window. A change that broke the touch
readers in `main.lua`, the phone layout, the rule that the revealing touch
presses nothing, or the rule that a gamepad press hides the pad and drops
the held touches would leave every e2e hash green.

Plan §2 (Tests) asks for a few short `tests/e2e/touch_*.json`, each the
touch version of a pad scenario, whose capture hash equals the pad
equivalent's: the pad is drawn on the screen, never on canvas 0 (§2
Layout), so the capture cannot see it and the two runs must produce the
same frames. The planned scenarios are: title to first slipway; aim pan on
a diagonal; Back held to end the year; show, hide by a gamepad button, show
again. Step 15 says each pad equivalent is a **pad twin with the same
frames, written by this step** (the existing pad replays are long full
scenarios), that the touch replay's `.sha256` is the twin's hash and never
a baseline of the touch replay itself, that the twins count toward the e2e
time, and that `make e2e` stays green and under 9 s with the 1 s margin
left for step 16's drawing.

## Solution

Four touch replays and four pad twins join `tests/e2e/`, each with its
`.txt`, `.json.sha256` and `.json.end-state`:

| scenario | touch replay | pad twin | frames |
|---|---|---|---|
| title to first slipway (a resumed record) | `touch_first_slipway.json` | `pad_twin_first_slipway.json` | 36 |
| aim pan on a diagonal, then slid straight | `touch_aim_diagonal.json` | `pad_twin_aim_diagonal.json` | 65 |
| Back held 30 frames ends the year | `touch_year_hold.json` | `pad_twin_year_hold.json` | 54 |
| show, hide by a gamepad button, show again | `touch_show_hide.json` | `pad_twin_show_hide.json` | 121 |

276 touch frames and 276 twin frames: 552 frames, under the 720-frame cap
step 1.4 budgeted (§6), every replay under the 180-frame limit (no
long-replay exception).

Each touch replay runs in a 390×844 window (the iPhone of §2, tall mode)
and presses the pad's buttons with touches at exact coordinates taken from
`love.window_layout` and `love.pad_layout`; one scenario also presses with
the mouse. Its twin holds, frame for frame, the gamepad buttons the touch
frame yields, in the default 640×480 window. The twin's hash is taken from
its own run; the touch replay's `.json.sha256` and `.json.end-state` are
byte copies of the twin's. `make e2e` and `make end-state` then prove the
touch run equal to the twin run, frame by frame and in its end state.

`tools/e2e_limits.sh`, the guard of the e2e files that `make e2e` already
runs, gains one rule so the equality cannot drift silently (for example
after a `make e2e-baseline`): every `touch_<s>.json` has a
`pad_twin_<s>.json` beside it with the same hash, and every `pad_twin_<s>`
has its `touch_<s>`.

For the player nothing changes; for a maintainer, `make e2e` now fails when
a touch stops reaching the game the way a gamepad press does.

## User Stories

1. As a phone player, I want my touches to keep driving the game exactly as gamepad presses would, so that the virtual pad never plays a different game.
2. As a phone player, I want my first touch to keep pressing nothing, so that I never press a button I could not see.
3. As a phone player, I want a held stick on a diagonal to keep panning on both axes, so that aim pans feel like a real stick.
4. As a phone player, I want sliding the stick from a diagonal to straight up, without lifting, to change the direction at once, so that the stick follows my thumb.
5. As a phone player, I want holding Back for half a second to keep ending the year, so that the year hold works with no controller.
6. As a phone player, I want Back to stay a button even though it lies in the stick zone, so that my thumb on Back never steers the camera.
7. As a phone player, I want the stick to move the title's rows and to jump between objects, so that menus and the map are reachable by touch.
8. As a phone player, I want A to choose a title row, start a slipway and build it, so that my first slipway can be made by touch.
9. As a player with a controller and a phone, I want a gamepad press to hide the pad and release what my finger held, so that no button stays stuck.
10. As a player who picked up the controller, I want my next touch to show the pad again without pressing anything, so that the reveal rule holds every time.
11. As a desktop player, I want a left click on the pad to press its button, so that the pad can be tried with a mouse.
12. As a desktop player, I want a held click to count as a held button, so that a year hold works with the mouse.
13. As a maintainer, I want each touch replay's hash equal to its twin's, so that a divergence points at the touch path and nothing else.
14. As a maintainer, I want each twin to be a short scenario written for this step, so that the comparison costs few frames.
15. As a maintainer, I want the twins to run in the default window, so that the equality also proves the window size never reaches canvas 0.
16. As a maintainer, I want the touch replay's `.sha256` copied from the twin, never baselined from the touch run, so that the reference is the gamepad game.
17. As a maintainer, I want `make e2e` to fail when a touch replay's hash differs from its twin's, so that an e2e-baseline cannot hide a touch regression.
18. As a maintainer, I want `make e2e` to fail when a touch replay lacks its twin or a twin its touch replay, so that the pairing stays complete.
19. As a maintainer, I want each touch replay's `.end-state` equal to its twin's, so that `make end-state` stays green and compares the whole end state too.
20. As a maintainer, I want every touch frame to begin with `"pad"`, so that the `.txt` notes' awk cut (the help-me-debug recipe) cuts touch replays like pad replays.
21. As a maintainer, I want each touch replay's `.txt` to list every touch with its coordinates and the button it hits, so that a moved button is easy to trace.
22. As a maintainer, I want each twin's `.txt` to give the state at its key frames, so that the twin is documented like every pad replay.
23. As a maintainer, I want each `.txt` to state a negative run showing a leak would change the result, so that the replay is known to be sensitive.
24. As a maintainer, I want every new replay under 180 frames, so that no long-replay exception is needed.
25. As a maintainer, I want the touch replays and twins to total at most 720 frames, so that the budget step 1.4 set holds.
26. As a reviewer, I want `make e2e` measured beside the "before" commit, so that the 1 s margin for step 16's drawing is proven, not assumed.
27. As a reviewer, I want `make test`'s two affected checks timed before and after, so that the loop is known not to slow down.
28. As a reviewer, I want the 20 original replays and their files untouched, so that the C-set references stay the reference.
29. As a developer of step 16, I want touch replays that already pass, so that drawing the pad on the screen is proven not to touch canvas 0 by them staying green.
30. As a developer of step 16, I want the touch replays to cover tall mode at 390×844, so that the drawing is exercised on the phone layout.
31. As a maintainer, I want design.md to describe the touch replays, the twins and the pairing rule, so that the next e2e change knows the convention.
32. As a maintainer, I want the replay reader's test to list the new replays with their windows, so that its "every e2e replay is listed" guard stays true.
33. As a maintainer, I want the record of the slipway scenario copied from an existing replay, never typed, so that the save convention of `tests/e2e/` holds.

## Implementation Decisions

**Hypotheses on step 14 (not yet done; its PRD is
`docs/prd/triage/phase3-step14-wire-virtual-pad.md`).** The implementer
checks each one first against the committed code; one that fails is
reported, and the decision resting on it is revisited (step 14's real code
wins over this PRD's wording).

- **H-14-1**: step 14 is committed and `make test-all` is green on it; its
  last commit (the plan's §6 step-14 note included, if any) is this step's
  "before" commit for every timing.
- **H-14-2**: `main.lua`'s `love.update` passes as `pad_buttons` the mask
  of `love.virtual_pad`'s `update`, fed every frame with
  `read_pad_buttons()`, the touches read by polling
  `love.touch.getTouches` / `getPosition`, the mouse read with
  `love.mouse.getPosition` / `isDown(1)` as one more touch while down, and
  `love.graphics.getDimensions()`; no touch or mouse callback.
- **H-14-3**: the composition keeps steps 12–13's rules unchanged: the pad
  starts hidden; any new touch or click on a hidden pad shows it and is
  held back (presses nothing) until it lifts; a gamepad bit going down
  hides the pad and holds back every current touch and the mouse if down;
  a stick touch's first frame yields mask 0 (the knob on the base); a touch
  that starts on a button is never the stick, even inside the stick zone.
- **H-14-4**: `love.draw`, `screen.present` and `debug_state()` are
  unchanged: nothing of the pad reaches canvas 0, and the end-state dump
  holds no pad, touch or window field. A touch run and its twin can thus
  give the same capture and the same end state.
- **H-14-5**: step 14 changes neither `love.window_layout`,
  `love.pad_layout`, `love.pad_stick`, `love.pad_touch`,
  `love.pad_visibility`, the bindings, nor the fake LÖVE and its replay
  format; so the coordinates below hold.
- **H-14-6**: the 20 replays keep their `.json.sha256` and
  `.json.end-state`, and `tests/e2e/` holds exactly those 20 replays and
  their two `.cfg` files (step 14's scratch replay is not committed).
- **H-14-7**: step 14 measured `make e2e` (median and per-frame cost,
  recorded in §6 or its report); if not, step 15 measures its "before"
  commit itself, which it does anyway (Testing Decisions).

**Facts from the committed code.** At 390×844 `window_layout.compute`
gives tall mode, pad rectangle at (0, 486.75), scale 2.4375. With
`pad_layout.place` the screen rectangles are (half-open): A x 263.25–321.75
y 691.5–750; Back x 131.625–190.125 y 715.875–774.375; stick zone x 0–195
y 486.75–779.25, base 117 (radius 58.5, dead zone 14.625); L x 9.75–68.25
y 496.5–555. The fake takes touch and mouse coordinates as integers inside
the window. `pad_stick` from a base at (90, 640): (90, 580) → UP (1),
(90, 700) → DOWN (2), (30, 640) → LEFT (4), (130, 600) → UP + RIGHT
(9). The pad buttons of a replay are numbered 1–12 (Up 1, Down 2, Left 3,
Right 4, A 5, …, Back 12; design.md's pad table). `test_fake_love_replay`
lists every `tests/e2e/*.json` with its frame count and asserts each runs
at 640×480 with no resize, and fails when a replay is unlisted.
`make end-state` reads a `.json.end-state` for every replay and fails on a
missing one. `make e2e-baseline` rewrites every `.json.sha256`, the touch
replays' included.

**The touch points** (390×844, integers, re-derived first by the
implementer with a LuaJIT one-liner over `window_layout` and `pad_layout`,
then written in each `.txt`): A (292, 720); Back (160, 745); stick base
(90, 640), pushed to (90, 580) up, (90, 700) down, (30, 640) left,
(130, 600) up-right. Each finger has its own id, never reused within a
replay.

**Common opening of the three new-game scenarios** (f0–14; the twin's
frames are pad_probe's f0–14: idle ×10, Down, idle, A ×3):

- f0–3: touch 1 on A, the revealing touch; f4 lifted. Twin: idle. On the
  title A is choose_row on "forgiving": a leaked press would start a
  forgiving game at once.
- f9: touch 2 on the stick base (mask 0, twin idle); f10: touch 2 at the
  down point (twin Down); f11 lifted.
- f12–14: touch 3 on A (twin A ×3), the new game on "reasonable"; f15
  lifted. Aim mode from f15 (a new game knows no object).

**The four scenarios.** Frames are the twin's, as verified on scratch
prototypes of the twins in this session (`make state`); the implementer
re-runs them and writes the readings into each twin's `.txt`.

1. **Title to first slipway** (36 frames). Both replays carry
   `pad_bankrupt.json`'s `initial_save` byte for byte (a record cut from a
   real game, never typed: four labs s1–s4, year 3401 month 9, $40, no
   slipway), so the title shows "resume". Touch: f0–3 reveal on A, f4
   lifted; f9 stick base, f10 up point, f11 lifted; f12 A (resume), f13
   lifted; f17 A (slipway from the hovered s4), f18 lifted; f20 stick
   base, f21 left point (aims at s3), f22 lifted; f25 A (build), f26
   lifted; idle to f35. Twin: idle ×10, Up, idle, A, idle ×4, A, idle ×3,
   Left, idle ×3, A, idle ×10. Prototype end state: year 3401 month 10,
   $37, `slipway s4 s3 normal`, slipways 1. The first slipway of the game,
   reached from the title; a new game cannot reach one within 180 frames
   (pad_slipway needs ~250).
2. **Aim pan on a diagonal** (65 frames). Opening; f15–17 idle; f18
   touch 4 on the stick base (mask 0); f19–48 touch 4 at the up-right
   point (twin Up + Right ×30); f49–58 the same finger slid to the up point
   (twin Up ×10); f59 lifted; idle to f64. Prototype camera: (176, 196)
   after f18, (226, 146) after f48, (226, 126) at the end.
3. **Back held to end the year** (54 frames). Opening; f15–17 idle;
   f18–47 touch 4 on Back (twin Back ×30); f48 lifted; idle to f53.
   Prototype: year 3402 at the end (29 frames of Back leave 3401). Back
   lies in the stick zone: the year ending proves the touch was the
   button's, not the stick's.
4. **Show, hide by a gamepad button, show again** (121 frames). Opening;
   f15–17 idle; f18–32 touch 4 on Back (twin Back ×15); f33 the gamepad's
   Up pressed (`"pad": [1]`) with touch 4 still on Back: the pad hides and
   drops it (twin Up alone); f34–40 touch 4 still held, pressing nothing;
   f41 lifted; f50–84 touch 5 on Back, the revealing touch of the shown-
   again pad, held 35 frames, pressing nothing; f85 lifted; f86 mouse
   `[160, 745, 1]`, kept down to f115 (twin Back ×30); f116 mouse
   `[160, 745, 0]`; idle to f120. Prototype end state: year 3402, camera
   (176, 195). Negative prototype: a reveal that leaked (Back pressed
   f50–84) ends one more year, 3403.

**Replay files.**

- Touch replay members: `"input": "pad"` (required by `e2e_limits.sh`),
  `"window": [390, 844]`, `initial_save` for scenario 1, `frames`. No
  `screens`, no `pad_sheet` (design.md's "no replay of `tests/e2e/`
  carries `screens` or `pad_sheet`" stays true), no `resize`.
- Twin members: `"input": "pad"`, `initial_save` for scenario 1, `frames`;
  **no `window`**: nothing on canvas 0 depends on the window (the present
  only reads canvas 0, H-14-4; step 2.7 kept the 20 hashes at 640×480 and
  checked 390×844 by screen tests), so the equality of a 390×844 touch run
  with a 640×480 twin proves the touch path and the window's absence from
  the capture at once. If a pair diverges, the first diagnostic is to rerun
  the twin with the touch replay's window (scratch, never committed), which
  separates the two causes.
- One frame per line, each starting with `"pad"` (empty list when nothing
  is held) followed by `touches` and/or `mouse` when the frame holds them,
  e.g. `{ "pad": [], "touches": [[2, 90, 640]] }`. Touches are listed in
  id order. Every `.txt` cut recipe of `tests/e2e/` then works unchanged.

**References.**

- Each twin's `.json.sha256` is the sha256 of its own capture (`make
  replay EVENTS=<twin>` writes it under `tmp/love-replay/`), copied in; its
  `.json.end-state` is written by `make end-state-baseline` while the touch
  replays are not yet in `tests/e2e/` (that target writes every missing
  `.end-state`, so a touch replay present at that moment would get its own).
- Each touch replay's `.json.sha256` and `.json.end-state` are byte copies
  of its twin's. Never `make e2e-baseline` in this step: it rewrites every
  `.sha256`, the touch replays' included.
- **`.end-state` files: yes, for touch replays and twins.** `make
  end-state` requires one per replay of `tests/e2e/` and would otherwise
  fail ("no reference"); and the touch copy turns `make end-state` into a
  second equality check, over the whole game state (credits, clock,
  camera, planets, slipways, cheat activations), not only the frames.

**Notes (`.txt`).** First paragraph: what the replay does and exactly one
"<N> frames." Then, for a touch replay: its twin's name; the 390×844 layout
and the touch points with the rectangle each lies in; a frame list giving
each touch or click and the button bits it yields (the twin's frame); the
rules exercised (reveal presses nothing, stick start frame 0, Back in the
stick zone, gamepad press hides and drops, mouse kept between frames);
that its `.sha256` and `.end-state` are the twin's copies and never a
baseline of its own; the negative scratch run(s) and their observed result;
"Not proven here" (the drawing, step 16; a resize; overlay mode; the
5 + 6 chord, covered by the Lua tests). For a twin: its frames
hand-written in the default bindings so the file can be rebuilt, the state
at the key frames (the cut recipe as the other notes give it), and that its
hash is the reference of its touch replay. No exception line in
`tools/e2e_long_exceptions.txt`.

**`tools/e2e_limits.sh`, the pairing rule.** For each given replay named
`touch_<s>.json`, a replay `pad_twin_<s>.json` in the same directory must
be among the given ones, and the first line of the two `.json.sha256`
files must be equal; for each `pad_twin_<s>.json`, `touch_<s>.json` must be
given. Failures, each naming the file: `e2e: <touch>: no pad twin
<twin>`, `e2e: <twin>: twin of no touch replay <touch>`, `e2e: <touch>:
.sha256 differs from its twin's`. A missing `.sha256` is already reported
by the companion rule and is not reported twice. Read **inside the one awk**
the script already runs (the `.sha256` files as extra inputs): no new
process, so `make e2e` pays nothing measurable. Every file is still
examined before concluding. The script's header comment states the rule
(it is a contract the name does not say).

**`test_fake_love_replay`.** `E2E_REPLAYS` gains the eight replays with
their frame counts (counted with `jq '.frames | length'`) and each entry
its expected window: 390×844 for the touch replays, 640×480 for the others;
the test asserts that window and no resize on any frame. Its "every e2e
replay is listed" guard then holds.

**Docs (design.md).** In "`make e2e`": a "**Touch replays.**" paragraph —
`touch_<s>.json` plays a pad scenario with touches (and the mouse) in a
390×844 window; `pad_twin_<s>.json` holds the same frames as gamepad
buttons in the default window; the touch replay's `.json.sha256` and
`.json.end-state` are copies of the twin's; `e2e_limits.sh`'s pairing rule;
after an intended change, `make e2e-baseline` rewrites both and the rule
fails if they differ. Update the counts that name the suite ("`tests/e2e/`:
the 20 replays…" in the layout list, "each missing `.end-state` of the 20
replays" in "`make end-state`"): the 20 C-set replays plus four touch
replays and their four twins. The sentences that say the 20 replays keep
the C game's hashes stay as they are (still true of those 20). `CLAUDE.md`
is not changed. The plan's §6 note is the coordinator's.

## Implementation Chunks

1. **The pairing rule** in `e2e_limits.sh` with its `test_e2e_limits`
   assertions (red first), and design.md's sentence on it.
2. **The four twins** with their `.txt`, `.json.sha256` (from `make
   replay`) and `.json.end-state` (from `make end-state-baseline`), and
   their four entries in `test_fake_love_replay`.
3. **The four touch replays** with their `.txt` and copied references,
   their four entries in `test_fake_love_replay`, the touch replays
   paragraph of design.md; then `make e2e`, `make end-state` and the
   timings.

## Testing Decisions

- **A good test here is a replay pair**: the touch replay's capture equals
  the twin's, frame for frame, and its end state too. That compares the
  external behaviour (what the game draws and holds), never how the pad
  computed the mask. The twin is itself checked like every pad replay: its
  `.txt` readings come from `make state` on the cut file.
- **The pairing rule** is tested in `test_e2e_limits` **without a new
  run** of the script (each run costs ~15 ms on `make test`'s critical
  path): the passing case `test_e2e_of_180_frames_passes` also writes
  `touch_same` and `pad_twin_same` with equal hashes, still exit 0; the
  failing case `test_e2e_over_180_frames_fails_and_is_named` also writes
  `touch_other` and `pad_twin_other` with different `.sha256`, a
  `touch_alone` with no twin and a `pad_twin_orphan` with no touch replay,
  and asserts the three messages and that a matching pair `touch_same` /
  `pad_twin_same` written there too is not named. The write helper takes the hash text as a parameter
  (today it always writes `0000`). Prior art: the existing cases' `strstr`
  on the log.
- **`test_fake_love_replay`**: the extended list (frames and windows),
  prior art its own first test.
- **Scratch negative runs** (never committed, in the session's
  scratchpad), each recorded in the touch replay's `.txt`: scenario 4 with
  the reveal leaking (Back held f50–84 in a twin copy) ends year 3403
  (prototyped); scenario 1 with A pressed on f0–3 is expected to start a
  forgiving game instead of resuming; scenario 3 with Back 29 frames leaves
  year 3401 (prototyped). They show each pair is sensitive
  to the rule it exercises.
- **Check of the step**: each touch `.json.sha256` byte-equal to its twin's
  (`cmp`) and `make e2e` reporting 28 match of 28; `make end-state` 28 of
  28; the 20 original replays, their `.sha256`, `.end-state`, `.txt` and
  the `.cfg` files unchanged (`git diff --stat` on them is empty); `make
  test-all` green; design.md states the convention.
- **Timing, `make e2e`** (§2, step 15 is in the list): the median of 5
  `make e2e` runs here and 5 at the "before" commit (H-14-1), interleaved,
  in the same session; per-frame cost = (user + sys) / frames / 4 cores
  (4 624 frames here, 4 072 before). Pass when this step's median ≤ 8 s
  (1 s under 9 s left for step 16's drawing; the committed touch replays
  are now measured, not the 720-frame cap). Expected: +552 × ~1.1 ms ≈
  +0.6 s over "before". If the "before" commit's own median already
  exceeds 8 s − 0.62 s (a slow session), judge on the paired difference
  (median of per-run differences within the estimated +552 frames' cost
  plus 2 %), and record both numbers and the flag for step 16. A failure
  shortens the scenarios (first lever: idle tails, then scenario 4's
  35-frame reveal hold down to 31, still over the 30-frame year hold)
  before the step closes.
- **Timing, `make test`**: the step adds no process; it adds eight small
  files to `test_fake_love_replay` and a few to two `test_e2e_limits` runs.
  Record both checks' own times before and after (median of 11 runs of
  each binary); the full `make test` gate runs only if either grew by more
  than ~10 ms (§2's threshold).

## Out of Scope

- Drawing the pad (step 16); screen tests and `screens` in replays.
- Any change to `main.lua`, `love.virtual_pad` or the pad modules, the
  layout, the bindings, the fake LÖVE or its replay format.
- A touch replay with a `resize`, in overlay mode, or with the 5 + 6
  cheat chord (the Lua tests cover them; not among §2's planned scenarios).
- Changing the 20 original replays or their references, and `make
  e2e-baseline` on any touch replay.
- Making `make e2e-baseline` skip or derive the touch replays' hashes (the
  pairing rule catches a drift instead).
- `CLAUDE.md`, and the plan's §6 step note (the coordinator's).

## Further Notes

- The scenario frames were prototyped as twins in this session's
  scratchpad with `make state` on the committed fake and game (step 14
  not yet done, so only the twins could run): the end states quoted above
  are those runs.
- Scenario 1 starts from a record because a new game reaches its first
  slipway only after a probe, a fold and two colonies (~250 frames,
  pad_slipway), past the 180-frame limit and most of the 720-frame budget.
  The record is pad_bankrupt's, as attract.json reuses pad_tech_costs':
  cut from a real game, not typed.
- If step 14 lays out the touches at a size other than
  `getDimensions()` (H-14-2) or holds the pad state in `debug_state`
  (H-14-4), the equality of the `.end-state` copies may fail while the
  hashes match: the end-state decision is then revisited, not the hashes.
- iOS may report a finger as the mouse too; no replay models it (step
  14's PRD notes it is harmless).
