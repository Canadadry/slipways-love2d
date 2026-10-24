---
title: "Touch to mask: the virtual pad's touches become pad_buttons bits"
description: "Nothing turns the fingers (and the desktop mouse) on the virtual pad into the 12 bits of pad_buttons: no stick ownership, no button hit-test, no OR with the real gamepad."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`, §2 Input) plays the game on a portrait iPhone
with a virtual pad. Step 10 placed the pad's elements on the screen
(`love.pad_layout`), step 11 turns one stick touch into a knob and four
direction bits (`love.pad_stick`). What is still missing is the rule that
decides, frame after frame, what each finger presses:

- which touch is the stick's, and that it stays the stick's until it lifts,
  even after it leaves the stick zone;
- that a touch starting on L or Back (both inside the stick zone) is that
  button's, never the stick's;
- that the 8 other buttons are tested every frame against each touch's
  *current* position, so a finger sliding from A to B releases A and
  presses B;
- that several fingers press several buttons, so the developer cheat chord
  (buttons 5 + 6, A + B, fixed in `input_step`) stays reachable;
- that on desktop the mouse, left button down, is one more touch;
- that the virtual mask is OR-ed with the real gamepad's.

Without one tested module holding these rules, step 14 (wiring into
`main.lua`) would have to write them inline, where no headless test can
reach them, and the touch e2e replays of step 15 would be the first place a
wrong ownership rule shows.

## Solution

A new pure-Lua module, `love.pad_touch`, beside `love.pad_layout` and
`love.pad_stick`:

1. **`init(touch_state)`** empties a caller-held state table: no stick, no
   touch seen.
2. **`update(touch_state, touches, mouse, placed)`**, called once per
   frame, takes the touches held this frame as plain data (`{id, x, y}`
   records, LÖVE window units), the mouse (`nil`, or `{x, y, down}`), and
   the placed pad of `pad_layout.place`, and returns the virtual mask:
   the stick's direction bits OR the bits of every button under a
   non-stick touch's current position.
3. **`combine(gamepad_mask, virtual_mask)`** returns the OR of the two
   12-bit masks, the one call step 14 makes before `input_step.run`.
4. **`BUTTON_MASKS`**: the bit of each of the 8 buttons, keyed by the LÖVE
   gamepad names of `pad_layout.BUTTONS` (`a` = 16 … `back` = 2048).
5. **Lua tests** in `tests/lua/game/` (the one `lua-test-game` process):
   slide A → B, two fingers (the cheat chord, stick + button), the stick
   touch leaving its zone, a touch starting on L or Back, a second finger
   in the stick zone, the mouse as a touch, the OR with the gamepad.
6. design.md gains a "**Pad touch.**" paragraph after "**Pad stick.**".

Nothing calls the module yet (step 14 does); the game and the 20 replays do
not change.

## User Stories

1. As a player on a portrait iPhone, I want the first finger that lands on the free part of the stick zone to become the stick, so that I steer from wherever my thumb falls.
2. As a player, I want my stick finger to keep steering after it slides out of the stick zone, so that a wide thumb movement never drops the direction.
3. As a player, I want my stick finger to keep steering even when it slides over A or another button, so that steering never presses a button by accident.
4. As a player, I want lifting the stick finger to release every direction at once, so that the cursor stops when I let go.
5. As a player, I want a new finger in the stick zone after lifting to start a new stick where it lands, so that I can re-centre the stick by lifting and touching again.
6. As a player, I want a finger that lands on L or Back to press that button, never the stick, so that the two buttons inside the stick zone work.
7. As a player, I want a finger that started on Back and slid off it to press nothing and never turn into the stick, so that a slip off a button does not suddenly steer.
8. As a player, I want sliding a finger from A to B to release A and press B, so that I can roll between face buttons without lifting.
9. As a player, I want a finger between two buttons to press neither, so that the gaps of the diamond are dead space.
10. As a player, I want a finger exactly on a button's right or bottom edge to press nothing from that button, so that the rule matches the half-open rectangles of the layout.
11. As a player, I want a finger that lands outside every button and slides onto one to press it, so that buttons answer the finger's current position, not where it started.
12. As a player, I want two fingers on A and B to press both, so that the developer cheat chord stays reachable on the phone.
13. As a player, I want to steer with one thumb and press A with the other at the same time, so that I can aim and confirm as with a gamepad.
14. As a player, I want a second finger landing in the stick zone while the stick is held to press nothing (unless it lands on L or Back), so that one stick never jumps to another finger.
15. As a player, I want a finger that stays in the stick zone while another finger holds the stick never to become the stick later, so that ownership is decided once, when a finger lands.
16. As a player on desktop, I want a left click held on a button to press it, so that I can try the virtual pad with a mouse.
17. As a player on desktop, I want a left click held in the stick zone to drive the stick, so that the mouse plays like a finger.
18. As a player on desktop, I want releasing the mouse button to lift that "touch", so that nothing stays pressed.
19. As a player on desktop, I want the mouse position to be ignored while its button is up, so that hovering presses nothing.
20. As a player with a touch screen and a mouse, I want the mouse and the fingers to count together, so that neither input hides the other.
21. As a player with a controller and the pad shown, I want the controller's buttons to keep working while I touch the pad, so that both inputs add up.
22. As a player, I want a button pressed on both the controller and the pad to count once, so that the game sees an ordinary press.
23. As a player, I want the stick to keep its base where it started when the window resizes mid-touch, so that a rotation or a resize does not jerk the direction.
24. As a developer of step 13 (show / hide), I want the touches as plain data in a list I can filter before `update`, so that the revealing touch can be held back without the module knowing about visibility.
25. As a developer of step 13, I want `init` to drop every held touch and the stick, so that a gamepad button can release everything the pad held.
26. As a developer of step 14 (wiring), I want `update` to take plain records, so that I build them from `love.touch.getTouches` / `getPosition` and `love.mouse.getPosition` / `isDown` in a few lines.
27. As a developer of step 14, I want `combine` to OR the gamepad's mask with the virtual one, so that `love.update` passes one mask to `input_step.run`.
28. As a developer of step 14, I want the placed pad passed on every call, so that a resize is followed on the next frame with no cached layout.
29. As a developer of step 16 (drawing), I want the stick of the state table readable (base and knob), so that I draw it without recomputing anything.
30. As a developer of step 16, I want the returned mask and `BUTTON_MASKS` to tell which buttons to draw pressed, so that the pressed sprites follow exactly what the game sees.
31. As a developer, I want the button bits to come from `p8.input`'s `PAD_BUTTON_5` … `PAD_BUTTON_12`, so that the virtual pad and `main.lua`'s `PAD_BUTTON_NAMES` cannot disagree silently.
32. As a developer, I want the module free of `love.*` calls and of module state, so that it runs under plain LuaJIT in `lua-test-game` and `lua-test-modules`.
33. As a developer, I want malformed input (a touch with no id, a non-number coordinate, two touches with the same id, a mouse with a non-boolean `down`, a mask outside 0–4095) refused with an `assert` naming the field, so that a wiring mistake fails at the call.
34. As a developer, I want the exports to follow CLAUDE.md naming (no `pad` prefix, verb names for functions), so that `make lua-names` stays green.
35. As a maintainer, I want the ownership rules written once in design.md, so that a later change is a decision, not a drift.
36. As a reviewer, I want every test to use positions computed from the placed layout of a real window size, so that a layout change cannot make the tests pass by accident.
37. As a reviewer, I want the 20 replays' `*.sha256` and `*.end-state` unchanged, so that the step is proven to change no game behaviour.
38. As a reviewer, I want `lua-test-game`'s own time recorded before and after, so that plan §2's timing rule for steps 10–13 is followed.

## Implementation Decisions

**Hypotheses on step 11 (not yet done; its PRD is
`docs/prd/triage/phase3-step11-floating-stick.md`).** The implementer checks
each first; one that fails is reported, and the decision resting on it is
revisited (step 11's real interface wins over this PRD's wording).

- **H-11-1**: step 11 is committed and `make test-all` is green on it; its
  last commit is this step's "before" commit for the `lua-test-game`
  timing.
- **H-11-2**: `love.pad_stick` exists in `game/love/`, is pure (no
  `love.*` call, no module state), and loads under plain LuaJIT.
- **H-11-3**: `pad_stick.start(x, y, base_size)` returns a new stick table
  with numeric fields `base_x`, `base_y`, `radius`, `knob_x`, `knob_y` and
  `mask`, its `mask` 0; `base_size` is the placed diameter
  `stick_base_size` of `pad_layout.place`.
- **H-11-4**: `pad_stick.follow(stick, x, y)` updates `knob_x`, `knob_y`
  and `mask` in place for a touch anywhere on the screen (no clamp to the
  stick zone or the window) and returns nothing; the base never moves.
- **H-11-5**: `pad_stick.UP`, `DOWN`, `LEFT`, `RIGHT` are 1, 2, 4, 8,
  equal to `p8.input.PAD_BUTTON_1` … `PAD_BUTTON_4`; a stick's `mask` only
  ever holds those bits.
- **H-11-6**: `follow` at the start point gives `mask == 0` (the dead zone
  holds the base centre), and an offset of at least half the base radius
  straight along an axis gives that axis's bit alone (the tests here use
  offsets far from every sector and dead-zone edge, so they do not depend
  on step 11's tie rules).
- **H-11-7**: step 11 adds design.md's "**Pad stick.**" paragraph right
  after "**Pad layout.**"; this step's paragraph goes right after it (if
  absent, right after "**Pad layout.**").
- **H-11-8**: step 11 adds only `tests/lua/game/test_pad_stick.lua` to the
  runner and changes neither `runner.lua`, `check.lua`, `lua-names`' prefix
  rule, `love.pad_layout` nor `love.window_layout`.

**Facts from the code (step 10, committed).** `pad_layout.place(pad)`
returns `{stick_zone, stick_base_size, stick_knob_size, buttons}` in LÖVE
units, never rounded; rectangles `{x, y, width, height}` are half-open
(`x <= px < x + width`, same for y); `buttons` is keyed by `a`, `b`, `x`,
`y`, `leftshoulder`, `rightshoulder`, `start`, `back`, the rectangles never
overlap; L (`leftshoulder`) and Back lie inside the stick zone. There is no
hit-test function in `pad_layout`.

**The module, `love.pad_touch`** (in `game/love/`, beside
`love.pad_layout`, `love.pad_sheet` and `love.pad_stick`: it is the
LÖVE-side virtual pad, not the cart). Pure: it requires `p8.input`,
`love.pad_stick` and `bit`; no `love.*` call; no module state — everything
lives in the state table the caller holds (the `input_step.init(step)` /
`cheat.init(cheat)` pattern). Works in LÖVE window units, y down.

**Exports** (CLAUDE.md naming; none is `pad` or starts with `pad_`):

- `BUTTON_MASKS`: `a = PAD_BUTTON_5` (16), `b = PAD_BUTTON_6` (32),
  `x = PAD_BUTTON_7` (64), `y = PAD_BUTTON_8` (128),
  `leftshoulder = PAD_BUTTON_9` (256), `rightshoulder = PAD_BUTTON_10`
  (512), `start = PAD_BUTTON_11` (1024), `back = PAD_BUTTON_12` (2048),
  from `p8.input`: the order of `main.lua`'s `PAD_BUTTON_NAMES` (bit
  `n − 1` for button `n`). `main.lua` keeps its own list in this step;
  step 14 may assert the two agree.
- `init(touch_state)`: sets `touch_state.stick = nil`,
  `touch_state.stick_id = nil`, `touch_state.held = {}` (the set of ids
  present on the previous `update`). Returns nothing. Calling it again
  drops the stick and forgets every touch (step 13's "drop the held
  touches"); a touch still down afterwards is new on the next `update`, so
  step 13 must hold it back itself (Out of Scope).
- `update(touch_state, touches, mouse, placed)`: returns the virtual mask
  (an integer 0–4095) and updates the state.
  - `touches`: an array of records `{id = <any non-nil value>, x =
    <number>, y = <number>}` (LÖVE's ids are light userdata; tests use
    numbers). The array may be empty. Two records with the same id are
    refused.
  - `mouse`: `nil` (no mouse, e.g. a phone) or `{x = <number>, y =
    <number>, down = <boolean>}`. When `down` is true, the mouse is one
    more touch with a private id (a sentinel table local to the module, so
    it can collide with no touch id), at `(x, y)`; when false, it is
    absent (lifted), its position ignored.
  - `placed`: the table `pad_layout.place` returns for this frame's window.
- `combine(gamepad_mask, virtual_mask)`: `bit.bor` of the two; each must be
  an integer `0 <= m <= 4095` (else an `assert` naming the argument).
- `touch_state.stick` is the `pad_stick` table of the stick touch (or
  `nil`), readable by step 16; `stick_id` its owner; `held` is private.

**The rules of one `update`** (in this order):

1. Build the frame's touch list: `touches`, then the mouse if `down`.
2. **Lift.** If `stick_id` is not among this frame's ids, the stick is
   dropped (`stick = nil`, `stick_id = nil`).
3. **Follow.** If a stick remains, `pad_stick.follow(stick, x, y)` at its
   touch's current position, wherever it is (inside the zone, outside it,
   over a button, outside the pad).
4. **Claim.** If no stick remains, the first touch in list order that is
   *new* (its id not in `held`), whose position lies inside
   `placed.stick_zone` and inside no button rectangle, becomes the stick:
   `stick = pad_stick.start(x, y, placed.stick_base_size)`, then
   `follow` at the same point (mask 0 by H-11-6). Only new touches claim:
   a touch that landed on Back, or landed in the zone while another finger
   held the stick, never becomes the stick later. A stick lifted and a new
   touch landing on the same frame: the new touch claims it.
5. **Buttons.** Every touch except the stick's is tested against every
   button rectangle at its *current* position, half-open; each hit ORs that
   button's `BUTTON_MASKS` bit. A touch that started on a button and slid
   off presses nothing; a touch that landed anywhere (outside the pad
   included) and slid onto a button presses it.
6. `held` becomes the set of this frame's ids.
7. Return `stick.mask` (0 without a stick) OR the button bits.

Decisions behind them:

- **The stick's touch never presses a button**, even sliding over one:
  plan §2 says it "stays the stick's touch until it lifts".
- **One stick at a time.** A second new touch in the zone while the stick
  is held is an ordinary button touch (it presses L or Back if on them,
  else nothing).
- **Ownership is decided when a touch lands**, never later, which is why
  the state remembers `held`. "Lands" means "first seen by `update`"
  (polling: plan §2 rules out callbacks).
- **A resize mid-stick keeps the stick** where it started, in window units,
  with its start-time radius: the stick follows its finger, not the layout;
  the next stick uses the new layout. Buttons use the `placed` of the
  current call, so they follow a resize at once.
- **The mouse and the fingers add up** (no "mouse only when no touch"):
  the plan counts the mouse as "one more touch". On a touch screen whose
  OS also synthesises a mouse from touches, a finger could count twice;
  it only ORs the same bit twice or, at worst, claims nothing more (the
  stick is single), so no extra rule (Further Notes).
- **Refusals** (`assert`, message starting with `pad_touch.update:` or
  `pad_touch.combine:` and naming the field, the style of
  `window_layout.compute`'s `check_size`): a non-table `touches`; a record
  with a `nil` id or a non-number `x` or `y` (message names the index);
  a duplicate id; a `mouse` that is neither `nil` nor a table with numeric
  `x`, `y` and boolean `down`; a `placed` without `stick_zone`, `buttons`
  or a number `stick_base_size`; a mask outside 0–4095 or non-integer in
  `combine`. `update` with a non-table `touch_state` or one never `init`ed
  (`held == nil`) is refused too.
- The hit-test is a local of the module (point in half-open rectangle);
  `pad_layout` is not changed.

**Docs.** design.md gets a "**Pad touch.**" paragraph right after
"**Pad stick.**" (H-11-7): the module and its three functions, the record
shapes, the claim / follow / lift rules, "a touch that starts on a button
is never the stick's", "buttons answer the current position", the mouse as
a touch while its left button is down, `BUTTON_MASKS` from `p8.input`, and
`combine`. `docs/plan-6.md`'s step note is the coordinator's.

## Implementation Chunks

1. `BUTTON_MASKS`, `init`, `combine`, and `update` for buttons only
   (non-stick touches, mouse included, refusals): slide A → B, two
   fingers, half-open edges, mouse tests.
2. The stick in `update`: claim, follow, lift, the "button before stick
   zone" rule and "only new touches claim"; then design.md.

## Testing Decisions

- **A good test** calls `init`, then `update` frame after frame with
  literal touch lists, and checks the returned mask (and, for the stick,
  `touch_state.stick ~= nil` and its `base_x`/`base_y`) with exact `==`
  (`check.expect`). It never reads the module's source or the `held`
  field. Masks are written as literals (16, 32, 48, 2048, 8 + 16 …) and
  `BUTTON_MASKS` is checked against those literals once, so a wrong export
  fails.
- **New test file** `tests/lua/game/test_pad_touch.lua` in the one
  `lua-test-game` process, ending with `return check.run({...})` (prior
  art: `test_pad_layout.lua`'s `place_for_window`, `test_window_layout.lua`'s
  `expect_refused` pcall pattern).
- **Positions** come from `pad_layout.place(window_layout.compute(390,
  844).pad)` and are named by what they hit; a helper returns a button's
  centre. At 390×844 (checked in LuaJIT while writing this PRD): stick zone
  x 0–195, y 486.75–779.25, base size 117; centres A (292.5, 720.75),
  B (351, 662.25), X (234, 662.25), Y (292.5, 603.75), L (39, 525.75),
  R (351, 525.75), Back (160.875, 745.125), Start (229.125, 745.125); A
  spans x 263.25–321.75, y 691.5–750; Back x 131.625–190.125, y
  715.875–774.375. A free stick-zone point: (100, 650).
- **Cases** (each a test, its name saying the behaviour):
  - `BUTTON_MASKS` equals 16, 32, 64, 128, 256, 512, 1024, 2048 for `a`,
    `b`, `x`, `y`, `leftshoulder`, `rightshoulder`, `start`, `back`, and
    has exactly the 8 keys of `pad_layout.BUTTONS`.
  - No touch, mouse `nil` → 0.
  - **Slide A → B**: one id at A's centre → 16; same id in the gap (331.75,
    700) → 0; same id at B's centre → 32; lifted → 0.
  - **Half-open**: a touch at (321.75, 720.75) (A's right edge) → 0; at
    (263.25, 691.5) (A's top-left corner) → 16.
  - **Two fingers**: A and B → 48 (the cheat chord); each finger lifted in
    turn → 32, then 0. X + Y + Start → 64 + 128 + 1024.
  - **Lands outside, slides on**: a touch landing at (300, 400) (the game
    half, outside the pad) → 0, then moved onto A → 16.
  - **Stick claims and steers**: a touch at (100, 650) → 0 and a stick with
    base (100, 650); moved to (160, 650) (60 right) → 8.
  - **Stick touch leaving its zone**: the same stick moved to (380, 650)
    (outside the zone, x > 195) → 8; then onto A's centre (292.5, 720.75)
    → 8 (the stick's touch never presses A; offset (192.5, 70.75), well
    inside the right sector). Lifted → 0, `stick == nil`.
  - **Stick + button**: stick pushed right and a second finger on A → 24.
  - **L and Back belong to their buttons**: a touch landing on L's centre →
    256, no stick; one landing on Back's centre → 2048, no stick.
  - **Off Back never steers**: the Back touch slid to (100, 650) → 0 and
    still no stick; slid on to (160, 650) → 0. A new touch at (100, 650)
    now claims the stick.
  - **One stick**: stick held at (100, 650); a second new touch at (60,
    600) in the zone → 0 and the stick's base unchanged; slid to Back's
    centre → 2048; after the stick lifts, that second touch, held, still
    does not claim (its id is not new) — the mask stays what its buttons
    give.
  - **Re-centre**: stick lifted, a new touch at (50, 700) → new base
    (50, 700).
  - **Lift and land on one frame**: stick id 1 absent and id 2 new in the
    zone in the same `update` → id 2 is the stick.
  - **Mouse**: `{x = A.x, y = A.y, down = true}` → 16; `down = false` at
    the same place → 0; down in the free zone → stick claimed; moved 60
    right → 8; up → 0, `stick == nil`; down on A with a finger on B → 48.
  - **Resize mid-stick**: stick claimed with the 390×844 placement, next
    `update` with the 640×480 placement and the finger 60 units right of
    the base → still 8, base unchanged (the module does not check that a
    position lies in the window); a second finger on 640×480's A centre
    (480, 384) → 8 + 16 (buttons follow the new placement).
  - **`init` drops everything**: stick and a button held, `init`, then the
    same touches: the stick finger, now new and in the zone, claims a new
    stick at its position; the button finger presses its button (this pins
    the contract step 13 must work around).
  - **combine**: `combine(0, 0) == 0`, `combine(16, 16) == 16`,
    `combine(1, 2) == 3`, `combine(4095, 0) == 4095`; refused: `-1`, 4096,
    1.5, `"16"`, naming the argument.
  - **Refusals of `update`**: a record without id, with a string `x`, two
    records with id 7, `mouse = {x = 1, y = 2, down = 1}`, a `placed` of
    `{}`, a state never `init`ed: each `pcall` fails with a message naming
    the field.
- **Not tested here**: show / hide (step 13), `love.*` reading and the call
  in `love.update` (step 14), drawing (step 16), sector and dead-zone edges
  (step 11's tests).
- **Timing** (plan §2, steps 10–13): only a `tests/lua/game/` file is
  added. Record `lua-test-game`'s own time before (H-11-1's commit) and
  after: median of 11 runs of the runner command itself (the Makefile's
  `LUA_PATH=… luajit tests/lua/game/runner.lua …` line), not through
  `make`. Expected growth ~1 ms; the full `make test` gate runs only if it
  grew by more than ~10 ms. No `make e2e` timing: nothing the game or the
  fake does per frame changes.
- **Check of the step**: the new tests pass; `make test-all` green; `make
  e2e` and `make end-state` 20/20 with every `*.sha256` and `*.end-state`
  unchanged; `make lua-names` green; design.md states the touch rules; the
  two timing numbers in the report.

## Out of Scope

- Any game change: `main.lua` does not require `love.pad_touch`; reading
  `love.touch` / `love.mouse` and calling `combine` in `love.update` are
  step 14's.
- Show / hide: the revealing touch pressing nothing, a gamepad button
  hiding the pad and dropping touches (step 13). This module offers the
  hooks (filter the list before `update`; `init` to drop) but knows nothing
  of visibility.
- The stick's geometry: knob cap, dead zone, sectors (step 11).
- Drawing the pad, pressed sprites (step 16).
- The touch e2e replays (step 15).
- Touch or mouse callbacks (`love.touchpressed` …): plan §2 rules them out.
- Right or middle mouse buttons; multiple sticks; analogue output.
- Any change to `pad_layout`, `window_layout`, `pad_nav`, `input_step`,
  `cheat` or the bindings.
- Writing plan §6's step note (the coordinator does, from the report).

## Further Notes

- On iOS, LÖVE may also report a finger as the mouse (SDL's touch-to-mouse
  events). If step 14 finds that `love.mouse.isDown(1)` follows the first
  finger on the phone, the doubled touch ORs the same button bit twice
  (harmless) or lands as a second new touch in the zone while the finger
  holds the stick (it presses nothing, by the "one stick" rule). Only a
  doubled finger landing on L or Back first could make the mouse claim
  nothing and the finger press the button — still the same bit. So no
  rule here; step 14 may pass `mouse = nil` when `love.touch.getTouches`
  is non-empty if the phone misbehaves (Open question for step 14, not a
  decision of this step).
- The cheat chord is A + B = 16 + 32 = 48, `cheat.chord_buttons()`'s value
  (`p8.input.PAD_BUTTON_5 | PAD_BUTTON_6`).
- The diamond's squares meet only at corners (step 10's test), so a single
  finger hits at most one button; the module still ORs every hit, which
  stays right if a later layout overlaps.
