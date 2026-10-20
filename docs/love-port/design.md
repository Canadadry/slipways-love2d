# LÖVE port

What plans 0 to 5 build: a Lua port of the C game for LÖVE 11.5, checked by
the same 20 e2e replays as the C game, with the same `.json.sha256`. This file
holds the decisions those plans share. Each plan holds its own steps.

## Goal and scope

- The game becomes a real LÖVE 11.5 game in Lua: it reads the pad through
  LÖVE's gamepad API and draws through `love.graphics`. PC first. iOS comes
  later through the love2d studio app, which runs `.love` files: nothing in
  these plans targets iOS.
- The 20 replays of `tests/e2e/` must produce **the same sha256** as the C
  game, unchanged. The C game is the reference until plan 4 deletes it.
- Audio stays out of scope.

## The fake LÖVE (`tools/fake_love/`)

A C program that embeds LuaJIT (system package: `libluajit-5.1-dev` on
Linux, `brew install luajit` on macOS; its version is checked at startup) and
plays the role of LÖVE for a replay. It never opens a window. PNGs are read
with `stb_image.h`, vendored as a single file.

For one replay it:

1. loads the game directory (`game/`) as LÖVE would: `conf.lua`, then
   `main.lua`;
2. builds the fake disk: `save.bin` holds the bytes of the replay's
   `initial_save` (hex string) when present, `bindings.cfg` the content of the
   file named by `bindings` (resolved next to the replay). Writes from the game
   stay in memory; the real disk is never written by the game;
3. calls `love.load(arg)`, with `--attract-after N` in `arg` when the replay
   has `"attract_after": N`;
4. for each frame of `frames`: compares the held buttons with the previous
   frame's, calls `love.gamepadreleased` then `love.gamepadpressed` for those
   that changed, each in ascending button number, answers
   `Joystick:isGamepadDown` from the current list, calls `love.update(1/60)`
   then `love.draw()`;
5. after each `love.draw()`, reads the 160×120 canvas, turns each pixel's RGB
   into its palette number 0–15 and appends the 19 200 bytes to the capture.
   The capture format is the C replay's: its sha256 is compared with the
   replay's `.json.sha256`;
6. after the last frame, calls the game's `debug_state()` and writes the text
   it returns next to the capture, compared with the replay's `.end-state`.

Anything outside the contract below is a **fatal error** that names the
function (or the frame and pixel): a LÖVE function not on the list, a colour
that is not one of the 16, a pixel of another colour on the canvas.

### `make test` and the fake's checks

`CLAUDE.md` forbids making `make test` slower. Every check that runs the
fake follows these rules, and every step that adds one measures it.

1. **Parallel, never serial.** Each check of the fake (a C test that drives
   `tmp/fake_love`, a capture program comparison) is its own make target,
   listed in the Makefile's `FAKE_LOVE_CHECKS`, a direct prerequisite of
   `test`. No recipe loops over programs, and no check orders itself after
   the unit tests: `-j` runs them on the cores the unit tests leave idle.
2. **Cheap by construction.** One process per check, no interpreter but the
   embedded LuaJIT, no external tool (no Python, no ImageMagick, no shell
   pipeline over PNGs); comparisons are done in C. Budget: 100 ms of wall
   time per check on the container. A check over budget is split before
   its step closes.
3. **Out of `make e2e`.** No fake check is a prerequisite of `e2e`;
   `make love-e2e` is its own target, outside `make test` and `make e2e`.
4. **Measured, with one protocol, against two baselines.**
   - Baselines: the step's "before" commit, and the plan-wide reference,
     commit `2e7215c` (4 cores, plain median 7.70 s over 15 runs when
     recorded). A median is never compared across machines: the reference
     is re-measured beside the current commit every time.
   - Protocol: each commit under comparison in its own git worktree under
     `tmp/`; one `make test` in each to warm the build; then 11 rounds, each
     timing one `make test` (wall time) per worktree back to back, rotating
     which goes first.
   - Gate: for each round, r = current / baseline − 1, against "before" and
     against the reference; the statistic is the median of the r values per
     baseline. Both ≤ 3 % → pass. Either > 3 % → 11 more rounds, decide on
     the 22 pooled rounds: still > 3 % → fail, and the step reworks its
     checks under rules 1 and 2, or rule 5. No third batch, and no gate on
     the spread: per-run spread is about 4 % and does not shrink with more
     runs, while in-round pairs cancel the drift.
   - Record, in the step note: the plain medians, the paired medians, each
     commit's spread (for information only), the number of rounds, the
     core count, and the slowest new check's own time.
5. A check that rules 1 and 2 cannot hold under 3 % goes into a separate
   target outside `make test-all`, and this file then says that `make test`
   no longer runs every capture.

### Pad buttons

The replays hold button numbers 1–12. The fixed table, from the PSX mapping
of `docs/psx-game.md`:

| number | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PSX | Up | Down | Left | Right | Cross | Circle | Square | Triangle | L1 | R1 | Start | Select |
| LÖVE | `dpup` | `dpdown` | `dpleft` | `dpright` | `a` | `b` | `x` | `y` | `leftshoulder` | `rightshoulder` | `start` | `back` |

**Pad semantics.** Nothing is held before frame 0, so frame 0 presses
every button its list holds. Each frame, the fake first makes the frame's
list current, then calls `love.gamepadreleased` for each button that went
up, then `love.gamepadpressed` for each that went down, each group in
ascending button number, then `love.update(1/60)` and `love.draw()`. A
button held over several frames is pressed once: no auto-repeat; only set
changes count, not the order of a frame's list. Nothing is called after the
last frame except `debug_state()`, so a button still held then is never
released. `love.joystick.getJoysticks()` returns a new list holding one
joystick, always the same object, also passed to the callbacks;
`love.joystick` is nil while `conf.lua` runs. `Joystick:isGamepadDown`
answers from the current frame, from the start of its callbacks to the
start of the next frame: false for everything during `love.load`, the last
frame during `debug_state()`. With several names it is true when any is
held; `guide`, `leftstick` and `rightstick` are never held; any other name
fails with `Invalid gamepad button: <name>`. The ascending order is the
fake's convention, not LÖVE's: the game must not depend on the order within
a frame.

### The LÖVE functions the game may use

The fake implements exactly this list; extending it is a decision recorded
here, with its real-LÖVE capture.

- Loop: `love.load(arg)`, `love.update(dt)`, `love.draw()`. The game ignores
  `dt`: one `update` is one game frame (60 per second).
- Drawing: `love.graphics.newCanvas(160, 120)`, `setCanvas`, `clear`,
  `setColor`, `points`, `rectangle("fill", …)`, `newImage`, `newQuad`,
  `draw(image, quad, x, y)`, `setDefaultFilter("nearest", "nearest")`,
  `push`, `pop`, `translate`.
- Images: `love.image.newImageData(w, h)`, `love.image.newImageData(file)`,
  `ImageData:setPixel`, `ImageData:getPixel`.
- Pad: `love.joystick.getJoysticks`, `Joystick:isGamepadDown`, the callbacks
  `love.gamepadpressed` and `love.gamepadreleased`.
- Files: `love.filesystem.read`, `write`, `getInfo`.

In real LÖVE the game draws its canvas scaled up to the window at the end of
`love.draw()`; the fake reads the canvas and needs nothing more.

The exact pixel conventions (where `points(x, y)` lands, how `rectangle`
rounds) are those of real LÖVE 11.5, settled by the captures of
`tools/love_capture/` and written down here once captured.

### Real-LÖVE captures (`tools/love_capture/`)

Small LÖVE programs, each exercising some functions of the list, and the PNG
real LÖVE 11.5 draws for each, read from the canvas
(`Canvas:newImageData():encode("png")`, never a window screenshot).
`make love-capture` runs them in real LÖVE under Xvfb with Mesa software
rendering and rewrites the PNGs; it is the **only** place a virtual screen is
allowed (see `CLAUDE.md`), agents and CI may run it, and it is outside
`make test` and `make e2e`. CI runs it on every push and fails when a PNG
changes. `make test` runs the same programs in the fake LÖVE and compares
pixel for pixel with the PNGs.

## How the Lua game draws

The game only uses functions LÖVE draws pixel-exact, so what the e2e check is
what the player sees:

- **Colours:** a 16-entry table holding the C palette's RGB values
  (`src/p8/palette.c`), passed exactly to `setColor` (byte/255).
- **Circles and lines:** drawn by the game itself, pixel by pixel (`points`)
  or in spans (`rectangle`), with the C game's algorithms.
- **Fill patterns** (PICO-8 `fillp`): the game builds them as images and draws
  those. The pattern is aligned to the **screen**, not the shape: the drawn
  image is offset accordingly. Rectangles and lines tile a 4×4 pattern image;
  a patterned disk is built pixel by pixel into an image once per radius and
  kept.
- **See-through colours and colour swaps** (PICO-8 `palt` and the 16 palette
  blocks of `hud.c`): the game builds a recoloured copy of the sprite sheet per
  combination it uses, the first time it is needed, and keeps it. Flat shapes
  apply the swap by choosing the colour before drawing.
- **Text:** the game places each letter itself from `font.png`, as pieces of
  an image. Outlined text is the word drawn shifted one pixel in each
  direction in the outline colour, then the word on top.
- **Sprites and the map:** pieces of `sprites.png` (or of its recoloured
  copies), drawn with quads.

### The asset PNGs

`make asset-export` writes `game/sprites.png` and `game/font.png` from the C
data; both are committed, and `make test` fails when either is stale.

- **Format:** 8-bit RGB, no alpha, non-interlaced; stored (uncompressed)
  deflate blocks.
- **Colours:** every pixel is one of the 16 `P8_PALETTE_RGB` triples, byte
  for byte. Colour 0 is opaque black in both files: the game builds its
  see-through copies itself (the recoloured sprite sheets; per-colour letter
  images, background made transparent) and never draws raw `font.png` pieces.
- **`sprites.png`:** 128×128; pixel (x, y) is sprite sheet pixel (x, y).
- **`font.png`:** 48×30, a grid of 16 columns by 6 rows of 3×5 cells, no
  gutter, for the codes 32 to 127. Code `c` sits in column `(c - 32) % 16`,
  row `(c - 32) / 16`, its top-left pixel at `(column * 3, row * 5)`. A lit
  pixel is colour 7, every other pixel colour 0. Lowercase holds the
  uppercase glyph, `v` and `V` hold the `U` glyph, 32 is empty, and every code
  with no glyph (127 among them) holds the placeholder, a full 3×5 block; a
  code outside 32..126 draws cell 127. The 4-pixel advance and the outline
  are drawing rules, not image content.

## Numbers and randomness

The C game computes with PICO-8 16.16 fixed-point numbers (`src/p8/fixed.c`)
and its own random generator. The Lua game uses the same format and the same
generator through a Lua library built on LuaJIT's `bit` library, bit-exact.
Its tests (`tests/lua/`, run with LuaJIT) compare it with the C over millions
of values while the C exists, then with saved reference values.

## Layout of the fork

- `game/`: the Lua game (`conf.lua`, `main.lua`, `sprites.png`, `font.png`);
  `make love` zips it into `slipways.love`.
- `tools/fake_love/`: the fake LÖVE's C source; built as `tmp/fake_love`.
- `tools/love_capture/`: the capture programs and their PNGs.
- `tests/e2e/`: the 20 replays, unchanged, plus `order.txt`, measured with
  Apple clang 16 and its llvm-cov gcov (`--coverage -O0`); GCC's gcov
  counts lines differently, so `make e2e-order-check` holds on that
  toolchain only.
- `tests/lua/`: the maths library tests.
- `src/`: the C game, the reference, until plan 4.

## Rules for the fork

- The e2e replays are the source of truth; no C unit test is ported to Lua.
- `make e2e` may become slower than the C one; the rule becomes "under N
  seconds", N set by plan 4 from a measurement. The replays run in parallel,
  LuaJIT with its JIT on.

## Open questions

- Which LÖVE version does the love2d studio iOS app run? 11.x keeps everything
  aligned.
- Keyboard play on PC: not needed by the e2e, undecided.
