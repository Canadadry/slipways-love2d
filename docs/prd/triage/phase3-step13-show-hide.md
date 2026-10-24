---
title: "Show / hide: the virtual pad starts hidden, a touch reveals it, a gamepad button hides it"
description: "Nothing decides when the virtual pad is visible, keeps the revealing touch from pressing an invisible button, or releases every touch when a real gamepad takes over."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`, §2 Input, "Shown / hidden") plays the game on a
portrait iPhone with a virtual pad, and on desktop with a gamepad or a mouse.
Step 10 placed the pad (`love.pad_layout`), step 11 turned one stick touch
into directions (`love.pad_stick`), step 12 turns the touches into the
virtual mask (`love.pad_touch`). What is still missing is the rule that
decides whether the pad is there at all:

- the pad starts hidden: a player with a controller never sees it;
- the first touch or click while it is hidden shows it and **presses
  nothing for as long as it is held** — otherwise a tap would press a button
  the player could not see (A, Start, or the stick under the finger);
- any real gamepad button pressed hides it **and drops the held touches**,
  so no virtual button and no stick direction stays stuck under a finger
  that is still resting on the glass;
- the next touch shows it again, under the same rule;
- once shown it stays shown (no fade, no timer);
- in the attract demo the first touch only shows the pad (the demo goes
  on); the next press leaves the demo like any button.

Without one tested module holding these rules, step 14 (wiring into
`main.lua`) would write them inline, where no headless test reaches them,
and the first place a stuck button showed would be step 15's touch replays.

## Solution

A new pure-Lua module, `love.pad_visibility`, beside `love.pad_touch`. It is
a **filter in front of `pad_touch.update`**: each frame it takes the real
gamepad mask and this frame's touches and mouse, updates the shown / hidden
state, and returns the touches and the mouse that `pad_touch` may see. A
touch it holds back is simply absent from what `pad_touch` gets, so
`pad_touch` treats it as lifted: its stick is dropped and its buttons are
released with no call to `pad_touch.init`.

1. **`init(visibility)`**: hidden, nothing held back, previous gamepad mask
   0.
2. **`filter(visibility, gamepad_mask, touches, mouse)`**, once per frame,
   before `pad_touch.update`; returns `touches, mouse` to pass on.
3. **`shown(visibility)`**: whether the pad is shown, for step 16's drawing.
4. **Lua tests** in `tests/lua/game/` (the one `lua-test-game` process): the
   module alone, then through `pad_touch` (the revealing touch gives mask 0
   wherever it lands; a gamepad press releases a held button and a held
   stick), and through `attract.frame` (the revealing touch does not leave
   the demo; the next press does).
5. design.md gains a "**Pad visibility.**" paragraph after
   "**Pad touch.**".

Nothing calls the module yet (step 14 does); the game and the 20 replays do
not change.

## User Stories

1. As a player with a controller, I want the virtual pad hidden when the game starts, so that it never covers the game I play with my controller.
2. As a player on a phone, I want my first touch to show the pad, so that I find the controls without a menu.
3. As a player on a phone, I want that first touch to press nothing, even if it lands where A or Start will appear, so that I never press a button I could not see.
4. As a player on a phone, I want that first touch to press nothing even if it lands in the stick zone, so that the cursor does not move before I see the stick.
5. As a player on a phone, I want that first touch to keep pressing nothing while I hold it and slide it, so that sliding the revealing finger onto a button does nothing.
6. As a player on a phone, I want that first finger, once lifted, to be forgotten, so that my next touch at the same place presses normally.
7. As a player on a phone, I want a second finger landing while the revealing finger is still held to work normally, so that I can start playing at once with my other hand.
8. As a player, I want two fingers landing together on a hidden pad both to press nothing, so that a two-finger tap cannot press an invisible chord.
9. As a player on desktop, I want my first left click to show the pad and press nothing while held, so that the mouse follows the same rule as a finger.
10. As a player on desktop, I want the click after that to press, so that I can try the pad with the mouse.
11. As a player on desktop, I want a mouse that only moves with its button up to show nothing, so that hovering never reveals the pad.
12. As a player, I want the pad to stay shown while I do not touch it, with no fade or timeout, so that it is there when I come back.
13. As a player who picks up the controller, I want any of its buttons to hide the pad, so that the game is unobstructed again.
14. As a player who picks up the controller while a finger still rests on A, I want A released at once, so that no button stays stuck.
15. As a player who picks up the controller while my thumb still holds the stick, I want the direction released at once, so that the cursor stops.
16. As a player, I want the fingers that were down when the pad hid to stay ignored until I lift them, so that a resting finger does not show the pad again by itself.
17. As a player, I want my next new touch after a hide to show the pad again and press nothing while held, so that showing it again follows the same rule as the first time.
18. As a player holding a controller button while playing on the shown pad, I want the pad to stay shown, so that only a new press hides it, not a button held since before.
19. As a player, I want any controller button — direction, face, shoulder, Start or Back — to count as a press that hides the pad, so that the rule needs no exceptions.
20. As a player, I want a controller press on the same frame as a new touch to win (the pad hides, the touch is ignored until lifted), so that the outcome does not depend on which came first within a frame.
21. As a player, I want a controller press while the pad is already hidden to change nothing, so that hiding twice is harmless.
22. As a player, I want the controller's own buttons always to reach the game, shown or hidden, so that hiding the pad never eats a controller press.
23. As a player watching the attract demo, I want my first touch only to show the pad, so that I see the controls before anything happens.
24. As a player watching the attract demo, I want my next press on the pad to leave the demo like any button, so that the demo ends the way it ends with a controller.
25. As a player watching the attract demo, I want a controller button to leave the demo as before and hide the pad, so that nothing changes for controller players.
26. As a developer of step 14 (wiring), I want one call that turns this frame's raw touches and mouse into the ones `pad_touch.update` may see, so that `love.update` stays a few lines.
27. As a developer of step 14, I want the filter to take the gamepad mask `read_pad_buttons` already computes, so that it reads no `love.*` function itself.
28. As a developer of step 14, I want the filter never to modify the lists I pass, so that I can build them fresh or reuse them.
29. As a developer of step 16 (drawing), I want `shown(visibility)`, so that I draw the pad only when it is shown.
30. As a developer of step 16, I want a hidden pad always to give `pad_touch` no touch, so that no pressed sprite or knob is ever computed for an invisible pad.
31. As a developer, I want the held-back touches to look lifted to `pad_touch`, so that `pad_touch` needs no knowledge of visibility and no new function.
32. As a developer, I want the module free of `love.*` calls and of module state, so that it runs under plain LuaJIT in `lua-test-game` and `lua-test-modules`.
33. As a developer, I want malformed input (a gamepad mask outside 0–4095 or not an integer, a non-table touch list, a touch with no id, two touches with one id, a mouse without a boolean `down`, a state never `init`ed) refused with an `assert` naming the field, so that a wiring mistake fails at the call.
34. As a developer, I want the exports to follow CLAUDE.md naming, so that `make lua-names` stays green.
35. As a maintainer, I want the show / hide rules written once in design.md, so that a later change is a decision, not a drift.
36. As a reviewer, I want tests that drive the real `pad_touch` and `attract` with positions from a real placed layout, so that the "presses nothing" and "leaves the demo" rules are proven end to end in Lua.
37. As a reviewer, I want the 20 replays' `*.sha256` and `*.end-state` unchanged, so that the step is proven to change no game behaviour.
38. As a reviewer, I want `lua-test-game`'s own time recorded before and after, so that plan §2's timing rule for steps 10–13 is followed.

## Implementation Decisions

**Hypotheses on step 12 (not yet done; its PRD is
`docs/prd/triage/phase3-step12-touch-to-mask.md`).** The implementer checks
each first; one that fails is reported, and the decision resting on it is
revisited (step 12's real interface wins over this PRD's wording). The
module itself (chunk 1) depends on none of them; only the tests through
`pad_touch` (chunk 2) and the design.md placement do.

- **H-12-1**: step 12 is committed and `make test-all` is green on it; its
  last commit is this step's "before" commit for the `lua-test-game`
  timing.
- **H-12-2**: `love.pad_touch` exists in `game/love/`, is pure (no
  `love.*` call, no module state) and loads under plain LuaJIT; it exports
  `BUTTON_MASKS`, `init(touch_state)`, `update(touch_state, touches, mouse,
  placed)` and `combine(gamepad_mask, virtual_mask)`.
- **H-12-3**: `update` takes `touches` as an array of records `{id, x, y}`
  (any non-nil id, numeric coordinates, ids unique), `mouse` as `nil` or
  `{x, y, down}` with a boolean `down`, and `placed` from
  `pad_layout.place`; it returns the virtual mask, an integer 0–4095.
- **H-12-4**: a touch id absent from one `update` is lifted: if it owned the
  stick, the stick is dropped (`touch_state.stick == nil`); its buttons are
  released. A `nil` mouse and a mouse with `down == false` are both "no
  mouse touch". So leaving a touch out of the list is exactly dropping it.
- **H-12-5**: only a new touch (an id absent from the previous `update`)
  can claim the stick; a touch present on every frame since it landed but
  hidden from `update` for a while, then shown, would be new — this module
  never does that (a held-back touch is passed again only after it lifts).
- **H-12-6**: `update` accepts an empty array (it returns 0 with no stick)
  and an array that is a fresh table holding the caller's own records.
- **H-12-7**: `BUTTON_MASKS.a` is 16, `.b` 32, `.start` 1024 (bits of
  `p8.input.PAD_BUTTON_5 … 12`), and `combine` is the OR of two masks.
- **H-12-8**: step 12 adds design.md's "**Pad touch.**" paragraph right
  after "**Pad stick.**"; this step's paragraph goes right after it (if
  absent, right after "**Pad stick.**").
- **H-12-9**: step 12 adds only `tests/lua/game/test_pad_touch.lua` to the
  runner and changes neither `runner.lua`, `check.lua`, `love.pad_layout`,
  `love.pad_stick`, `love.window_layout` nor `game.attract`.

**Facts from the code (committed).** `game.attract.frame(attract, on_title,
raw_buttons, played_buttons)` leaves the demo (`ACTION_RETURN`) on any frame
of `PHASE_PLAYING` whose `raw_buttons ~= 0`, then swallows input until every
button is up; `attract.init(attract, attract_after)` takes a threshold ≥ 1.
`main.lua`'s `read_pad_buttons` returns the 12-bit gamepad mask (0 with no
joystick). `love.pad_layout.place` and `love.window_layout.compute` give the
placed pad; at 390×844 the stick zone is x 0–195, y 486.75–779.25, A's
centre (292.5, 720.75), B's (351, 662.25), Start's (229.125, 745.125); a
free stick-zone point is (100, 650).

**The module, `love.pad_visibility`** (in `game/love/`, beside
`love.pad_touch`). Pure: it requires only `bit`; no `love.*` call; no module
state — everything lives in the state table the caller holds (the
`pad_touch.init(touch_state)` / `input_step.init(step)` pattern). It never
reads touch coordinates: positions are `pad_touch`'s business.

**Exports** (CLAUDE.md naming; none repeats `pad` or `visibility`):

- `init(visibility)`: sets `visibility.visible = false`,
  `visibility.held_back = {}` (the set of touch ids ignored until they
  lift), `visibility.mouse_held_back = false`,
  `visibility.previous_gamepad_mask = 0`. Returns nothing. The fields are
  private: tests and callers use `shown`.
- `shown(visibility)`: `true` when the pad is shown, else `false`.
- `filter(visibility, gamepad_mask, touches, mouse)`: returns two values,
  `passed_touches` (a new array, possibly empty, holding in their original
  order the caller's records that are not held back — the same tables, not
  copies) and `passed_mouse` (the caller's `mouse` table, or `nil` when the
  mouse is `nil` or held back). `touches` and `mouse` have step 12's shapes
  (H-12-3); the inputs are never modified.

**The rules of one `filter`** (in this order):

1. **Forget lifted touches.** Every id in `held_back` absent from `touches`
   leaves the set. `mouse_held_back` becomes `false` when `mouse` is `nil`
   or `mouse.down` is `false`.
2. **Gamepad press.** A *press* is a bit set in `gamepad_mask` and not in
   `previous_gamepad_mask` (`bit.band(gamepad_mask,
   bit.bnot(previous)) ~= 0`); any of the 12 bits counts. On a press the
   pad becomes hidden (if it was shown) and **every** touch of this frame
   joins `held_back`, plus the mouse if `down` (`mouse_held_back = true`).
   A press while already hidden changes nothing more.
3. **Reveal.** Otherwise, if the pad is hidden and some touch of this frame
   is not in `held_back` (or the mouse is `down` and not held back), the pad
   becomes shown and every such touch (and the mouse) joins `held_back`: all
   the touches that reveal the pad on one frame are revealing touches.
4. While the pad is shown and there is no press, new touches pass through;
   the held-back ones stay out until they lift.
5. `previous_gamepad_mask = gamepad_mask`.
6. Return the touches not in `held_back`, and the mouse unless it is `nil`
   or held back.

Decisions behind them:

- **"Pressed" is an edge**, not a level: a gamepad button held since before
  the pad was shown does not keep it hidden or hide it again. A level rule
  would let a held controller button and a touch hide and show the pad on
  alternate frames. A button already held at the first `filter` counts as a
  press (previous mask 0), harmless since the pad starts hidden.
- **The press wins over a reveal on the same frame**: the pad hides and the
  new touch is held back like the others, so it does not reveal the pad
  later; only a touch that lands after the press does.
- **While hidden, `filter` passes nothing**: every touch present while
  hidden is held back (a revealing one, or one held at the hide). This is
  what makes the revealing touch press nothing wherever it lands or slides,
  and what makes a hide release every button and the stick (H-12-4), with
  no call to `pad_touch.init`.
- **Held back until lifted**, not just for one frame: the plan's "for as
  long as it is held". A held-back mouse is lifted by its button going up
  (or a `nil` mouse); its position is ignored.
- **A held-back touch that lifts is forgotten**, so an id reused later by
  the OS is a new touch, both here and in `pad_touch` (which saw it absent,
  H-12-5).
- **No timer, no fade**: once shown, only a gamepad press hides the pad.
- **The gamepad mask is never filtered**: `filter` reads it but step 14
  ORs it in with `pad_touch.combine` whatever the visibility.
- **The attract demo needs no special case**: the revealing touch gives
  `pad_touch` nothing, so the combined mask stays 0 and `attract.frame`
  keeps playing; the next touch that presses a button (or pushes the stick
  out of its dead zone) gives a non-zero mask and leaves the demo. A touch
  that lands in the stick's dead zone is not a press, exactly as for any
  input (`raw_buttons ~= 0`). A revealing touch on the title does not reset
  the attract idle counter either (it presses nothing): accepted, the demo
  may start while the revealing finger is still down, and the next press
  leaves it.
- **Refusals** (`assert`, message starting with `pad_visibility.filter:` and
  naming the field, the style of `window_layout.compute` and step 12):
  a `visibility` that is not a table or was never `init`ed
  (`held_back == nil`); a `gamepad_mask` that is not an integer in 0–4095;
  a non-table `touches`; a record that is not a table or has a `nil` id
  (message names the index); two records with the same id; a `mouse` that
  is neither `nil` nor a table with a boolean `down`. `shown` refuses a
  never-`init`ed state the same way.

**Step 14's call order**, written in design.md as the contract (not wired
here): `gamepad = read_pad_buttons()`; `touches, mouse =
pad_visibility.filter(visibility, gamepad, raw_touches, raw_mouse)`;
`virtual = pad_touch.update(touch_state, touches, mouse, placed)`;
`pad_buttons = pad_touch.combine(gamepad, virtual)`.

**Docs.** design.md gets a "**Pad visibility.**" paragraph right after
"**Pad touch.**" (H-12-8): the module and its three functions, the six
rules, "pressed is an edge", "the press wins", "hidden passes nothing", the
call order above, and the attract consequence. `docs/plan-6.md`'s step note
is the coordinator's.

## Implementation Chunks

1. `love.pad_visibility` alone: `init`, `shown`, `filter` with every rule and
   refusal, tested with literal touch lists and masks (no `pad_touch`
   needed); then design.md.
2. The tests through `pad_touch` and `attract.frame` (they rest on H-12-2 to
   H-12-7).

## Testing Decisions

- **A good test** calls `init`, then `filter` frame after frame with literal
  touch lists, mouse records and gamepad masks, and checks `shown(...)`, the
  ids of the passed touches (in order) and whether the mouse passed, with
  exact `==` (`check.expect`). It never reads the module's private fields.
  The chunk-2 tests check the mask `pad_touch.combine(gamepad,
  pad_touch.update(...))` returns, and `attract.frame`'s action, never
  internals.
- **New test file** `tests/lua/game/test_pad_visibility.lua` in the one
  `lua-test-game` process, ending with `return check.run({...})` (prior art:
  `test_pad_stick.lua`, `test_pad_layout.lua`'s placed-layout helper,
  `test_window_layout.lua`'s `pcall` refusal pattern).
- **Cases, module alone** (each a test, its name saying the behaviour):
  - Hidden after `init`; `filter` with no touch, mouse `nil`, mask 0 →
    still hidden, nothing passed.
  - **Reveal**: touch 1 lands → shown, nothing passed; held and moved for
    several frames → nothing passed; lifted → nothing; touch 1 again (id
    reused) → passed.
  - **Second finger works**: touch 1 reveals, touch 2 lands while 1 is held
    → `{2}` passed; then `{1, 2}` in that list order still passes `{2}`.
  - **Two revealing fingers**: touches 1 and 2 land together on a hidden pad
    → shown, nothing passed until each lifts.
  - **Mouse**: `down = false` on a hidden pad → hidden, mouse not passed;
    `down = true` → shown, mouse not passed while down; up then down again
    → passed.
  - **Stays shown**: 100 empty frames after a reveal → still shown.
  - **Hide by a press**: shown, touches 3 and 4 passing, mouse down; mask
    16 → hidden, nothing passed; touches 3, 4 and the mouse still held on
    later frames (mask 16 held, then 0) → still hidden, nothing passed; a
    new touch 5 → shown, not passed; touch 6 → passed.
  - **Edge, not level**: shown, mask 16 held since before the reveal (press
    at frame 1, reveal at frame 2 while still 16) → stays shown on later
    frames with 16; mask 16 + 32 (32 new) → hidden.
  - **Every bit counts**: for each of the 12 bits alone, a press hides a
    shown pad (one loop, the bit in the message).
  - **The press wins**: hidden, a new touch and a press on the same frame →
    hidden, the touch held back; the next frame, that touch still held →
    still hidden; a new touch → shown.
  - **Press while hidden**: hidden, no touch, press → hidden; a new touch
    next frame → shown.
  - **Inputs untouched**: the passed array is not the input array; the
    input array and its records are unchanged (same length, same ids); the
    passed records are the same tables.
  - **Refusals**: masks `-1`, 4096, 1.5, `"16"`; `touches = nil`; a record
    without id; two records with id 7; `mouse = {x = 1, y = 2, down = 1}`; a
    state never `init`ed (for `filter` and `shown`): each `pcall` fails with
    a message naming the field.
- **Cases through `pad_touch`** (positions from
  `pad_layout.place(window_layout.compute(390, 844).pad)`, named by what
  they hit; masks as literals):
  - **The revealing touch presses nothing**: landing on A's centre → mask
    0; slid to B's centre → 0; a second finger on Start → 1024; the first
    lifted and a new touch on A → 1024 + 16.
  - **The revealing touch never steers**: landing at (100, 650), then slid
    60 right → 0 and `touch_state.stick == nil`.
  - **A press releases a held button**: shown, finger on A → 16; gamepad 1
    (dpup) pressed, finger still on A → combined mask 1; gamepad released,
    finger still on A → 0.
  - **A press releases the stick**: shown, stick claimed at (100, 650) and
    pushed 60 right → 8; gamepad 16 pressed → combined 16 and
    `touch_state.stick == nil`; the finger lifts, a new finger at (100,
    650) → reveals, 0, no stick; a third finger at (100, 650) after the
    second lifts → claims the stick.
- **Case through `attract`**: `attract.init(a, 1)`; frames on the title
  with combined mask 0 until `ACTION_START`; then during `PHASE_PLAYING`: a
  revealing touch on A → combined 0 → `ACTION_NONE`; that finger lifts, a
  new touch on A → 16 → `ACTION_RETURN`. And from a fresh demo, a gamepad
  press → `ACTION_RETURN` and the pad hidden.
- **Not tested here**: reading `love.touch` / `love.mouse` / the joystick
  and the calls in `love.update` (step 14), drawing (step 16), the touch e2e
  replays (step 15), touch-to-mask rules (step 12's tests).
- **Timing** (plan §2, steps 10–13): only a `tests/lua/game/` file is
  added. Record `lua-test-game`'s own time before (H-12-1's commit) and
  after: median of 11 runs of the runner command itself (the Makefile's
  `LUA_PATH=… luajit tests/lua/game/runner.lua …` line), not through
  `make`. Requiring `game.attract` loads `game.demo_data` and
  `game.save_state`'s graph: expected growth a few ms; the full `make test`
  gate runs only if it grew by more than ~10 ms.
- **Check of the step**: the new tests pass; `make test-all` green; `make
  e2e` and `make end-state` 20/20 with every `*.sha256` and `*.end-state`
  unchanged; `make lua-names` green; design.md states the show / hide rules;
  the two timing numbers in the report. No `make e2e` timing: nothing the
  game or the fake does per frame changes.

## Out of Scope

- Any game change: `main.lua` requires neither `love.pad_visibility` nor
  `love.pad_touch`; reading the touches, the mouse and the joystick and
  calling `filter`, `update` and `combine` in `love.update` are step 14's.
- Drawing the pad or choosing not to draw it (step 16; it reads `shown`).
- Any change to `love.pad_touch`, `love.pad_stick`, `love.pad_layout`,
  `love.window_layout`, `game.attract`, `game.input_step` or the bindings.
- Fading, a timeout, a "hide" button on the pad, a setting to keep it shown
  or hidden (plan §2 Out of scope).
- Touch or mouse callbacks; right or middle mouse buttons.
- Writing plan §6's step note (the coordinator does, from the report).

## Further Notes

- On iOS, LÖVE may also report a finger as the mouse (step 12's Further
  Notes). Here the doubled finger and its mouse land on the same frame, so
  both are revealing and both are held back: still nothing pressed. A
  controller press drops both. No extra rule.
- The decision that held-back touches look lifted to `pad_touch` (rather
  than calling `pad_touch.init`) keeps step 12's module untouched and makes
  the drop exact: `init` would forget every touch, so a finger still down
  would be new on the next `update` and could claim the stick (step 12's
  PRD pins that contract).
