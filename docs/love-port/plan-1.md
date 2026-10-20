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
  run it. It is not part of `make test` or `make e2e`. This exception goes
  into `CLAUDE.md`.
- `make test` runs every capture program in the fake and compares with the
  PNGs; it stays fast.
- LuaJIT: system package, version checked at startup. PNG: vendored
  `stb_image.h`. Nothing else is vendored without a recorded decision.
- The fake does not reuse the C game's drawing code: it only draws pixels,
  filled rectangles and images.
- Not this plan: the Lua game.

## 3. Phases and steps

### Phase 1 — Real LÖVE in the container and CI

1. Install LÖVE 11.5, Xvfb and Mesa in the container and in CI (setup script
   and `ci.yml`); `make love-capture` runs one capture program that clears a
   canvas to colour 1 and writes its PNG. Verified by the PNG holding 160×120
   pixels of `1D2B53`.
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

For each of: `newCanvas`/`setCanvas`/`clear`, `setColor`, `points`,
`rectangle("fill")`, `newImageData`/`setPixel`/`getPixel`, `newImage`,
image `newImageData(file)` from a PNG, `newQuad` and `draw`, `push`/`pop`/
`translate`, `setDefaultFilter`:

6. A capture program in `tools/love_capture/` exercising the function
   (including edge cases: off-canvas, negative coordinates, transparent
   pixels, nested translate), its PNG from `make love-capture`, the fake's
   implementation, and the comparison in `make test`. One step per function.
   Verified by the comparison passing.

### Phase 4 — Capture and errors

7. Capture: after each `love.draw()`, canvas → palette numbers → appended to
   the capture; `debug_state()` written after the last frame. Verified by a
   test game drawing known colours and the capture's bytes.
8. Fatal errors: unlisted function, colour outside the 16, foreign pixel on
   the canvas, each with its message. Verified by one test game per error.
9. `make love-e2e`: runs `tmp/fake_love game/` on every replay of
   `tests/e2e/`, in parallel, and checks each capture's sha256 with
   `tools/frames_check.sh`. Verified by it running (all red is expected: there
   is no game yet).

## 4. Done when

- Every function of the list has a capture program and its PNG, and the fake
  matches each one in `make test`.
- CI's capture job is green.
- `make love-e2e` runs the fake on the 20 replays.
- `CLAUDE.md` carries the Xvfb exception.

## 5. Open questions

None.

## 6. Step notes
