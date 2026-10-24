---
title: "Wire the virtual pad into love.update: touches and clicks reach the game, nothing drawn yet"
description: "The modules that turn touches into the virtual pad's mask and decide when the pad is shown exist, but main.lua never reads a touch or a click, so a phone player still cannot play."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) plays the game on a portrait iPhone with a virtual
pad and keeps the real gamepad on desktop. Steps 10 to 13 built the pieces
as pure modules: `love.pad_layout` places the pad, `love.pad_stick` turns a
stick touch into directions, `love.pad_touch` turns the touches into the
virtual 12-bit mask and ORs it with the gamepad's, and `love.pad_visibility`
(step 13) decides when the pad is shown and holds back the touch that
reveals it. None of them is called: `main.lua`'s `love.update` still builds
`pad_buttons` from `read_pad_buttons()` alone. A finger on the glass or a
left click does nothing; the game on the phone cannot be played at all.

Plan §2 (Input) fixes how the wiring must look: touches polled with
`love.touch.getTouches` / `getPosition`, the mouse with its left button
down as one more touch (`love.mouse.getPosition` / `isDown`), polling and no
callbacks, and the virtual mask OR-ed with the real gamepad's in
`love.update`, **before** `input_step.run`, with no other input path. Step
13's PRD states the call order as a contract: gamepad mask, then
`pad_visibility.filter`, then `pad_touch.update`, then `pad_touch.combine`.

The step must change nothing for the 20 replays of `tests/e2e/`: they carry
no touch and no click, so their `*.sha256` and `*.end-state` must stay byte
for byte, and the extra per-frame work runs on all 4 072 of their frames, so
`make e2e` must stay inside plan §2's time budget.

## Solution

1. A new pure-Lua module, **`love.virtual_pad`** (in `game/love/`, beside
   the pad modules), owns the whole per-frame pipeline behind one call:
   from the gamepad mask, this frame's touches and mouse, and the window
   size, it lays the pad out, filters through `pad_visibility`, runs
   `pad_touch`, and returns the `pad_buttons` the game sees. It also says
   whether the pad is shown and keeps this frame's placed layout, which is
   what step 16 will draw from.
2. **`main.lua`** gains two small readers — the touches from `love.touch`,
   the mouse from `love.mouse` — a `virtual_pad` state table initialised in
   `love.load`, and one line in `love.update` that passes
   `virtual_pad.update(...)`'s result as `raw.pad_buttons` to
   `input_step.run`. Nothing is drawn: `love.draw` is unchanged.
3. **Lua tests** in `tests/lua/game/` (the one `lua-test-game` process)
   prove the composition: with no touch the gamepad mask passes unchanged
   (the reason the 20 replays cannot move), the revealing touch presses
   nothing, the next touch presses, a gamepad press hides the pad and
   releases the touch, the layout follows the window size frame by frame.
4. A **scratch replay** (not committed; step 15 commits the touch replays)
   drives the real `main.lua` in the fake with touches and a click, to prove
   the `love.touch` / `love.mouse` readers before step 15 depends on them.
5. **design.md** gains a "**Virtual pad.**" paragraph after "**Pad
   visibility.**" describing the wiring.

For the player: on a phone, the first touch reveals the (still invisible,
until step 16) pad and presses nothing; later touches play the game through
the pad's layout; picking up a controller works as before. On desktop
nothing changes until the first left click.

## User Stories

1. As a player on a phone, I want my touches on the pad's area to reach the game as button presses, so that I can play with no controller.
2. As a player on a phone, I want the stick zone to steer the cursor, so that I can aim and pan with my thumb.
3. As a player on a phone, I want a diagonal on the stick to move on both axes, so that aim pans behave as with a real stick.
4. As a player on a phone, I want sliding a finger from A to B to release A and press B, so that the pad behaves like the plan says.
5. As a player on a phone, I want two fingers on A and B to press both, so that the developer cheat chord stays reachable.
6. As a player on a phone, I want my first touch to press nothing, so that I never press a button I could not see.
7. As a player on a phone, I want my second touch, after the first, to press normally, so that I can start playing right away.
8. As a player on desktop, I want a left click on the pad's area to count as a touch, so that I can try the pad with the mouse.
9. As a player on desktop, I want hovering the mouse with its button up to press nothing and reveal nothing, so that moving the mouse is harmless.
10. As a player with a controller, I want the game to play exactly as before, so that the virtual pad costs me nothing.
11. As a player with a controller, I want pressing any of its buttons to hide the pad and release whatever my fingers held, so that no button stays stuck.
12. As a player using both, I want the controller's buttons and the pad's buttons to add up on the same frame, so that neither input eats the other.
13. As a player watching the attract demo, I want my first touch only to reveal the pad, so that the demo goes on until I press something.
14. As a player watching the attract demo, I want my next press on the pad to leave the demo, so that it ends as with a controller.
15. As a player who rotates or resizes the window, I want the pad's buttons to follow the new layout on the very frame of the resize, so that a touch lands where the pad is.
16. As a player in tall mode, I want a touch on the game's half (top) to press nothing, so that only the pad's half is the pad.
17. As a player in overlay mode, I want the pad to lie exactly over the game's rectangle, so that the touch areas match what step 16 will draw.
18. As a player on a phone, I want a touch that lifts to release its button at once, so that nothing stays held.
19. As a player on a phone, I want a finger that stays on the glass across frames to keep the same identity, so that the stick stays mine while I hold it.
20. As a developer of step 15 (touch replays), I want `main.lua` to read the touches and the mouse from the fake's polling functions, so that a replay's `touches` and `mouse` fields drive the game.
21. As a developer of step 15, I want the readers proven on a scratch replay now, so that a failure in step 15 is about the replay, not about the wiring.
22. As a developer of step 16 (drawing), I want one state table holding the pad's visibility, its stick and this frame's placed layout, so that the draw reads what `love.update` computed and never recomputes it.
23. As a developer of step 16, I want `virtual_pad.shown(...)`, so that I draw the pad only when it is shown.
24. As a developer, I want the pipeline in a pure module, so that its order and its result run under plain LuaJIT in `lua-test-game` with no stub of `love`.
25. As a developer, I want `main.lua` to stay a thin layer of `love.*` reads, so that the untestable part is a few lines.
26. As a developer, I want the call order fixed by step 13 (filter, then update, then combine) kept exactly, so that the held-back touches look lifted to `pad_touch`.
27. As a developer, I want the window size read with `love.graphics.getDimensions()`, the same call as `screen.present`, so that the touch layout and the drawn game always agree on a frame.
28. As a developer, I want the mouse passed as a record every frame (`down` false when up), so that `pad_touch` and `pad_visibility` see one shape.
29. As a developer, I want malformed input to the new module refused by an `assert` naming the field, so that a wiring mistake fails at the call.
30. As a developer, I want the new module's exports to follow CLAUDE.md naming, so that `make lua-names` stays green.
31. As a developer, I want `lua-guard` green with the new `love.touch` and `love.mouse` calls in `main.lua`, so that only listed LÖVE functions are used.
32. As a reviewer, I want the 20 replays' `*.sha256` and `*.end-state` unchanged, so that the wiring is proven to change nothing without touches.
33. As a reviewer, I want `make e2e` measured beside the "before" commit and projected with the 720-frame touch cap, so that steps 15 and 16 still fit under 9 seconds.
34. As a reviewer, I want `lua-test-game`'s own time recorded before and after, so that the new test file is known not to slow `make test`.
35. As a maintainer, I want design.md to state how `love.update` builds `pad_buttons`, so that a later input change is a decision, not a drift.
36. As a maintainer, I want the hypotheses about step 13 listed and checked first, so that a difference in step 13's real interface is caught before any code is written.

## Implementation Decisions

**Hypotheses on step 13 (not yet done; its PRD is
`docs/prd/triage/phase3-step13-show-hide.md`).** The implementer checks each
one first against the committed code; one that fails is reported, and the
decision resting on it is revisited (step 13's real interface wins over this
PRD's wording).

- **H-13-1**: step 13 is committed and `make test-all` is green on it; its
  last commit (the plan's step-13 note included, if any) is this step's
  "before" commit for both timings.
- **H-13-2**: `love.pad_visibility` exists in `game/love/`, is pure (no
  `love.*` call, no module state, requires only `bit`) and loads under plain
  LuaJIT; it exports `init(visibility)`, `shown(visibility)` and
  `filter(visibility, gamepad_mask, touches, mouse)`.
- **H-13-3**: `filter` takes `touches` and `mouse` in `pad_touch.update`'s
  shapes (an array of `{id, x, y}`; `nil` or `{x, y, down}`) and returns
  `touches, mouse` to pass to `pad_touch.update` unchanged: a new array of
  the caller's records, and the caller's mouse or `nil`. It never modifies
  its inputs.
- **H-13-4**: the pad starts hidden after `init`, and while hidden `filter`
  passes no touch and no mouse; a frame with no touch and the mouse up
  passes an empty array and leaves the pad hidden whatever the gamepad mask.
  Together with `pad_touch.update` returning 0 for no touch, this is why the
  20 replays keep their hashes.
- **H-13-5**: `filter` asserts that `gamepad_mask` is an integer 0–4095;
  `read_pad_buttons()` always is one.
- **H-13-6**: the call order is the one step 13's PRD writes into design.md:
  `gamepad = read_pad_buttons()`; `touches, mouse =
  pad_visibility.filter(visibility, gamepad, raw_touches, raw_mouse)`;
  `virtual = pad_touch.update(touch_state, touches, mouse, placed)`;
  `pad_buttons = pad_touch.combine(gamepad, virtual)`.
- **H-13-7**: design.md has a "**Pad visibility.**" paragraph right after
  "**Pad touch.**"; this step's paragraph goes right after it (if absent,
  right after "**Pad touch.**").
- **H-13-8**: step 13 changes neither `main.lua`, `love.pad_touch`,
  `love.pad_stick`, `love.pad_layout`, `love.window_layout`,
  `love.screen`, `game.attract` nor `game.input_step`, and adds only
  `tests/lua/game/test_pad_visibility.lua` to the runner.

**Facts from the committed code.** `main.lua`'s `love.update` builds `raw =
{mouse_x = 0, mouse_y = 0, mouse_buttons = 0, pad_buttons =
read_pad_buttons()}` and calls `input_step.run(step, raw)`, then
`cart.update60()`; `read_pad_buttons` returns 0 with no joystick.
`screen.present` lays out with `window_layout.compute(
love.graphics.getDimensions())` every frame; there is no `love.resize`
handler. `pad_touch.update` refuses a touch whose `x` or `y` is not a
number and a mouse without a boolean `down`; it treats a touch missing
from a call as lifted. `tools/lua_guard.lua` already lists
`love.touch.getTouches`, `love.touch.getPosition`,
`love.mouse.getPosition` and `love.mouse.isDown`. The fake answers them
(design.md "Touch and mouse semantics"): `getTouches()` a new sequence of
light-userdata ids, stable while a finger is held; `getPosition(id)` its x
and y; the mouse at (0, 0), up, until a replay frame carries `mouse`;
`isDown` only as `isDown(1)`. `conf.lua` leaves the touch and mouse modules
enabled.

**The module, `love.virtual_pad`.** Pure: it requires `love.window_layout`,
`love.pad_layout`, `love.pad_visibility` and `love.pad_touch`; no `love.*`
call; no module state — everything lives in the caller's table (the
`input_step.init(step)` / `pad_touch.init(touch_state)` pattern). Its job
is the order and the layout, nothing else: every rule about touches,
sticks and visibility stays in the modules it calls.

Exports (CLAUDE.md naming; none repeats `virtual` or `pad`):

- `init(virtual_pad)`: sets `virtual_pad.visibility = {}` and
  `virtual_pad.touch_state = {}`, initialised by `pad_visibility.init` and
  `pad_touch.init`, and `virtual_pad.placed = nil`. Returns nothing.
- `update(virtual_pad, gamepad_mask, touches, mouse, width, height)`, once
  per frame: `layout = window_layout.compute(width, height)`; `placed =
  pad_layout.place(layout.pad)`; then the four calls of H-13-6 in that
  order; stores `virtual_pad.placed = placed`; returns the combined mask
  (an integer 0–4095). `touches` and `mouse` are the raw readings in
  `pad_touch`'s shapes; they are never modified.
- `shown(virtual_pad)`: `pad_visibility.shown(virtual_pad.visibility)`.

Readable fields, documented in design.md for step 16 and nothing else:
`touch_state.stick` (the stick to draw, `nil` without one) and `placed`
(this frame's screen rectangles; `nil` before the first `update`).
`visibility` is private: callers use `shown`.

Decisions behind it:

- **A module rather than inline code in `main.lua`**: `main.lua` runs only
  in the fake or real LÖVE, where no Lua test reaches it; the module makes
  the order and the per-frame layout a plain-LuaJIT test, and gives step 16
  one table to read. `main.lua` keeps only what needs `love.*`.
- **The layout is recomputed every frame** from the size passed in, as
  plan §2 ("recomputed every frame") and `screen.present` do; no cache
  keyed on the size. Its cost is a few small tables per frame, measured by
  the `make e2e` timing below; a cache is the first rework if the budget
  fails.
- **The window size comes from the caller** (`love.graphics.getDimensions()`
  in `love.update`), the same call `screen.present` makes in `love.draw` of
  the same frame; the fake calls `love.resize` before `love.update`, so a
  resize frame lays touches and the present out alike.
- **Refusals**: `virtual_pad` not a table or never `init`ed (`touch_state
  == nil`) fails an `assert` starting with `virtual_pad.update:` (or
  `virtual_pad.shown:`) naming the field; bad sizes, masks, touches and
  mice are left to the asserts of `window_layout.compute`,
  `pad_visibility.filter` and `pad_touch.update`, which already name the
  field. No second copy of their checks.

**`main.lua`.**

- Requires `love.virtual_pad`; a module-level `virtual_pad = {}` beside
  `step`, initialised by `virtual_pad.init` in `love.load` after
  `input_step.init`.
- `read_touches()`: `love.touch.getTouches()`, then one `{id = id, x = x,
  y = y}` record per id from `love.touch.getPosition(id)`, in the returned
  order (the game must not depend on it, design.md; `pad_touch` takes the
  first new touch in list order for the stick, and two touches landing on
  the same frame in the stick zone is an ordering the plan accepts).
- `read_mouse()`: `x, y = love.mouse.getPosition()`, `down =
  love.mouse.isDown(1)`; returns `{x = x, y = y, down = down}` every frame
  (never `nil`). Only the left button, as the fake's function list.
- `love.update(dt)`: `raw.pad_buttons = virtual_pad.update(virtual_pad,
  read_pad_buttons(), read_touches(), read_mouse(),
  love.graphics.getDimensions())`, then `input_step.run` and
  `cart.update60()` as today. `mouse_x`, `mouse_y`, `mouse_buttons` stay 0:
  the PICO-8 mouse is not a path the plan adds ("no other input path").
- `love.draw` is unchanged; no `love.resize` handler; no touch or mouse
  callback.
- Comments follow CLAUDE.md: none, unless one carries a trap (e.g. why the
  mouse is a record even when up, if the name does not say it).

**Docs.** design.md gets a "**Virtual pad.**" paragraph after "**Pad
visibility.**" (H-13-7): `love.update` reads the gamepad, the touches, the
mouse and `getDimensions()`, calls `virtual_pad.update`, and passes its
result as `pad_buttons`; the module's three functions and two readable
fields; "nothing is drawn yet" is not written (design.md says what is true:
the pad is not drawn, step 16 changes that sentence). design.md's
function list is unchanged (the four functions are already listed). The
plan's §6 note is the coordinator's.

## Implementation Chunks

1. `love.virtual_pad` with its tests (module alone, through the real
   `pad_visibility`, `pad_touch` and layouts), then design.md.
2. `main.lua`'s readers and the `love.update` line; then the scratch replay,
   the 20 replays, the end states and the two timings.

## Testing Decisions

- **A good test** calls `virtual_pad.init`, then `virtual_pad.update` frame
  after frame with literal gamepad masks, touch records, mouse records and
  window sizes, and checks the returned mask, `shown(...)`, and where it
  matters `touch_state.stick == nil` and `placed`'s rectangles, with exact
  `==` (`check.expect`). Positions come from the real layout
  (`pad_layout.place(window_layout.compute(w, h).pad)`, the helper of
  `test_pad_touch.lua`), named by what they hit (A's centre, B's centre,
  a free stick-zone point). It never reads `visibility`'s private fields
  and never re-tests a rule owned by `pad_touch` or `pad_visibility` beyond
  what proves they are called in order.
- **New test file** `tests/lua/game/test_virtual_pad.lua` in the one
  `lua-test-game` process, ending with `return check.run({...})` (prior
  art: `test_pad_touch.lua`, `test_window_layout.lua`'s `pcall` refusals).
- **Cases**:
  - **No touch, the gamepad passes unchanged**: for every mask 0–4095 in
    one loop on one state (mouse `{0, 0, down = false}`, no touch, 640×480)
    the result equals the mask, and the pad stays hidden; same at 390×844.
    This is the 20 replays' invariant.
  - **The revealing touch presses nothing, the next one presses** (390×844):
    touch 1 on A → 0, shown; held → 0; lifted, touch 2 on A → 16; gamepad
    32 held too → 48.
  - **Stick through the wiring**: after a reveal, a touch at a free
    stick-zone point then 60 points right → 8 (RIGHT) and
    `touch_state.stick ~= nil`.
  - **A gamepad press hides and releases**: shown, finger on A → 16;
    gamepad 1 → 1 and hidden; finger still on A, gamepad 0 → 0.
  - **Mouse as a touch**: at 640×480 (overlay), a click (down) on A's
    centre reveals → 0; up → 0; down again on A → 16; hovering with `down
    = false` on a hidden pad reveals nothing.
  - **Tall mode's game half is not the pad**: after a reveal, a touch at
    (195, 200) at 390×844 → 0.
  - **The layout follows the size frame by frame**: shown at 390×844, a
    touch new on A's 390×844 centre → 16; the next frame at 640×480 a new
    touch at A's 640×480 centre → 16, and `placed.buttons.a` equals
    `pad_layout.place(window_layout.compute(640, 480).pad).buttons.a`
    field by field.
  - **Inputs untouched**: after an `update`, the caller's touch array keeps
    its length and records, and the mouse record its fields.
  - **Refusals**: `update` with a non-table state, a never-`init`ed state;
    `shown` with a never-`init`ed state: each `pcall` fails with a message
    naming the field. One case that a bad width reaches
    `window_layout.compute`'s message (proves the size is passed, not
    re-checked).
- **`main.lua` is not unit-tested** (it needs `love`); it is covered by:
  - **the 20 replays**: `make e2e` and `make end-state` 20/20, every
    `*.sha256` and `*.end-state` unchanged — no replay carries `touches` or
    `mouse`, so the readers return an empty array and an up mouse at (0, 0)
    on every frame (H-13-4);
  - **a scratch replay**, in the session's scratchpad, never committed (step
    15 owns the committed touch replays): 390×844, `"input": "pad"`, a
    revealing touch on A held a few frames then lifted, then touches on A
    and B together for several frames (the 5 + 6 cheat chord), then a click
    with `mouse`; run with `make end-state EVENTS=<scratch> REFERENCE=<a
    scratch end-state>` or `make replay`; the run must finish with no error
    and its end state must show `cheat_activations` 1 (the chord reached
    `input_step` through the touches; the revealing touch did not). The
    replay, the command and the observed line go in the report.
  - `lua-guard`, `lua-test-modules` and `lua-names` green.
- **Timing, `make test`**: only a `tests/lua/game/` file and a pure module
  are added. Record `lua-test-game`'s own time before (H-13-1's commit) and
  after: median of 11 runs of the runner command itself (the Makefile's
  `LUA_PATH=… luajit tests/lua/game/runner.lua …` line), not through
  `make`. The 4 096-mask loop must cost under ~2 ms; the full `make test`
  gate runs only if `lua-test-game` grew by more than ~10 ms (plan §2).
- **Timing, `make e2e`** (plan §2, step 14 is in the list): the median of 5
  `make e2e` runs here and 5 at the "before" commit, in the same session,
  interleaved; per-frame cost = (user + sys) / 4 072 frames / 4 cores.
  Pass when this step's median + 720 × 1.124 ms ≤ 8 s (1 s under 9 s for
  steps 15–16). If the "before" commit's own projection already exceeds it
  (a slow session), judge on the paired difference (median of per-run
  differences ≤ 2 %) and record both numbers and the flag. A failure is
  reworked here (first lever: cache `placed` by window size) before the
  step closes.
- **Check of the step**: the new tests pass; `make test-all` green; `make
  e2e` and `make end-state` 20/20 with every `*.sha256` and `*.end-state`
  unchanged; the scratch replay's result; design.md states the wiring; the
  timing numbers in the report.

## Out of Scope

- Drawing the pad, its sprites, the stick base and knob (step 16);
  `love.draw` does not change.
- Committed touch e2e replays and their pad twins (step 15).
- Any change to `love.pad_touch`, `love.pad_stick`, `love.pad_layout`,
  `love.pad_visibility`, `love.window_layout`, `love.screen`,
  `game.input_step`, `game.attract` or the bindings.
- Any change to the fake LÖVE or its function list.
- The PICO-8 mouse fields of `raw` (`mouse_x`, `mouse_y`, `mouse_buttons`):
  they stay 0.
- Touch or mouse callbacks, right or middle mouse buttons, a `love.resize`
  handler.
- Writing plan §6's step note (the coordinator does, from the report).

## Further Notes

- **`highdpi` and units**: in LÖVE 11, touch and mouse positions and
  `love.graphics.getDimensions()` are all in DPI-scaled units on a
  `highdpi` window, so they agree; the fake does not model DPI. If the pad
  answers in the wrong place on the phone, check this first (beside the
  `newCanvas` DPI risk of step 2.7's note).
- **iOS may report a finger as the mouse too**: both land on the same frame,
  so both are revealing and both are held back (step 13); later the finger
  and its mouse press the same button, an OR, harmless.
- **The first frame**: a gamepad button already held on the first
  `love.update` counts as a press for `pad_visibility` (previous mask 0);
  the pad is hidden anyway, so nothing changes.
- **Touches during `love.load`**: none in the fake; `virtual_pad.init` reads
  no input, so a real touch held at start is simply the revealing touch on
  the first frame.
