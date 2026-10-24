---
title: "The pad sheet: game/pad.png, its quad table, the flat sheet and the static sheet test"
description: "The virtual pad has no art yet: no sprite sheet, no table that cuts it, no flat-colour test sheet, and nothing that would notice if the art or its cutting changed."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) draws the virtual pad entirely from sprites of
`game/pad.png`, cut by a Lua quad table (§2 "Look"): a stick base, a stick
knob, and for each of the 8 buttons (A, B, X, Y, L, R, Start, Back) a
released and a pressed sprite, labels included. The tests of the pad
(steps 10–16) use a flat-colour stand-in, `tests/fixtures/pad_flat.png`,
served by the fake in place of `pad.png` through the replay's `pad_sheet`
member (step 1.5), so that changing the look never breaks a layout test.

Today none of this exists: no `pad.png`, no quad table, no flat sheet. And
nothing would catch a redrawn sheet whose sprites moved, a quad table that
cuts the wrong rectangle, or a flat sheet whose pressed and released sprites
share a colour (step 16 could then not tell a pressed button from a
released one). Nobody may draw the art by hand (no human in the loop), and
the art must be reproducible.

## Solution

1. **A committed generator, `tools/pad_art.c`**, built as `tmp/pad_art` and
   run by a new target `make pad-art` (outside `make test` and
   `make test-all`), writes three files deterministically, byte for byte
   the same on every run:
   - `game/pad.png`, the real sheet (first art): pixel-art sprites in the
     PICO-8 palette, labels drawn with the game's own `font.png` glyphs,
     translucent bodies so the game shows through in overlay mode;
   - `tests/fixtures/pad_flat.png`, the flat sheet: the same size and the
     same sprite rectangles, each sprite one solid opaque colour of its own,
     every colour distinct (so pressed ≠ released);
   - `tests/fixtures/pad_sheet_expected.png`, the static test's expected
     frame: every sprite of the real sheet composited at a fixed position
     on black, as design.md's screen rule draws it.
   The art is "committed, reproducible means": the generator is the source;
   the PNGs are committed outputs, never edited by hand, regenerated on
   purpose with `make pad-art` when the art changes.
2. **The quad table, `love.pad_sheet`** (`game/love/`, the LÖVE side): the
   sheet size and the rectangle of every sprite as plain data, and a
   `load()` that reads `pad.png` once and returns the Image and one Quad per
   sprite. Nothing in the game calls it yet (step 16 does, in `love.load`),
   so the 20 replays cannot change.
3. **The static sheet test**, one new fake run inside the existing check
   `test_fake_love_image_draw` (no new process, plan §6 step 2.6's budget):
   a fixture game requires the real `love.pad_sheet`, loads the real
   `pad.png`, draws every sprite at a fixed position onto a 240×84 screen,
   and the test compares the screen PNG exactly with the committed expected
   frame. The same run reports the quad table through `debug_state`, and the
   test checks on `pad_flat.png` that every sprite rectangle is one solid
   opaque, non-black colour and that the 18 colours are distinct.
4. Docs: design.md ("The asset PNGs", "Layout of the fork", "How the Lua game
   draws") and the README's target list name the sheets, the generator,
   `make pad-art` and the test.

## User Stories

1. As a player on an iPhone, I want the pad drawn from real sprites, so that it looks like a game pad and not like debug rectangles.
2. As a player, I want each button labelled (A, B, X, Y, L, R, START, BACK), so that I know what I press.
3. As a player, I want A, B, X and Y in their usual Xbox colours (green, red, blue, yellow), so that the diamond reads at a glance.
4. As a player, I want a pressed button to look clearly different from a released one, so that I see my press register.
5. As a player in overlay mode (desktop, landscape), I want the pad's bodies translucent, so that the game stays visible under the pad.
6. As a player, I want the sprites in the game's pixel-art style and palette, so that the pad matches the game.
7. As a player, I want a stick base and a knob, so that the floating stick of later steps has something to show.
8. As a maintainer, I want the art produced by a committed program, so that nobody has to draw it and it can be rebuilt at will.
9. As a maintainer, I want the generator to write identical bytes on every run, so that regenerating unchanged art leaves `git diff` empty.
10. As a maintainer, I want one command, `make pad-art`, that rewrites the real sheet, the flat sheet and the expected frame together, so that they never drift apart after an art change.
11. As a maintainer, I want `make pad-art` outside `make test` and `make test-all`, so that no check runs a generator.
12. As a maintainer, I want a one-line `## ` help for `pad-art` in `make help`, so that the target is discoverable.
13. As a maintainer, I want the generator to use only the fake's own PNG module and C, so that no Python, ImageMagick or font tool is needed.
14. As a maintainer, I want the labels drawn from `game/font.png`, so that the pad's lettering is the game's own font.
15. As a maintainer, I want the generator to use integer arithmetic for its shapes, so that no floating-point difference between machines changes a byte.
16. As a developer, I want the sprite rectangles held once in `love.pad_sheet`, so that the layout and drawing code of later steps never hard-code a sheet position.
17. As a developer, I want `love.pad_sheet.load()` to return an Image and a Quad per sprite, so that step 16 draws a sprite with one `draw` call.
18. As a developer, I want the buttons of the quad table keyed by their LÖVE gamepad names (`a`, `b`, `x`, `y`, `leftshoulder`, `rightshoulder`, `start`, `back`), so that steps 12–16 map a button bit to its sprite without a second table.
19. As a developer, I want the module's top level to be pure data, so that `lua-test-modules` can load it under plain LuaJIT with no `love`.
20. As a developer, I want `pad.png` named only inside `love.pad_sheet`, so that the completeness check of step 8 sees one literal and the file it names.
21. As a test author (step 16), I want `pad_flat.png` to have exactly the real sheet's size and rectangles, so that it can be served as `pad.png` and cut by the same quads.
22. As a test author, I want every flat sprite one solid opaque colour, so that an expected frame is a set of filled rectangles.
23. As a test author, I want every flat colour distinct and non-black, so that a missing, misplaced or wrong-state sprite shows on a black screen.
24. As a test author, I want a pressed flat sprite to differ from its released one, so that a press is visible in a screen test.
25. As a test author, I want the flat colours stated by one rule in design.md, so that I can build an expected frame without reverse-engineering the PNG.
26. As a reviewer, I want every sprite of the real sheet drawn at a fixed position and compared exactly with a committed PNG, so that any change to the art or to the quad table fails `make test`.
27. As a reviewer, I want that expected PNG produced by the generator, independently of the quad table, so that the test checks the Lua table against the art rather than against itself.
28. As a reviewer, I want the sprites drawn in an order different from the sheet's, so that a transposed or swapped quad shows as a wrong frame.
29. As a reviewer, I want the flat sheet's properties checked in `make test`, so that a regenerated flat sheet cannot silently lose them.
30. As a reviewer, I want the static test to fail with the first differing pixel, so that a failure says where the art or the cutting moved.
31. As a reviewer, I want the static test inside an existing fake check, so that `make test` gains no process.
32. As a reviewer, I want its cost measured and the timing gate passed, so that `make test` stays as fast as before the plan.
33. As a reviewer, I want the 20 replays' `*.sha256` and `*.end-state` unchanged, so that this step is proven to change no game behaviour.
34. As a reviewer, I want `lua-guard` (with step 8's completeness rule) green with the new module and PNG, so that the archive for the phone stays complete.
35. As a phone tester, I want `make love-file` to ship `pad.png` with no change to the target, so that the archive is ready for step 16.
36. As a later maintainer, I want design.md to say the pad sheets are generated and never edited by hand, so that I change the generator, not the PNG.
37. As a later maintainer, I want design.md to say how to regenerate after an art change, so that a red static test after a redraw is fixed in one command.
38. As an implementer of step 10, I want each sprite authored at one texel per reference unit, so that a layout rectangle the sprite's size draws it at the game's own pixel scale.

## Implementation Decisions

**Hypotheses on step 8 (not yet done; its PRD is
`docs/prd/triage/phase2-step8-love-file-and-completeness-check.md`).** The
implementer checks each first; one that fails is reported, and the decision
resting on it is revisited.

- **H-8-1**: step 8 is committed and `make test-all` is green on it; its
  last commit is this step's "before" commit for the timing gate.
- **H-8-2**: step 8's completeness rule in `lua-guard` accepts a short
  string literal `"pad.png"` in a `game/` file when `game/pad.png` exists,
  and it accepts a module with no `require` at all; so `love.pad_sheet`
  passes with no guard change, provided `game/pad.png` is committed in the
  same commit as the module or before it.
- **H-8-3**: the rule reads only the files `lua-guard` already reads (the
  `game/` wildcard), so the fixture game under `tests/fixtures/pad_sheet/`
  (its `"pad.png"` literal, its symlinked `love/`) is not checked and needs
  nothing from the guard.
- **H-8-4**: `make love-file` zips every file under `game/` and rebuilds the
  archive when any of them changes; `pad.png` ships with no Makefile change,
  and `make love-file` still passes after this step (no replay loads
  `pad.png` yet, so `pad_title.json`'s hash is unchanged).
- **H-8-5**: step 8 left unchanged what this step uses:
  `tests/test_fake_love_image_draw.c`, `tests/expected_screen.{c,h}`,
  `tests/fake_love_cli.{c,h}`, `png_file_load` / `png_file_write`, the
  `FAKE_LOVE_CHECKS` list, the fake (frozen after phase 1), and design.md's
  "The asset PNGs" section (no `pad.png` there yet).
- **H-8-6**: step 8 cost at most ~0.3 point of `make test`; the headroom
  against 8c830c6 is still about 14 points (step 2.7 left ~15), so this
  step's ~0.4 point needs no reclaim.

**The sheet layout (the contract between the generator, the quad table and
the flat sheet).** `pad.png` and `pad_flat.png` are 240×72, 8-bit RGBA.
Every sprite is square; one texel is meant to be one unit of the 160×120
reference rectangle (step 10 may size its rectangles so; the sprites
stretch either way).

| sprite | x | y | size |
|---|---|---|---|
| stick base | 0 | 0 | 48×48 |
| stick knob | 0 | 48 | 24×24 |
| button k released (k = 0…7: a, b, x, y, leftshoulder, rightshoulder, start, back) | 48 + 24k | 0 | 24×24 |
| button k pressed | 48 + 24k | 24 | 24×24 |

Every texel outside a sprite is `00000000`. Changing a size or position
means changing the generator and the quad table together, then
`make pad-art`.

**The art (`pad.png`, first version).** Shapes are binary: a texel is in a
shape when its centre is, tested with integer arithmetic on doubled
coordinates (centre `2x + 1`), so no antialiasing and no floating point.
Colours are PICO-8 palette entries (`palette.RGB`); alphas are only `00`,
`66`, `99` and `FF`.

- Face buttons A, B, X, Y: a disc of radius 12 centred in the sprite, body
  colour 11 (A), 8 (B), 12 (X), 10 (Y); a 1-texel ring (in radius 12, not in
  radius 11) as outline. Label: the letter's `font.png` glyph scaled ×2
  (6×10), top-left at (9, 7). Released: body alpha `99`, outline the body
  colour opaque, label colour 7 opaque. Pressed: body opaque, outline colour
  7, label colour 0.
- L and R: a 24×24 square with corners rounded at radius 6, body colour 13,
  same outline, label and released/pressed rules, glyphs `L` and `R`.
- Start and Back: a pill 24×12 at rows 6–17 (radius-6 ends), body colour 5,
  same rules; label `START` / `BACK` at ×1 (3×5 glyphs, advance 4), centred:
  `START` from x 2, `BACK` from x 4, both from y 9.
- Stick base: a disc of radius 24, body colour 1 alpha `66`, 1-texel outline
  colour 6 opaque. Stick knob: a disc of radius 12, body colour 6 opaque,
  outline colour 7.
- The generator may tune cosmetic details (exact colours, offsets) when
  building it, within these rules (layout fixed, binary shapes, palette
  colours, the four alphas, a visible released/pressed difference); the
  final values are written in design.md, not in a comment.

**The flat sheet (`pad_flat.png`).** Same layout; each sprite's whole square
is one opaque colour; texels outside sprites `00000000`. Sprite number `n`
in the fixed order *stick base, stick knob, then for a, b, x, y,
leftshoulder, rightshoulder, start, back: released then pressed*
(n = 0…17) has red `0x20 + 0x0B·n`, green `0xE0 − 0x0B·n`, blue `0x40` for
even `n` and `0xC0` for odd `n`. So every colour is distinct (red differs),
none is black, and a released sprite (even n from 2) and its pressed one
(odd) also differ by 128 in blue. The rule goes in design.md for step 16's
tests.

**The static frame.** A 240×84 screen, black, every sprite of the real sheet
drawn at scale 1 (`draw(image, quad, x, y)` with the screen selected, white):
the stick base at (4, 4); then the 17 others in the order above without the
base (knob, a released, a pressed, b released, …, back pressed), sprite `k`
(k = 0…16) at (56 + 26·(k mod 7), 4 + 26·floor(k / 7)). The sheet's own
order is by state rows, this one interleaves states, so a transposed or
swapped quad changes the frame. Each texel lands as design.md's screen rule
gives over black: channel `mul(c, a)`, `mul(p, q) = (p·q + (p·q >> 8) +
128) >> 8`, alpha `FF` (an opaque texel writes itself, `00` leaves black).

**The generator, `tools/pad_art.c`.** One C file, linked with
`tools/fake_love/png_file.c` (and `-lm` as the other links of that file).
Usage: `tmp/pad_art <font.png> <pad.png> <pad_flat.png> <expected.png>`;
it reads `font.png` with `png_file_load` (layout from design.md: 16
columns × 6 rows of 3×5 cells, lit = colour 7), builds the three RGBA
buffers, writes them with `png_file_write` (stored deflate blocks, no
timestamp: deterministic), and exits 1 with a message naming the file on
any read or write failure. It holds its own copy of the layout table, the
16 palette colours and the slot rule: deliberately independent of
`love.pad_sheet` and of the fixture, so the static test compares two
sources. Makefile: a `$(TMP_DIR)/pad_art` rule like the other tool
binaries, and `pad-art: ## rewrites game/pad.png, tests/fixtures/pad_flat.png
and the static sheet test's expected PNG` (at most 20 words), a phony
target running the binary on the four paths. Not in `LOCKED_GOALS`, not in
`test`, `test-all` or CI.

**The quad table, `game/love/pad_sheet.lua` (`love.pad_sheet`).** No
`require`. Exports:

- `WIDTH`, `HEIGHT`: 240, 72.
- `STICK_BASE`, `STICK_KNOB`: `{x, y, width, height}` tables.
- `BUTTONS`: keyed by the LÖVE gamepad names of `main.lua`'s
  `PAD_BUTTON_NAMES` 5–12, each `{released = {x, y, width, height},
  pressed = {…}}`.
- `load()`: reads `love.image.newImageData("pad.png")` (the only place the
  name appears), makes one Image, and returns `{image = Image, stick_base =
  Quad, stick_knob = Quad, buttons = {a = {released = Quad, pressed = Quad},
  …}}`, each Quad built with `newQuad(x, y, w, h, WIDTH, HEIGHT)`. Call it
  once, in `love.load` (design.md's asset rule); it draws nothing. A sheet
  of another size is refused by the fake at the first draw (quad reference
  size ≠ image size), so `load` checks nothing itself.
- Names follow CLAUDE.md: no export repeats `pad` or `sheet`. Top level is
  data only (plain-LuaJIT `lua-test-modules`, `lua-names`). Being under
  `game/love/`, it may use `love.graphics` and `love.image` (`lua-guard`'s
  glue rule).
- Nothing requires it in this step; `main.lua` is untouched.

**The fixture game, `tests/fixtures/pad_sheet/`.** Prior art:
`tests/fixtures/love_runtime/`, a fixture game whose `love` and assets are
committed symlinks into `game/`. It holds `main.lua` and two symlinks,
`love -> ../../../game/love` and `pad.png -> ../../../game/pad.png`, so the
run uses the real module and the real sheet. `main.lua`: `love.load`
creates the 160×120 canvas 0 and clears it to colour 0 (the fake requires a
canvas, and canvas 0 must hold palette colours), then calls
`pad_sheet.load()`; `love.draw` selects the screen, sets white, and draws the
18 sprites at the static frame's positions (its own literal order and slot
rule, matching the generator's); `debug_state()` returns `WIDTH HEIGHT` on
the first line, then one line per sprite in the flat order,
`<name> <x> <y> <width> <height>` with names `stick_base`, `stick_knob`,
`a released`, `a pressed`, …, `back pressed`, read from the module's tables.
Replay `tests/fixtures/pad_sheet.json`: `{"input": "pad", "window": [240,
84], "screens": [0], "frames": [{}]}`.

**The test, in `tests/test_fake_love_image_draw.c`.** One new test function,
one fake run (`tmp/fake_love tests/fixtures/pad_sheet
tests/fixtures/pad_sheet.json <capture> <state>`), with outputs under the
test's own `tmp/` names:

1. the run succeeds (on failure, print the fake's stderr);
2. the screen PNG of frame 0 equals `tests/fixtures/pad_sheet_expected.png`
   exactly, all four channels, through `expected_screen_assert_file` (the
   expected loaded with `png_file_load` into an `expected_screen_t`), which
   names the first differing pixel; on failure the message adds "if the art
   changed on purpose, run make pad-art";
3. the state parses as the size line plus exactly 18 sprite lines, with the
   names in the flat order (the test's own literal list);
4. `tests/fixtures/pad_flat.png` loads at the state's size, and for each
   sprite line, every texel of its rectangle equals the rectangle's first
   texel, with alpha `FF` and RGB not `000000`; the 18 colours are pairwise
   distinct;
5. the screen PNG, capture and state are removed at the end, as the file's
   other tests do.

The binary's Makefile prerequisites gain the fixture's `main.lua`, the
replay, the two fixture PNGs, `game/pad.png` and `game/love/pad_sheet.lua`.
The check stays one target in `FAKE_LOVE_CHECKS`.

**Why there, and not elsewhere.** `test_game_screens` is reserved for step
16's grouped cases (~90 ms projected, ~10 ms spare: step 2.7's note), and
adding this run there would reach the 100 ms per-check budget.
`test_fake_love_png_file` is a pure C unit test that does not run the fake.
`test_fake_love_image_draw` already covers screen draws, `pad_sheet` and the
expected-frame helpers; measured today at 60–72 ms, it goes to about
72–86 ms. A standalone check would add a process, against plan §6 step
2.6's budget.

**Why the flat checks in C, not in Lua.** A prototype at HEAD showed
reading a 240×72 sheet with `ImageData:getPixel` in the fake costs ~4–5 ms,
half the run; `png_file_load` and a C loop cost well under 1 ms.

**Docs.**

- design.md "The asset PNGs": `pad.png` (game) and `pad_flat.png`
  (fixture) are generated by `tools/pad_art.c` through `make pad-art`, never
  edited by hand; 8-bit RGBA, stored deflate blocks, 240×72; the layout
  table; the art rules and their final values; the flat colour rule; the
  static test (`test_fake_love_image_draw`, against
  `tests/fixtures/pad_sheet_expected.png`, regenerated with the sheets) and
  "a red static test after an intended redraw: run `make pad-art`, review
  the three PNGs, commit them". The existing sentence "no tool writes them"
  stays scoped to `sprites.png` and `font.png`.
- design.md "Layout of the fork": `game/` lists `pad.png`; `tools/` lists
  `pad_art.c`.
- design.md "How the Lua game draws": one item, the pad's sprites come from
  `love.pad_sheet` (drawn onto the screen only, may be translucent).
- README: `make pad-art` in the target list, one line.

## Implementation Chunks

1. **The generator and the three PNGs.** `tools/pad_art.c`, the
   `tmp/pad_art` rule and `make pad-art`; run it; commit `game/pad.png`,
   `tests/fixtures/pad_flat.png`, `tests/fixtures/pad_sheet_expected.png`.
   Verified by hand: a second `make pad-art` leaves `git diff` empty; the
   three files decode with `png_file_load` at 240×72, 240×72 and 240×84;
   `make test` green (nothing reads them yet; `lua-guard` sees no new
   literal).
2. **The static test, then the module.** First the fixture game, its
   symlinks (`pad.png` resolves: chunk 1), the replay and the new test in
   `test_fake_love_image_draw`: red, since `love.pad_sheet` does not exist.
   Then `game/love/pad_sheet.lua`: green. `lua-guard` (H-8-2),
   `lua-test-modules`, `make lua-names` green.
3. **Docs, gate and report.** design.md and README; the timing gate; `make
   e2e` and `make end-state` 20/20; `make love-file` passes and
   `unzip -l tmp/slipways.love` lists `pad.png` (H-8-4); the report.

## Testing Decisions

- **A good test here** runs the real fake on the real module and the real
  sheet and compares whole outputs exactly: a screen PNG with a committed
  PNG, a state text parsed by rule. It never stubs `love`, never reads the
  module's source, and never compares with a tolerance (plan §1).
- **Tested in `make test`:** the static sheet test and the flat-sheet
  properties, one run in `test_fake_love_image_draw` (prior art: that file's
  `test_scaled_tinted_and_blended_draws_reach_the_screen_with_the_replay_s_pad_sheet`
  for a screen PNG check, `tests/test_love_runtime.c` with
  `tests/fixtures/love_runtime/` for a fixture game symlinking `game/`).
- **The expected frame** is made by the generator, outside `make test`,
  and committed (design.md rule 2: no outside tool in a check). Its
  independence from the quad table is what makes the test check the table;
  its regeneration together with the art is the plan's "regenerated on
  purpose when the art changes".
- **Not tested in `make test`:** the generator itself (it is a tool; its
  output is pinned by the static test, its reproducibility checked by hand
  in chunk 1). No Lua runner test for the table: the static frame and the
  flat checks cover every rectangle (a wrong, overlapping or missing
  rectangle fails one of them).
- **Timing.** The step adds work to a `make test` check, so plan §2's gate
  applies: 11 rounds of paired `make test` medians against the "before"
  commit (H-8-1), 8dff812 and 8c830c6, each ≤ 3 %. Prototype at HEAD, `-O2`,
  4 cores: one fake run of a 240×72 sheet drawn 19 times with `screens` at
  256×128, 6–11 ms; loading and comparing a 256×128 PNG twice ~2 ms.
  Expected: `test_fake_love_image_draw` +10–13 ms of own time (~0.4 point),
  far within the ~14 points of headroom (H-8-6). Record its own time before
  and after. No `make e2e` timing: neither the fake's per-frame work nor the
  game's frame changes.
- **Check of the step:** the static test passes; `make test-all` green;
  `make e2e` and `make end-state` 20/20 with every `*.sha256` and
  `*.end-state` unchanged; `make pad-art` twice leaves `git diff` empty;
  `make help` lists `pad-art` (≤ 20 words); `make love-file` passes with
  `pad.png` in the archive; design.md and the README state the sheets, the
  generator and the test; the gate passes.

## Out of Scope

- Any game change: `main.lua` does not require `love.pad_sheet`; drawing the
  pad is step 16.
- The pad element table in reference units and the screen rectangles (step
  10); the ≥ 44-point rule is step 10's, the sprite sizes only make it easy.
- Stick, touch → mask, show/hide (steps 11–14), touch replays (15).
- Any fake change (frozen after phase 1); `pad_sheet` already exists.
- Any `lua-guard` change (H-8-2) or `make love-file` change (H-8-4).
- A `make test` check that the committed PNGs equal a fresh generator run
  (drift is caught when the art is regenerated; the static test pins the
  committed bytes).
- Antialiased or high-resolution art; a light effect on press (plan §2).
- Writing plan §6's step note (the coordinator does, from the report).

## Further Notes

- **Prototype** (scratch, not committed): a fixture game holding a 240×72
  RGBA sheet with translucent texels, a canvas 0 cleared to black, 19 quad
  draws at integer positions on a 256×128 screen, `screens: [0]`: the run
  succeeds in 6–11 ms; the screen PNG is 131 KB (stored blocks); a
  `getPixel` pass over the whole sheet adds ~4–5 ms. Without the canvas the
  fake fails the frame ("no canvas exists at the end of the frame"), hence
  the fixture's canvas.
- The fake's `newImage` accepts translucent texels and the 4-argument quad
  draw takes them onto the screen (step 1.5), so the fixture needs no
  opacity workaround; it must never draw the sheet onto a canvas.
- `pad.png` is ~69 KB uncompressed in the repository (stored deflate, the
  fake's writer); the `.love` zip compresses it.
- The symlinks follow the `love_runtime` fixtures' precedent; the fake's
  disk opens files with `fopen`, which follows them.
- Step 16's replay `tests/fixtures/game_screens.json` can name
  `"pad_sheet": "pad_flat.png"`: the flat sheet sits next to it.
