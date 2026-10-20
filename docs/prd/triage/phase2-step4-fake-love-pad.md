---
title: "fake_love plays the pad"
description: "The fake LÖVE runs a game's loop but ignores the replay's buttons, so no game that reads the pad can be checked headless."
status: needs-triage
---

## Problem Statement

After plan step 3, `tmp/fake_love` loads a LÖVE game and calls `love.load`,
then `love.update(1/60)` and `love.draw()` once per replay frame. It ignores
the `pad` list that every frame of every replay holds. The Lua game of plans
2–3 reads its input only through LÖVE's gamepad API: the
`love.gamepadpressed`/`love.gamepadreleased` callbacks and
`Joystick:isGamepadDown`, on the joystick that `love.joystick.getJoysticks()`
returns. The fake implements none of these, so a game cannot leave the title
screen, and none of the 20 e2e replays can be played by the Lua game.

Nothing yet fixes the edge cases real LÖVE leaves to the event order: the
order of several callbacks in one frame, what a button held on the first
frame does, what happens after the last frame, and what `isGamepadDown`
answers inside a callback. If nobody fixes them once, plans 2–3 will
discover them through e2e mismatches.

The step 3 replay reader also accepts any frame object. A replay with a
button 13, a duplicated button or a mouse field would run silently with the
wrong input. The C reader refuses all of these.

## Solution

The fake plays the pad as the design's loop describes. Before each frame's
`love.update`, it compares the frame's held buttons with the previous
frame's. It calls `love.gamepadreleased(joystick, name)` for each button
that went up, then `love.gamepadpressed(joystick, name)` for each button
that went down, each group in ascending button number. Buttons are
translated through the fixed table of the design: 1 `dpup`, 2 `dpdown`,
3 `dpleft`, 4 `dpright`, 5 `a`, 6 `b`, 7 `x`, 8 `y`, 9 `leftshoulder`,
10 `rightshoulder`, 11 `start`, 12 `back`.

`love.joystick.getJoysticks()` returns a list holding exactly one joystick.
It is always the same object, and the callbacks receive it too.
`joystick:isGamepadDown(name, ...)` answers from the current frame's list.

The replay reader now checks the input: `"input": "pad"`, then on every
frame a `pad` array of distinct integers 1–12, and no other key. It refuses
anything else with a message that names the file, the frame (`f<N>`) and
the problem, before `love.load` runs.

A test game records every callback and every `isGamepadDown` answer into
its `debug_state()`. It runs against a replay that holds several buttons
across frames, and the CLI test compares its state file with the exact
expected log. The settled pad conventions go into `docs/love-port/design.md`.

## User Stories

1. As a plan-2 developer, I want `love.gamepadpressed(joystick, button)` called when a button goes down in the replay, so that my game reacts to presses as it would in real LÖVE.
2. As a plan-2 developer, I want `love.gamepadreleased(joystick, button)` called when a button goes up, so that my game can track releases.
3. As a plan-2 developer, I want the button argument to be the LÖVE name from the design's table (`dpup` … `back`), so that the same code runs in real LÖVE with a real pad.
4. As a plan-2 developer, I want all 12 numbers of the table mapped, each to a distinct name, so that every PSX button the C game reads reaches the Lua game.
5. As a plan-2 developer, I want the pad callbacks of a frame called before that frame's `love.update`, so that the update sees the frame's input, as LÖVE's event pump runs before `update`.
6. As a plan-2 developer, I want every release of a frame called before every press of that frame, so that a button swap in one frame has one fixed order.
7. As a plan-2 developer, I want the releases, and then the presses, of one frame called in ascending button number, so that two simultaneous presses always reach the game in the same order.
8. As a plan-2 developer, I want a button held on frame 0 to fire `gamepadpressed` on frame 0, so that the state before the replay is "nothing held".
9. As a plan-2 developer, I want a button held over several frames to fire `gamepadpressed` only once, with no auto-repeat, so that holding is distinguished from tapping.
10. As a plan-2 developer, I want a button still held on the last frame to get no `gamepadreleased` after the loop, so that the fake calls nothing after the last `love.draw`, except `debug_state`.
11. As a plan-2 developer, I want a button listed in a different order on two consecutive frames to fire nothing, so that only set changes count.
12. As a plan-2 developer, I want `love.joystick.getJoysticks()` to return a list of exactly one joystick on every replay, even one where no button is ever pressed, so that my game can pick its pad at `love.load`.
13. As a plan-2 developer, I want the joystick that `getJoysticks` returns to be the same object every time, and the same one the callbacks receive, so that I can compare joysticks by identity.
14. As a plan-2 developer, I want `getJoysticks` to return a new list on each call, as LÖVE does, so that changing the list does not change the pad.
15. As a plan-2 developer, I want `joystick:isGamepadDown(name)` to answer true exactly when the current frame's list holds that button, so that I can poll held buttons in `update`.
16. As a plan-2 developer, I want `joystick:isGamepadDown(name1, name2, …)` to answer true when any of them is held, as in LÖVE 11.5, so that code written for real LÖVE behaves the same.
17. As a plan-2 developer, I want `isGamepadDown` inside `gamepadpressed` and `gamepadreleased` to already reflect the current frame, so that a callback sees the state its event produced.
18. As a plan-2 developer, I want `isGamepadDown` during `love.load` to answer false for every button, so that the state before frame 0 is "nothing held".
19. As a plan-2 developer, I want `isGamepadDown` during `love.draw` to answer from the same frame as the frame's `love.update`, so that update and draw agree.
20. As a plan-2 developer, I want `isGamepadDown` during `debug_state` to answer from the last frame, so that the end state can include held buttons.
21. As a plan-2 developer, I want `isGamepadDown` on a valid LÖVE gamepad button outside the table (`guide`, `leftstick`, `rightstick`) to answer false, as a real pad that never presses them would, so that code valid in LÖVE does not crash in the fake.
22. As a plan-2 developer, I want `isGamepadDown` with a name LÖVE does not know to fail with LÖVE's message (`Invalid gamepad button: <name>`), so that a typo is caught at once.
23. As a plan-2 developer, I want `isGamepadDown` with no argument, or with a non-string, to fail with a message naming `isGamepadDown`, so that a wrong call is reported rather than answered false.
24. As a plan-2 developer, I want `isGamepadDown` called with a dot instead of a colon to fail with a message naming `isGamepadDown`, so that the common Lua slip is reported clearly.
25. As a plan-2 developer, I want a game that does not define `love.gamepadpressed` or `love.gamepadreleased` to run, with the callback skipped, as LÖVE does, so that a polling-only game needs no callbacks.
26. As a plan-2 developer, I want a `love.gamepadpressed` that is defined but is not a function to be a fatal error naming it, so that this case behaves like the other callbacks of step 3.
27. As a plan-2 developer, I want a Lua error inside a pad callback reported with the callback's name, the frame `f<N>` and a traceback, as step 3 reports `update` errors, so that I find the failing line at once.
28. As a plan-2 developer, I want `love.joystick` absent while `conf.lua` runs and present when `main.lua` runs, as in LÖVE where modules load after `love.conf`, so that a `conf.lua` that works in the fake also works in LÖVE.
29. As a plan-2 developer, I want `love.joystick` to hold `getJoysticks` and nothing else, so that the fake never offers a function the design's list does not hold.
30. As a plan-1 developer, I want a replay without `"input": "pad"` refused, as the C reader refuses it, so that a mouse recording never runs with no input.
31. As a plan-1 developer, I want a frame without a `pad` key refused, so that a frame never silently means "nothing held".
32. As a plan-1 developer, I want a `pad` that is not an array, or an element that is not an integer from 1 to 12, refused with the value named, so that a wrong button never reaches the game.
33. As a plan-1 developer, I want a button listed twice in one frame refused, as the C reader refuses it, so that the list is a set.
34. As a plan-1 developer, I want a frame with any key other than `pad` (`mouse_x`, `dpad`, …) refused, so that an old mouse-era frame is reported, not ignored.
35. As a plan-1 developer, I want the buttons of a frame accepted in any order, as the C reader accepts them, so that every replay the C game plays is valid for the fake.
36. As a plan-1 developer, I want every refusal to name the file, the frame `f<N>` and the problem, and to stop the fake with status 1 before `love.load`, removing its outputs, so that a broken fixture never runs half a game.
37. As a plan-1 developer, I want the reader to accept all 20 replays of `tests/e2e/` with the new checks, so that the checks are proven on the files the fake will play.
38. As a plan-1 developer, I want the held buttons of `pad_title.json` read back exactly, frame by frame, so that the reader is proven to read the right buttons and not only to accept the file.
39. As a plan-1 developer, I want the button table defined once in the fake's C, in a module of its own with no Lua in it, so that the table and the edge rule can be tested without a game.
40. As a plan-1 developer, I want a test game that logs callbacks against a replay holding several buttons across frames, so that the order, the first-frame press, the holds, the swaps and the end of the replay are all checked through the real binary.
41. As a plans-2–3 reader, I want the pad conventions (edge order, frame 0, after the last frame, `isGamepadDown` timing, one joystick) written in `docs/love-port/design.md`, so that I do not need plan 1, which will be deleted.
42. As the repository owner, I want `make test` measured before and after this step with step 3's protocol and kept within its gate, so that the fast loop stays fast.
43. As the repository owner, I want `make e2e` and every `*.sha256` untouched, so that the C reference is unaffected.
44. As a step-5 implementer, I want the reader's new checks to leave `initial_save`, `bindings` and `attract_after` accepted at the top level, so that step 5 only adds their accessors.

## Implementation Decisions

### Modules

- **Pad table and edges** (new deep module, pure C, no Lua, in the fake's
  sources). Its interface is small and stable:
  - the LÖVE name of a button number 1–12 (an assert on any other number);
  - whether a string is one of LÖVE 11.5's 15 gamepad button names (`a`,
    `b`, `x`, `y`, `back`, `guide`, `start`, `leftstick`, `rightstick`,
    `leftshoulder`, `rightshoulder`, `dpup`, `dpdown`, `dpleft`, `dpright`),
    and for those 15 the button number in the table, or 0 for the three
    outside it;
  - the changes between two held sets: given the previous and the current
    12-bit mask (bit `n-1` is button `n`), it fills a caller-owned array of
    at most 24 entries (kind released or pressed, button number) in
    callback order: every release in ascending number, then every press in
    ascending number. It returns the count.
  - The held set is a 12-bit mask with the same bit layout as the C
    game's `P8_PAD_BUTTON_*`, but the module includes nothing from `src/`.
    The fake must survive plan 4.
- **Replay reader** (step 3's module, extended, see H-3-1 and H-3-2). At
  read time, after the existing checks, it validates the pad input and
  keeps one 12-bit mask per frame. It adds one accessor: the held mask of
  frame `i`. Checks:
  - the top-level `input` is present and is the string `"pad"`;
  - every frame object holds exactly one key, `pad`;
  - `pad` is an array whose elements are integers 1–12, distinct, in any
    order;
  - the other top-level keys (`initial_save`, `bindings`, `attract_after`,
    unknown ones) stay accepted and ignored until step 5.

  Each refusal message names the file, the frame as `f<N>` (0-based) and
  the problem, quoting the bad value or key (for example
  `f1: button 13 is not 1-12`, `f1: button 5 listed twice`,
  `f1: unexpected key "mouse_x"`, `f1: no "pad"`,
  `"input" is not "pad"`). The exact wording is the implementer's, and
  tests match only the frame tag and the quoted value.
- **Joystick binding** (new module, the only one that knows both Lua and
  the pad). It installs `love.joystick` into the host's Lua state:
  - one full userdata, created once, is the joystick. Its metatable is
    named for the registry, and its `__index` table holds only
    `isGamepadDown`;
  - `love.joystick` is a table holding only `getJoysticks`, which returns
    a new array table `{ joystick }` on every call;
  - `isGamepadDown(self, ...)` checks that `self` is that joystick
    (otherwise it raises an error naming `Joystick:isGamepadDown` and the
    colon). It needs at least one argument. Each argument must be a
    string. A string outside LÖVE's 15 names raises
    `Invalid gamepad button: <name>`. A LÖVE name outside the table counts
    as never held. The result is true when any named button is in the
    current mask;
  - it keeps the current mask in the Lua state (as an upvalue or a registry
    value). The host sets it, and it starts at 0.
  It also pushes the joystick so that the host can pass it to the
  callbacks.
- **LÖVE host** (step 3's module, extended, see H-3-3 and H-3-4):
  - after `love.conf(t)` and before `main.lua`, it calls the joystick
    binding's install. `love.joystick` is therefore nil inside `conf.lua`,
    as in LÖVE, where modules load after the configuration;
  - its "run one frame" operation takes the frame's held mask as well as
    the frame index. It computes the changes against the mask of the
    previous frame (0 before frame 0), which the host keeps. It sets the
    binding's current mask to the new frame's mask **before** any callback
    of that frame. It calls each change's callback through the same
    `xpcall`/traceback path and the same "skip if undefined, fatal if not a
    function" rule as `update` and `draw`, with the callback name
    (`love.gamepadreleased` or `love.gamepadpressed`) and the frame index
    in the message. Then it runs `update(1/60)` and `draw()` as before;
  - nothing is called after the last frame except `debug_state`, which
    sees the last frame's mask.
- **Command**: it passes each frame's mask from the reader to the host. An
  invalid replay is refused at read time, before the capture file is
  created or `love.load` runs. The exit status is 1, and the outputs are
  removed as step 3 removes them (H-3-9).

### Contracts settled here

- Initial held set: empty. Frame 0 presses everything its list holds.
- Per frame, in order: set the current mask, then the releases in ascending
  number, then the presses in ascending number, then `love.update(1/60)`,
  then `love.draw()`.
- No auto-repeat. No callback after the last frame. No `joystickadded`
  callback, which is not in the design's list.
- One joystick, always connected, the same object everywhere.
- `isGamepadDown` reads the current frame from the start of the frame
  (callbacks included) until the start of the next one. It reads "nothing"
  before frame 0 (`love.load`).
- Unknown LÖVE methods on the joystick (`getName`, `isGamepad`, …) and
  other `love.joystick` functions are absent in this step: calling one
  gives the plain Lua error. The named "unlisted function" fatal error,
  for these as for every other unlisted function, is step 17's.
- These conventions go into `docs/love-port/design.md`, in a short "Pad
  semantics" paragraph under "Pad buttons". The design's loop item 4
  already names the release-then-press order, so it only gains the
  ascending order and the `isGamepadDown` timing.

### Decisions on validation strictness

- `"input": "pad"` is required and every frame must hold exactly `pad`.
  This is stricter than step 3. It matches the C reader, which refuses
  mouse recordings, missing `pad` and `mouse_*`/`dpad` fields. A replay the
  C game accepts is therefore never refused by the fake. Unknown frame keys
  are refused even though the C reader only refuses four named ones: no
  replay holds another key, and the fake's input must be unambiguous.
- Step 3's test replays whose frames lack `pad`, or that lack
  `"input": "pad"`, are updated in this step to `{ "pad": [] }` frames and
  the `input` key (H-3-2). This is a fixture edit, not a behaviour change
  for the step 3 tests.
- A zero-frame replay stays valid (step 3's contract). The C reader
  refuses it, but the fake keeps step 3's rule.

## Implementation Chunks

1. **Pad table and edges.** The pure C module and its unit test. Covers
   stories 3, 4, 6–9, 11, 39.
2. **Reader checks.** The pad validation, the per-frame mask accessor, the
   refusals, the update of step 3's fixture replays, and the reader tests
   (all 20 e2e replays accepted, `pad_title.json` masks read back, each
   refusal). Covers stories 30–38, 44.
3. **Joystick binding, host and the logging game.** `love.joystick`, the
   joystick userdata, `isGamepadDown`, the host's per-frame callbacks, the
   command passing masks, the logging test game and its replay, and the
   CLI cases. Then the design.md paragraph, the "after" timing against
   step 3's protocol, and the step note. Covers stories 1, 2, 5, 10,
   12–29, 40–43.

## Testing Decisions

- A good test drives a public interface and checks external behaviour: the
  pad module's results, the reader's masks and messages, and the binary's
  exit status, stderr and state file. No test reaches into the Lua state or
  the reader's tree.
- **Pad module, unit test** (a new `test_fake_love_pad`, its own target, a
  direct prerequisite of `test` per step 3's rule 1, H-3-6). It checks:
  - the 12 names in the design's order, all distinct;
  - all 15 LÖVE names recognised, with numbers 1–12 for the table and 0 for
    `guide`/`leftstick`/`rightstick`, and unknown strings (`""`, `"A"`,
    `"dpUp"`, `"select"`) refused;
  - changes: empty to empty gives none; empty to {1, 5} gives presses 1
    then 5; {1, 5} to {5} gives release 1; {2, 4} to {1, 3} gives
    releases 2, 4 then presses 1, 3; all 12 to none gives 12 releases in
    order; none to all 12 gives 12 presses in order; an equal mask gives
    none.
- **Reader, unit test** (step 3's `test_fake_love_replay`, extended):
  - every `tests/e2e/*.json` is still accepted;
  - for `pad_title.json`, each of its 32 masks is compared with a
    transcription in the test. Frames 10 and 12 hold {2}, frame 14 {4},
    frame 16 {1}, frames 18–20 {5}, and every other frame is empty;
  - in-memory texts: buttons in any order (`[5, 1]` gives the same mask as
    `[1, 5]`); and each refusal, with the frame tag and the quoted value in
    the message: no `input`, `input` not `"pad"`, a frame `{}`, `pad` not
    an array, `0`, `13`, `-1`, a string element, a duplicate, `mouse_x`,
    `dpad`;
  - texts are in memory and do not reuse the C game's
    `tests/fixtures/events_*.json`, so that plan 4 can delete those
    fixtures freely.
- **The binary, CLI test** (step 3's `test_fake_love`, extended, H-3-6).
  - **The logging game** (`pad_log`, under step 3's fixture directory for
    the fake, H-3-6). It appends one line per event to a log returned by
    `debug_state()`:
    - `load` with the joystick count and the result of
      `isGamepadDown("a")`;
    - `f<N> released <name>` and `f<N> pressed <name>`, each with the
      result of `isGamepadDown(<name>)` and whether the joystick argument
      is the one `getJoysticks()[1]` returns;
    - `f<N> update` with the list of the 12 names held, in table order;
    - `f<N> draw` with the same list.
    - At the end, `debug_state()` adds the list held at that time.
  - **Its replay**, about 9 frames, to cover in order:
    - f0: two buttons out of order, `[5, 1]`;
    - f1: the same held (nothing fires);
    - f2: one released and one kept, `[5]`;
    - f3: a swap in one frame, `[2, 6]` (release 5 before the presses
      2, 6);
    - f4: all 12 at once (all 12 names);
    - f5: none (12 releases in ascending order);
    - f6: none (nothing fires);
    - f7: `[11, 12]`;
    - f8: still `[11, 12]` on the last frame, so no release follows.
  - **The expected log** is written out in full in the test, and the state
    file must equal it byte for byte.
  - Further cases:
    - a game that defines no pad callbacks and only polls runs;
    - `love.gamepadpressed = 1` is fatal, naming it and `f0`;
    - an `error()` inside `gamepadreleased` is fatal, with the callback,
      `f<N>` and `main.lua:<line>`;
    - `isGamepadDown("select")` gives a fatal error with
      `Invalid gamepad button: select`;
    - `isGamepadDown("guide")` answers false;
    - `joystick.isGamepadDown("a")` (with a dot) is fatal, naming
      `isGamepadDown`;
    - a `conf.lua` that checks `love.joystick == nil` (it raises an error
      if not) runs;
    - an invalid replay (button 13) exits with status 1 and leaves no
      output files.
  - If this test goes over step 3's 100 ms check budget, the pad cases
    move to a test of their own, with its own target (step 3's rule 2,
    H-3-7).
- **Prior art:**
  - `tests/test_input.c` for the pad refusals and the "any order" rule this
    step mirrors;
  - step 3's `test_fake_love` and `tests/test_replay_usage.c` for driving
    a binary with `system()`;
  - `tests/test_frames_check.c` for a tool tested from C.
- **Timing:** `make test` is measured before and after with step 3's
  protocol and gate (paired ratios against this step's "before" and
  against the plan-wide reference, within 3 %, H-3-7). The step note
  records the numbers and the slowest new check's own time.
- The unit tests of the C game, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- The fake disk, `love.filesystem`, `initial_save`, `bindings`, `arg` and
  `--attract-after` (step 5). The reader keeps accepting those keys.
- Every drawing function and any byte in the capture (phases 3 and 4).
- The named "unlisted function" fatal error, including for unlisted
  `Joystick` methods and `love.joystick` functions (step 17).
- Keyboard input (an open question of the design, not needed by the e2e).
- Axes, triggers, `joystickadded`/`joystickremoved`, several joysticks,
  vibration: none is on the design's list.
- The game's own `bindings.cfg` logic, which maps buttons to actions in
  the Lua game (plans 2–3).
- Audio, always.

## Hypotheses

These are assumptions about plan step 3, which is being implemented while
this PRD is written. They are taken from its PRD
(`phase2-step3-fake-love-loop`), not from its code. Where one turns out
false, the implementer adapts the named decision and keeps the behaviour
this PRD settles.

- **H-3-1**: step 3 left a replay reader module in the fake's sources
  (named for the replay). Its interface: read from a path, parse from a
  text and a length, frame count, free, and an error buffer naming the
  file and the problem. It keeps the whole parsed JSON tree in one
  allocation it owns, and refuses non-integer numbers. If the tree is not
  kept, the pad checks and masks are computed inside its parse pass
  instead. The new accessor is the same either way.
- **H-3-2**: step 3's reader requires only `frames`, an array of objects,
  and ignores every other key, including `input` and `pad`. Some of step
  3's fixture replays may have `{}` frames or no `input`, and chunk 2
  updates them. If step 3 already checks part of the pad input, chunk 2
  keeps its checks and adds only the missing ones.
- **H-3-3**: step 3 left a LÖVE host module. Its operations: open, call
  load, run one frame (by index: `update(1/60)` then `draw()`), get the
  debug state, close. Every Lua call goes through `xpcall` with
  `debug.traceback`. Messages name the callback and `f<N>`. An undefined
  callback is skipped, and a defined non-function is fatal. This step
  adds the held mask to "run one frame" and reuses the call path for the
  pad callbacks.
- **H-3-4**: the host creates an empty global `love` table before
  `conf.lua`, calls `love.conf(t)` if defined, then runs `main.lua`. This
  step installs `love.joystick` between `love.conf` and `main.lua`. If
  step 3 orders these differently, the install still goes after the
  configuration and before `main.lua`.
- **H-3-5**: `tmp/fake_love` is built by the public `fake-love` target
  from the fake's C sources, with LuaJIT flags from `pkg-config luajit`
  added only to the fake's objects. New source files in the fake's
  directory join the build the same way the existing ones do, whether by
  wildcard or by an explicit list that this step extends.
- **H-3-6**: the step 3 tests are `test_fake_love_replay` (a reader unit
  test) and `test_fake_love` (a CLI test that runs `tmp/fake_love` through
  `system()` and reads its outputs under `tmp/`). Each is its own make
  target and a direct prerequisite of `test`. The test games and their
  replays live in a `fake_love/` sub-directory of the test fixtures, one
  directory per game. The new `test_fake_love_pad` joins the same way.
- **H-3-7**: step 3 wrote the timing rule into `docs/love-port/design.md`:
  - each check is its own target, run in parallel;
  - a check's budget is 100 ms;
  - the protocol is git worktrees under `tmp/`, one warm-up, then 11
    rotated rounds with paired ratios, and 11 more on a failure;
  - the gate is 3 % against "before" and against the plan-wide reference
    (commit 2e7215c in its PRD).

  If step 3's note records different numbers, this step uses those.
- **H-3-8**: step 3 implemented `debug_state()` and the optional state
  file argument, so that the logging game's log can reach the test through
  the state file.
- **H-3-9**: on any failure of the game or its inputs, the fake exits
  with status 1, prints a message prefixed `fake_love: `, and removes the
  outputs it created. Step 3's replay is read before the capture file is
  created, so an invalid replay leaves nothing behind.

## Further Notes

- The step 4 note in the plan must hold the before and after `make test`
  measurements (both paired medians, the three plain medians, rounds, core
  count) and the slowest new check's time.
- Real LÖVE delivers simultaneous SDL events in whatever order SDL queues
  them. The ascending order fixed here is the fake's convention. The Lua
  game must not depend on the order within a frame. The ascending order
  only makes the fake deterministic. No capture program checks the pad:
  the plan's capture rule is for drawing functions only, and Xvfb has no
  gamepad.
- The C game reads held buttons as a mask per frame. The Lua game can
  rebuild that mask from `isGamepadDown` on the 12 names, so the
  callbacks are offered for fidelity to LÖVE, and the polling is enough
  for the port.
