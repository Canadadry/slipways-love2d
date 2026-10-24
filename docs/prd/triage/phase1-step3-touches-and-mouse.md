---
title: "Fake LÖVE: replay touches and mouse, love.touch and love.mouse polling"
description: "The fake LÖVE has no touch screen and no mouse, so the virtual pad of plan 6 cannot be driven or verified headless."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) plays the game on a portrait iPhone with a virtual
pad drawn on the touch screen, and on desktop a mouse click counts as one
more touch. The pad reads its input by polling, every frame:
`love.touch.getTouches` / `getPosition` and `love.mouse.getPosition` /
`isDown`. Today the fake LÖVE (`tools/fake_love/`) has neither `love.touch`
nor `love.mouse`: reading either fails with "is not in the function list of
docs/love-port/design.md". A replay cannot say that a finger touched the
screen, moved or lifted, nor where the mouse is: every frame must hold
`"pad"` and nothing but `"pad"` and `"resize"`. And `tools/e2e_limits.sh`,
the guard of the committed replays, counts a replay's frames by its `"pad"`
keys, so a replay made of touch-only frames would be counted wrong. Without
this step no later step of the plan (the touch → mask logic wired into
`main.lua`, the touch e2e replays, the pad's screen tests) can be verified
headless, which is the plan's only kind of verification.

## Solution

The replay format gains two optional per-frame fields, every existing
replay keeping its meaning and its bytes:

- `"touches": [[id, x, y], …]`: the touches held on that frame, in window
  coordinates. A touch whose id appears on consecutive frames is the same
  finger (moving if its position changes); an id that disappears has
  lifted. A frame without `touches` holds no touch.
- `"mouse": [x, y, left]`: the mouse position and its left button (0 or 1)
  from that frame on. A frame without `mouse` keeps the previous frame's
  mouse; before the first `mouse`, the mouse is at (0, 0), not down.

`pad` becomes optional too: a frame without it holds no button. So a frame
may hold only touches, only a mouse, or nothing (`{}`).

The fake answers, from the current frame:

- `love.touch.getTouches()`: a new sequence of the held touches' ids, in the
  frame's order; `love.touch.getPosition(id)`: that touch's `x, y`;
- `love.mouse.getPosition()`: `x, y`; `love.mouse.isDown(1)`: whether the
  left button is down.

Polling only: no touch or mouse callback joins the list. Without replay
data (`love_capture_check`, or a replay with neither field) there is no
touch and no click.

`tools/e2e_limits.sh` counts a replay's frames as the objects of its
`frames` array, whatever keys they hold, so a replay using the new fields,
touch-only frames included, is checked with its true frame count.

All of it enters `docs/love-port/design.md` (the function list, the replay
members paragraph, a touch-and-mouse semantics paragraph, the `make e2e`
limits) and the static `lua-guard` list in the same step. The 20 e2e
replays are unchanged: they carry neither field, and the game does not call
the new functions yet.

## User Stories

1. As a plan-6 implementer, I want a frame to carry `"touches": [[id, x, y], …]`, so that a replay can put fingers on the screen at exact positions on exact frames.
2. As a plan-6 implementer, I want a touch held over several frames to keep its id, so that the virtual pad can tell the stick's finger from a button's finger.
3. As a plan-6 implementer, I want a touch that changes position between frames under the same id to be read as moving, so that a replay can slide a finger from A to B or push the stick.
4. As a plan-6 implementer, I want a touch missing from a frame to be lifted on that frame, so that a replay releases a virtual button by omitting its finger.
5. As a plan-6 implementer, I want several touches in one frame, so that a replay can press two virtual buttons at once (the 5 + 6 developer chord).
6. As a plan-6 implementer, I want an id to be usable again after its touch lifted, so that a replay can model iOS reusing touch ids.
7. As a plan-6 implementer, I want a frame to carry `"mouse": [x, y, left]`, so that a replay can drive the desktop click that counts as a touch.
8. As a plan-6 implementer, I want the mouse kept as it is on a frame without `mouse`, so that holding a click over 100 frames takes one field, not 100.
9. As a plan-6 implementer, I want the mouse at (0, 0) with the left button up before the first `mouse`, so that a replay without mouse data never clicks.
10. As a plan-6 implementer, I want `pad` optional in a frame, absent meaning no button held, so that a touch replay holds touches only, as on a phone with no controller.
11. As a plan-6 implementer, I want a frame to hold any mix of `pad`, `resize`, `touches` and `mouse`, so that the step-13 test "a gamepad button hides the pad and drops the touches" can be replayed.
12. As a plan-6 implementer, I want an empty frame `{}` accepted as a frame with nothing held, so that a replay can wait without noise.
13. As the game, I want `love.touch.getTouches()` to return a new sequence of the current touches' ids, so that I can iterate them as in LÖVE 11.5.
14. As the game, I want touch ids to be light userdata, as in LÖVE 11.5, so that code that does arithmetic on an id fails in the fake as it would on the phone, and an id still works as a table key and with `==`.
15. As the game, I want the same touch to give the same id value on every frame it is held, so that I can keep per-touch state between frames.
16. As the game, I want `love.touch.getPosition(id)` to return the touch's x and y as numbers in window coordinates, so that the pad hit-tests in the same units as `getDimensions`.
17. As the game, I want `getPosition` on an id that is not held this frame to raise `Invalid active touch ID`, so that a stale id fails loudly as in LÖVE.
18. As the game, I want `love.mouse.getPosition()` to return the mouse x and y as numbers, so that the mouse can be treated as one more touch.
19. As the game, I want `love.mouse.isDown(1)` to answer whether the left button is down, so that only a held click counts as a touch.
20. As the game, I want the touches and the mouse of a frame current from the frame's first pad callback to the start of the next frame, so that `love.update` and `love.draw` agree on the input.
21. As the game, I want no touch and the mouse at (0, 0), up, from the top of `main.lua` and during `love.load`, so that start-up reads the same as `isGamepadDown`'s "nothing held".
22. As the game, I want `debug_state()` to see the last frame's touches and mouse, so that it reads like `isGamepadDown` there.
23. As the game, I want `love.touch` and `love.mouse` nil while `conf.lua` runs, so that they behave like the other modules.
24. As a developer, I want `getTouches` with an argument, `getPosition` with something other than one touch id, `mouse.getPosition` with an argument, and `isDown` with anything but the single argument `1` to raise an error naming the function, so that the contract stays "one form, anything else refused".
25. As a developer, I want `love.touchpressed`, `love.mousepressed` and the other touch and mouse callbacks still refused by the function-list guard, so that polling stays the only input path the plan allows.
26. As a developer, I want any other field of `love.touch` or `love.mouse` (`getPressure`, `setVisible`, …) to fail with the function-list error, so that the list stays exact.
27. As a replay author, I want a malformed `touches` (not an array, a touch not three integers, an id below 1 or above 2147483647, an id listed twice in a frame, more than 10 touches) refused with the replay, the byte offset, the frame and the problem, so that a typo never runs.
28. As a replay author, I want a touch outside the window in force on that frame refused, naming the touch, its position and the window size, so that a replay never touches where no screen is.
29. As a replay author, I want a malformed `mouse` (not three integers, `left` other than 0 or 1, a position outside that frame's window) refused the same way, so that a typo never runs.
30. As a replay author, I want a frame key given twice refused, so that a frame never silently keeps one of two `touches`.
31. As a replay author, I want any frame key other than `pad`, `resize`, `touches` and `mouse` refused, with the message listing the four, so that unknown fields never pass.
32. As a replay author, I want every refusal to exit 1 before `love.load` with no capture nor state file, like the other replay refusals.
33. As a maintainer, I want `tools/e2e_limits.sh` to count a replay's frames as the objects of its `frames` array, so that a touch-only frame counts as a frame and the `.txt`'s "N frames." is checked against the truth.
34. As a maintainer, I want `tools/e2e_limits.sh` to keep refusing the old `mouse_x`, `mouse_y`, `mouse_buttons` and `dpad` fields while accepting the new `mouse`, so that the old C-era mouse format never comes back by accident.
35. As a maintainer, I want `tools/e2e_limits.sh` to stay one awk process for all replays, so that `make e2e` pays nothing per replay for the new count.
36. As a maintainer, I want `love_capture_check` to answer the new functions with no touch and no click, so that both binaries share one function list.
37. As a maintainer, I want `lua-guard` to accept `love.touch.getTouches`, `love.touch.getPosition`, `love.mouse.getPosition` and `love.mouse.isDown` in `game/`, outside `game/love/` too, so that `main.lua` can poll them as it polls the joystick.
38. As a maintainer, I want design.md's function list, module list, replay members paragraph, a touch-and-mouse semantics paragraph and the `make e2e` limits to state the new behaviour, so that the contract stays the single description of the fake.
39. As a maintainer, I want the 20 replays of `tests/e2e/` to keep their `*.sha256` and `*.end-state`, so that the step provably changes no game behaviour.
40. As a maintainer, I want the new check of `make test` to pass design.md's timing gate with its medians recorded in plan 6 §6, so that `make test` is no slower.

## Implementation Decisions

- **Replay reader** (`replay` module of the fake), per frame:
  - The frame reader (the one step 2 extended with `resize`, H-2-3) reads
    four optional keys: `pad`, `resize`, `touches`, `mouse`. Each at most
    once per frame (`f<N>: "<key>" given twice`). Any other key:
    `f<N>: unexpected key "<k>": a frame holds only "pad", "resize",
    "touches" and "mouse"`. The "no \"pad\"" refusal is removed: an absent
    `pad` is an empty held set.
  - `touches`: an array (possibly empty) of at most 10 touches, each an
    array of exactly three integers `[id, x, y]`; `id` 1–2147483647,
    unique within the frame; `0 <= x < w` and `0 <= y < h` for the window
    size in force on that frame, after its own `resize` (H-2-2). Kept in
    the frame's listed order.
  - `mouse`: an array of exactly three integers `[x, y, left]`, `left` 0 or
    1, the position inside that frame's window as for touches. The reader
    resolves "kept between frames" at parse time: every frame gets the
    mouse state in force on it (the default `(0, 0, up)` before the first
    `mouse`). A kept mouse is not rechecked against a later, smaller
    window: LÖVE does not move the cursor on a resize.
  - Messages follow the existing style, e.g. `<file>: offset <n>: f<N>:
    "touches" is <kind>, not an array of [id, x, y]`, `... f<N>: touch
    [<value>] is not [id, x, y] with integers`, `... f<N>: touch id <id> is
    not 1-2147483647`, `... f<N>: touch id <id> listed twice`, `... f<N>:
    <n> touches, more than 10`, `... f<N>: touch <id> at (<x>, <y>) is
    outside the <w>x<h> window`, `... f<N>: "mouse" <value> is not [x, y,
    left] with integers, left 0 or 1`, `... f<N>: mouse at (<x>, <y>) is
    outside the <w>x<h> window`.
  - Two queries: the touches of a frame (a pointer to an array of
    `{id, x, y}` owned by the replay, and its count, 0 for none) and the
    mouse of a frame (`{x, y, left}`, already resolved).
  - Integers only: the reader has no JSON numbers with a fraction, and
    integer window coordinates are enough for every test the plan lists.
- **Frame input to the host.** The frame-input struct `host_frame` takes
  (H-2-1) gains the frame's touches (pointer and count, not owned) and its
  mouse (`x`, `y`, `left`). A zero struct means no touch and the mouse at
  (0, 0), up, which is what `love_capture_check` passes (H-2-6).
- **New fake module `touch`** (C), installing both `love.touch` and
  `love.mouse` from `host_open`, beside `joystick_install`, and holding the
  current touches and mouse. Its interface: install, and set the current
  frame's touches and mouse. Before the first frame: no touch, mouse
  `(0, 0)`, up.
  - `getTouches()`: no argument, else an error naming the function; returns
    a new sequence of light userdata, one per touch, in the frame's order.
    A touch's light userdata is its replay id, so the same finger gives the
    same value every frame.
  - `getPosition(id)`: exactly one light userdata argument (else an error
    naming the function and the argument's type); an id not held this
    frame raises `Invalid active touch ID` (LÖVE's message). Returns two
    Lua numbers.
  - `love.mouse.getPosition()`: no argument; two Lua numbers.
  - `love.mouse.isDown(1)`: exactly one argument, the integer 1; anything
    else raises `love.mouse.isDown: only isDown(1), the left button, is in
    the function list`. Returns a boolean.
  - Each module table and its functions go through
    `function_list_protect_module`, like `love.joystick`.
- **Frame order in the host.** The touches and the mouse become current
  together with the pad list, after `love.resize` (H-2-4) and before the
  pad callbacks; so, like `isGamepadDown`, they answer the previous frame
  inside `love.resize`. They stay current until the next frame starts,
  `debug_state()` included.
- **Function-list guard.** `touch` and `mouse` join the module names (read
  nil until installed, so nil in `conf.lua`). No callback is added.
- **`tools/e2e_limits.sh`.** The frame count becomes the number of objects
  directly inside the `frames` array: with string contents ignored, every
  `{` at nesting depth 2 (root object → array → object). In the replay
  format `frames` is the only member holding objects in an array, so this
  is exact. The count is computed for every replay in the script's single
  awk process (no process per replay is added; the per-replay `grep` that
  counted `"pad":` goes). The `"input": "pad"` requirement and the
  refusal of `mouse_x`, `mouse_y`, `mouse_buttons` and `dpad` stay (the
  quoted key `"mouse"` matches none of them). The header comment states
  the new count rule.
- **`"input": "pad"`** stays the only accepted input header: it names the
  fake's input model, which now includes touches and the mouse; step 15's
  touch replays declare it too.
- **`lua-guard`** (`tools/lua_guard.lua`): `touch = {getTouches,
  getPosition}` and `mouse = {getPosition, isDown}` join its functions,
  not glue-only (like `joystick`: `main.lua` polls input).
- **design.md** (`docs/love-port/design.md`), same step (H-2-5):
  - "The LÖVE functions the game may use": a `Touch and mouse` line with
    the four functions and their one form; polling only, no callback.
  - "Fake disk and arguments" / replay members: frames hold `pad`,
    `resize`, `touches` and `mouse`, each optional and at most once, with
    their rules; an absent `pad` holds nothing.
  - A **Touch and mouse semantics** paragraph beside "Pad semantics":
    ids are light userdata equal to the replay id, getTouches in the
    frame's order (the game must not depend on that order), a missing id is
    a lifted touch, the mouse is kept between frames, the default state,
    when the state is current (after `love.resize`, until the next frame,
    `debug_state()` included), coordinates in the units of `getDimensions`
    (one unit one fake pixel; `highdpi` not modelled, H-2-8).
  - `make e2e`: `tools/e2e_limits.sh` counts the objects of `frames`.
- **No change to the game** (`game/`): no module calls the new functions in
  this step; step 14 will.
- **No real-LÖVE capture**, per the plan's rule (design.md's function-list
  sentence already says plan 6's touch extensions are verified by fake C
  tests).

## Implementation Chunks

1. **Replay reader.** `touches`, `mouse`, optional `pad`, frame keys at
   most once, the two queries; the committed fixture replay of the new
   fields (below). Unit tests in the replay reader's C test. The 20 e2e
   replays still read with their frame counts.
2. **Frame counter of `tools/e2e_limits.sh`.** Count by objects of
   `frames`; its C test gains the cases below, using the same fixture.
3. **`love.touch` and `love.mouse` in the host.** The `touch` module, the
   frame-input fields, the function-list modules, the new CLI check;
   `make e2e` and `make end-state` 20/20.
4. **Static guard, contract and gate.** `lua-guard` lists and their test;
   design.md sections; timing gate run and medians recorded in plan 6 §6,
   with `tools/e2e_limits.sh`'s own time before and after.

## Testing Decisions

- A good test drives the code from the outside: a replay text or file and,
  for the fake, a tiny fixture game whose `debug_state()` returns a log,
  then an exact comparison (the style of the joystick and step 2's window
  CLI tests). No test reads the host's or the reader's internals.
- **Shared fixture** `tests/fixtures/touch_frames.json`: a short replay
  (about 12 frames) with `pad` frames, touch-only frames, a mouse-only
  frame, an empty `{}` frame, a frame spanning several lines, and a touch
  appearing, moving, a second finger, the first lifting, all lifting, an id
  reused. Both the replay reader test and the e2e-limits test read it, so
  the fake's frame count and the checker's count are tied to one file.
- **Replay reader unit tests** (existing C test of the reader): the
  fixture's frame count; per frame, the touches (ids, positions, order)
  and the resolved mouse (default, given, kept, released); a frame without
  `pad` holds 0; touches checked against the window after a same-frame
  `resize`. Refusals: `touches` not an array; a touch of two or four
  elements; a non-integer; id 0 and 2147483648; an id twice in one frame;
  11 touches; x = w, y = h, negative coordinates; `mouse` of two elements,
  `left` 2; mouse outside the window; a key given twice; an unknown key
  (message listing the four). Each refusal asserts the frame and the
  problem in the message.
- **New CLI check** `test_fake_love_touch` (its own make target in
  `FAKE_LOVE_CHECKS`, its binary in the fake-side lists), with a fixture
  game logging `getTouches` (ids compared by identity with the previous
  frame's, positions via `getPosition`), `mouse.getPosition` and
  `isDown(1)` at the top of `main.lua`, in `love.load`, in the pad
  callbacks, in `love.update`, `love.draw` and `debug_state`, plus the
  type of an id. Replays:
  - neither field: no touch and mouse (0, 0) up everywhere;
  - touches appearing, moving, a second finger, lifting (the log shows the
    same id value across frames, `getPosition` following it, the lifted id
    gone), an id reused;
  - mouse given, kept over frames without `mouse`, released;
  - pad frames mixed with touch-only frames: the pad callbacks still fire
    (a button held then a frame without `pad` releases it), touches and pad
    read on the same frame;
  - failures, chosen from an environment variable like the `pad_misuse`
    fixture: `getPosition` of a lifted id (`Invalid active touch ID`),
    `getPosition(1)`, `getTouches(1)`, `isDown(2)`, `isDown()`,
    `mouse.getPosition(1)`, `love.touchpressed = function` and
    `love.touch.getPressure` (function-list errors), each naming
    `main.lua:<line>:`; and a refused replay (exit 1, no capture nor state
    file).
  Prior art: the joystick CLI test and its `pad_log` / `pad_misuse`
  fixtures; step 2's window CLI test.
- **Function-list unit test**: `love.touch` and `love.mouse` read nil
  before install; creating `love.touchpressed` still fails.
- **lua-guard test**: a game file polling the four functions outside
  `game/love/` passes; another `love.touch` or `love.mouse` name fails.
- **e2e-limits test** (`test_e2e_limits`): the shared fixture, with a
  `.txt` stating its true frame count and a `.sha256`, passes; the same
  with a `.txt` one frame short fails with "the file has <N>"; a replay of
  181 touch-only frames fails as over 180; a replay carrying `"mouse"`
  passes while `"mouse_x"` still fails.
- **Regression**: `make e2e` (20/20 match) and `make end-state` (20/20).
- **Timing gate**: design.md's protocol (11 rounds, both baselines, ≤ 3 %),
  medians recorded in plan 6 §6 (H-2-7). The new check stays under the
  100 ms budget. `make e2e` wall time is not gated by the plan for this
  step (not one of steps 4, 5, 7, 16), but `tools/e2e_limits.sh` changed:
  its own time on the 20 replays is measured before and after (median of
  5) and recorded; it must not grow by more than 10 ms.

## Hypotheses on step 2 (not done yet)

- **H-2-1**: step 2 made `host_frame` take one frame-input struct (held
  buttons, whether a resize is carried, its width and height) instead of
  the bare mask; this step adds fields to that struct without changing the
  signature.
- **H-2-2**: step 2's replay reader tracks, at parse time, the window size
  in force on each frame (the top-level `window`, default 640×480, then
  each `resize`, which takes effect at the start of its frame), and this
  step can read that size per frame for its bounds checks; if it is not
  exposed per frame, this step keeps it inside the reader while parsing.
- **H-2-3**: step 2 kept one per-frame reader in the replay module, which
  reads `pad` (required) and an optional `resize`, refuses other keys with
  "a frame holds only \"pad\" and \"resize\"", and left `tools/e2e_limits.sh`
  unchanged; this step makes `pad` optional, adds two keys and changes the
  message.
- **H-2-4**: step 2's frame order is: make the resize current, call
  `love.resize` if defined, then make the pad list current, the pad
  callbacks, `love.update`, the transform reset, `love.draw`, the
  "Canvas is active" check. This step makes touches and the mouse current
  together with the pad list.
- **H-2-5**: step 2 added `getDimensions` and `love.resize` to design.md's
  function list (seven callbacks), `window` and `resize` to the replay
  members paragraph, the resize to "Pad semantics", and the two names to
  `tools/lua_guard.lua`; this step edits those places in place and adds a
  touch-and-mouse paragraph beside "Pad semantics".
- **H-2-6**: `love_capture_check` passes a zero-initialised frame input
  (no resize, nothing held); zero values of this step's new fields mean no
  touch and the mouse at (0, 0), up.
- **H-2-7**: step 2's final commit is this step's "before" baseline for
  the timing gate; its window CLI check is already in that baseline.
- **H-2-8**: step 2 made the window size integers 1–4096 in LÖVE units,
  one unit one fake pixel, `highdpi` not modelled; touch and mouse
  coordinates use the same units and fit an `int`.

## Out of Scope

- Touch and mouse callbacks (`touchpressed`, `touchmoved`, `mousepressed`,
  …), `getPressure`, the right and middle buttons, `setVisible`, cursors,
  wheel: polling of the four functions only, per the plan.
- Fractional touch or mouse coordinates (the reader has no JSON fractions).
- The screen framebuffer, `screens` (step 4); `pad_sheet` (step 5).
- Any game change: touch → mask, show/hide, wiring into `main.lua`
  (steps 11–14), the touch e2e replays and their twins (step 15).
- `"input"` values other than `"pad"`.

## Further Notes

- Decided without a guideline agent: every question had an answer in the
  plan (§2 Input, Fake LÖVE, Tests) or in the code. Choices of this PRD not
  dictated by the plan: `pad` optional and `{}` allowed (the plan's
  "touch-only frames" and "the checker adapted if needed"); the whole
  mouse state, not only its position, kept between frames (a mouse is a
  state, not an event); light userdata ids equal to the replay id; the
  10-touch cap; touches and mouse inside the frame's window; `isDown(1)`
  as the only form; the `touch` C module installing both tables; counting
  frames by objects of `frames`; `lua-guard` extended now rather than at
  step 14.
- Real LÖVE 11.5's `getTouches` returns ids as light userdata and
  `getPosition` raises "Invalid active touch ID" for an id that is not
  active; the fake follows both. Which unit iOS touch positions use under
  `highdpi` on the phone is left to plan 6 §5's open question on the
  embedded LÖVE version.
