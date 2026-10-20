---
title: "fake_love draws canvases and clears them, and make test compares every capture with real LÖVE"
description: "The fake LÖVE has no love.graphics and nothing compares it with the real-LÖVE captures, so no drawing convention can be proven and clear.png is only checked by a one-off tool outside make test."
status: needs-triage
---

## Problem Statement

After plan phase 2, `tmp/fake_love` runs a LÖVE game's loop, plays the pad
and gives the game its disk and arguments, but `love.graphics` does not
exist: a game cannot create a canvas, select it or clear it. Phase 3 adds
the drawing functions one at a time, and each must match real LÖVE 11.5
pixel for pixel. Nothing can prove that yet:

- `tools/love_capture/` holds one program, `clear.lua`, and its PNG from
  real LÖVE. Only `png_check`, a separate tool run by `make love-check`
  (outside `make test`), looks at the PNG, and it only knows "every pixel
  is one colour". It never runs the program anywhere but in real LÖVE.
- There is no way to run a capture program in the fake and read the
  canvas it drew: the fake's capture file stays empty until step 16, and
  even then it holds palette numbers, which cannot show an alpha channel.
- The conventions of the three functions the game will call first are
  nowhere written: what a fresh canvas holds, what `clear()` with no
  argument does, how a component like 0.3 becomes a byte, whether `clear`
  with alpha 0 keeps the colour, what happens when `love.draw()` ends with
  a canvas still selected.

If the harness is not built now, every later drawing step has nothing to
prove itself against, and plans 2–3 would find the conventions through e2e
mismatches.

## Solution

The fake gains `love.graphics` with exactly `newCanvas`, `setCanvas` and
`clear`, behaving as real LÖVE 11.5 does for the forms the game uses, and
stricter where it does not need more.

A new comparison command runs one capture program in the fake, through
the fake's own host and a fake-side harness that does what the real
harness does (create the 160×120 canvas, select it, call the program's
draw function, unselect it), reads the canvas's RGBA bytes, loads the
program's PNG and compares all 19 200 pixels, four channels each. A
mismatch names the program, the first differing pixel, both values and
the number of differing pixels.

`make test` runs one comparison per capture program, each its own target
in `FAKE_LOVE_CHECKS`, in parallel with the unit tests. A program without
a PNG, or a PNG without a program, fails its target.

The capture programs of this step pin the conventions of the three
functions: fresh canvas, `clear()` defaults, alpha 0, rounding ties,
clamping, and canvas switches. Their PNGs come from `make love-capture`.

`png_check` is deleted; `make love-check` keeps only the LuaJIT probe.

A new `make test` guard fails when anything on the fake side (its
sources, the capture tools, the tests and binaries that drive the fake)
includes a header or links an object from `src/`, which plan 4 deletes.
The conventions go into `docs/love-port/design.md`.

## User Stories

1. As a plan-2 developer, I want `love.graphics.newCanvas(160, 120)` to return a canvas object, so that the Lua game draws into a 160×120 canvas as in real LÖVE.
2. As a plan-2 developer, I want a fresh canvas to hold transparent black (every byte 0) everywhere, as real LÖVE's does, so that a frame I do not clear is predictable.
3. As a plan-2 developer, I want `newCanvas` with any other size, a missing size, non-number sizes or a settings argument to raise an error naming `love.graphics.newCanvas`, so that the fake never accepts a canvas the design does not list.
4. As a plan-2 developer, I want `setCanvas(canvas)` to make that canvas the target of later drawing, so that the game draws its frame off-screen.
5. As a plan-2 developer, I want `setCanvas()` and `setCanvas(nil)` to return drawing to the screen, so that the game can unselect its canvas before showing it.
6. As a plan-2 developer, I want `setCanvas` with anything but a canvas or nothing (a number, a table of canvases, a mipmap argument) to raise an error naming `love.graphics.setCanvas`, so that a slip is reported at once.
7. As a plan-2 developer, I want switching to another canvas to leave the previous one untouched, and switching back to draw into it again, so that several canvases behave as in LÖVE.
8. As a plan-2 developer, I want a canvas's content to persist across frames until it is cleared or drawn over, so that the fake behaves like LÖVE's GPU canvases.
9. As a plan-2 developer, I want `love.draw()` ending with a canvas still selected to be fatal with LÖVE's message (`present cannot be called while a Canvas is active.`) and the frame, so that a game the fake accepts does not crash real LÖVE.
10. As a plan-2 developer, I want that check made after the draw step of every frame, even when `love.draw` is not defined, as LÖVE presents every frame, so that a canvas left selected by `love.update` is caught too.
11. As a plan-2 developer, I want `clear(r, g, b, a)` to set every pixel of the selected canvas to that colour, so that each frame starts from a known background.
12. As a plan-2 developer, I want `clear(r, g, b)` to use alpha 1, so that the three-argument form is opaque as in LÖVE.
13. As a plan-2 developer, I want `clear()` to set every pixel to transparent black, as real LÖVE does, so that the no-argument form is defined.
14. As a plan-2 developer, I want `clear` with alpha 0 to keep the red, green and blue bytes, as real LÖVE does (no premultiplication), so that a transparent clear is reproduced byte for byte.
15. As a plan-2 developer, I want each component turned into a byte exactly as real LÖVE on Mesa does (single precision, clamped to 0–1, times 255, rounded to nearest with ties to even), so that values that are not `byte/255` still match.
16. As a plan-2 developer, I want `byte/255` values to come back as that exact byte, so that the palette passed by the game reaches the canvas unchanged.
17. As a plan-2 developer, I want `clear` with no canvas selected to clear only the screen, which the fake does not model, so that a game clearing the window before drawing its canvas runs in the fake.
18. As a plan-2 developer, I want `clear` with a wrong number of arguments, a non-number, a non-finite number or a table argument to raise an error naming `love.graphics.clear`, so that unsupported forms are not answered with a guess.
19. As a plan-2 developer, I want `love.graphics` to hold exactly `newCanvas`, `setCanvas` and `clear` after this step, so that the fake never offers a drawing function without its capture.
20. As a plan-2 developer, I want `love.graphics` absent while `conf.lua` runs and present from `main.lua` on, as `love.joystick` is, so that the fake stays stricter than LÖVE.
21. As a plan-1 developer, I want a command that runs one capture program in the fake and compares its canvas with the program's PNG pixel by pixel, so that each drawing step is proven against real LÖVE.
22. As a plan-1 developer, I want the program run through the fake's real host and binding, not a copy, so that the comparison proves the code the game will use.
23. As a plan-1 developer, I want the fake-side harness to do what the real harness does, in the same order (canvas created before the program is loaded, selected, draw function called with the canvas, unselected), so that the same program means the same thing on both sides.
24. As a plan-1 developer, I want the draw function to receive the harness canvas as its argument on both sides, so that a program can switch to another canvas and back.
25. As a plan-1 developer, I want the comparison to check all four channels of every pixel, so that transparency differences are caught.
26. As a plan-1 developer, I want a mismatch to name the program, the first differing pixel as `(x, y)`, the fake's and the PNG's RGBA in hex, and the count of differing pixels, so that I see at once what to fix.
27. As a plan-1 developer, I want a PNG of another size, a missing PNG, an unreadable PNG, a program that fails, or a program that does not return a function each reported with the program's name and a distinct reason, so that every failure is understandable.
28. As a plan-1 developer, I want every PNG encoding real LÖVE writes (1-bit grey with transparency, palette with transparency, RGB) read correctly, so that the comparison never fails on the file format.
29. As a plan-1 developer, I want `make test` to run one comparison per capture program, each its own target, in parallel, so that the fast loop proves every capture without getting slower.
30. As a plan-1 developer, I want a capture program without its PNG, or a PNG without its program, to fail `make test` with both names, so that a capture cannot be half-added or left stale.
31. As a plan-1 developer, I want capture programs for the fresh canvas, `clear()`, alpha 0, three-argument rounding ties, clamping and canvas switches, so that each convention of this step is settled by real LÖVE.
32. As a plan-1 developer, I want `clear.png` checked by the comparison instead of `png_check`, so that one mechanism checks every capture.
33. As a plan-1 developer, I want `png_check` deleted and `make love-check` reduced to the LuaJIT probe, with its help line saying so, so that no tool duplicates the comparison.
34. As a plan-1 developer, I want one PNG pixel deliberately changed to turn `make test` red with the program and pixel named, then reverted, so that the harness is proven to catch a one-pixel difference.
35. As the repository owner, I want the CI love-capture job to stay green without `png_check`, so that the capture workflow still guards drift.
36. As the repository owner, I want `make love-capture` to rewrite `clear.png` byte for byte unchanged after the real harness change, so that the harness change is proven harmless.
37. As the repository owner, I want `make test` measured before and after this step with the design's protocol and kept within its gate, so that the fast loop stays fast.
38. As a plans-2–3 reader, I want the canvas, `setCanvas`, `clear` and comparison conventions in `docs/love-port/design.md`, so that I do not need plan 1, which will be deleted.
39. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.
40. As the repository owner, I want `make test` to fail when any fake source, fake test or fake-driving binary includes a header from `src/`, so that plan 4 can delete `src/` without breaking the fake.
41. As the repository owner, I want `make test` to fail when any binary of the fake side links an object built from `src/`, naming the binary and the object, so that the rule covers link lines as well as includes.
42. As a plan-1 developer, I want the guard itself tested on a passing and a failing case, so that a broken guard cannot pass silently.

## Implementation Decisions

### What real LÖVE 11.5 does (explored while writing this PRD)

`make love-capture`, pointed at scratch programs, on LÖVE 11.5.0 with
Mesa llvmpipe, gave these canvas bytes (RGBA). They are the expected
answers of this step's capture programs; the committed PNGs remain the
proof.

- A fresh canvas, nothing drawn: `00000000` everywhere.
- `clear(1, 0, 0, 1)` then `clear()`: `00000000`.
- `clear(1, 0.5, 0.25, 0)`: `FF804000` (colour kept at alpha 0).
- `clear(0.1, 0.3, 0.5)`: `1A4C80FF` (alpha defaults to 1; 0.3 gives 76,
  not 77).
- `clear(2, -1, 0.3, 1.5)`: `FF004CFF` (clamped).
- All 255 ties `(k + 0.5) / 255`, k = 0..254, in four channels: each byte
  is `rint((float)v * 255.0f)` after clamping, ties to even (k = 1 gives 2,
  k = 2 gives 2). A round-half-up in double precision is wrong on 128 of
  them. Mesa's own float-to-unorm8 formula agrees with this on all 255.
- A program that selects another canvas and clears it red, then clears
  the screen green, then reselects the harness canvas it was given: the
  harness canvas keeps the blue it was cleared to before the switch.
- `love.draw()` returning with a canvas selected: LÖVE fails with
  `present cannot be called while a Canvas is active.`;
  `Canvas:newImageData` also refuses while its canvas is selected.
- `love.encode("png")` writes the smallest encoding: 1-bit grey with a
  transparency chunk for a fully transparent canvas, a palette with
  transparency otherwise; stb_image expands both to RGBA.

### Modules

- **Canvas store** (new deep module, pure C, no Lua, in the fake's
  sources). It owns every canvas's RGBA8 pixels (160×120×4 bytes, all 0
  at creation) and the current target. Interface:
  - create and free the store;
  - create a canvas: returns its handle (a small index, in creation
    order, the first being 0), or a "too many" result past 16 canvases.
    The game needs one; the limit keeps a canvas-per-frame bug from
    eating memory, and the binding turns it into an error naming
    `newCanvas`;
  - select a canvas as target, or none (the screen);
  - the current target (a handle, or none);
  - clear the current target with four bytes; with no target it does
    nothing (the screen is not modelled);
  - read a canvas's pixels: a pointer owned by the store, row-major,
    valid until the store is freed;
  - convert one colour component (a double) to a byte: to single
    precision, clamp to [0, 1], multiply by 255 in single precision,
    round to nearest with ties to even. This function is shared by every
    later colour-taking function (`setColor`, step 9).
- **Graphics binding** (new module, the only one that knows both Lua and
  the store, laid out like the joystick binding). It installs
  `love.graphics` holding exactly `newCanvas`, `setCanvas` and `clear`,
  as C closures over the store. A canvas is a full userdata holding its
  handle, with a metatable of its own and no methods; `setCanvas` checks
  that metatable. Accepted forms:
  - `newCanvas(160, 120)` — exactly two numbers equal to 160 and 120;
  - `setCanvas(canvas)`, `setCanvas()`, `setCanvas(nil)`;
  - `clear()`, `clear(r, g, b)`, `clear(r, g, b, a)` with finite numbers.

  Everything else raises a Lua error starting with the function's full
  name (`love.graphics.newCanvas: ...`) and saying what was wrong. A
  missing `love.graphics` function stays a plain Lua error until step 17
  names it.
- **LÖVE host** (extended):
  - owns the canvas store; installs the graphics binding after
    `love.conf(t)` and before `main.lua`, next to the joystick and
    filesystem bindings (H-5-3), so `love.graphics` is nil in `conf.lua`;
  - after the draw step of each frame (whether or not `love.draw` is
    defined), fails when a canvas is selected, with the error
    `love.draw f<N>: present cannot be called while a Canvas is active.`
    through the existing fatal path;
  - gains one read accessor: the pixels and size of the canvas with a
    given creation index, or none when fewer exist. The comparison reads
    canvas 0. Step 16 decides how the capture finds the game's canvas.
- **PNG reader** (new small module, pure C): loads a PNG file into newly
  allocated RGBA bytes with its width and height (the caller frees them),
  or returns a reason. It is the only translation unit that compiles
  `stb_image.h` (PNG only, no HDR, no linear), so the vendored header's
  warnings are relaxed for that one file and nowhere else. Step 12 reuses
  it for `newImageData(file)`.
- **Comparison command** (new binary, built as `tmp/love_capture_check`,
  linked with the host, its bindings, the disk, the canvas store and the
  PNG reader; not with the replay reader or the command of
  `tmp/fake_love`). Usage: `<harness dir> <program.lua> <program.png>`.
  It:
  1. opens a host on the fake-side harness directory, with a disk over
     that directory (H-5-1);
  2. loads it with the argument list `{ <program.lua> }` (H-5-2);
  3. runs one frame with nothing held;
  4. reads canvas 0 and the PNG, and compares size, then every pixel's
     four bytes in row-major order.

  Exit 0 with nothing on stderr when all match. Exit 1 with one line,
  `love_capture_check: <program.lua>: <reason>`, otherwise. Reasons:
  `pixel (x, y) is RRGGBBAA in the fake, RRGGBBAA in <png> (N pixels
  differ)`, `<png> is WxH, the canvas is 160x120`, `<png>: <reader
  reason>` (missing or undecodable), `no canvas was created`, or the
  host's error (a Lua error in the program, a program that does not
  return a function, a canvas left selected). Exit 2 for a usage error.
- **Fake-side harness** (a small LÖVE game directory beside the fake's
  sources, only `main.lua`, no `conf.lua`). `love.load(args)` creates the
  canvas, then loads `args[1]` with `loadfile` and calls it, checking it
  returned a function. `love.draw()` selects the canvas, calls the draw
  function with the canvas, unselects it. It uses no unlisted function.
- **Real harness** (changed): it creates the canvas before loading the
  program and calls `program(canvas)`, mirroring the fake side. The
  environment check, the read-back and the PNG write stay real-only.
  `clear.png` must come out byte-identical.
- **Nothing-from-`src/` guard** (new, plan rule "Nothing under
  `tools/fake_love/` or `tools/love_capture/`, nor any test or binary
  driving the fake, includes a header or links an object from `src/`").
  One Makefile variable lists every fake-side binary's objects: those of
  `tmp/fake_love`, `tmp/love_capture_check` and every `test_fake_love*`
  and comparison test, helpers included (each link rule names its own
  object-list variable and the guard's list is their union).
  `tmp/luajit_version`, built straight from its source, is covered by
  the include scan below. A target `fake-love-guard`, a prerequisite of
  `test` (not of `e2e`), depends on those objects and runs a small shell
  guard script on them that fails, naming the offender, when:
  - an object's source lies under `src/`;
  - an object's compiler dependency file (the `.d` the build already
    writes) lists a header under `src/`. This catches includes written
    through `-Isrc` or relative paths alike, without changing `CFLAGS`;
  - a source under `tools/fake_love/`, `tools/love_capture/` or a fake
    test has an `#include` naming `src/`, `../src` or a `src/` directory
    (`p8/`, `engine/`, `game/`, `platform/`, `devtools/`, `cmd/`), so a
    file not yet compiled is caught too.
  Dropping `-Isrc` for fake objects was rejected: `CFLAGS` adds it with
  `override` for the whole build, and the dependency-file check proves
  the same thing without forking the flags. The guard runs in one
  process over files the build already made, well under 100 ms.
- **Makefile**:
  - one target per capture name, the union of the `.lua` and `.png`
    stems in `tools/love_capture/`, generated from a pattern (no recipe
    loops over programs); each runs the comparison on its pair, or fails
    naming the missing half (`<name>.png missing: run make
    love-capture`, `<name>.lua missing for <name>.png`). Each target
    depends on the comparison binary, its program, its PNG and the
    fake-side harness, and is listed in `FAKE_LOVE_CHECKS`;
  - the new pure-C modules join the fake's plain-C source list, the
    binding joins the LuaJIT-flagged list (H-5-4);
  - `png_check` and its rule are deleted; `love-check` runs only the
    LuaJIT probe and its `## ` help says so (at most 20 words).

### Capture programs of this step

Each is a whole-canvas colour, since `clear` has no coordinates. The
plan's "off-canvas, negative coordinates, nested translate" edge cases do
not apply to these functions; `clear` under `translate` and under
`setColor` belongs to steps 13 and 9, which add those functions.

| program | draws | expected RGBA |
|---|---|---|
| `clear` (kept) | `clear(0x1D/255, 0x2B/255, 0x53/255, 1)` | `1D2B53FF` |
| `canvas_fresh` | nothing | `00000000` |
| `clear_default` | `clear(1, 0, 0, 1)` then `clear()` | `00000000` |
| `clear_transparent` | `clear(1, 0.5, 0.25, 0)` | `FF804000` |
| `clear_rounding` | `clear(0.1, 0.3, 0.5)` | `1A4C80FF` |
| `clear_ties` | `clear(0.5/255, 1.5/255, 2.5/255, 254.5/255)` | `000202FE` |
| `clear_clamp` | `clear(2, -1, 0.3, 1.5)` | `FF004CFF` |
| `canvas_switch_away` | clear blue; select a second canvas (created at the program's top level), clear it red; select none, clear green | `0000FFFF` |
| `canvas_switch_back` | select a second canvas, clear it red; select the harness canvas again, clear green | `00FF00FF` |

The expected values are what the PNGs from `make love-capture` must hold;
if a PNG disagrees, the PNG wins and the convention in design.md follows
it.

### Conventions written into `docs/love-port/design.md`

A "Canvas and clear" paragraph after step 5's "Fake disk and arguments"
paragraph (H-5-6), holding: `newCanvas(160, 120)` only, fresh canvas
transparent black, at most 16 canvases; the `setCanvas` forms; the
end-of-draw rule; the `clear` forms, defaults, colour kept at alpha 0,
clamping, the byte conversion with its tie rule; `clear` with no canvas
selected touches nothing the fake models. The "Real-LÖVE captures"
paragraph gains the harness contract (canvas created first and passed to
the draw function, one draw call, all four channels compared, one `make
test` target per program, a lone `.lua` or `.png` fails) and says
`make love-check` is the LuaJIT probe only.

### Decisions taken without the plan

- **The comparison runs in-process, not through `tmp/fake_love`.** The
  capture file holds palette numbers from step 16 and cannot show alpha;
  adding a hidden mode to the game-facing binary would widen its
  contract. A second binary over the same host and bindings proves the
  same code.
- **The program path reaches the fake-side harness through `love.load`'s
  argument list**, as it reaches the real harness: no extra host
  operation, no global.
- **Canvas 0 is the harness canvas.** The harness creates it before the
  program's top level runs, so the creation index identifies it without
  a Lua reference crossing into C.
- **The draw function receives the canvas on both sides.** It costs one
  argument and lets a program switch back to the harness canvas, which
  step 15 needs.
- **Strict forms.** No table colours, no `setCanvas` tables or mipmaps,
  no other canvas sizes, no non-finite components: none is needed by the
  port, each would widen the contract.
- **The screen is not modelled.** `clear` with no canvas selected is
  accepted and changes nothing the fake reads; whether the fake checks
  anything else drawn to the screen is decided by the step that adds that
  drawing.
- **Exact RGB at alpha 0 is compared.** Real LÖVE keeps it; comparing
  three channels when alpha is 0 would hide a difference that a later
  `draw` of that canvas could show.

## Implementation Chunks

1. **Nothing from `src/`.** The `fake-love-guard` target, its script and
   its test, in `make test`; existing fake tests and binaries pass it
   (or are fixed). Stories 40–42.
2. **Canvas store.** The pure-C module, the byte conversion, and its unit
   test. Stories 2, 7, 8, 11–17 (store side).
3. **`love.graphics` in the host.** The binding, its install after
   `love.conf`, the end-of-draw check, the canvas accessor, the CLI tests
   on fixture games. Stories 1, 3–6, 9, 10, 18–20.
4. **The comparison.** The PNG reader, the comparison command, both
   harnesses, the test PNG encoder, its CLI test, the per-program Makefile targets in
   `FAKE_LOVE_CHECKS`, the lone-file failures; `clear.lua` passes.
   Stories 21–30, 32, 36.
5. **Captures, retirement, docs, proof.** The eight new capture programs
   and their PNGs from `make love-capture`, `png_check` deleted and
   `love-check` reduced, design.md, the deliberate one-pixel change and
   its revert, the timing, the CI run, the step note. Stories 31, 33–35,
   37–39.

## Testing Decisions

- A good test drives a public interface and checks external behaviour:
  the store's bytes, the binary's exit status, stderr line and output
  files. No test reaches into the Lua state or the store's layout.
- **Canvas store, unit test** (new, pure C, linked with the store only,
  joins the unit list like the pad test):
  - a new canvas is 160×120 of `00000000`; handles count from 0; the 17th
    creation is refused;
  - clearing with no target changes no canvas; clearing the target fills
    it entirely and leaves the other canvas untouched; selecting back and
    clearing again works;
  - byte conversion: `k/255` gives `k` for every k; `(k + 0.5)/255` for
    k = 0, 1, 2, 127, 254 gives 0, 2, 2, 128, 254; 0.1, 0.3, 0.5 give 26,
    76, 128; -1, 2, -0.0 give 0, 255, 0.
- **Graphics binding, CLI test** (new, its own target in
  `FAKE_LOVE_CHECKS`, through `tmp/fake_love` and the shared CLI helper,
  with fixture games and step 4's replay format):
  - a game whose `conf.lua` logs `love.graphics == nil` and whose
    `love.load` logs the sorted names of `love.graphics` (`clear
    newCanvas setCanvas`) and `type(newCanvas(160, 120))` (`userdata`),
    selects and unselects it, clears with each form, and returns the log
    from `debug_state()`; compared byte for byte;
  - fatal cases, each exit 1 naming the function: `newCanvas(100, 100)`,
    `newCanvas()`, `newCanvas(160, 120, {})`, `setCanvas(5)`,
    `setCanvas({canvas})`, `clear(1, 2)`, `clear("1", 0, 0)`,
    `clear(0/0, 0, 0)`, `clear({1, 0, 0})`, the 17th canvas;
  - a game leaving its canvas selected at the end of `love.draw` exits 1
    with `f0` and LÖVE's present message; the same from `love.update`
    with no `love.draw`;
  - if over 100 ms, the fatal cases move to their own target, as step 3
    split its failures.
- **Comparison command, CLI test** (new, its own target in
  `FAKE_LOVE_CHECKS`). Fixture programs in the fake's fixtures; expected
  PNGs written at test time into `tmp/` by a small RGBA PNG encoder in
  the fake-side test helpers (stored deflate blocks, its own CRC-32 and
  Adler-32, about 60 lines), not the repository.s PNG writer, which lives
  in `src/` and is forbidden to the fake (plan rule). No binary fixture is
  committed. The encoder is checked by decoding its output with the
  fake.s PNG reader in the same test:
  - a program clearing to `1D2B53` against a uniform `1D2B53` PNG: exit
    0, empty stderr;
  - the same PNG with pixel (37, 81) changed: exit 1, stderr names the
    program, `pixel (37, 81)`, both values and `1 pixels differ` (exact
    wording the implementer's; the test matches the program, the
    coordinates and both hex values);
  - a PNG equal to the canvas except pixel (0, 0) alpha 0 instead of
    255: exit 1 naming `pixel (0, 0)` (alpha is compared);
  - a 16×12 PNG: size reason; a missing PNG and a text file named `.png`:
    reader reason with the path; a program raising an error, and one
    returning a number: the host's error with the program's name; a
    program leaving another canvas selected: the present message;
  - usage error: exit 2.
- **Guard, test** (new C test driving the guard script, as
  `test_float_guard` does): on fixture objects and dependency files in
  `tmp/`, a clean set passes; an object from `src/`, a dependency file
  listing `src/p8/palette.h`, and a fake source including
  `"game/planet.h"` each fail with the file named.
- **The capture checks themselves** are the tests of the conventions:
  each program of the table above passes in `make test`. Transparency
  and the PNG encodings real LÖVE writes are covered there, by real PNGs.
- **Prior art:** `test_fake_love`/`test_fake_love_failures` and the
  shared CLI helper; `test_fake_love_pad` for a pure-C fake module;
  `test_float_guard`/`test_alloc_guard` for a guard script driven by a C
  test on fixtures; `test_png_writer` only as a reading reference (the
  fake side does not link it).
- **Proofs recorded in the step note:**
  - one byte of one pixel of a committed PNG changed by hand, `make
    test` red with the program and pixel named (output quoted), the
    change reverted;
  - `make love-capture` leaves every PNG byte-identical (`git diff`
    empty), `clear.png` included;
  - the CI run on the pushed commit, love-capture job green without
    `png_check` (run id);
  - `make test` before and after with the design's protocol: 11 rotated
    paired rounds against the step's "before", bee1cb3 (H-5-5) and against
    `2e7215c`, both ≤ 3 %; plain and paired medians, spreads, rounds,
    core count, and the slowest new check's own time (budget 100 ms).
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged.

## Out of Scope

- Every other drawing function (`points`, `rectangle`, `setColor`,
  images, quads, `draw`, `push`/`pop`/`translate`,
  `setDefaultFilter`): steps 7–14.
- `clear` under a transform or a colour: steps 13 and 9 add the capture.
- Canvas methods (`newImageData`, `getWidth`, …), `getCanvas`, canvas
  formats, MSAA, stencil and depth forms of `clear`.
- Drawing to the screen beyond `clear`, and drawing a canvas onto the
  screen: the fake reads the canvas, per design.md.
- The capture file's bytes and the choice of which canvas the capture
  reads: step 16.
- Named fatal errors for unlisted functions: step 17.
- Audio, always.

## Hypotheses

These were written while plan step 5 was being implemented. Step 5 is now
committed (note at 40e184b) and the coordinator confirmed all eight:
`host_open(host, game_dir, disk)` with the caller owning the disk,
`host_load(host, arguments, count)`, `love.filesystem` installed after
`love.conf` and the joystick, before `main.lua`; `FAKE_LOVE_LUA_SRCS`
lists the Lua-binding sources; the "before" baseline is bee1cb3, step
2.5's last code commit. They are kept for traceability.

- **H-5-1**: step 5 changed the host's open operation to take a fake disk
  besides the game directory; the caller owns the disk, which outlives
  the host. The comparison command creates a disk over the fake-side
  harness directory with no in-memory file. If the disk is optional, it
  passes none.
- **H-5-2**: step 5 changed the host's load operation to take a list of
  argument strings, pushed as a new sequence table for `love.load(arg)`.
  The comparison passes `{ <program.lua> }`. If the shape differs, the
  command uses whatever step 5 offers to pass one string to
  `love.load`; the harness reads `args[1]` either way.
- **H-5-3**: step 5 installs `love.filesystem` after `love.conf` and
  before `main.lua`, next to the joystick binding. The graphics binding
  is installed at the same point.
- **H-5-4**: step 5 added the fake disk to the Makefile's plain-C source
  list of the fake and the filesystem binding to the LuaJIT-flagged list.
  The new modules join the same lists, and the comparison binary links
  the same objects as `tmp/fake_love` minus its command and the replay
  reader.
- **H-5-5**: step 5's note records its timings and its last commit is
  this step's "before" baseline (confirmed: bee1cb3); its new CLI checks are in
  `FAKE_LOVE_CHECKS`.
- **H-5-6**: step 5 wrote a "Fake disk and arguments" paragraph in
  `docs/love-port/design.md` after "Pad semantics"; this step's paragraph
  follows it.
- **H-5-7**: step 5 kept the fatal path: a host error prints
  `fake_love: <message>`, exits 1 and leaves no capture or state file.
  The end-of-draw check uses it.
- **H-5-8**: step 5 left the per-frame host call unchanged (frame index
  and held mask), `stb_image.h`, `png_check` and `love-check` untouched,
  and the global `arg` nil.

## Further Notes

- The plan's "the harness replaces `png_check` and the PNG half of
  `make love-check`" is met by the `clear` comparison: it checks
  `clear.png` is 160×120 of `1D2B53FF` and, beyond `png_check`, that the
  fake draws it.
- The exploration programs used for the table above lived in a scratch
  directory passed to `make love-capture`; nothing of them is committed.
- The 100 ms budget is per check; each capture comparison is one process
  starting LuaJIT, running one frame and decoding one small PNG, well
  under it. Nine capture checks run in parallel on cores the unit tests
  leave idle; if the gate fails, design.md rule 5 applies.
