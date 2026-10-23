# LÖVE port

What plans 0 to 5 build: a Lua port of Slipways for LÖVE 11.5, checked by
the 20 e2e replays of `tests/e2e/` against the `.json.sha256` the C game set
before plan 4 deleted it. This file holds the decisions those plans share.
Each plan holds its own steps.

## Goal and scope

- The game becomes a real LÖVE 11.5 game in Lua: it reads the pad through
  LÖVE's gamepad API and draws through `love.graphics`. PC first. iOS comes
  later through the love2d studio app, which runs `.love` files: nothing in
  these plans targets iOS.
- The 20 replays of `tests/e2e/` must produce **the same sha256** as the C
  game did, unchanged: those hashes, set by the C game (now deleted), are the
  reference.
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
5. after each frame's draw step, even without `love.draw`, reads canvas 0
   (the first created), turns each opaque pixel's RGB into its palette
   number 0–15, packs two pixels per byte (even x in the low nibble), row by
   row, and appends the 9 600 bytes to the capture (see Capture). The
   capture format is the one the `.json.sha256` were set with: its sha256
   is compared with the replay's `.json.sha256`;
6. after the last frame, once the capture is closed, calls the game's
   `debug_state()` and writes the text it returns next to the capture,
   compared with the replay's `.end-state`.

**Fatal errors.** Anything outside the contract below is fatal, with one
of three errors, each naming its cause:

1. a LÖVE function not on the list (see The LÖVE functions the game may
   use), checked when the game reads or creates the field, in both
   binaries: `love.timer is not in the function list of docs/love-port/design.md`;
2. a colour that is neither one of the 16 nor white, checked by `setColor`
   on the bytes it would draw, in `tmp/fake_love` only (see Colour):
   `love.graphics.setColor: 4C0000 is not one of the 16 palette colours nor white FFFFFF (pass each palette byte as byte / 255)`;
3. a pixel of another colour on canvas 0, or no canvas, checked at the end
   of each frame, in `tmp/fake_love` only (see Capture):
   `fake_love: frame f3: pixel (0, 0) of canvas 0 is FF0000FF, not one of the 16 palette colours (1 such pixels in the frame)`.

The first two are ordinary Lua errors, carrying the game's `file:line:`
like every refusal of the fake: uncaught, the run fails (exit 1, no
capture nor state file); caught by `pcall`, the game goes on. No game code
can catch the third. `love_capture_check` keeps the function list but
allows any colour in `setColor` and never checks frames: its programs
measure real LÖVE, the palette is the game's contract.

### `make test` and the fake's checks

`CLAUDE.md` forbids making `make test` slower. Every check that runs the
fake follows these rules, and every step that adds one measures it.

1. **Parallel, never serial.** Each check of the fake (a C test that drives
   `tmp/fake_love`, a capture program comparison) is its own make target,
   listed in the Makefile's `FAKE_LOVE_CHECKS`, a direct prerequisite of
   `test`. No recipe loops over programs, and no check orders itself after
   the unit tests: `-j` runs them on the cores the unit tests leave idle.
   `-j` starts prerequisites in list order, so a long check goes early in
   `test`'s list: queued behind the others, it would set the wall time
   alone.
2. **Cheap by construction.** One process per check, no interpreter but the
   embedded LuaJIT, no external tool (no Python, no ImageMagick, no shell
   pipeline over PNGs); comparisons are done in C. A capture check's recipe
   is the bare `love_capture_check` command, run without a shell: a lone
   `.lua` or `.png` is found when the Makefile is read, and its target
   fails naming the missing half. Budget: 100 ms of wall time per check
   on the container. A check over budget is split before its step closes.
3. **Out of `make e2e`.** No check of `make test` is a prerequisite of
   `e2e`; `make e2e` runs the fake on the replays itself (see `make e2e`).
4. **Measured, with one protocol, against two baselines.**
   - Baselines: the step's "before" commit, and the plan-wide reference,
     commit `8dff812`, the merge of pull request #1: the last commit before
     plan 2's own work, with its fake checks (4 cores, plain median 5.60 s
     over 11 rounds when recorded). A median is never compared across
     machines: the reference is re-measured beside the current commit every
     time.
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

### `make e2e`

`make e2e` runs `tmp/fake_love <game dir> <replay> <capture> <state>` on
every replay of `tests/e2e/`, one make job per replay, the largest replay
file (the most frames) first, every replay rerun on every invocation (make
does not see a Lua edit). It builds `tmp/fake_love` when missing. The game
directory is `LOVE_E2E_GAME`, default `game`; a `%` in it becomes the
replay's file name (`pad_title.json`), so a verification can give each
replay its own stub. The replay's `initial_save`, `bindings` and
`attract_after` reach the game as the fake gives them; the game must define
`debug_state`.

Per replay, under `tmp/love-e2e/tests/e2e/`: `<name>.json.bin` (the
capture), `<name>.json.sha256` and `<name>.json.state` (the game's
`debug_state()`) when the fake exits 0, `<name>.json.error` otherwise
(`fake_love exited <status>` or `killed by signal <n>`, then the fake's
stderr); all four are removed before each run. These runs are shared:
`make e2e end-state` runs each replay once.

`tools/e2e_limits.sh` checks the replay limits, then each replay is
reported on one line:

- **matches**: the capture's sha256 equals the replay's `.json.sha256`;
- **DIVERGENT**: it does not, with the expected and actual hash and, if the
  divergence is intended, `make e2e-baseline`;
- **FAILED**: the fake stopped before the last frame, with its message
  indented below.

Both checks always run; the report ends with the count line
`e2e: <m> match, <d> diverge, <f> failed, of <n> replays in tests/e2e`.
The exit status is 0 only when the limits hold and every replay matches.
`make e2e-baseline` copies each replay's Lua hash to its `.json.sha256`
and writes nothing for a replay the fake failed on, failing the target.
Both take `tmp/make.lock` and have no timeout. `make e2e` is part of
`make test-all` and CI, and runs under 9 seconds (median 5.9 s over 6 cold
runs on 4 cores, measured once the C was deleted).

### `make end-state`

`make end-state` checks the Lua end states of `make e2e`'s runs against
each replay's committed `<name>.json.end-state` with
`tools/end_state.sh check`, with the normalization the committed end
states were written with; it never writes an `.end-state`. Each replay is reported:

- **matches**: the normalized state equals the reference;
- **differs**: it does not, with the `-` reference and `+` Lua lines;
- **FAILED**: the fake stopped before the last frame (a missing
  `debug_state` included), with its message indented below.

All replays are reported before the count line
`end-state: <m> match, <d> differ, <f> failed, of <n> replays in tests/e2e`;
the exit status is 0 only when every replay matches.
`make end-state EVENTS=<events> [REFERENCE=<file>]` runs the fake once on
that file under `tmp/end-state/` and checks its state against `REFERENCE`,
default `<events>.end-state`: the only way to check a `worst-case/` game.
`make end-state-baseline` writes each missing `.end-state` of the 20
replays from the Lua state, never overwrites one, and fails naming a
replay the fake failed on. All take `tmp/make.lock`, have no timeout, and
are outside `make test-all` and CI.

### Pad buttons

The replays hold button numbers 1–12. The fixed table, inherited from the
PSX game's pad mapping:

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
most once: `initial_save` a non-empty string of an even number of hex
digits, `bindings` the name of a `.cfg` file next to the replay,
`attract_after` a positive integer; `start` is refused, other members are
ignored. A refusal exits 1 before `love.load` and leaves no output.
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

**Capture.** A capture is the frames one after the other, no header, so
its size is the replay's frame count × 9 600 bytes (a replay of no frame
gives an empty file). A frame is canvas 0, the first canvas the game
created, as it stands after the draw step, whatever canvas was selected
last and including pixels kept from earlier frames; other canvases and the
screen never reach it. For each row y from 0 to 119, for each even x from
0 to 158, one byte `n(x, y) | n(x + 1, y) << 4`. A pixel is colour n when
its alpha is `FF` and its RGB equals triple n byte for byte:

| n | RGB | n | RGB | n | RGB | n | RGB |
|---|---|---|---|---|---|---|---|
| 0 | `000000` | 4 | `AB5236` | 8 | `FF004D` | 12 | `29ADFF` |
| 1 | `1D2B53` | 5 | `5F574F` | 9 | `FFA300` | 13 | `83769C` |
| 2 | `7E2553` | 6 | `C2C3C7` | 10 | `FFEC27` | 14 | `FF77A8` |
| 3 | `008751` | 7 | `FFF1E8` | 11 | `00E436` | 15 | `FFCCAA` |

Any other pixel is not a palette colour: alpha `00`, even over a palette
RGB (a fresh canvas's `00000000` included), and every RGB outside the
table. In `tmp/fake_love`, after the "Canvas is active" check of each
frame, a foreign pixel on canvas 0 or no canvas at all stops the run at
once (exit 1, no capture nor state file), naming the first such pixel in
row order and the frame's count:
`fake_love: frame f<N>: pixel (<x>, <y>) of canvas 0 is <RRGGBBAA>, not one of the 16 palette colours (<count> such pixels in the frame)`
or
`fake_love: frame f<N>: no canvas exists at the end of the frame (create the game canvas first, in love.load)`.
No game code can catch it; zero frames give an empty capture and no
error. `love_capture_check` never runs this check: it compares all four
channels with a PNG, transparent pixels included. **The
rule for the game: create the game canvas first, in `love.load`, before any
other canvas; end every frame with every pixel of it opaque and one of the
16 triples, drawn as `byte / 255`.**

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
does not model the blender. In `tmp/fake_love`, the three bytes the
conversion gives must then be one of the 16 triples of the Capture table
or white `FFFFFF`, whatever numbers gave them (`2, -1, 77/255` is colour
8); anything else is refused naming those bytes:
`love.graphics.setColor: 4C0000 is not one of the 16 palette colours nor white FFFFFF (pass each palette byte as byte / 255)`.
White is accepted because it tints texels to themselves (see Images).
`clear`, `setPixel` and texels are not checked at the call: on a
scratch canvas any bytes are legal, and what reaches canvas 0 is judged at
the end of the frame (see Capture). `love_capture_check` accepts any
opaque colour. **The rule for the game: set only palette colours and
white; pass each palette byte as the Lua expression `byte / 255`**, which
always gives that byte; a component computed any other way can lose a
whole byte. Never pass an alpha: translucent colours blend in real LÖVE
and are refused by the fake. Captures: `color_bytes`, `color_precision`,
`color_state`.

**Image data.** `love.image` is nil while `conf.lua` runs, then holds
exactly `newImageData`; an ImageData holds exactly `setPixel` and
`getPixel`, lives from `love.load` on like any Lua value and is freed by
the garbage collector. `love.image.newImageData(w, h)` takes two Lua
numbers, each an integer from 1 to 1024, and returns an ImageData that is
`00000000` everywhere; the file form is the next paragraph.
`data:setPixel(x, y, r, g, b)` stores alpha `FF`,
`data:setPixel(x, y, r, g, b, a)` stores any alpha as given (alpha 0 keeps
the RGB bytes: an ImageData blends nothing). `data:getPixel(x, y)` returns
four numbers. Both methods floor x and y, as real LÖVE 11.5 does
(`0.99` is pixel 0, `-0` is pixel 0, `-0.5` is outside), and a floored
pixel outside the ImageData (NaN, infinities and 2^32 + 3 included) is an
error in both. Each component becomes a byte as real LÖVE does, in a third
way: clamped to [0, 1], times 255 plus 0.5 in **double** precision,
floored, i.e. half up: `0.5` stores 128 where `setColor(0.5)` draws 127,
and `2.5/255` stores 3 where `clear(2.5/255)` gives 2. `getPixel` returns
each byte as `byte / 255` in double, exactly the Lua expression, so a
`getPixel` value written back with `setPixel` keeps its byte and passed to
`setColor` draws it. Every refusal leaves the ImageData unchanged and is an
error starting `love.image.newImageData: `, `ImageData:setPixel: ` or
`ImageData:getPixel: `: another argument count (an extra argument too), a
non-number (no string coercion, no table form of `setPixel`), a
non-finite component, a size that is not an integer from 1 to 1024, a
format argument, a dot call instead of a colon call. **The rule for the
game: write palette bytes as `byte / 255`; read a byte back as
`math.floor(v * 255 + 0.5)` or compare it with `byte / 255`; a `getPixel`
value may go straight to `setPixel` or `setColor`; use integer
coordinates.** Captures: `imagedata_rounding`, `imagedata_forms`.

**Image files.** `love.image.newImageData(name)` takes exactly one Lua
string, a name of the fake disk, and returns a new ImageData of the PNG's
size holding its RGBA8 bytes, the same kind of object as
`newImageData(w, h)`. The bytes are read as `love.filesystem.read` reads
them: a file the game wrote first, as LÖVE's save directory shadows the
source, then the game directory, subdirectories included. Each call decodes
anew: two loads of one file are independent, as in LÖVE (measured). Real
LÖVE decodes with lodepng, the fake with `stb_image.h` behind a gate of its
own; the two give the same bytes on every well-formed 8-bit PNG measured
(52 probes, the five fixtures and both assets). The fake accepts exactly 8-bit
RGB and RGBA, interlaced or not, any row filters, a `tRNS` colour key on
RGB (alpha 0, RGB kept), image data split over several `IDAT` chunks and
bytes after `IEND`; ancillary chunks and a suggested `PLTE` are ignored,
and **no gamma is applied**, as in LÖVE (`gAMA`, `sRGB`, `cHRM` measured).
It refuses: 16-bit PNGs, which LÖVE turns into an `rgba16` ImageData;
grey, grey-alpha and palette PNGs, measured identical but unused (palette
indices beyond the palette differ besides); every file LÖVE refuses and
stb_image would decode (a bad chunk CRC, a bad zlib Adler-32, image data of
the wrong length), and the files both refuse (bad signature, unknown
critical chunk, no `IEND`, bad filter byte, truncated file); sides above
1024, the ImageData limit, which LÖVE does not have (2048×2 measured). The
fake is stricter than LÖVE on the form too: no File or FileData, no format
argument, no trailing nil, no leading `/`. Every refusal changes nothing and
is an error starting `love.image.newImageData: `: the argument count or
type, `invalid path "<name>": <reason>` as `love.filesystem` gives it,
`Could not open file <name>. Does not exist.` (LÖVE's wording, a directory
included), or `"<name>" is not a PNG the fake decodes as LÖVE 11.5 does:
<reason>`. A translucent alpha loads and `getPixel` reads it, but
`newImage` refuses it. **The rule for the game: load each asset once, in
`love.load`, by its bare name in the game directory; keep the assets 8-bit
RGB or RGBA (both assets are RGB); make no Image from an
ImageData holding a translucent alpha.** Capture: `imagedata_files`.

**Images.** `love.graphics` holds exactly `clear`, `draw`, `newCanvas`,
`newImage`, `newQuad`, `points`, `pop`, `push`, `rectangle`, `setCanvas`,
`setColor`, `setDefaultFilter` and `translate` from `main.lua` on. `newImage(imagedata)` takes exactly one ImageData and
copies its size and bytes at the call: a later `setPixel` on the ImageData
leaves the Image as it was, as in LÖVE. `newQuad(x, y, w, h, sw, sh)` takes
six integers: reference sides 1 to 1024, origin from 0, size from 1, the
quad inside the reference size. `draw(image, quad, x, y)` is the only draw
form onto a canvas: exactly an Image, a Quad of the image's own size, and
two integers of magnitude below 2^20, also once translated (see
Transforms). Texel (`qx + i`, `qy + j`) goes to pixel (`x + i`, `y + j`); pixels off the canvas are clipped silently, so a draw
far off draws nothing. An alpha-`00` texel leaves the pixel's four bytes,
transparent pixels included; an alpha-`FF` texel writes, per channel,
`round(c × t / 255)` (c the current colour's byte, t the texel's: a fourth
colour rule, neither the floor nor llvmpipe's 8-bit blender multiply) and
alpha `FF`. With white, the default colour, that is the texel itself. With
the screen selected, a draw changes nothing the fake models. Images and
Quads have no methods and are freed by the garbage collector. Every
refusal is an error starting `love.graphics.newImage: `,
`love.graphics.newQuad: ` or `love.graphics.draw: ` and changes nothing:
another argument count (a trailing nil and the quad-less
`draw(image, x, y)`; six arguments are taken as the present below, which
refuses an Image), a file name, a table or a Canvas where an
ImageData or Image belongs, a non-number (no string coercion), a
non-finite or fractional value, a value out of range, a quad whose
reference size differs from the image's (it would scale), and an
ImageData holding an alpha byte other than `00` and `FF`, named by its
pixel. Real LÖVE accepts most of these; the fake refuses them because
each one either draws texels LÖVE would blend or hides a bug. **The
filter.** LÖVE's default filter is `linear` (`getDefaultFilter()` gives
`linear linear 1`, a new Image is `linear`, wrap `clamp`). Integer draws
of quads on the image's own size are byte-identical under `linear` and
`nearest` (measured on 8 images and about 2 400 quads at random
positions): the fake does not model filtering and needs no `nearest` to be
exact. From
1/512 of a pixel off an integer, `linear` blends texels into colours
outside the palette, and `nearest` drops or doubles texel columns near
half a pixel: hence the refusals. `setDefaultFilter`: see Default
filter. A translucent texel blends with llvmpipe's 8-bit
arithmetic (`min(255, mul(s, a) + mul(d, 255 − a))`,
`mul(p, q) = (p·q + (p·q >> 8) + 128) >> 8`), never into a palette byte,
hence its refusal. **The rule for the game: build
images with alpha exactly 0 or 1; call `setColor(1, 1, 1)` before drawing
an image's own colours (a palette colour tinting an `FF` texel gives
exactly that colour, any other tint gives non-palette bytes); pass
integer positions, flooring fixed-point coordinates first; build each
quad with integers, inside the image, on the image's own size.**
Captures: `draw_quads`, `draw_tint`.

**The present.** `draw(canvas, x, y, 0, s, s)` is the only other draw
form: exactly six arguments, a Canvas, two integers of magnitude below
2^20, a rotation equal to 0, and two equal integer scales from 1 to 16.
It is accepted only while the screen is selected, the current colour is
white `FFFFFFFF` and the current offset is 0 on both axes, and it changes
nothing the fake models, as every screen draw. Every refusal is an error
starting `love.graphics.draw: ` and changes nothing (the arguments are
checked before the state): a canvas selected
(real LÖVE refuses a canvas drawn onto itself, and the fake does not model
scaled draws between canvases), a non-white colour (it tints the window),
a non-zero offset (it shifts it), unequal, fractional or out-of-range
scales (they blur or distort it), another argument count, a
non-Canvas first argument, a non-number (no string coercion), a
non-finite or fractional position and a rotation other than 0. **The rule
for the game: present canvas 0 once per frame, last in `love.draw`, with
the screen selected, after `setColor(1, 1, 1)`, outside any `push`, at
`(0, 0)` and scale `SCREEN_SCALE`** (`screen.screen_present()`). Capture:
`game_frame`.

**Transforms.** `push()`, `pop()` and `translate(dx, dy)` are the only
forms. The fake keeps a current offset, integer on each axis, and a stack
of up to 128 saved offsets. `translate` adds to the current offset;
`push` saves a copy of it; `pop` makes the last saved one current again.
Neither touches the colour or the selected canvas, as LÖVE's default
`push` saves the transform only. `points`, `rectangle` and `draw` apply
the offset as real LÖVE 11.5 does on Mesa llvmpipe, each sum a separate
single-precision rounding before the rules above: a point coordinate
becomes `(float) x + (float) dx`; a rectangle's near edge
`(float) x + (float) dx` and its far edge
`((float) x + (float) w) + (float) dx`, never `(x + dx) + w`; a draw
position `x + dx`, an exact integer sum. A double sum lights another
pixel near the 1/512 ties (60 of 60 measured point cases and 24 of 24
rectangle cases follow the float rule). With offset 0 nothing changes. Each
of `dx` and `dy` is an integer of magnitude below 2^20, and the current
offset stays below 2^20 on each axis; a translated point coordinate, a
rectangle's translated near edge and a translated draw position must stay
below 2^20 too, refused with `out of range after translate`. The 129th
nested `push` fails with LÖVE's `Maximum stack depth reached (more pushes
than pops?)`, a `pop` with nothing pushed with `Minimum stack depth reached
(more pops than pushes?)`. As LÖVE's `love.run` calls `origin()`, the
current offset goes back to 0 right before each frame's draw step, with or
without `love.draw`; the stack is kept from frame to frame and nothing is
checked at a frame's end, so a leaked `push` fails at the 129th nesting, as
in LÖVE. The offset set in `love.load` holds in the first `love.update`,
the one left by `love.draw` in the next `love.update`. The offset belongs
to the graphics state, not to a canvas: a translation made with the screen
selected holds after `setCanvas(canvas)`. `clear` ignores it and fills the
whole canvas. Every refusal is an error starting `love.graphics.push: `,
`love.graphics.pop: ` or `love.graphics.translate: ` and changes nothing:
an argument to `push` or `pop` (`push("all")`, `push("transform")`, a
trailing nil), another argument count to `translate`, a non-number (no
string coercion), a non-finite or fractional value, a value of 2^20 or
more, an offset that would reach 2^20. Real LÖVE accepts most of these:
a fractional offset puts integer rectangle edges on pixel-centre ties and
blends image texels into colours outside the palette (`translate(1/512)`
already changes 37 pixels of a draw), and `push("all")`, coercion and
NaN are not modelled. **The rule for the game: port `camera(x, y)` as
`push()`, `translate(-x, -y)`, the drawing, `pop()`, with the integer
camera; keep `points(x + 0.5, y + 0.5)`, integer rectangles and integer
draws under it; never translate by a fraction; pair every `push` with a
`pop` in the same frame.** Capture: `translate_stack`.

**Default filter.** `setDefaultFilter("nearest", "nearest")` is the only
form. It is accepted at any time and any number of times (in any
callback, with a canvas or the screen selected, before or after any
Image or Canvas exists), checks its arguments and stores nothing. Real
LÖVE 11.5 on llvmpipe: the default is `linear linear 1`; the call gives
`nearest` only to Images and Canvases created after it, and an existing
one keeps its filter. Every pixel the fake models is byte-identical
under both filters: every capture re-run with `nearest` set in the
harness's `love.load` (31 of 31), and 3 000 random integer quad draws
from images created under either filter, tinted or not. Fractional
shifts still differ under `nearest` (a 0.5 shift drops or doubles texels
in 7 249 pixels of those draws, −0.5 in 14 945), so fractional positions
stay refused. Every refusal is an error starting
`love.graphics.setDefaultFilter: ` and changes nothing, checked in this
order: another argument count (one name, an anisotropy third argument,
a trailing nil), a non-string (no number coercion), an unknown name
with LÖVE's message `Invalid filter mode '<v>', expected one of: 'none',
'linear', 'nearest'` (case-sensitive), then `"linear"` or `"none"`. Real
LÖVE accepts one name, an unchecked anisotropy, mixed pairs, `linear`
and `none`: the fake refuses them because `linear` and `none` would blur
the window it cannot see, and anisotropy is not measured. A capture
program may call it at its top level, unlike `translate`: the fake
stores nothing, and in real LÖVE the program's images are all created
after its own top-level call in either harness, while the harness
canvas is only read back, never sampled. **The rule for the game: call
`setDefaultFilter("nearest", "nearest")` once, as the first graphics
call of `love.load`, before `newCanvas` and every `newImage`, so the
canvas is `nearest` when it is scaled up to the window; the fake cannot
check this order. Integer draws are unchanged by the call, and
fractional draws stay refused.** Capture: `filter_nearest`.

### The LÖVE functions the game may use

The fake implements exactly this list; extending it is a decision recorded
here, with its real-LÖVE capture.

- Loop: `love.conf(t)`, `love.load(arg)`, `love.update(dt)`, `love.draw()`.
  The fake calls `love.conf` with empty `window`, `modules` and `audio`
  tables and reads nothing back. The game ignores `dt`: one `update` is one
  game frame (60 per second).
- Drawing: `love.graphics.newCanvas(160, 120)`, `setCanvas`, `clear`,
  `setColor(r, g, b)` or `setColor(r, g, b, 1)`, `points(x1, y1, …)` or
  `points({x1, y1, …})`,
  `rectangle("fill", x, y, w, h)`, `newImage(imagedata)`,
  `newQuad(x, y, w, h, sw, sh)`, `draw(image, quad, x, y)`,
  `draw(canvas, x, y, 0, s, s)` onto the screen only (see The present),
  `setDefaultFilter("nearest", "nearest")`,
  `push()`, `pop()`, `translate(dx, dy)` with integer offsets.
- Images: `love.image.newImageData(w, h)`, `love.image.newImageData(name)`,
  `ImageData:setPixel(x, y, r, g, b[, a])`, `ImageData:getPixel(x, y)`.
- Pad: `love.joystick.getJoysticks`, `Joystick:isGamepadDown`, the callbacks
  `love.gamepadpressed` and `love.gamepadreleased`.
- Files: `love.filesystem.read`, `write`, `getInfo`.

Reading anything else from `love`, a module table or an object, or creating
a field in `love` or a module table, fails at that line with
`love.timer is not in the function list of docs/love-port/design.md`
(`love.graphics.getWidth`, `ImageData:mapPixel`, `love[1]` for other keys).
The six callbacks are the only fields the game may set on `love`; before it
sets them, and for the four modules while `conf.lua` runs, they read `nil`.
It is an ordinary Lua error, like every refusal of the fake. Both binaries,
`tmp/fake_love` and `tmp/love_capture_check`, run under this guard.

The game shows its canvas in the window with `draw(canvas, 0, 0, 0,
SCREEN_SCALE, SCREEN_SCALE)`: **present canvas 0 once per frame, last in
`love.draw`, with the screen selected, after `setColor(1, 1, 1)`, outside
any `push`, at `(0, 0)` and scale `SCREEN_SCALE`** (see The present). The
fake reads the canvas, and the present changes nothing it models.

The exact pixel conventions (where `points(x, y)` lands, how `rectangle`
rounds) are those of real LÖVE 11.5, settled by the captures of
`tools/love_capture/` and written down here once captured.

### Real-LÖVE captures (`tools/love_capture/`)

Small LÖVE programs, each exercising some functions of the list, and the PNG
real LÖVE 11.5 draws for each, read from the canvas
(`Canvas:newImageData():encode("png")`, never a window screenshot).
In the fake, `tmp/love_capture_check` runs them under the function-list
guard, so their drawing part uses listed functions only, but with any
opaque colour allowed in `setColor` and no frame check: they measure
LÖVE's conversions and transparent pixels. `make love-capture` runs them in real LÖVE under Xvfb with Mesa software
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
its target. A capture program reads a fixture file as `files/<name>`:
the capture directory's `files/` directory, reached through a relative
`files` symlink in each harness, so both resolve it to the same bytes.
Fixtures are committed bytes, never regenerated by `make`, and are neither
programs nor checks: only the top-level `.lua` and `.png` files are.
`files/sprites.png` and `files/font.png` are byte copies of the game's
`game/` assets as of step 3.15, kept as fixtures and never synchronised
with `game/`: they pin drawing conventions on real asset bytes, not the
assets. A scratch `LOVE_CAPTURE_DIR` must copy `files/` too. A capture program never
changes the transform at its top level: the real harness loads it inside
`love.draw`, after LÖVE's `origin()`, the fake one in `love.load`, before
the fake's reset, so such a translation would hold in real LÖVE only. `make love-check` only
checks the installed LuaJIT.

## How the Lua game draws

The game only uses functions LÖVE draws pixel-exact, so what the e2e check is
what the player sees:

- **Colours:** a 16-entry table holding the RGB values of the Capture
  table, passed exactly to `setColor` (byte/255).
- **Circles and lines:** drawn by the game itself, pixel by pixel (`points`)
  or in spans (`rectangle`), with the algorithms of the C game the Lua was
  copied from.
- **Fill patterns** (PICO-8 `fillp`): the game builds them as images and draws
  those. The pattern is aligned to the **world** coordinates, the ones before
  the camera, as the C game aligned it, not to the screen or the shape: each span
  draws the quad of a screen-sized pattern image (one per pattern, colour
  byte and transparent flag, built once and kept) that starts at the world
  phase of its first pixel. A patterned disk is drawn span by span from that
  image, like a plain one, rather than built once per radius.
- **See-through colours and colour swaps** (PICO-8 `palt` and the 16 palette
  blocks of `hud.lua`): the game builds a recoloured copy of the sprite sheet per
  combination it uses, filling each 8×8 cell the first time a draw reaches
  it, and keeps it. Flat shapes
  apply the swap by choosing the colour before drawing.
- **Text:** the game places each letter itself from `font.png`, as pieces of
  an image: either per-colour letter images, or one image whose lit cells
  are white `FF` texels (every other texel alpha 0) drawn under
  `setColor(palette colour)`. Both give exact palette bytes, since a tint
  of white gives `round(c × 255 / 255)`, which is c (measured in
  `game_frame`). Outlined text is the word drawn shifted one pixel in each
  direction in the outline colour, then the word on top.
- **Sprites and the map:** pieces of `sprites.png` (or of its recoloured
  copies), drawn with quads.

**In combination.** Real LÖVE 11.5 and the fake agree when these functions
are mixed in a game frame: the `game_frame` capture (camera over a map,
pattern rectangles, discs of spans, rings of points, sprites, outlined
text in a nested `push`/`translate`, a HUD, another canvas, screen draws
including the present `draw(canvas, 0, 0, 0, 4, 4)`, a second frame over
the first), and once, at step 3.15, 60 random scenes
of 400 calls each mixing palette and white colours, nested integer
translations, point tables, rectangles partly off the canvas, quads of
see-through images, canvas switches and screen draws (60 of 60 equal,
136 distinct colours); the frame is also byte-identical in real LÖVE
without its `setDefaultFilter` line. Colour, transform and default filter
are graphics state: set once, they hold across `points`, `rectangle`,
`draw`, canvas switches and screen draws until changed; `clear` ignores
the transform and the colour. An Image built in the middle of `love.draw`,
a canvas selected, draws like one built in `love.load`. **The rule for
the game: set the colour before every primitive and `setColor(1, 1, 1)`
before every image drawn in its own colours (a leftover palette colour
tints the texels into colours outside the palette); undo every
translation before leaving the canvas for the present, or wrap the frame
in `push`/`pop`; build see-through and recoloured copies with `getPixel`
compared with `byte / 255` and `setPixel` of palette bytes.** Capture:
`game_frame`.

### The asset PNGs

`game/sprites.png` and `game/font.png` are committed sources, edited
directly; no tool writes them. `make test` checks that both decode in the
fake, opaque, at their sizes (`test_fake_love_png_file`), and the replays
check every pixel the game draws from them.
`game/p8/sprite_sheet_packed.lua` holds the same sprite sheet for the
PICO-8 runtime, and nothing checks that the two agree: an edit to the
sheet changes both.

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

The game computes with PICO-8 16.16 fixed-point numbers and its own random
generator, both in `game/p8/fixed.lua`, built on LuaJIT's `bit` library and
bit-exact with the C library it was copied from. `make test` runs
`lua-test-fixed`, which checks the edge values and a sample against
`tests/lua/fixed_reference.txt`, frozen from the C library before plan 4
deleted it.

## Layout of the fork

- `game/`: the Lua game (`conf.lua`, `main.lua`, `sprites.png`, `font.png`,
  and the modules of `p8/`, `engine/`, `game/`, `devtools/` and `love/`);
  `make love` zips it into `tmp/slipways.love`.
- `tools/fake_love/`: the fake LÖVE's C source; built as `tmp/fake_love`
  and `tmp/love_capture_check`.
- `tools/love_capture/`: the capture programs, their PNGs and the fixtures
  they read (`files/`).
- `tools/`: the scripts behind the targets (`love_e2e_run.sh`,
  `end_state.sh`, `e2e_limits.sh`, `lua_guard.lua`, `love_install.sh`, …).
- `tests/`: the C tests of the fake and the tools (`test_<name>.c`, listed
  in the Makefile's `UNIT_TESTS`), with their fixtures in `tests/fixtures/`.
- `tests/lua/`: the Lua tests of the fixed-point library, the runtime, the
  module tables and the capture reader, with their frozen references.
- `tests/lua/game/`: the regression tests of `game/` modules
  (`test_<module>.lua`), each run by `make test`.
- `tests/e2e/`: the 20 replays, their hashes, end states and notes.
- `worst-case/`: long games checked one at a time with `make end-state
  EVENTS=`.

## Conventions inherited from the C

`game/p8/`, `game/engine/`, `game/game/` and `game/devtools/` began as a
faithful copy of the C game: each C file became the Lua file of the same
name under the same directory, with the same functions in the same order,
the same names, computations and data tables. The C is deleted; its last
commit shows any original, e.g. `git show 60e62c1:src/game/planet.c` for
`game/game/planet.lua`. The Lua still follows the rules below, which say
how a C construct reads in Lua, until plan 5 replaces them. `game/love/`
has no C original.

**Naming.** Exported functions are `function M.<c name>(` at line start,
`static` ones `local function <c name>(`; a module's file-level tables are
`M.NAME = {` or `local NAME = {`, its header constants and enum values
`M.<MACRO> = value`, computed as the C does; a `.c`-local macro is a module
local. A C identifier that is a Lua keyword takes a trailing underscore
(the slipway's `end_`). A module local that would
clash with a C parameter or local takes the `_module` suffix
(`local planet_module = require("game.planet")` in `save_load.lua`). A
function the Lua adds is marked `-- Lua only` and, when another module
needs it, exported after the C functions as `M.p8_<file>_<what>` in
`game/p8/` (`p8_gfx_software_slot_pixels`) or with a C-style name
elsewhere (`planet_keys_zero`). `game/love/` names follow
`module_verb[_object]`.

**Arrays and integers.** A C array keeps its C index, from 0
(`{ [0] = 1, 0, 1, 2 }`), 2-D as `t[y][x]`; `#t` and `ipairs` are never used
on one, and `memcpy`/`memcmp` are loops over the C length. A pointer into an
array is an integer index, `NULL` is `nil`; a pointer difference into a
table goes through a Lua-only reverse map built at load
(`project_index_of` in `research.lua`). Lists handed to LÖVE, the trade
route lists, the id lists (a planet's slipways, a route's planets and
slipways, the UI's ephemerals and hover layers, a wormhole's anchors),
needs, project inputs, invented technologies, particles, transit dots, the
registry's buckets (live, per-message, per-tag) and the exit queue are plain
1-based arrays without holes; the data tables shaped `{ items, count }`
stay 0-based until the 1-based step. C `int` `/` is `cint.div` (truncates, never `-0`, raises on 0) and
`%` is `cint.mod` (the dividend's sign); `math.floor` and Lua `%` appear only
where the C floors. `(unsigned char)x` is `bit.band(x, 0xFF)`; a `bit`
result above 2^31 − 1 is brought back to unsigned where it is compared or
printed. Fixed point goes through `game/p8/fixed.lua`. A `char` is its byte
value, a `const char *` or `char[]` a Lua string, and a walk to `'\0'` a loop
over `1 .. #s`.

**Structs and out-parameters.** A struct is a table with the C field names;
a pointer to it is that table, and an init on `T *` fills the caller's table.
A by-value struct (passed, returned, assigned, a constant) is a new table
built by a constructor listing every field, copied at each use; a table
received by value is never mutated. The copy is shallow as in C: pointer
members are shared, inline mutable
sub-structs are copied by the caller. An entity id is a number handed out by
`entity_id_take`, never reused, resolved by `entity_get` through the current
table, which drops an entity when the exit queue flushes it; `nil` is "no
link"; ids are compared with `==`. A re-init or `memset` of a table that another holder may
point at zeroes its fields in place and keeps its identity:
`pad_nav_init` keeps its sub-tables and bindings table,
`input_step_init` zeroes `played` and `played_frame` in place,
`economy_score_set_difficulty` clears and refills `g_difficulty`. A
static struct, or inline array of structs, is a table (or 0-indexed array of
tables) with every field at its C zero value (0, or `nil` for a pointer). A
zero constructor other modules use is exported by the module owning the
struct (`button_zero` in `ui_button`, `emitter_zero` in `particle`,
`targeting_zero` in `targeting`); a module needing one its owner does not
export keeps a local one. A C `int *` context is
a `{ value = 0 }` table. A by-value `p8_graphics_t` is a local
`graphics_copy` of its five fields. An out-parameter is an input argument
whose new value comes back after the C return value, several in C order, a
`void` function returning only them (`result, state =
p8_fixed_rnd_from_state(state, x)`; `pad_panel_step` returns `at, x, y,
mouse_buttons`).

**Memory.** A new object is a table built where it is needed. The
plain-array lists are emptied or refilled in place when several owners hold them; a list that
outlives a copy point is copied, never shared by accident (an offer's routes
are a copy of the source list, assigned back when the offer returns); a
route's id arrays are never changed once built, so route copies share them.
An id list is read through `entity_id_resolve`, which asserts the id is
alive. A walk whose body may change its list walks a copy and says so.
Strings are Lua strings, compared with `==`.

**Truth values.** A C int predicate returns 1 or 0, never a Lua boolean;
`if (x)` on an int is `if x ~= 0`, `!x` is `x == 0` as a test and
`(x == 0) and 1 or 0` as a value, a comparison used as a value is
`(c) and 1 or 0`, and a pointer test is `p ~= nil`. `c ? a : b` is
`(c) and a or b` only when `a` is never `nil` or `false`; a ternary between
two function pointers is an `if`/`else`.

**Control flow and errors.** `assert(c && "msg")` is `assert(c, "msg")`, a
compile-time check a load-time `assert`; `switch` is an `if` chain or a
lookup table built once; `continue` is `goto continue`; a C `for` is a
numeric `for` only when the body changes neither counter nor bound.
Function pointers are Lua functions, a vtable a table with the C field
names; an X-macro list is a module function calling its entry function once
per entry, in order (`P8_IMAGE_SLOTS(SLOT)`). `fatal.p8_fatal(msg)` raises
`error(msg, 0)`; `snprintf` messages are `string.format` with the C text.

**Modules and loading.** One module per C pair, required by its path under
`game/` (`require("p8.gfx")`), returning `M`, headed by a comment naming the
C files it was copied from, in order; a function declared in a ported
header but defined elsewhere is named in that comment and called from its
defining module, never re-exported. No globals,
LuaJIT `bit` only, no FFI. Statics are module-level `local`s initialised as
the C does; a forward-declared `static` is a `local name` filled later by
`name = function`, never `local function name`, and a table entry naming
one is stored right after its definition
(`M.galaxy_menu_on_click_table[3]` in `menu.lua`). Modules of a C include
cycle require each other at call time, inside the function that needs the
partner, and read a partner's constants there, never cached at module level.
A partner needed at load (a class holding its methods by identity, a table
built from its value) is required at load in one direction per pair only:
`planet` requires `planet_display` and `planet_trade` at load, they require
`planet` at call time. `love.graphics` and `love.image` appear only in
`game/love/`.

**Data tables and checks.** Every table of a data file is a module field
(LuaJIT allows 200 locals per function); a struct the C partly initializes
goes through a local constructor, defined before the tables that call it at
load, that fills each missing field with its C zero and rejects a field the
struct lacks (`map_tables.map_tables_check_fields`); a positional C
initializer gets a positional constructor. A pointer in a table is the table
pointed to. The data modules once generated from the C (`p8.quarter_wave`,
`p8.sprite_sheet_packed`, `game.demo_data`, `game.bindings_default`) are
now the source, edited by hand; their `-- Generated by` headers still name
the deleted generators, and nothing checks `game.bindings_default` against
`bindings.default.cfg`. `make test` runs two checks over the whole
tree: `lua-guard` (every file compiles, no undeclared global, only this
file's `love.` names) and `lua-test-modules`, which requires every module
except `main` and `conf` under plain LuaJIT and reports each one that fails
to load; it clears the sentinels a failed `require` leaves in
`package.loaded`, so each failing module reports its own missing module.

## Rules for the fork

- The e2e replays are the source of truth; no C unit test was ported to
  Lua.
- `make e2e` runs under 9 seconds (the rule of `CLAUDE.md`). The replays
  run in parallel, longest first, LuaJIT with its JIT on.

## Open questions

- Which LÖVE version does the love2d studio iOS app run? 11.x keeps everything
  aligned.
- Keyboard play on PC: not needed by the e2e, undecided.
