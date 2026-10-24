---
title: "Fake LÖVE: replay window size, per-frame resize, getDimensions and love.resize"
description: "The fake LÖVE has no window size, so neither the layout of plan 6 nor a resizable window can be tested headless."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) puts the game on a portrait iPhone and makes the
desktop window resizable. Everything it draws outside the 160×120 canvas
(the fractional present, the tall/overlay layout, the virtual pad) depends
on the window size, and the layout is recomputed every frame from it. Today
the fake LÖVE (`tools/fake_love/`) has no notion of a window:
`love.graphics.getDimensions` is not in the function list (reading it fails
with "is not in the function list of docs/love-port/design.md"), `love.resize`
cannot even be set by the game (only six callback names may be created in
`love`), and a replay cannot say what size the window is or that it changed.
So no later step of the plan can be verified headless at 390×844, 640×480 or
after a resize, which is the plan's only kind of verification.

## Solution

The replay format gains two optional fields, every existing replay keeping
its meaning and its bytes:

- top level `"window": [w, h]`: the window size for the whole run until a
  resize, default 640×480 (the size plan 6's `conf.lua` asks for);
- per frame `"resize": [w, h]`: the window becomes that size at the start of
  that frame.

The fake answers `love.graphics.getDimensions()` with the current window
size, and on a frame that carries `resize` calls the game's `love.resize(w, h)`
before anything else of that frame — before the pad callbacks and before
`love.update`. Both enter the function list of `docs/love-port/design.md`
("The LÖVE functions the game may use"), the static `lua-guard` list, and the
replay-members paragraph, in the same step. The 20 e2e replays are
unchanged: they carry neither field, and the game does not call either new
function yet.

## User Stories

1. As a plan-6 implementer, I want a replay to state its window size with `"window": [w, h]`, so that a later screen test can run the game as on a 390×844 iPhone.
2. As a plan-6 implementer, I want a replay without `window` to run at 640×480, so that the 20 existing replays need no edit and match the desktop `conf.lua` of step 7.
3. As the game, I want `love.graphics.getDimensions()` to return the window width and height as two numbers, so that the layout module can choose tall or overlay mode every frame.
4. As the game, I want `getDimensions` to work from the top of `main.lua` on, so that I can read the size during `love.load` as in real LÖVE, where the window exists before `main.lua` runs.
5. As the game, I want `getDimensions` to return the window size whatever canvas is selected, so that its meaning matches LÖVE 11.5 (the window, not the active canvas).
6. As the game, I want `love.graphics` to stay nil while `conf.lua` runs, so that the existing rule "love.graphics is nil in conf.lua" still holds.
7. As a plan-6 implementer, I want a frame to carry `"resize": [w, h]`, so that a replay can drive a window resize at an exact frame.
8. As the game, I want `love.resize(w, h)` called with the new size on a resize frame, so that I can react as in real LÖVE.
9. As the game, I want `love.resize` called before that frame's pad callbacks and before `love.update`, so that update and draw of that frame already see the new size.
10. As the game, I want `getDimensions` to return the new size inside `love.resize` and for the rest of that frame and every later frame, so that the callback argument and the query never disagree.
11. As the game, I want `getDimensions` to return the old size on every frame before the resize frame, so that the change is visible at exactly one frame boundary.
12. As the game, I want no `love.resize` call when the game does not define it, so that a game ignoring resizes runs unchanged.
13. As the game, I want no `love.resize` call at start-up for the initial size, so that the fake behaves as LÖVE, which never calls `resize` for the window it opens with.
14. As a developer, I want `love.resize` set to something that is not a function to fail with `love.resize is a <type>, not a function (f<N>)`, so that it reads like the other callbacks' refusals.
15. As a developer, I want an error raised inside `love.resize` to name `love.resize f<N>:`, the game's `file:line:` and a stack traceback, so that it reads like an error in any other callback.
16. As a developer, I want `love.resize` to be creatable in `love` while every other unknown name still fails, so that the function-list guard keeps its strictness.
17. As a developer, I want `love.resize` to read nil before the game sets it, so that it behaves like the six existing callbacks.
18. As a developer, I want `getDimensions` called with any argument to raise an error naming the function, so that the contract stays "one form, anything else refused".
19. As a replay author, I want `window` and `resize` refused unless each is an array of exactly two integers from 1 to 4096, with an error naming the replay, the byte offset and the field (and the frame for `resize`), so that a typo never runs.
20. As a replay author, I want `window` given twice refused like `bindings` given twice, so that the start state is never ambiguous.
21. As a replay author, I want a `resize` equal to the current window size refused, naming the frame and the size, so that a replay never claims a resize LÖVE would not report.
22. As a replay author, I want a frame that still lacks `"pad"` to be refused, so that `tools/e2e_limits.sh`, which counts frames by their `"pad"` keys, keeps the right frame count; touch-only frames are step 3's business.
23. As a replay author, I want any frame key other than `pad` and `resize` still refused, with the message listing the keys a frame may hold, so that unknown fields never pass silently.
24. As a replay author, I want a refusal to exit 1 before `love.load` with no capture nor state file, so that it behaves like the other replay refusals.
25. As a developer, I want `resize` allowed on frame 0, so that a test can resize before the first update.
26. As a developer, I want several resizes in one replay, each against the size the previous one left, so that a back-and-forth (tall, then overlay, then tall) can be replayed.
27. As a developer, I want the window size to be integers in LÖVE units, one unit one fake pixel, so that step 4's screen framebuffer has an exact size; `highdpi` is not modelled.
28. As a developer, I want the fake to keep ignoring `t.window` set by `love.conf`, so that the replay alone states the size and `conf.lua` cannot make a replay depend on it.
29. As a developer, I want `love_capture_check` to answer `getDimensions` with the default 640×480 and never call `love.resize`, so that both binaries share one function list.
30. As a maintainer, I want `lua-guard` to accept `love.resize` and `love.graphics.getDimensions` in `game/`, so that the static guard and the fake agree on the list (and `getDimensions` stays glue-only like the rest of `love.graphics`).
31. As a maintainer, I want design.md's function list, callback count, replay members paragraph and pad-semantics frame order to state the new behaviour, so that the contract stays the single description of the fake.
32. As a maintainer, I want the 20 replays of `tests/e2e/` to keep their `*.sha256` and `*.end-state`, so that the step provably changes no game behaviour.
33. As a maintainer, I want the new check of `make test` to pass design.md's timing gate with its medians recorded in plan 6 §6, so that `make test` is no slower.

## Implementation Decisions

- **Replay reader** (`replay` module of the fake). Two new readers:
  - top-level `window`, added to the start-state members table beside
    `initial_save`, `bindings`, `attract_after` (so "given twice" is
    refused the same way); exposed as a query returning the width and
    height, 640 and 480 when absent;
  - per-frame `resize`, read with the frame's `pad`; exposed as a query
    "does frame f carry a resize, and to what size". The reader checks the
    resize against the size in force (the `window` value, or the previous
    resize), so the equal-size refusal is a parse-time refusal.
  - Value rule for both: an array of exactly two integers, each 1–4096.
    4096 caps step 4's RGBA screen at 64 MiB. Messages follow the existing
    style: `"<file>: offset <n>: "window" <value> is not [width, height]
    with integers 1-4096`, `... f<N>: "resize" ...`, and
    `... f<N>: "resize" [w, h] is already the window size: LÖVE calls
    love.resize only on a change`.
  - The frame-key refusal message becomes "a frame holds only \"pad\" and
    \"resize\""; `pad` stays required in every frame.
- **Frame input to the host.** `host_frame` takes the frame's input as one
  struct (held buttons, whether a resize is carried, its width and height)
  instead of the bare `held` mask, so step 3 adds touches and mouse to the
  same struct without changing the signature again. The host owns the
  current window size; the window size of the start state is given to the
  host once, after `host_new` and before `host_open`, default 640×480 when
  never given (which is what `love_capture_check` relies on).
- **Frame order in the host**, per frame: make the resize current (the size
  `getDimensions` answers), call `love.resize(w, h)` if defined; then the
  existing order — make the pad list current, `gamepadreleased`, then
  `gamepadpressed`, `love.update(1/60)`, transform reset, `love.draw`, the
  "Canvas is active" check. Like the ascending button order, putting resize
  before the pad callbacks is the fake's convention (real LÖVE delivers
  events in arrival order); design.md says the game must not depend on the
  order of resize versus pad callbacks within one frame. `w` and `h` are
  pushed as Lua numbers.
- **`love.graphics.getDimensions`** joins the graphics table built by
  `graphics_install`, reading the host's current window size (passed in by
  pointer, like the canvas store and the transform). No argument accepted:
  any argument raises `love.graphics.getDimensions: ...` naming the
  function. It does not look at the selected canvas.
- **Function-list guard.** `resize` joins the callback names (the guard's
  list in the fake, and the "six callbacks" sentence of design.md becomes
  seven). Module names unchanged.
- **`lua-guard`** (`tools/lua_guard.lua`): `resize` joins its callbacks,
  `getDimensions` joins its `graphics` functions (`graphics` stays glue-only).
- **design.md** (`docs/love-port/design.md`), same step:
  - "The LÖVE functions the game may use": add `getDimensions()` under
    Drawing (window size in LÖVE units, whatever canvas is selected) and
    `love.resize(w, h)` to Loop; seven callbacks.
  - "Fake disk and arguments" / replay members: `window` joins the
    members read, with its rule and default; frames hold `pad` and an
    optional `resize`, with its rules.
  - "Pad semantics" frame order: resize first, then the existing order;
    the no-call-at-start rule; the "must not depend on the order" sentence
    extended to resize.
  - The sentence on how an extension is verified is the one step 1 rewrote
    (H-1-1); this step adds nothing to it.
- **No change to the game** (`game/`): no module calls the new functions in
  this step; step 6/7 will.
- **No real-LÖVE capture** for these functions, per the plan's rule that
  plan 6's extensions are verified by fake C tests (H-1-1).

## Implementation Chunks

1. **Replay reader.** `window` and `resize` parsed, validated, queried;
   frame-key message updated. Unit tests in the replay reader's C test.
   The 20 e2e replays still read with their frame counts.
2. **Host and graphics.** Frame-input struct, window size in the host,
   `love.resize` called first on a resize frame, `getDimensions`, `resize`
   allowed by the function-list guard. CLI tests through a logging fixture
   game; the 20 replays unchanged (`make e2e`, `make end-state`).
3. **Static guard, contract and gate.** `lua-guard` lists and their test;
   design.md sections; timing gate run and medians recorded in plan 6 §6.

## Testing Decisions

- A good test drives the fake from the outside: a replay file and a tiny
  fixture game whose `debug_state()` returns a log, then an exact
  comparison of that log (the style of the joystick and function-list CLI
  tests). No test reads the host's internals.
- **Replay reader unit tests** (in the existing C test of the replay
  reader, parsing in-memory text): default 640×480 without `window`;
  `window` read; a resize on frame 0 and on a later frame, two resizes in
  a row; refusals — not an array, one or three elements, a non-integer,
  0, 4097, `window` given twice, `resize` equal to `window`, a second
  `resize` equal to the first, an unknown frame key, a frame with only
  `resize` (no `pad`). Each refusal asserts the frame and the problem in
  the message.
- **New CLI check** `test_fake_love_window` (its own make target in
  `FAKE_LOVE_CHECKS`, a binary in the fake-side lists), with a fixture game
  logging: `getDimensions` at the top of `main.lua`, in `love.load`, inside
  each `love.resize` (its arguments and `getDimensions`), in the pad
  callbacks, in `love.update` and `love.draw` (with a canvas selected once,
  to prove the answer ignores it), and in `debug_state`. Replays:
  - no `window`: 640×480 everywhere, no resize call;
  - `window` 390×844 with resizes to 844×390 on a frame that also presses a
    button and back to 390×844 two frames later: the log shows the size
    before and after, and on the resize frames the order
    `resize` → `pressed`/`released` → `update` → `draw`;
  - a resize on frame 0: `love.resize` precedes the first `update`;
  - a game without `love.resize`: the resize frame runs, `getDimensions`
    changes, nothing fails;
  - failures: `love.resize` a number (`love.resize is a number, not a
    function (f<N>)`), an error raised in `love.resize`
    (`love.resize f<N>:`, `main.lua:<line>:`, traceback), `getDimensions(1)`
    raising an error naming the function, and a refused replay (exit 1, no
    capture nor state file). The misuse variants pick their case from an
    environment variable, like the `pad_misuse` fixture.
  Prior art: the joystick CLI test and its `pad_log` / `pad_misuse`
  fixtures, the unlisted CLI test.
- **Function-list unit test**: `love.resize` can be created and reads nil
  before; another unknown callback name still fails.
- **lua-guard test**: a game file using `love.graphics.getDimensions` in
  glue and defining `love.resize` passes; `getDimensions` outside glue
  still fails as other graphics names do.
- **Regression**: `make e2e` (20/20 match) and `make end-state` (20/20).
- **Timing gate**: design.md's protocol (11 rounds, both baselines, ≤ 3 %),
  medians recorded in plan 6 §6. The new check's own time stays under the
  100 ms budget. `make e2e` time is not measured: the step adds no
  per-frame drawing work (not one of steps 4, 5, 7, 16).

## Hypotheses on step 1 (not done yet)

- **H-1-1**: step 1 rewrote design.md's sentence "extending it is a
  decision recorded here, with its real-LÖVE capture" so that plan 6's
  extensions are verified by fake C tests and screen tests; this step
  needs no real-LÖVE capture and does not touch that sentence.
- **H-1-2**: step 1 left the structure of design.md's function-list
  section, replay-members paragraph and pad-semantics paragraph as they
  are at HEAD (only the `setColor` wording and the colour fatal error
  changed), so this step edits those paragraphs in place.
- **H-1-3**: step 1 did not touch the replay reader, the host's frame
  order, `host_frame`'s signature, the function-list callback names or
  `tools/lua_guard.lua`.
- **H-1-4**: step 1 may have removed the colour switch
  (`host_allow_any_color` / the palette-or-white mode) and the fatal error
  it raised; this step depends on neither, and adds no new kind of fatal
  error (its refusals are replay refusals before `love.load` and ordinary
  Lua errors), whatever the numbering of design.md's "Fatal errors" list
  became.
- **H-1-5**: step 1's final commit is this step's "before" baseline for the
  timing gate; whatever check step 1 added is already in that baseline.
- **H-1-6**: the canvas 0 end-of-frame check is unchanged by step 1, so the
  fixture game still clears canvas 0 to opaque palette black in
  `love.load`, as the existing fixtures do.

## Out of Scope

- `touches` and `mouse` replay fields and the touch/mouse functions (step 3).
- The screen framebuffer, any present offset or scale, `screens` (step 4).
- `pad_sheet`, scaled quads, blending (step 5).
- Any game change: the layout module (step 6), `screen.present` and
  `conf.lua` (step 7).
- `love.window.*`, `getWidth`/`getHeight`, DPI scale, safe areas, and
  modelling `t.window` from `love.conf`.
- Adapting `tools/e2e_limits.sh` (frames still all carry `pad`).

## Further Notes

- Decided without a guideline agent: every question had an answer in the
  plan (§2 Fake LÖVE, Tests) or in the code. The choices of this PRD not
  dictated by the plan: the 1–4096 range, refusing an equal-size resize,
  resize before the pad callbacks, `pad` still required, `lua-guard`
  extended now rather than at step 6/7, and the frame-input struct.
- Real LÖVE's `love.resize` comes from SDL's size-changed event, which fires
  only on a change and not for the initial window: hence stories 13 and 21.
