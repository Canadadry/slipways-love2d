# Plan 1 — The fake LÖVE and its real-LÖVE captures

## 1. Goal

Build `tools/fake_love/`: a headless C program that runs a LÖVE game on a
replay and writes the same capture as the C replay, and prove it draws like
real LÖVE 11.5 through captures taken from real LÖVE. Without it, the Lua
game of plans 2–3 cannot be checked. The contract (functions, buttons, fake
disk, capture format, fatal errors) is in `docs/love-port/design.md`; read it first.

## 2. Rules

- The fake implements **exactly** the function list of `docs/love-port/design.md`;
  everything else is fatal with the function's name. A new function is added
  to that list and gets a capture program first.
- Pixel conventions come from real LÖVE 11.5, never from guesses: each drawing
  function has at least one capture program in `tools/love_capture/` whose PNG
  real LÖVE produced, and the fake must match it pixel for pixel.
- Captures read the canvas (`Canvas:newImageData():encode("png")`), never the
  window.
- Xvfb (with Mesa software rendering) is allowed **only** inside
  `make love-capture`, which runs the capture programs in real LÖVE 11.5,
  rewrites their PNGs and stops; no window is ever visible. Agents and CI may
  run it. It is not part of `make test` or `make e2e`. Step 1 writes this
  exception into `CLAUDE.md`, before any agent needs it.
- `make test` runs every capture program in the fake and compares with the
  PNGs. `CLAUDE.md` forbids making it slower: step 3 decides once how the
  fake's checks stay out of its wall time, and every step that adds to it
  records the time before and after in its step note. The median measured at
  the start of step 3 is the plan-wide reference: every "after" is also
  compared with it, under the same cap, so small per-step growth cannot add up.
- A capture program's drawing part uses only listed functions; the canvas
  read-back and PNG write run only in real LÖVE, so the same program runs
  in the fake.
- LuaJIT: system package, version checked at startup. PNG: vendored
  `stb_image.h`. Nothing else is vendored without a recorded decision.
- Each drawing step writes the convention its capture settled (where a
  point lands, how a rectangle rounds, off-canvas and transparent behaviour)
  into `docs/love-port/design.md`: plan 1 is deleted, design.md is what
  plans 2–3 read.
- The fake does not reuse the C game's drawing code: it only draws pixels,
  filled rectangles and images.
- At each phase end, every CI job is green on the last pushed commit, or the
  red job goes through `fix-bug` before the next phase.
- Not this plan: the Lua game.

## 3. Phases and steps

### Phase 1 — Real LÖVE in the container and CI

1. One committed install script for LÖVE 11.5, Xvfb, Mesa and the LuaJIT dev
   package, called by every `ci.yml` job that runs `make` and by a
   session-start hook, so the container
   and CI run the same versions; `make love-capture` runs one capture program
   that clears a canvas to colour 1 and writes its PNG. Verified by the PNG
   holding 160×120 pixels of `1D2B53`, by a C program built against the
   system LuaJIT printing its version, both in the container and in CI, and
   by the Xvfb exception being in `CLAUDE.md`. Step 1 also makes `ci.yml`
   run on pushes to the working branch (today: `master` and pull requests
   only); verified by a green CI run on the pushed commit.
2. CI job: `make love-capture` then `git diff --exit-code tools/love_capture/`.
   Verified by a deliberate change to a program turning the job red, then
   reverting it.

### Phase 2 — The fake's skeleton

3. `tmp/fake_love <game dir> <events.json> <capture.bin> [<state.txt>]`:
   embeds LuaJIT, loads `conf.lua` and `main.lua`, runs the loop of
   `docs/love-port/design.md` with no drawing yet. Verified by a test game that counts
   `update` calls and a 3-frame replay.
4. Pad: the button table, pressed/released callbacks, `isGamepadDown`,
   `getJoysticks`. Verified by a test game that logs callbacks against a
   replay holding several buttons across frames.
5. Fake disk and arguments: `save.bin` from `initial_save`, `bindings.cfg`
   from `bindings`, in-memory writes, `read`/`write`/`getInfo`,
   `--attract-after`. Verified by a test game that reads both files back and
   echoes `arg`.

### Phase 3 — Drawing, one function at a time

Each step uses only functions from earlier steps. Each step: a capture
program in `tools/love_capture/` exercising the function (including edge
cases: off-canvas, negative coordinates, transparent pixels, nested
translate), its PNG from `make love-capture`, the fake's
implementation, the comparison in `make test`, and the settled convention in
`docs/love-port/design.md`. Verified by the comparison passing.

6. `newCanvas`/`setCanvas`/`clear`, and the comparison harness in
   `make test` (run the program in the fake, read the PNG, compare pixel by
   pixel). The harness replaces `png_check` and the PNG half of
   `make love-check`. Also verified by one PNG pixel deliberately changed
   turning `make test` red with the program and pixel named, then reverted,
   and by the CI love-capture job staying green without `png_check`.
7. `points`.
8. `rectangle("fill")`.
9. `setColor`, shown through `points` and `rectangle`.
10. `newImageData`/`setPixel`/`getPixel`, verified by `getPixel`'s returned
    values in a test game (nothing reaches the canvas yet).
11. `newImage`, `newQuad` and `draw`: an image only shows through `draw`.
12. Image `newImageData(file)` from a PNG.
13. `push`/`pop`/`translate`.
14. `setDefaultFilter`.
15. One capture program combining the functions as the game will: colour
    before `draw`, `translate` over `points`, `rectangle` and `draw`,
    transparent image pixels over existing content, canvas switches.

### Phase 4 — Capture and errors

16. Capture: after each `love.draw()`, canvas → palette numbers → appended to
    the capture; `debug_state()` written after the last frame. Verified by a
    test game drawing known colours and the capture's bytes.
17. Fatal errors: unlisted function, colour outside the 16, foreign pixel on
    the canvas, each with its message. Verified by one test game per error.
18. `make love-e2e`: runs `tmp/fake_love game/` on every replay of
    `tests/e2e/`, in parallel, and checks each capture's sha256 with
    `tools/frames_check.sh`. Verified with a stub game drawing a fixed frame:
    all 20 replays run, each capture has the replay's frame count of frames,
    each reports a sha256 mismatch rather than a crash, and a game that fails
    to load reports a distinct, named error.

## 4. Done when

- Every function of the list has a capture program and its PNG, the fake
  matches each one in `make test`, and its convention is in
  `docs/love-port/design.md`.
- Every CI job is green.
- The `make test` median is within the cap of the step 3 reference.
- The combined capture program of step 15 matches.
- `make love-e2e` runs the fake on the 20 replays.
- `CLAUDE.md` carries the Xvfb exception.

## 5. Open questions

None.

## 6. Step notes

### Step 1.1

- `tools/love_install.sh` installs love 11.5-1build1, xvfb, xauth, Mesa
  (llvmpipe), luajit and libluajit-5.1-dev from Ubuntu 24.04 apt; it skips
  apt when everything is installed. Run by a SessionStart hook
  (`.claude/hooks/session_start.sh`, cloud sessions only) and by both CI jobs.
- Deviation: minimized images exclude `/usr/share/man` and love's postinst
  fails without its man page; the script re-includes it.
- `make love-capture` runs `tools/love_capture/*.lua` through a real-LÖVE-only
  harness in `tools/love_capture/harness/`; programs return a draw function
  using listed functions only. `make love-check` (outside `make test`) runs the
  LuaJIT probe and checks `clear.png` is 160×120 of `1D2B53`. `stb_image.h`
  v2.30 vendored in `tools/fake_love/`.
- Two local captures are byte-identical. CI triggers on `claude/**`; run
  39487833316 green on every job (love-capture job 23 s).

### Step 1.2

- The love-capture CI job, after `make love-capture`, fails when a file under
  `tools/love_capture/` differs from the commit or is not in it, naming the
  files; it got `timeout-minutes: 10`. Extends step 1's job, no new one.
- Proof: commit 75111d5 changed `clear.lua` to colour 2 without its PNG; run
  39489217080 went red on the drift check. The revert's run 39489901977 is
  green on every job (love-capture 24 s). Container and CI write identical
  PNG bytes.
- Phase 1 end: warn-review flagged 160×120, "11.5" and `1D2B53` repeated
  across files, and `png_check.c` using the `stb_image.h` of `tools/fake_love/`.
  plan-review: step 6 now retires `png_check`; `make test` time gets a
  plan-wide reference. The repeated constants are left: design constants and
  a runtime version guard.

### Step 2.3

- `tmp/fake_love` (`tools/fake_love/`: `replay.c` strict JSON reader keeping
  the whole document, `host.c` the LuaJIT host, `fake_love.c` the command)
  runs conf.lua, `love.conf`, main.lua, `love.load`, then `update(1/60)` and
  `draw` per frame; `require` only looks in the game directory. Capture
  stays empty. Deviation: `debug_state()` and the state file land here, not
  in step 16, as the only output a test can read yet.
- Tests: `test_fake_love_replay` (20 e2e frame counts, refusals),
  `test_fake_love` (counting game, 3-frame replay, 23 ms),
  `test_fake_love_failures` (every fatal error, 70 ms; split out to stay
  under 100 ms per check). Fixtures in `tests/fixtures/fake_love/`.
- `make test` time: each fake check is its own prerequisite of `test`
  (`FAKE_LOVE_CHECKS`), rule in design.md. guideline replaced the PRD's
  spread check (per-run spread ~4–8 %, never passes) with the median of
  per-round paired ratios over 11 rounds, cap 3 %. Plan-wide reference:
  2e7215c. After: medians 7.93 s (reference) / 7.87 s (current), paired
  +1.28 %, 4 cores.

### Step 2.4

- `pad.c` (button table, frame-to-frame changes), reader pad checks
  (`"input": "pad"`, frames `{"pad": [...]}` of distinct 1–12, refusals tagged
  `f<N>`; `replay_frame_held()`), `joystick.c` (`getJoysticks`,
  `isGamepadDown`). Per frame: releases, then presses, each ascending, then
  update and draw. `love.joystick` is nil in conf.lua. design.md has "Pad
  semantics".
- Tests: `test_fake_love_pad` (3 ms), reader cases incl. `pad_title.json`'s
  32 masks, `test_fake_love_joystick` (40 ms: 9-frame `pad_log` byte for
  byte, polling, misuse). Deviations: pad CLI cases in their own check to
  keep `test_fake_love_failures` (76 ms) under 100 ms; a non-function
  callback is only detected when called.
- `make test`: before 05ee35b 7.77 s, reference 2e7215c 7.67 s, current
  7.70 s; paired −0.92 % / +0.39 %, 11 rounds, 4 cores.

