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
2. builds the fake disk before `love.load`: `save.bin` holds the bytes of
   the replay's `initial_save` (hex string) when present, `bindings.cfg` the
   content of the file named by `bindings` (resolved next to the replay, at
   most 65 536 bytes); no other file is in memory. Writes from the game stay
   in memory; the real disk is never written by the game;
3. calls `love.load(arg)`, `arg` being `{"--attract-after", "N"}` when the
   replay has `"attract_after": N` (plain decimal), `{}` otherwise;
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

**Fake disk and arguments.** The replay members the fake reads are
`input`, `frames`, `initial_save`, `bindings` and `attract_after`, each at
most once and as strict as the C replay; `start` is refused, other members
are ignored. A refusal exits 1 before `love.load` and leaves no output.
`love.filesystem` is nil in `conf.lua` and holds exactly `read`, `write`
and `getInfo` from `main.lua` on. In-memory files lie over the game
directory, visible read-only, and shadow its files of the same name; the
disk lives for one run. `read(name)` returns the content and its size in
bytes, or `nil, "Could not open file <name>. Does not exist."` for a
missing file or a directory. `write(name, data)` with a string replaces the
whole file and returns `true`; a name holding `/` returns `false` and the
same message, since the fake creates no directory. `getInfo(name[,
filtertype])` returns a new `{type = "file", size = N}` or
`{type = "directory"}` (game directory only), no `modtime`, or `nil`
when missing or of another type; `filtertype` is one of `"file"`,
`"directory"`, `"symlink"`, `"other"`. Names are byte-compared; empty
names, a leading `/`, an empty, `.` or `..` element, `\` and `:` raise an
error naming the function and the path. Any other form (read's size or
container argument, a third argument to write, non-string data, the
info-table forms) raises an error naming the function. `love.load` gets its
arguments as its parameter only: the global `arg` is nil.

**Canvas and clear.** `love.graphics` is nil in `conf.lua`, like
`love.joystick`. `newCanvas(160, 120)` is the only size accepted; a fresh
canvas is transparent black (`00000000`) everywhere, and at most 16
canvases exist in a run. `setCanvas(canvas)` selects a canvas,
`setCanvas()` and `setCanvas(nil)` select the screen; switching leaves the
previous canvas untouched, and a canvas keeps its pixels across frames
until cleared or drawn over. After the draw step of every frame, even
without `love.draw`, a selected canvas is fatal:
`love.draw f<N>: present cannot be called while a Canvas is active.`, as
in LÖVE. `clear(r, g, b, a)` fills the selected canvas, `clear(r, g, b)`
uses alpha 1, `clear()` is transparent black; at alpha 0 the RGB bytes are
kept (no premultiplication). Each component becomes a byte as Mesa does:
to single precision, clamped to [0, 1], times 255 in single precision,
rounded to nearest with ties to even (`1.5/255` gives 2, `2.5/255` gives
2, 0.3 gives 76), so `byte/255` gives back that byte. `clear` with the
screen selected changes nothing the fake models. Any other form (table
colours, `setCanvas` tables or mipmaps, other sizes, settings, non-finite
or non-number components, a wrong count) raises an error naming the
function.

**Points.** `points(x1, y1, …, xn, yn)` takes zero or an even number of
Lua numbers; `points(t)` takes one flat table of them, its elements
1 … `#t`. `points()` and `points({})` draw nothing. Each point lights one
pixel of the selected canvas with the current colour (see Colour),
replacing its four bytes, also over a transparent pixel; a pixel lit twice
is lit once. The pixel, per axis
(size 160 for x, 120 for y), is the one real LÖVE 11.5 on Mesa llvmpipe
lights: column n when n < x′ ≤ n + 1, x′ being the coordinate through
LÖVE's single-precision projection and Mesa's viewport, snapped to 1/256
pixel. In C, each rounding separate, the viewport fused:

```
v = (float) x;  s = 2.0f / (float) size;  p = v * s;  ndc = p + (-1.0f);
win = fmaf(ndc, size / 2, size / 2);  fix = lrintf(win * 256.0f);
pixel = floor((fix - 1) / 256.0)    // lit only if 0 <= pixel < size
```

**The rule for the game: to light pixel (x, y), call
`points(x + 0.5, y + 0.5)`.** Integer coordinates light (x − 1, y − 1),
so `points(0, y)` and `points(x, 0)` light nothing; a fraction up to
1/1024 behaves like 0. Fractions within 1/512 of an integer land on a
pixel that depends on the position (at c + 1/512 some columns light c,
others c − 1, and rows differ from columns): the game must not produce
them. Points off the canvas draw nothing and raise no error; 160 + 1/1024
still lights column 159, 160 + 3/1024 nothing, likewise for row 119. With
no canvas selected, points touch nothing the fake models. Every refusal is
an error starting `love.graphics.points: ` and draws none of the call's
points: an odd number of coordinates, a non-number argument or element
(no string coercion; this catches LÖVE's pairs and per-point-colour
tables and a table with a hole), a table followed by more arguments, a
non-finite coordinate, and a magnitude of 2^20 or more, because llvmpipe
does not clip before its fixed-point conversion and wraps coordinates
from 2^23 up back onto the canvas. Captures: `points_landing`,
`points_subpixel`, `points_columns`, `points_rows`, `points_offcanvas`,
`points_forms`.

**Rectangles.** `rectangle("fill", x, y, w, h)` is the only form: exactly
five arguments, the string `"fill"` and four Lua numbers. It fills with
the current colour (see Colour), replacing each pixel's four bytes, also
over a transparent pixel, and
pixels around keep their bytes; overlaps and repeats leave one opaque
fill. Coverage, per axis (size 160 for x with w, 120 for y with h), is
real LÖVE 11.5 on Mesa llvmpipe: each argument to single precision, the
far edge added in single precision, both edges through the points
projection, viewport and 1/256 snap, then a pixel is filled when its
centre lies between the edges, the low edge inclusive and the high edge
exclusive (the top-left rule):

```
a = (float) x;  b = a + (float) w;      // float addition, never double
A = snap(a, size);  B = snap(b, size);  // fix of the points rule above
lo = min(A, B);  hi = max(A, B);
pixel i is filled iff lo <= 256 * i + 128 < hi   // clipped to 0 … size − 1
```

**The rule for the game: pass integers.** An integer rectangle fills
columns x … x + w − 1 and rows y … y + h − 1 at every position, so a
PICO-8 `rectfill(x0, y0, x1, y1)` with integer corners is
`rectangle("fill", min(x0, x1), min(y0, y1), |x1 − x0| + 1,
|y1 − y0| + 1)`, and `rectangle("fill", x, y, 1, 1)` fills pixel (x, y).
A negative size fills the same pixels as the positive rectangle spanning
the same edges; a zero size fills nothing and raises no error. Edges
within 1/512 of a pixel centre are position-dependent (at
c + 0.5 + 1/512 some columns include c, others not, and rows differ from
columns), and a far edge near such a tie depends on the float sum: the
game must not produce them. Parts off the canvas are clipped:
`(−0.49, y, 1, 1)` fills column 0, `(−0.5, y, 1, 1)` and
`(159.51, y, 1, 1)` nothing. With no canvas selected, rectangles touch
nothing the fake models. Every refusal is an error starting
`love.graphics.rectangle: ` and draws nothing: another argument count (a
sixth argument too, even 0 or nil, so no rounded rectangle), a mode that
is not a string, any mode but `"fill"` (`"line"` is antialiased in real
LÖVE), a non-number (no string coercion), a non-finite value, and a
magnitude of 2^20 or more for any of x, y, w, h, the bound of `points`;
it keeps every snapped value in a `long` and both edges far inside the
2^31 where llvmpipe's clipper breaks down. Captures:
`rectangle_integers`, `rectangle_subpixel`, `rectangle_columns`,
`rectangle_rows`, `rectangle_offcanvas`, `rectangle_forms`.

**Colour.** `setColor(r, g, b)` and `setColor(r, g, b, 1)` are the only
forms: three Lua numbers, or four whose fourth equals 1. The current colour
is opaque white (`FFFFFFFF`) until the first `setColor`, then the last one
set, for the whole run: from `love.load` on (before any canvas exists),
across frames, canvas switches, `setCanvas()` and `clear`, which ignores
it and leaves it unchanged. Each component becomes a byte as real LÖVE
11.5 does: to single precision, clamped to [0, 1], times 255 in single
precision, **truncated**, unlike `clear`, which rounds: a component 0.5
gives 127 with `setColor` and 128 with `clear`, 0.3 gives 76, and
`128/255 × (1 − 2^-30)`, one float step below `128/255`, gives 127. A
point or rectangle drawn takes the colour's four bytes, alpha `FF`, over
any pixel, transparent included. Every refusal is an error starting
`love.graphics.setColor: ` and leaves the current colour unchanged: another
argument count (a fifth argument too), a non-number (no string coercion,
no table form), a non-finite component, and an alpha other than 1, because
real LÖVE blends a translucent colour with the pixels under it and the fake
does not model the blender. **The rule for the game: pass each palette
byte as the Lua expression `byte / 255`**, which always gives that byte; a
component computed any other way can lose a whole byte. Never pass an
alpha: translucent colours blend in real LÖVE and are refused by the fake.
Captures: `color_bytes`, `color_precision`, `color_state`.

### The LÖVE functions the game may use

The fake implements exactly this list; extending it is a decision recorded
here, with its real-LÖVE capture.

- Loop: `love.load(arg)`, `love.update(dt)`, `love.draw()`. The game ignores
  `dt`: one `update` is one game frame (60 per second).
- Drawing: `love.graphics.newCanvas(160, 120)`, `setCanvas`, `clear`,
  `setColor(r, g, b)` or `setColor(r, g, b, 1)`, `points(x1, y1, …)` or
  `points({x1, y1, …})`,
  `rectangle("fill", x, y, w, h)`, `newImage`, `newQuad`,
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

A capture program returns its draw function. Both harnesses (the real one
in `tools/love_capture/harness/`, the fake one in `tools/fake_love/harness/`)
create the 160×120 canvas before loading the program, so the program's top
level may create other canvases and the harness canvas is the fake's
canvas 0; they select it, call the draw function once with the canvas as
argument, and unselect it. `tmp/love_capture_check` compares all four
channels of all 19 200 pixels, RGB included where alpha is 0, and names the
program, the first differing pixel, both RGBA values and the count of
differing pixels. Each program is its own `love-capture-check-<name>`
target of `make test`; a `.lua` without its `.png`, or the reverse, fails
its target. `make love-check` only checks the installed LuaJIT.

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
