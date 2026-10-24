---
title: "Draw the virtual pad on the screen, released and pressed"
description: "Since step 14 the virtual pad plays the game, but nothing is drawn: a phone player touches an empty half of the screen and cannot see a button, the stick or what they press."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) plays the game on a portrait iPhone in Love2D
Studio with a virtual pad. Steps 9 to 14 built everything but the picture:
the sheet `game/pad.png` and its quads (`love.pad_sheet`), the element
table and its screen rectangles (`love.pad_layout.place`), the floating
stick (`love.pad_stick`), touch → mask (`love.pad_touch`), the shown /
hidden rule (`love.pad_visibility`), and the per-frame pipeline
`love.virtual_pad`, wired into `main.lua` before `input_step.run`. A touch
on the right spot presses A; but the bottom half of a 390×844 phone stays
black, so the player does not know where A is, whether the pad is shown,
where the stick's base appeared or which button their finger is on. The
README still says "until the on-screen pad exists, the game needs a
gamepad".

Plan §2 (Look) fixes how it must look: every element is a sprite of
`game/pad.png`, cut by `love.pad_sheet`'s quads (stick base, stick knob,
and per button a released and a pressed sprite), each stretched into its
layout rectangle, nearest; drawn on the screen after the present, never on
canvas 0, so the capture hashes cannot see it. Step 16 must prove it
headless and exactly: screen tests with `pad_flat.png` — hidden, shown in
tall mode, shown in overlay mode, a button pressed, the stick pushed on a
diagonal, after a `resize` — each against an exact expected frame; the 20
replays and the touch replays unchanged; `make love-file` still passing.

## Solution

When the pad is shown, every frame, after the present, the game draws over
the screen:

- the eight buttons, each in its `pad_layout` rectangle, with its
  **pressed** sprite when a touch (or the held mouse) presses it this frame
  and its **released** sprite otherwise;
- the stick, **only while a finger holds it**: the base centred where that
  touch started, the knob centred where the touch is (capped at the base's
  edge by `pad_stick`), the knob over the base.

When the pad is hidden (at start, and after a gamepad press) nothing of the
pad is drawn. The game itself never moves: canvas 0 and its hashes are
untouched. In tall mode the pad fills the bottom half's rectangle; in
overlay mode it lies over the game's rectangle. A resize moves the drawn
pad on its own frame, exactly where the touches are tested.

For the maintainer, `test_game_screens` gains four screens of a shown pad,
drawn with the flat-colour sheet, and its two existing screens now also
prove a hidden pad draws nothing, all six compared pixel for pixel with
frames the test builds itself.

## User Stories

1. As a phone player, I want to see the eight buttons in the bottom half of the screen once I touch it, so that I know where to press.
2. As a phone player, I want a button I hold to change to its pressed sprite, so that I know my finger is on it.
3. As a phone player, I want a button to look released again as soon as my finger leaves it, so that sliding from A to B shows B pressed and A released.
4. As a phone player, I want two fingers on two buttons to show both pressed, so that a chord is visible.
5. As a phone player, I want the stick's base to appear where my thumb lands, so that the floating stick is visible where it is.
6. As a phone player, I want the knob to follow my thumb and stop on the base's edge, so that I see the direction I push.
7. As a phone player, I want the base and knob to disappear when I lift my thumb, so that the stick zone is clear when unused.
8. As a phone player, I want the revealing touch to show the pad with every button released, so that the screen agrees with the rule that it pressed nothing.
9. As a phone player, I want the pad never drawn over the game in portrait, so that the game stays fully visible.
10. As a player with a controller, I want the pad to vanish when I press a gamepad button, so that the screen shows only the game.
11. As a player with a controller, I want the pad not to appear before any touch, so that a gamepad-only session looks as before.
12. As a desktop player, I want a click to show the pad over the game, so that I can try it with the mouse.
13. As a desktop player, I want a held click on a button to draw it pressed, so that the mouse behaves like a finger.
14. As a desktop player, I want the pad to follow the window when I resize it, so that what I see is where I click.
15. As a player on any window, I want each sprite stretched into exactly the rectangle that receives the touches, so that the drawn button is the touch area.
16. As a player, I want sprites drawn nearest, so that the pixel art stays sharp at fractional scales.
17. As a player, I want the pad's translucent art to show the game through it in overlay mode, so that the pad hides as little as possible.
18. As an artist, I want to change the pad's look by redrawing `pad.png` alone, so that no drawing code changes with the art.
19. As a maintainer, I want the drawing never to touch canvas 0, so that the 20 replays and the touch replays keep their hashes.
20. As a maintainer, I want the pad drawn from the very rectangles the touches were tested against this frame, so that the picture and the hit test cannot disagree.
21. As a maintainer, I want the pressed state to come from the virtual pad's own mask, not from the combined mask, so that a held gamepad button never draws a virtual button pressed.
22. As a maintainer, I want the stick's d-pad bits never to press a drawn button, so that only the eight button sprites have a pressed state.
23. As a maintainer, I want a screen test of the hidden pad in both modes, so that a pad drawn too early is caught.
24. As a maintainer, I want a screen test of the shown pad in tall mode, so that the phone layout is proven pixel for pixel.
25. As a maintainer, I want a screen test of the shown pad in overlay mode, so that drawing over the game is proven.
26. As a maintainer, I want a screen test with one button pressed, so that the pressed and released sprites are proven to be chosen per button.
27. As a maintainer, I want a screen test of the stick pushed on a diagonal past the base's edge, so that the base, the knob, the cap and their order are proven.
28. As a maintainer, I want a screen test on the frame of a resize, so that a pad laid out from a stale size is caught.
29. As a maintainer, I want a fractional sprite edge on a pixel centre among the cases, so that the half-open rule is exercised by the pad too.
30. As a maintainer, I want the expected frames built by the test from its own copies of the layout and sheet tables, so that a bug in `love.pad_layout` or `love.pad_sheet` is not copied into the expectation.
31. As a maintainer, I want the screen cases inside the existing `test_game_screens` run, so that `make test` gains no process.
32. As a maintainer, I want `test_game_screens` to stay under 100 ms, so that design.md's per-check budget holds.
33. As a maintainer, I want `make test`'s timing gate passed and recorded, so that the loop stays as fast as before the plan.
34. As a maintainer, I want `make e2e` measured beside the "before" commit, so that the drawing's per-frame cost is known and under 9 s.
35. As a maintainer, I want `make love-file` to load the real `pad.png` from the archive, so that a phone build that lacks the sheet fails before the phone.
36. As a maintainer, I want design.md to describe the pad's drawing and the present's new place in `love.draw`, so that the contract says what the game does.
37. As a maintainer, I want the README to stop saying a gamepad is needed, so that a phone user is not misled.
38. As a reviewer, I want the virtual pad's mask readable and unit-tested, so that the pressed rule is tested without a screen.

## Implementation Decisions

**Hypotheses on step 15 (not yet done; its PRD is
`docs/prd/triage/phase3-step15-touch-replays.md`).** The implementer checks
each one first against the committed code; one that fails is reported and
the decision resting on it is revisited (step 15's real code wins over
this PRD's wording).

- **H-15-1**: step 15 is committed and `make test-all` is green on it; its
  last commit (the plan's §6 step-15 note included, if any) is this step's
  "before" commit for every timing.
- **H-15-2**: `tests/e2e/` holds the 20 original replays plus four
  `touch_<s>.json` replays and their four `pad_twin_<s>.json`, each with
  its `.txt`, `.json.sha256` and `.json.end-state`; each touch replay's
  `.json.sha256` and `.json.end-state` are byte copies of its twin's, and
  `tools/e2e_limits.sh`'s pairing rule fails `make e2e` when they differ.
  "The touch replays unchanged" therefore means: `make e2e` and
  `make end-state` report every replay matching (28 of 28 if the four
  pairs landed) with no file of `tests/e2e/` changed.
- **H-15-3**: the touch replays run at 390×844 (tall mode), reveal the pad
  and keep it shown for most of their frames, press buttons and push the
  stick, and carry no `screens` and no `pad_sheet` (design.md: "no replay
  of `tests/e2e/` carries `screens` or `pad_sheet`"). So in `make e2e` the
  pad's draws run on many frames — every check the fake makes on a screen
  draw runs, listed or not — but are never rasterised; the touch replays
  prove the drawing is accepted and leaves canvas 0 alone, not how it
  looks.
- **H-15-4**: step 15 changes neither `game/` (`main.lua`, the
  `game/love/` pad modules, `screen.lua`), nor the fake LÖVE and its
  replay format, nor `tests/test_game_screens.c`,
  `tests/fixtures/game_screens.json` or `tests/expected_screen.{c,h}`; the
  facts below still hold.
- **H-15-5**: step 15 measured `make e2e` and left the projection at most
  8 s (the 1 s margin for this step's drawing), recording median and
  per-frame cost in §6; if it flagged a slow session instead, this step is
  judged on its paired difference (Testing Decisions).
- **H-15-6**: the touch replays' total frame count is at most the
  720-frame cap of step 1.4 (the step-15 PRD plans 552); the drawing's
  e2e cost is paid on those frames only, since the 20 original replays
  never show the pad.

**Facts from the committed code.** `love.draw` runs `screen.frame_begin`,
`cart.draw`, `screen.frame_end`, `draw_meter.frame_end`, `screen.present`.
`love.virtual_pad.update` runs in `love.update` (before `love.draw`, same
frame) and keeps `placed` (this frame's `pad_layout.place` result) and
`touch_state.stick` (`{base_x, base_y, radius, knob_x, knob_y, mask}` or
`nil`); it returns only the combined mask. `love.pad_sheet.load()` returns
`{image, stick_base, stick_knob, buttons = {[name] = {released,
pressed}}}`, and nothing calls it yet, so the game does not load
`pad.png` today. `screen.init` sets the default filter to nearest before
any image is built. The fake's scaled draw `draw(image, quad, x, y, 0, sx,
sy)` is accepted onto the screen only, at fractional positions and any
positive scales, with any colour, translucent texels blended.

**A new module `love.pad_view` (`game/love/`)** draws the pad. Interface:

- `init(view)`: loads the sheet (`pad_sheet.load()`) into the table the
  caller holds (no module state, like `virtual_pad`). Called in
  `love.load` after `screen.init` (so the sheet is built under the nearest
  filter) — once, never per frame.
- `draw(view, virtual_pad)`: draws nothing unless `virtual_pad.shown` is
  true and `placed` is not `nil`. Otherwise it selects the screen
  (`setCanvas()`), sets white (`setColor(1, 1, 1)`: the sprites' own
  colours), then draws, in this order: the eight buttons in the sheet's
  order (a, b, x, y, leftshoulder, rightshoulder, start, back — a fixed
  list, never `pairs`), then the stick base, then the knob. A later draw
  lies over an earlier one, so a base that floats over L or Back hides
  them, and the knob is never under the base.
- An un-`init`ed view or a non-table `virtual_pad` fails an `assert`
  starting `pad_view.draw:`.

`pad_view` is the only new module; it reads `virtual_pad`'s readable
fields and never changes them.

**Each sprite is stretched into its layout rectangle.** A button's quad
goes to `placed.buttons[name]`: `draw(image, quad, rect.x, rect.y, 0,
rect.width / quad_width, rect.height / quad_height)`, the quad's size taken
from `love.pad_sheet`'s table (not hard-coded 24). The stick base goes to
the square of side `placed.stick_base_size` centred on (`base_x`,
`base_y`); the knob to the square of side `placed.stick_knob_size`
centred on (`knob_x`, `knob_y`). Nothing is rounded. Today every scale is
the pad's scale (one reference unit per texel), but the code divides
rectangle by quad so a resized sprite in a future `pad.png` still fills
its rectangle (§2 Look: "changing the look means redrawing `pad.png`, and
the quad table if sizes change, never the layout code").

**What is drawn pressed.** A button is pressed when its bit
(`pad_touch.BUTTON_MASKS[name]`) is set in **the virtual pad's own mask
of this frame** — the touches' and held mouse's buttons after
`pad_visibility.filter`, before `pad_touch.combine` with the gamepad. So:

- a revealing touch, held back, draws nothing pressed;
- a gamepad button held while the pad is shown (possible: a new touch
  shows the pad again while a gamepad button stays down) does not draw a
  virtual button pressed — the pad shows the fingers, not the controller;
- the stick's d-pad bits (1–4) have no button sprite and press nothing
  drawn.

`love.virtual_pad` gains one readable field for this, `virtual_mask`: the
mask `pad_touch.update` returned this frame, `0` after `init` and before
the first `update`. `update`'s result and every other behaviour are
unchanged. (Recomputing hits inside `pad_view` from `placed` and the
touches was rejected: a second hit test that could disagree with the
first.)

**The stick is drawn only while held** (`touch_state.stick ~= nil`):
there is no idle base sprite (§2: a *floating* stick; "the base appears
where the touch starts"). On the stick's first frame the knob is on the
base's centre (mask 0) and both are drawn. The base is not clamped to the
pad or the window: the fake clips off-window pixels.

**Where it lives in `love.draw`, and the present contract.** `main.lua`'s
`love.draw` becomes: `frame_begin`, `cart.draw`, `frame_end`,
`draw_meter.frame_end`, `screen.present()`, `pad_view.draw(view,
virtual_pad)`. design.md's rule "present canvas 0 once per frame, **last
in `love.draw`**" (The present, and the function-list paragraph that
repeats it) is resolved by rewording, not by moving the pad before the
present: the present must come **after every draw onto canvas 0**, and the
only draws that may follow it are the pad's sprites onto the screen
(`love.pad_view.draw`). The reason the rule existed — nothing may change
canvas 0 after the fake reads it — still holds, since the pad never
selects a canvas; and drawing the pad after the present is what §2 Layout
requires (the pad lies over the game in overlay mode). `screen.present`'s
comment ("call it once, last in love.draw") is updated to the same rule.
`main.lua` holds the view table as it holds `virtual_pad`
(`pad_view.init(view)` in `love.load`, after `virtual_pad` is set up).

**The pad is drawn from `placed`, not from a new layout of
`getDimensions()`:** the rectangles drawn are those the touches were
tested against in this frame's `update`. In the fake and in real LÖVE the
size does not change between `love.update` and `love.draw` of a frame, so
this equals the present's layout; on a resize frame both use the new
size (the fake runs `love.resize` before `update`).

**Cost decisions.** `pad_view.init` decodes `pad.png` (240×72) once per
boot, in every run of the game — e2e replays, `make love-file`, every
fake check that boots `game/`. Loading it lazily on the first shown frame
was rejected: it would move a file read into `love.draw` and leave
`make love-file` unable to prove the archive's `pad.png` loads. If the
boot cost shows in `make e2e` (Testing Decisions), lazy loading is the
first lever, decided then and recorded. Per shown frame the pad makes at
most 10 scaled draws plus 2 state calls, rasterised only on `screens`
frames.

**The screen test replay.** `tests/fixtures/game_screens.json` (one boot,
step 7's replay) gains `"pad_sheet": "pad_flat.png"` (the fixture sits in
the same directory) and grows from two frames to **eight**, with **six
`screens`** — within step 2.7's grouping (nine frames, six screens,
"adds no frames beyond these nine"; one frame fewer was enough). Window
coordinates, LÖVE units:

| frame | input | screen | case |
|---|---|---|---|
| 0 | start at 390×844, nothing | yes | hidden, tall (step 7's case) |
| 1 | `resize` 640×480 | yes | hidden, overlay, after a resize (step 7's case) |
| 2 | `resize` 200×420; touch 1 at (150, 360), on A | yes | shown in tall mode: the revealing touch, every button released, no stick |
| 3 | nothing (touch 1 lifted) | no | |
| 4 | touch 2 at (150, 360), on A | yes | a button pressed: A pressed, the seven others released |
| 5 | touch 3 at (50, 315): stick zone, on no button (touch 2 lifted) | no | the stick claimed, knob on the base |
| 6 | touch 3 at (80, 275) | yes | the stick pushed up-right, past the edge |
| 7 | `resize` 320×240, nothing (touch 3 lifted) | yes | shown in overlay mode, over the game, on the resize's own frame, no stick |

Values (checked in this session with LuaJIT over the committed
`window_layout`, `pad_layout` and `pad_stick`; the implementer re-checks
them the same way first):

- 200×420: tall; game at (0, 30), scale 1.25; pad at (0, 240), scale
  1.25. Buttons 30×30: L (5, 245), R (165, 245), Y (135, 285), X (105,
  315), B (165, 315), A (135, 345), **Back (67.5, 357.5), Start (102.5,
  357.5)** — fractional left and top edges on pixel centres, the
  half-open rule's case. Base 60, knob 30.
- Stick: start (50, 315), touch at (80, 275), offset (30, −40), length
  50 > radius 30: knob capped at exactly (68, 291) (shrink 0.6, both
  products exact in double), mask 9 = up + right (a diagonal: 9 < 3·16
  and 16 < 3·9). Base square (20, 285)–(80, 345), knob square
  (53, 276)–(83, 306): neither overlaps a button, so the case tests the
  stick alone; the knob over the base is tested by their overlap.
- 320×240: overlay; game and pad at (0, 0), scale 2. Buttons 48×48: L (8,
  8), R (264, 8), Y (216, 72), X (168, 120), B (264, 120), A (216, 168),
  Back (108, 188), Start (164, 188).

The touches press A and push the stick for the game too (the title takes
A, the map takes up + right): canvas 0 changes accordingly, and the
expected frames are built from the run's own capture, as step 7's are.
The replay is a fixture of `make test`, not an e2e replay: it carries
`screens` and `pad_sheet`, which design.md forbids only in `tests/e2e/`.

**`test_game_screens`** keeps its one run and its literal cases. Each
case gains: shown or hidden, the pad origin and scale (literal, from the
table above), the set of pressed buttons, and the stick (none, or literal
base and knob centres). The expected frame is built in this order: black;
the present from the capture (as today); then, if shown, the eight
buttons at `origin + reference × scale` from **the test's own copy of
design.md's pad layout table**, each with the sheet rectangle of its
released or pressed sprite from **the test's own copy of design.md's
sheet table**, then base, then knob — each through
`expected_screen_draw` with a texel function reading
`tests/fixtures/pad_flat.png` (loaded once with `png_file_load`), white
tint, scale = rectangle / sprite size. Never computed through the game's
modules, so a wrong table there is caught rather than copied. Each case
also asserts, in its expected frame, one landmark pixel per drawn sprite
(e.g. A's centre holds A's pressed or released flat colour) and, for the
hidden cases, one pixel of the pad area holding the game's or black's
colour — the guard that the expected builder did not leave the pad out
(prior art: `assert_not_all_colour_0`, `bar_rows`). The capture-size
assertion becomes eight frames. The Makefile rule of `test_game_screens`
gains `tests/fixtures/pad_flat.png` among its prerequisites.

**design.md.** In "Virtual pad": "The pad is not drawn" goes; `virtual_mask`
joins the readable fields. A new "**Pad drawing**" paragraph after it:
`love.pad_view`, `init` in `love.load`, `draw` after the present, what is
drawn when (hidden → nothing; buttons pressed by `virtual_mask`; stick only
while held; order buttons, base, knob), each sprite stretched into its
`placed` rectangle, the screen selected and white set by `draw` itself.
The present's rule is reworded in both places (above). The game now loads
`pad.png` at boot: "The pad sheets" says so. The intro sentence saying
nothing in these plans targets iOS is updated (plan §4 asks for it gone;
the pad is now playable on a phone). No fake contract changes: every call
the pad makes is already in the function list (steps 1.4–1.5), so
`tools/lua_guard.lua` and the fake are untouched. If the implementer finds
otherwise, that is a deviation to report, not a silent extension.

**README.** The sentence "Until the on-screen pad exists, the game needs a
gamepad" becomes: with no controller, touch (or click) the screen to show
the virtual pad; a gamepad press hides it.

## Implementation Chunks

1. **`virtual_pad.virtual_mask`**, with its tests in
   `tests/lua/game/test_virtual_pad.lua` (red first), and its design.md
   line.
2. **The screen test, red**: `game_screens.json`'s eight frames and
   `pad_sheet`, `test_game_screens`'s extended cases and expected frames;
   it fails on frames 2, 4, 6 and 7 (no pad drawn) and still passes 0 and
   1.
3. **`love.pad_view` and `main.lua`'s wiring**, until the screen test is
   green; then design.md (Pad drawing, the present's rule, the sheets, the
   intro line), the README, `screen.present`'s comment; then `make e2e`,
   `make end-state`, `make love-file` and the timings.

## Testing Decisions

- **A good test here compares what reaches the screen**, whole frame,
  every channel, no tolerance (plan §1), with an expectation built
  independently of the game's modules. It never inspects `pad_view`'s
  calls. The decisions that a screen cannot see cheaply are tested in Lua
  through `virtual_pad`'s public interface.
- **Screen tests** (`test_game_screens`, the six cases above). Prior art:
  its own two cases, `tests/expected_screen.{c,h}`, and
  `test_fake_love_image_draw`'s static sheet test (sprites drawn from
  `pad_flat.png` and compared exactly). Written first, red, before
  `pad_view` exists (plan §2 TDD).
- **No Lua test of `pad_view`**: it is a thin drawer over `love.graphics`;
  stubbing `love` in the runner would test the calls, not the picture, and
  the screen test covers each of its branches (hidden, released, pressed,
  stick, overlay, resize). A mutation check by the implementer: drawing
  the pressed sprite for every button, drawing the stick when not held,
  drawing while hidden, swapping base and knob order, and laying out from
  a stale size each turn a screen case red.
- **Lua tests of `virtual_mask`** in `test_virtual_pad.lua` (the runner,
  no new process): `0` after `init`; `0` on the revealing touch; A's bit
  for a touch on A; the gamepad's bits never in it (gamepad mask held
  while a shown pad's touch presses B → `virtual_mask` is B alone, the
  result is both); the stick's bits in it while pushed (they are the
  touches' mask, even though no sprite shows them); `0` again after a
  gamepad press hides the pad. Prior art: the file's own tests.
- **The replays**: `make e2e` and `make end-state` all matching with no
  file of `tests/e2e/` changed (H-15-2): the 20 C-set hashes prove the
  hidden pad draws nothing that reaches canvas 0, the touch replays that a
  shown pad does not either (H-15-3).
- **`make love-file`** passes: it plays `pad_title.json` from the
  unpacked archive, which now loads the archive's `pad.png` at boot.
- **Timing, `make test`** (step 16 is in §2's gate list): the new work is
  inside `test_game_screens` (six more frames, four more screens, the
  pad's draws, the flat sheet read). Record its own time before and after
  (median of 11 runs of the binary); budget 100 ms (design.md rule 2;
  step 2.7 projected ~90 ms before the pad's draws). Then design.md's full
  gate (rule 4), 11 rounds, against the "before" commit (H-15-1), 8dff812
  and the plan's start 8c830c6 (≤ 3 % each); record the plain and paired
  medians, the rounds, the core count and the check's own time in the
  report. If the check exceeds 100 ms: first shrink the overlay window to
  160×120 (scale 1), then the tall one to 160×322 (scale 1, pad at
  y 181.5, still fractional), re-deriving the literals; splitting the
  check into a second process is the last resort.
- **Timing, `make e2e`** (step 16 is in §2's e2e list): median of 5 runs
  here and 5 at the "before" commit, interleaved, same session; per-frame
  cost = (user + sys) / frames / 4 cores. Pass when the median ≤ 9 s with
  the touch replays committed (H-15-5; no further budget is reserved
  after this step). If the "before" median already exceeds the limit in
  this session, judge on the paired difference (median of per-run
  differences ≤ 2 %) and record both numbers. Expected: the boot's
  `pad.png` decode on each replay and ≤ 12 calls per shown touch-replay
  frame, a few tens of ms in all.
- **Check of the step**: `make test-all` green; the six screen cases
  exact; `make e2e`, `make end-state` and `make love-file` green;
  `git diff --stat tests/e2e` empty; design.md states the pad drawing and
  the reworded present rule.

## Out of Scope

- Any change to the fake LÖVE, its function list, its replay format or
  `tools/lua_guard.lua`.
- New art: `game/pad.png`, `pad_flat.png`, `tools/pad_art.c` and the
  static sheet test are unchanged; an idle stick-zone sprite.
- Any change to the input rules (`pad_touch`, `pad_stick`,
  `pad_visibility`, `window_layout`, `pad_layout`) or to `virtual_pad`'s
  result.
- Drawing a pressed state for gamepad buttons, or d-pad sprites for the
  stick's direction.
- Fading the pad, a light effect on press, haptics (plan §2 Out of scope).
- `screens` or `pad_sheet` in `tests/e2e/` replays; new e2e replays.
- The `highdpi` canvas risk noted at step 2.7 (`dpiscale`), and testing
  in real LÖVE or on the phone.
- The plan's §6 step note (the coordinator's); `CLAUDE.md`.

## Further Notes

- Step 2.7 planned nine frames; eight suffice for the six cases. The
  missing ninth could have shown a pressed button in overlay mode; the
  pressed/released choice is mode-independent (`placed` is the only
  input that changes with the mode) and the overlay case already proves
  the pad over the game, so the frame was not worth its cost.
- On the phone the pad's art is translucent (step 3.9), so in tall mode
  it blends onto black and in overlay mode onto the game; the flat sheet
  is opaque, so the screen tests prove placement and sprite choice, and
  the blend formula stays proven by the fake's own tests (step 1.5) and
  the static sheet test.
- iOS may also report a finger as the mouse; the pad draws from one mask,
  so a button pressed by both is simply pressed.
