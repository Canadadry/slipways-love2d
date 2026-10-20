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
- Nothing under `tools/fake_love/` or `tools/love_capture/`, nor any test or
  binary driving the fake, includes a header or links an object from `src/`:
  plan 4 deletes `src/` and keeps the fake. Checked in `make test` from step 6.
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
   and by the CI love-capture job staying green without `png_check`. Also
   adds the `make test` check that the fake uses nothing from `src/`.
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

### Step 2.5

- Reader: `initial_save` (hex), `bindings` (fixture name, ≤ 64 KiB),
  `attract_after` (1..INT_MAX); `start` and duplicates refused. `disk.c`:
  in-memory files over a read-only view of the game directory, never writing
  the real disk. `filesystem.c`: `love.filesystem.read`/`write`/`getInfo` with
  LÖVE 11.5 returns, nil in conf.lua. `love.load` gets
  `{"--attract-after", "N"}` or `{}`; global `arg` stays nil. design.md has
  "Fake disk and arguments".
- Tests: `test_fake_love_disk` (1 ms), `test_fake_love_files` (46 ms; disk
  echo game, `pad_unbound_enter_aim.json`, `attract.json`).
- `make test`: before d358c85 7.66 s, reference 7.64 s, current 7.65 s;
  paired −0.17 % / +0.06 %, 11 rounds, 4 cores.
- Phase 2 end: warn-review flagged repeated file/path helpers, `replay.c`
  (806 lines) copying the C reader's rules, and copied test helpers.
  plan-review judged them code cleanup; it added the "nothing from `src/`"
  rule and its check to step 6.

### Step 3.6

- `fake-love-guard` (in `make test`) fails on a fake-side object from `src/`,
  a `.d` naming a `src/` header, or a `src/` include; a planted
  `#include "p8/palette.h"` turned it red. `canvas.c` (16 RGBA canvases,
  component = float clamp ×255 rounded half to even), `graphics.c`
  (`newCanvas`/`setCanvas`/`clear`, nil in conf.lua; a canvas selected after
  draw is fatal, as real LÖVE).
- `tmp/love_capture_check` runs a capture program in the fake's host and
  compares all four channels of canvas 0 with the PNG; one
  `love-capture-check-<name>` per program; both harnesses call
  `program(canvas)`. `png_check` deleted; `love-check` is the LuaJIT probe.
  Nine programs (`clear`, `canvas_*`, `clear_*`) match; a second
  `make love-capture` changes no PNG.
- Proof: pixel (37, 81) of `clear_transparent.png` changed → `make test` red
  naming it; restored. CI run 39501734124 green without `png_check`.
- `make test`: before bee1cb3 7.53 s, reference 7.57 s, current 7.59 s;
  paired +1.30 % / +1.67 %, 11 rounds, 4 cores; new checks 4 ms each.

### Step 3.7

- `point.c` reproduces llvmpipe's single-precision pipeline (product and sum
  rounded apart, fused viewport, 1/256 snap half to even); any other fusion
  fails at the c + 1/512 ties measured in real LÖVE. `points` takes a list or
  one flat table, checks all before drawing, refuses pairs, colours, strings,
  NaN, infinities, |v| ≥ 2^20 and odd counts. Colour is opaque white until
  step 9.
- Six captures `points_*` match; design.md "Points": to light (x, y), call
  `points(x + 0.5, y + 0.5)`; integers light (x − 1, y − 1).
- Tests: `test_fake_love_point` (1 ms), `test_fake_love_points` (26 ms).
- `make test`: before ab98ab6 7.60 s, reference 7.60 s, current 7.72 s;
  paired +1.22 % / +1.92 %, 11 rounds, 4 cores.

### Step 3.8

- `point_find_span` (shared snap): far edge added in float, pixel i filled
  when its centre is in [lo, hi), floor division, clipped.
  `rectangle("fill", x, y, w, h)` only; refuses other modes, a sixth
  argument, non-numbers, NaN, infinity, |v| ≥ 2^20. guideline: keep the
  2^20 bound on each argument; the off-canvas capture's widths shrink to fit.
- Six captures `rectangle_*` match (459/1728/7023/5327/636/3366 pixels);
  design.md "Rectangles": pass integers; `rectfill(x0,y0,x1,y1)` is
  `rectangle("fill", min, min, |dx|+1, |dy|+1)`.
- Tests: `test_fake_love_rectangle` (~25 ms), span cases in
  `test_fake_love_point`.
- `make test`: before 9c0a220 7.89 s, reference 7.81 s, current 7.89 s;
  paired −0.01 % / +1.03 %, 11 rounds, 4 cores.

### Step 3.9

- Drawing colour: float, clamp, ×255 in float, truncated (0.5 → 127, unlike
  `clear`'s 128); kept for the whole run. `setColor(r, g, b)` or
  `(r, g, b, 1)`; table form, other counts, non-numbers, non-finite and any
  alpha but 1 refused (the game draws opaque colours only).
- Three captures `color_bytes`, `color_precision`, `color_state` match;
  design.md "Colour": pass `byte / 255`, never an alpha.
- Test: `test_fake_love_set_color` (~15 ms).
- `make test`: before c157572 7.61 s, reference 7.58 s, current 7.66 s;
  paired +0.09 % / +1.20 %, 11 rounds, 4 cores.

### Step 3.10

- `image_data.c`: RGBA8 up to 1024×1024, zeroed; `setPixel` stores
  clamp ×255 +0.5 floored in double (0.5 → 128, a third rule beside `clear`
  and `setColor`), `getPixel` returns `byte / 255`; coordinates floored.
  `image.c`: `love.image.newImageData(w, h)`, `ImageData:setPixel/getPixel`,
  nil in conf.lua, freed by the GC, `image_find_data()` for step 11.
  Stricter than LÖVE: no table form, strings, extra arguments, sizes > 1024.
- Captures `imagedata_rounding`, `imagedata_forms` match (values shown
  through `setColor` and 1×1 rectangles); design.md "Image data".
- Tests: `test_fake_love_image_data` (4 ms), `test_fake_love_image` (15 ms).
- `make test`: before c1ba7d8 7.55 s, reference 7.24 s, current 7.49 s;
  paired −1.69 % / +2.79 % (close to the cap), 11 rounds, 4 cores.

### Step 3.11

- `newImage(imagedata)` copies (refusing a translucent pixel, named);
  `newQuad(x, y, w, h, sw, sh)` six integers inside the image;
  `draw(image, quad, x, y)` integer positions, quad made for that image;
  alpha-00 texels skipped, alpha-FF written with the tint
  round(c × t / 255). No methods on Image or Quad; `love.graphics` has 9
  functions. Real LÖVE: default filter is linear, but integer draws give the
  same bytes under linear and nearest, so the fake refuses what the filter
  would change instead of modelling it.
- Captures `draw_quads`, `draw_tint` match; design.md "Images".
- Tests: `test_fake_love_image_rules` (2 ms), `test_fake_love_image_draw`
  (24 ms), tint cases in `test_fake_love_canvas`.
- `make test`: before 75aaf46 7.13 s, reference 7.03 s, current 7.26 s;
  paired +1.27 % / +2.24 %, 11 rounds, 4 cores.

### Step 3.12

- `png_file_decode`: a gate before stb_image (signature, chunk CRCs, IHDR
  first, 8-bit RGB/RGBA only, sides ≤ 1024, no unknown critical chunk, IEND,
  Adler-32, exact data length), since real LÖVE refuses broken files stb
  decodes; on every accepted file both give the same bytes, no gamma. 16-bit,
  grey and palette refused (the game's assets are 8-bit RGB).
  `love.image.newImageData(name)` reads through the fake disk.
- Capture `imagedata_files` reads fixtures in `tools/love_capture/files/`
  through a `files` symlink in each harness; matches. design.md "Image files".
- Tests: `test_fake_love_png_file` (5 ms), cases in `test_fake_love_image`.
- `make test`: the gate failed (pooled 22 rounds +3.27 % against the
  reference, +1.00 % against step 3.11's end). Cause: `float-guard`, the
  longest job, started after the fake checks. Rework: longest prerequisites
  first, capture checks without a shell. After: 5.04 s against 7.09 s
  (reference) and 7.40 s (before); paired −27.97 % / −31.37 %.

### Step 3.13

- `transform.c`: offset plus 128 saved (LÖVE's limit and messages).
  `push()`, `pop()`, `translate(dx, dy)` with integer offsets below 2^20;
  `push("all")`, fractions, strings, NaN refused. Offsets compose in single
  precision as real LÖVE (point at (float)x + (float)dx; span far edge
  ((float)x + (float)w) + (float)dx), matching 8 measured ties. Offset reset
  before each draw step, stack kept across frames.
- Capture `translate_stack` matches; design.md "Transforms".
- Tests: `test_fake_love_transform` (4 ms), `transform_log` inside
  `test_fake_love_set_color` (22 ms).
- `make test`: before 0b5a98a 5.02 s, reference 7.08 s, current 5.06 s;
  paired +0.33 % / −28.87 %, 11 rounds, 4 cores.

### Step 3.14

- `setDefaultFilter("nearest", "nearest")` only, storing nothing; other forms
  refused (unknown names with LÖVE's message). Real LÖVE: only objects created
  after the call get nearest; 31 of 31 captures re-run under nearest were
  byte-identical; fractional positions still differ (0.5: 7 249 pixels), so
  they stay refused. design.md "Default filter": call it first in `love.load`.
- Capture `filter_nearest` matches; misuses in the `image_draw_log` game.
- `make test`: before 434db23 5.19 s, reference 7.21 s, current 5.15 s;
  paired +0.62 % / −28.42 %, 11 rounds, 4 cores.

