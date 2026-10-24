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
with `stb_image.h`, vendored as a single file, and written by the fake's own
PNG module (8-bit RGBA, stored deflate blocks).

For one replay it:

1. loads the game directory (`game/`) as LÖVE would: `conf.lua`, then
   `main.lua`;
2. builds the fake disk before `love.load`: `save.bin` holds the bytes of
   the replay's `initial_save` (hex string) when present, `bindings.cfg` the
   content of the file named by `bindings` (resolved next to the replay, at
   most 65 536 bytes), `pad.png` the content of the file named by
   `pad_sheet` (resolved next to the replay, at most 1 048 576 bytes, not
   decoded here), which shadows the game directory's `pad.png`; no other
   file is in memory. A `bindings` or `pad_sheet` file that is missing,
   unreadable or over its limit fails the run before `love.load`, exit 1,
   with no capture, state or PNG
   (`fake_love: <replay>: "pad_sheet": <path> cannot be read`,
   `… is over 1048576 bytes`). Writes from the game stay
   in memory; the real disk is never written by the game;
3. calls `love.load(arg)`, `arg` being `{"--attract-after", "N"}` when the
   replay has `"attract_after": N` (plain decimal), `{}` otherwise;
4. for each frame of `frames`: on a frame carrying `resize`, makes the new
   window size current and calls `love.resize(w, h)`; then starts the
   frame's screen, opaque black at the window size in force (see The
   screen); then makes the frame's touches and mouse current, compares the held
   buttons with the previous frame's, calls `love.gamepadreleased` then `love.gamepadpressed` for those
   that changed, each in ascending button number, answers
   `Joystick:isGamepadDown` from the current list, calls `love.update(1/60)`
   then `love.draw()`;
5. after each frame's draw step, even without `love.draw`, reads canvas 0
   (the first created), turns each opaque pixel's RGB into its palette
   number 0–15, packs two pixels per byte (even x in the low nibble), row by
   row, and appends the 9 600 bytes to the capture (see Capture). The
   capture format is the one the `.json.sha256` were set with: its sha256
   is compared with the replay's `.json.sha256`; then, on a frame listed
   in the replay's `screens`, writes the whole screen to
   `<capture>.screen-f<N>.png` (`N` the frame number in plain decimal; a
   failure is `fake_love: <path>: cannot write`, exit 1). A run that fails
   at any later point removes every screen PNG it wrote, with the capture
   and the state file; a frame that fails writes no PNG, and PNGs of
   frames not listed are left alone;
6. after the last frame, once the capture is closed, calls the game's
   `debug_state()` and writes the text it returns next to the capture,
   compared with the replay's `.end-state`.

**Fatal errors.** Anything outside the contract below is fatal, with one
of two errors, each naming its cause:

1. a LÖVE function not on the list (see The LÖVE functions the game may
   use), checked when the game reads or creates the field, in both
   binaries: `love.timer is not in the function list of docs/love-port/design.md`;
2. a pixel of another colour on canvas 0, or no canvas, checked at the end
   of each frame, in `tmp/fake_love` only (see Capture):
   `fake_love: frame f3: pixel (0, 0) of canvas 0 is FF0000FF, not one of the 16 palette colours (1 such pixels in the frame)`.

The first is an ordinary Lua error, carrying the game's `file:line:`
like every refusal of the fake: uncaught, the run fails (exit 1, no
capture nor state file); caught by `pcall`, the game goes on. No game code
can catch the second. `love_capture_check` keeps the function list and
never checks frames: its programs measure real LÖVE, the palette is the
game's contract.

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
   The game's Lua tests are the exception: every `tests/lua/game/test_*.lua`
   runs in one LuaJIT process, `lua-test-game`, whose runner
   (`tests/lua/game/runner.lua`) clears the `package.loaded` entries each
   file adds; a new test file adds tests, not a process.
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

`make lua-names` (`tools/lua_names.lua`, after its fixture self-test) fails
when a `game/` export repeats its module's prefix, or when a Lua file of
`game/`, `tests/lua/` or the `love_runtime` fixtures reads a field a
required module does not have, which a missed rename leaves silent; it
loads every module, so it stays outside `make test` and `make test-all`.

### `make e2e`

`make e2e` runs `tmp/fake_love <game dir> <replay> <capture> <state>` on
every replay of `tests/e2e/`, one make job per replay, the largest replay
file (the most frames) first, every replay rerun on every invocation (make
does not see a Lua edit). It builds `tmp/fake_love` when missing. The game
directory is `LOVE_E2E_GAME`, default `game`; a `%` in it becomes the
replay's file name (`pad_title.json`), so a verification can give each
replay its own stub. The replay's `initial_save`, `bindings`, `pad_sheet` and
`attract_after` reach the game as the fake gives them; the game must define
`debug_state`.

Per replay, under `tmp/love-e2e/tests/e2e/`: `<name>.json.bin` (the
capture), `<name>.json.sha256` and `<name>.json.state` (the game's
`debug_state()`) when the fake exits 0, `<name>.json.error` otherwise
(`fake_love exited <status>` or `killed by signal <n>`, then the fake's
stderr); all four are removed before each run. These runs are shared:
`make e2e end-state` runs each replay once.

`tools/e2e_limits.sh` checks the replay limits, counting a replay's frames
as the objects of its `frames` array, whatever they hold; then each replay
is reported on one line:

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
every button its list holds. Each frame, the fake first handles the frame's
`resize` when it carries one: it makes the new size the one
`love.graphics.getDimensions()` answers, then calls `love.resize(w, h)`
(two numbers) when the game defines it; `love.resize` is never called for
the start size. It then makes the frame's
list current, with its touches and mouse (see Touch and mouse semantics), then calls `love.gamepadreleased` for each button that went
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
fails with `Invalid gamepad button: <name>`. The ascending order and
resize before the pad callbacks are the fake's convention, not LÖVE's (which
delivers events as they arrive): the game must not depend on the order of
the resize and pad callbacks within a frame.

**Touch and mouse semantics.** Polling only: no touch or mouse callback
exists. `love.touch.getTouches()` returns a new sequence of the frame's
touches, one light userdata per touch, equal to its replay id, so a finger
gives the same value on every frame it is held; the sequence follows the
frame's `touches` order, which the game must not depend on. An id absent
from a frame is a lifted touch: `love.touch.getPosition(id)` then fails
with `Invalid active touch ID`; for a held id it returns its x and y. A
frame without `touches` holds none. The mouse, `love.mouse.getPosition()`
and `love.mouse.isDown(1)`, is kept from the last frame carrying `mouse`;
before it, the mouse is at (0, 0), up, as is every touch state before the
first frame. Touches and mouse become current with the frame's pad list,
after `love.resize` (which still sees the previous frame's, like
`isGamepadDown`), and stay current until the next frame starts,
`debug_state()` included; during `love.load` there is no touch. Positions
are numbers in the units of `getDimensions()`, one unit one fake pixel
(`highdpi` is not modelled). `love.touch` and `love.mouse` are nil while
`conf.lua` runs. Any other form (`getTouches(1)`, `getPosition(1)`, an
argument to `mouse.getPosition`) raises an error naming the function;
`isDown` with anything but the single integer 1 fails with
`love.mouse.isDown: only isDown(1), the left button, is in the function list`.

**Fake disk and arguments.** The replay members the fake reads are
`input`, `frames`, `initial_save`, `bindings`, `pad_sheet`, `attract_after`,
`window` and `screens`, each at most once: `initial_save` a non-empty string
of an even number of hex digits, `bindings` the name of a `.cfg` file next to
the replay, `pad_sheet` the name of a `.png` file next to the replay, served
as `pad.png` (5 to 64 characters `A-Za-z0-9_.-`, not starting with `.`;
otherwise `"pad_sheet" <value> is not a .png file name of 5-64 characters
A-Za-z0-9_.- next to the replay`; a bad PNG fails where the game loads it),
`attract_after` a positive integer, `window` the window size
`[w, h]` for the run until a resize (640×480 when absent), `screens` the
frames whose screen is written as a PNG (see The screen): an array,
possibly empty, of at most 16 integers, each a frame of the replay
(0 to frame count − 1, frames counted as the objects of `frames`, touch-only
and `{}` frames included), strictly ascending, refused otherwise with the
file, the byte offset and the problem (`"screens" is <kind>, not an array
of frame numbers`, `"screens" entry <v> is not an integer`, `"screens"
entry <n> is not a frame of this replay (0-<last>)`, `"screens" entry <n>
is not after <previous>`, `"screens" lists <n> frames, more than 16`,
`"screens" given twice`); no replay of `tests/e2e/` carries `screens` or
`pad_sheet`; `start` is
refused, other members are ignored. A frame holds `pad`, `resize`,
`touches` and `mouse`, each optional and at most once; any other frame key
is refused. `pad` lists the held buttons; a frame without it holds none.
`resize: [w, h]` is the window size from the start of that frame on, which
must differ from the size in force (LÖVE calls `love.resize` only on a
change). `touches: [[id, x, y], …]` lists at most 10 held touches, each id
an integer from 1 to 2147483647 given once in the frame, the same id on
consecutive frames being the same finger. `mouse: [x, y, left]` sets the
mouse from that frame on, `left` 0 (up) or 1 (down). Touch and mouse
coordinates are integers with 0 ≤ x < w and 0 ≤ y < h for the window in
force on that frame, after its own `resize`; a kept mouse is not checked
again after a resize. `window` and `resize` are
each two integers from 1 to 4096, in LÖVE units, one unit one fake pixel
(`highdpi` is not modelled; `t.window` set by `love.conf` is ignored). A refusal exits 1 before `love.load` and leaves no output.
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

**Colour.** `setColor(r, g, b)` and `setColor(r, g, b, a)` are the only
forms: three or four finite Lua numbers. The current colour
is opaque white (`FFFFFFFF`) until the first `setColor`, then the last one
set, for the whole run: from `love.load` on (before any canvas exists),
across frames, canvas switches, `setCanvas()` and `clear`, which ignores
it and leaves it unchanged. Each component, alpha included, becomes a byte
as real LÖVE 11.5 does: to single precision, clamped to [0, 1], times 255
in single precision, **truncated**, unlike `clear`, which rounds: a
component 0.5 gives 127 with `setColor` and 128 with `clear`, 0.3 gives 76,
`128/255 × (1 − 2^-30)`, one float step below `128/255`, gives 127, and an
alpha 2 gives `FF`. Any four bytes are a legal colour. Every refusal is an
error starting `love.graphics.setColor: ` and leaves the current colour
unchanged: another argument count (`takes 3 or 4 numbers, not 5
arguments`), a non-number (no string coercion, no table form) and a
non-finite component. A point, rectangle or texel drawn onto a canvas
takes an opaque colour's bytes over any pixel, transparent included; with
a translucent colour (alpha not `FF`, `00` included) `points`, `rectangle`
and `draw(image, quad, x, y)` refuse, once their own arguments are
checked, whether or not a pixel would land, and change no pixel, because
real LÖVE blends a translucent colour with the pixels under it and the
fake does not model the blender on a canvas:
`love.graphics.points: the colour's alpha is 7F, not FF (a translucent colour is drawn only onto the screen: blending on a canvas is not modelled)`.
Onto the screen, any colour is accepted and the draw is tinted and blended
(see The screen); points and rectangles onto the screen model nothing.
`setColor`, `clear`, `setPixel` and texels are not checked against the
palette: on a scratch canvas any opaque bytes are legal, and what reaches
canvas 0 is judged at the end of the frame (see Capture). **The rule for
the game: on a canvas, set only palette colours and white, with no alpha;
pass each palette byte as the Lua expression `byte / 255`**, which always
gives that byte; a component computed any other way can lose a whole byte.
White is the colour that tints texels to themselves (see Images). **On the
screen, any colour.** Captures: `color_bytes`, `color_precision`,
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
<reason>`. A translucent alpha loads, `getPixel` reads it and `newImage`
takes it; an Image holding a translucent texel is drawn only onto the
screen (see Images). **The rule for the game: load each asset once, in
`love.load`, by its bare name in the game directory; keep the assets 8-bit
RGB or RGBA; an asset drawn onto a canvas (`sprites.png`, `font.png`, both
RGB) has alpha exactly 0 or 1; an asset drawn only onto the screen (the pad
sheets) may be RGBA with any alpha.** Capture: `imagedata_files`.

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
the screen selected, the same checks hold, any colour is accepted, and the
quad is drawn onto the screen at the translated integer position with
scales 1 and 1 (see The screen). `newImage` remembers the Image's first
texel in row order whose alpha is neither `00` nor `FF`, found once at the
call; such an Image drawn onto a canvas is refused, after the translucent
colour, with
`love.graphics.draw: the image's texel (<x>, <y>) is <RRGGBBAA>, translucent (an Image with translucent texels is drawn only onto the screen: blending on a canvas is not modelled)`.
**The scaled draw.** `draw(image, quad, x, y, 0, sx, sy)`, exactly seven
arguments, draws onto the screen only: an Image (a Canvas is refused as
`argument 1 is a Canvas, not an Image`), a Quad of the image's own size, a
position `x`, `y` of two finite numbers of magnitude below 2^20 (fractions
accepted), a rotation equal to 0, and two scales, each finite, greater than
0 and at most 1024, equal or not, with the present's messages (see The
present); arguments are checked in this order, then the state: a canvas
selected (`a canvas is selected, a scaled Image is drawn only onto the
screen (scaled draws onto a canvas are not modelled)`), a non-zero offset
(`the offset is (<dx>, <dy>), not (0, 0) (a scaled draw onto the screen is
not translated)`). Any colour is accepted. Images and
Quads have no methods and are freed by the garbage collector. Every
refusal is an error starting `love.graphics.newImage: `,
`love.graphics.newQuad: ` or `love.graphics.draw: ` and changes nothing:
another argument count (a trailing nil and the quad-less
`draw(image, x, y)`; six arguments are taken as the present below, which
refuses an Image, seven as the scaled draw; the message is `takes 4
arguments, an Image, a Quad, x and y, 6, a Canvas, x, y, 0, s and s, or 7,
an Image, a Quad, x, y, 0, sx and sy, not <n>`), a file name, a table or a Canvas where an
ImageData or Image belongs, a non-number (no string coercion), a
non-finite or fractional value, a value out of range, and a quad whose
reference size differs from the image's (it would scale). Real LÖVE accepts most of these; the fake refuses them because
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
hence its refusal on a canvas; on the screen it is modelled (see The
screen). **The rule for the game: build images drawn onto a canvas with
alpha exactly 0 or 1 (images drawn only onto the screen may hold any
alpha); call `setColor(1, 1, 1)` before drawing
an image's own colours (a palette colour tinting an `FF` texel gives
exactly that colour, any other tint gives non-palette bytes); pass
integer positions onto a canvas, flooring fixed-point coordinates first;
scale and place fractionally only onto the screen; build each quad with
integers, inside the image, on the image's own size.**
Captures: `draw_quads`, `draw_tint`.

**The present.** `draw(canvas, x, y, 0, s, s)` is the only other draw
form: exactly six arguments, a Canvas, a position `x`, `y` of two finite
numbers of magnitude below 2^20 (fractions and off-window values
accepted), a rotation equal to 0, and two equal finite scales greater than
0 and at most 1024 (below 1 included). It is accepted only while the
screen is selected, the current colour is white `FFFFFFFF` and the current
offset is 0 on both axes, and only for a canvas with no translucent pixel;
it then draws the canvas onto the screen framebuffer (see The screen).
Every refusal is an error starting `love.graphics.draw: ` and changes
nothing (the arguments are checked before the state, the canvas's pixels
last): a canvas selected
(real LÖVE refuses a canvas drawn onto itself, and the fake does not model
scaled draws between canvases), a non-white colour (it tints the window),
a non-zero offset (it shifts it; the position argument is the present's
offset), unequal scales (they distort it, printed with `%g`), a scale that
is zero, negative, above 1024 or non-finite
(`argument <n> is out of range (s greater than 0, at most 1024)` or
`argument <n> is not finite`), a position of magnitude 2^20 or more
(`argument <n> is out of range (|v| >= 2^20)`) or non-finite, another
argument count, a non-Canvas first argument, a non-number (no string
coercion), a rotation other than 0, and a canvas holding a pixel whose
alpha is neither `00` nor `FF`, the first in row order:
`love.graphics.draw: the canvas's pixel (<x>, <y>) is <RRGGBBAA>, translucent (a translucent canvas is not modelled on the screen)`.
The canvas's 19 200 alpha bytes are scanned at every present. **The rule
for the game: present canvas 0 once per frame, last in `love.draw`, with
the screen selected, after `setColor(1, 1, 1)`, outside any `push`, at
the position and scale of the game rectangle that `love.window_layout`
computes from `getDimensions()`** (`screen.present()`). Capture:
`game_frame`.

**The screen.** The fake keeps an RGBA8 screen framebuffer of the window
size, opaque black `000000FF` at the start of every frame, after that
frame's resize and `love.resize`. Every screen draw reaches it: the
present, the 4-argument quad draw with the screen selected and the scaled
draw (points, rectangles and `clear` with the screen selected change
nothing the fake models). The rasterisation rule, in double precision, for
a source rectangle `qx`, `qy`, `qw`×`qh` (the quad, or the whole canvas for
the present) drawn at `x`, `y` with scales `sx`, `sy` (`s`, `s` for the
present, 1, 1 for the 4-argument draw): `x1 = x + qw * sx`,
`y1 = y + qh * sy`; screen pixel `(i, j)` has centre `cx = i + 0.5`,
`cy = j + 0.5` and belongs to the draw when `x <= cx < x1` and
`y <= cy < y1` (half-open: a centre on the left or top edge is in, on the
right or bottom edge out, so two abutting draws neither overlap nor leave
a gap); its texel is column `qx + floor((cx - x) / sx)` and row
`qy + floor((cy - y) / sy)`, each offset clamped to `0 … qw-1` /
`0 … qh-1` (the clamp only guards rounding), so only the quad's own texels
are sampled, nearest, and a centre on the edge between two texels takes
the right or lower one. Each texel `c` is tinted by the current colour `t`
on all four channels, `s_k = round(c_k × t_k / 255)` (the rule of Images),
then blended in LÖVE's default `"alpha"` mode with llvmpipe's 8-bit
arithmetic: each colour channel of the pixel `d` becomes
`min(255, mul(s_k, s_a) + mul(d_k, 255 − s_a))`,
`mul(p, q) = (p·q + (p·q >> 8) + 128) >> 8`, and its alpha stays `FF`
(LÖVE's alpha `s_a + mul(d_a, 255 − s_a)` with `d_a = FF` is 255). So an
alpha-`FF` source writes its bytes and an alpha-`00` source leaves the
pixel: the present (white, opaque or transparent texels) needs no blend.
**Translucent sprites are modelled on the screen, not proven opaque**: a
sprite stretched onto a phone looks better with antialiased edges. A later
draw lies over an earlier one; pixels off the window do not exist
(clipping). On a 390×844 window at
`(0, 64.75)` and scale 2.4375 = 39/16 every quantity is exact: column 18 is
texel column 7 and column 19 texel column 8 (centre 19.5 on the edge);
row 64 is black, row 65 texel row 0, row 74 texel row 4 (centre 74.5 on
the edge), row 356 texel row 119, row 357 black. Real LÖVE rasterises in
single precision on the GPU with a top-left rule that is also half-open;
this double rule is the fake's contract, verified by fake tests only, not
by a real-LÖVE capture, and so are the tint and blend on the screen. The
screen is rasterised only on the frames the
replay's `screens` lists, and no framebuffer exists in a run that lists
none; every check of every screen draw runs on every frame, listed or not,
and since the screen is black at each frame start and only `screens`
reads it, nothing else can observe the difference. A listed frame's
screen is written after its capture as an 8-bit RGBA PNG of the window
size, row 0 at the top, named `<capture>.screen-f<N>.png`. Checks:
`test_fake_love_screen`, `test_fake_love_screens`,
`test_fake_love_image_draw`, and `test_game_screens` for the game itself
(390×844, then a `resize` to 640×480).

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
here, verified by its real-LÖVE capture, or, for plan 6's extensions
(screen, window, touches), by fake C tests and screen tests.

- Loop: `love.conf(t)`, `love.load(arg)`, `love.update(dt)`, `love.draw()`,
  `love.resize(w, h)` (see Pad semantics).
  The fake calls `love.conf` with empty `window`, `modules` and `audio`
  tables and reads nothing back. The game ignores `dt`: one `update` is one
  game frame (60 per second).
- Drawing: `love.graphics.newCanvas(160, 120)`, `setCanvas`, `clear`,
  `setColor(r, g, b[, a])`, `points(x1, y1, …)` or
  `points({x1, y1, …})`,
  `rectangle("fill", x, y, w, h)`, `newImage(imagedata)`,
  `newQuad(x, y, w, h, sw, sh)`, `draw(image, quad, x, y)`,
  `draw(canvas, x, y, 0, s, s)` onto the screen only, at any finite
  position below 2^20 and an equal scale in (0, 1024] (see The present),
  `draw(image, quad, x, y, 0, sx, sy)` onto the screen only, at any finite
  position below 2^20 and two scales, equal or not, in (0, 1024] (see
  Images),
  `setDefaultFilter("nearest", "nearest")`,
  `push()`, `pop()`, `translate(dx, dy)` with integer offsets,
  `getDimensions()` with no argument: the window width and height in LÖVE
  units, whatever canvas is selected, from `main.lua` on.
- Images: `love.image.newImageData(w, h)`, `love.image.newImageData(name)`,
  `ImageData:setPixel(x, y, r, g, b[, a])`, `ImageData:getPixel(x, y)`.
- Pad: `love.joystick.getJoysticks`, `Joystick:isGamepadDown`, the callbacks
  `love.gamepadpressed` and `love.gamepadreleased`.
- Touch and mouse, polled only, no callback: `love.touch.getTouches()`,
  `love.touch.getPosition(id)`, `love.mouse.getPosition()`,
  `love.mouse.isDown(1)` (see Touch and mouse semantics).
- Files: `love.filesystem.read`, `write`, `getInfo`.

Reading anything else from `love`, a module table or an object, or creating
a field in `love` or a module table, fails at that line with
`love.timer is not in the function list of docs/love-port/design.md`
(`love.graphics.getWidth`, `ImageData:mapPixel`, `love[1]` for other keys).
The seven callbacks are the only fields the game may set on `love`; before it
sets them, and for the six modules while `conf.lua` runs, they read `nil`.
It is an ordinary Lua error, like every refusal of the fake. Both binaries,
`tmp/fake_love` and `tmp/love_capture_check`, run under this guard.

The game shows its canvas in the window with `draw(canvas, x, y, 0, s,
s)`: **present canvas 0 once per frame, last in `love.draw`, with the
screen selected, after `setColor(1, 1, 1)`, outside any `push`, at the
game rectangle's position and scale** (see The present). The
fake reads the canvas, and the present draws it onto the screen framebuffer
(see The screen), which only frames listed in `screens` rasterise.

The exact pixel conventions (where `points(x, y)` lands, how `rectangle`
rounds) are those of real LÖVE 11.5, settled by the captures of
`tools/love_capture/` and written down here once captured.

### Real-LÖVE captures (`tools/love_capture/`)

Small LÖVE programs, each exercising some functions of the list, and the PNG
real LÖVE 11.5 draws for each, read from the canvas
(`Canvas:newImageData():encode("png")`, never a window screenshot).
In the fake, `tmp/love_capture_check` runs them under the function-list
guard, so their drawing part uses listed functions only, with opaque
colours on the harness canvas and no frame check: they measure
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
  or in spans (`rectangle`); the replays fix every pixel they draw.
- **Fill patterns** (PICO-8 `fillp`): the game builds them as images and draws
  those. The pattern is aligned to the **world** coordinates, the ones before
  the camera, not to the screen or the shape: each span
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
- **The pad:** sprites of `pad.png`, cut by `love.pad_sheet`'s quads,
  drawn onto the screen only, never onto a canvas; they may be
  translucent.

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

**Window layout.** `love.window_layout.compute(width, height)` (LÖVE
units, recomputed on every call, no state) places canvas 0 and the pad's
160×120 reference rectangle in a window of any size. **Tall** when
`height > 2 * width`: the window is cut into two halves of `height / 2`,
the game fitted in the top one, the pad in the bottom one. **Overlay**
otherwise (`height == 2 * width` included): the game fitted in the whole
window, the pad fitted in the game rectangle, so equal to it. Fitting
`cw × ch` into an area takes `scale = min(aw / cw, ah / ch)`, the size
`cw × scale` by `ch × scale` and centres it; nothing is rounded, so a
rectangle is exactly what `draw(..., scale, scale)` covers. On a 390×844
iPhone: tall, scale 2.4375, game at y 64.75, pad at y 486.75, both
390×292.5. `screen.present()` draws canvas 0 at the game rectangle of the
layout of `getDimensions()` on every frame, with no `love.resize`
handler, so a resize shows on its own frame. `conf.lua` opens a
resizable, `highdpi` 640×480 window; the fake ignores `t.window`, so a
replay without `window` runs at 640×480, where the present is `(0, 0)`
at scale 4.

**Pad layout.** `love.pad_layout` (pure, no `love.*` call) holds the pad's
elements once, in reference units: origin at the top-left of the 160×120
pad rectangle, y down, one unit per `pad.png` texel. Rectangles are
`{x, y, width, height}`, half-open: `(px, py)` is inside when
`x <= px < x + width` and `y <= py < y + height`.

| element | key | x | y | width | height |
|---|---|---|---|---|---|
| stick zone | `STICK_ZONE` | 0 | 0 | 80 | 120 |
| L | `leftshoulder` | 4 | 4 | 24 | 24 |
| R | `rightshoulder` | 132 | 4 | 24 | 24 |
| Y (top) | `y` | 108 | 36 | 24 | 24 |
| X (left) | `x` | 84 | 60 | 24 | 24 |
| B (right) | `b` | 132 | 60 | 24 | 24 |
| A (bottom) | `a` | 108 | 84 | 24 | 24 |
| Back | `back` | 54 | 94 | 24 | 24 |
| Start | `start` | 82 | 94 | 24 | 24 |

The buttons are `BUTTONS`, keyed by the LÖVE gamepad names of
`love.pad_sheet`; `STICK_BASE_SIZE` 48 and `STICK_KNOB_SIZE` 24 are
diameters, the sheet's sprite sizes. The stick zone is the left half; the
diamond is centred at (120, 72) and its squares meet only at corners; Back
and Start are centred on x = 80, 2 units above the bottom edge; "small" is
their art's pill, not a smaller rectangle. L and Back lie inside the stick
zone: **a touch that starts on a button's rectangle is that button's, never
the stick's**; the stick zone takes only the touches that start on no
button. `place(pad)` takes the `pad` rectangle of `window_layout.compute`
(numeric `x`, `y`, a `scale > 0`, else an `assert` naming the field) and
returns fresh `{stick_zone, stick_base_size, stick_knob_size, buttons}` in
LÖVE units: each rectangle `pad.x + x × scale`, `pad.y + y × scale`,
`width × scale`, `height × scale`, never rounded, so it is exactly what a
sprite drawn there covers. At 390×844 every button is 58.5 points square
(≥ 44 required).

### The asset PNGs

`game/sprites.png` and `game/font.png` are committed sources, edited
directly; no tool writes them (the pad sheets below are generated). `make test` checks that both decode in the
fake, opaque, at their sizes (`test_fake_love_png_file`), and the replays
check every pixel the game draws from them.
`game/p8/sprite_sheet_packed.lua` holds the same sprite sheet for the
PICO-8 runtime, and nothing checks that the two agree: an edit to the
sheet changes both.

- **Format:** 8-bit RGB, no alpha, non-interlaced; stored (uncompressed)
  deflate blocks.
- **Colours:** every pixel is one of the 16 `palette.RGB` triples, byte
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

**The pad sheets.** `game/pad.png` (the game's virtual pad) and
`tests/fixtures/pad_flat.png` (its flat-colour stand-in for tests) are
generated by `tools/pad_art.c` through `make pad-art`, never edited by
hand; the same run writes `tests/fixtures/pad_sheet_expected.png`, the
static test's expected frame. Output is byte for byte the same on every
run. Both sheets are 240×72, 8-bit RGBA, stored deflate blocks; every
texel outside a sprite is `00000000`. `love.pad_sheet` (`game/love/`)
holds the same layout as data and is the only file naming `pad.png`; the
generator keeps its own copy, so the static test compares two sources.
Changing a size or position means changing both, then `make pad-art`.

| sprite | x | y | size |
|---|---|---|---|
| stick base | 0 | 0 | 48×48 |
| stick knob | 0 | 48 | 24×24 |
| button k released (k = 0…7: a, b, x, y, leftshoulder, rightshoulder, start, back) | 48 + 24k | 0 | 24×24 |
| button k pressed | 48 + 24k | 24 | 24×24 |

One texel is meant as one unit of the pad's 160×120 reference rectangle.

- **Art (`pad.png`):** binary shapes, a texel in a shape when its centre
  is (integer test on doubled coordinates); colours are `palette.RGB`
  entries; alphas only `00`, `66`, `99`, `FF`. Each body has a 1-texel
  outline (in the shape, not in the shape inset by 1).
  - A, B, X, Y: a disc of radius 12, body colour 11, 8, 12, 10; label the
    letter's `font.png` glyph at ×2 (6×10), top-left at (9, 7).
  - L, R: a 24×24 square, corners of radius 6, body colour 13; label `L`,
    `R` as the face buttons.
  - Start, Back: a pill 24×12 on rows 6–17 (radius-6 ends), body colour 5;
    label `START` from (2, 9), `BACK` from (4, 9), ×1, advance 4.
  - Released: body alpha `99`, outline the body colour opaque, label
    colour 7. Pressed: body opaque, outline colour 7, label colour 0.
  - Stick base: a disc of radius 24, body colour 1 alpha `66`, outline
    colour 6. Stick knob: a disc of radius 12, body colour 6 opaque,
    outline colour 7.
- **Flat colours (`pad_flat.png`):** each sprite's whole square is one
  opaque colour. Sprite `n` in the order stick base, stick knob, then a,
  b, x, y, leftshoulder, rightshoulder, start, back, each released then
  pressed (n = 0…17), has red `0x20 + 0x0B·n`, green `0xE0 − 0x0B·n`,
  blue `0x40` for even `n`, `0xC0` for odd `n`: all distinct, none black,
  a pressed sprite 128 bluer than its released one.
- **Static test** (`test_fake_love_image_draw`, fixture game
  `tests/fixtures/pad_sheet/`, symlinking `game/love` and `game/pad.png`):
  `love.pad_sheet.load()` cuts the real sheet, every sprite is drawn at
  scale 1 on a black 240×84 screen (base at (4, 4), the 17 others in the
  flat order at (56 + 26·(k mod 7), 4 + 26·floor(k / 7))), and the screen
  must equal `pad_sheet_expected.png` exactly; the same run checks that
  every rectangle of `pad_flat.png` is one solid opaque, non-black colour
  and that the 18 are distinct. A red static test after an intended
  redraw: run `make pad-art`, review the three PNGs, commit them.

## Numbers and randomness

The game computes with PICO-8 16.16 fixed-point numbers and its own random
generator, both in `game/p8/fixed.lua`, built on LuaJIT's `bit` library and
checked bit for bit against `tests/lua/fixed_reference.txt`, frozen from the
C library before plan 4 deleted it. `make test` runs `lua-test-fixed`, which
checks the edge values and a sample against it.

## Layout of the fork

- `game/`: the Lua game (`conf.lua`, `main.lua`, `sprites.png`, `font.png`,
  `pad.png`, and the modules of `p8/`, `engine/`, `game/`, `devtools/` and `love/`);
  `make love-file` zips it into `tmp/slipways.love`, `main.lua` at the
  archive's root, then plays `tests/e2e/pad_title.json` in the fake from
  the unpacked archive and checks the capture against its `.sha256`.
- `tools/fake_love/`: the fake LÖVE's C source; built as `tmp/fake_love`
  and `tmp/love_capture_check`.
- `tools/love_capture/`: the capture programs, their PNGs and the fixtures
  they read (`files/`).
- `tools/`: the scripts behind the targets (`love_e2e_run.sh`,
  `end_state.sh`, `e2e_limits.sh`, `lua_guard.lua`, `love_install.sh`, …)
  and `pad_art.c`, the pad sheets' generator (`make pad-art`).
- `tests/`: the C tests of the fake and the tools (`test_<name>.c`, listed
  in the Makefile's `UNIT_TESTS`), with their fixtures in `tests/fixtures/`.
- `tests/lua/`: the Lua tests of the fixed-point library and its frozen
  reference, the runtime, module loading, the capture reader and the
  `lua-names` check's fixture self-test.
- `tests/lua/game/`: the regression tests of `game/` modules
  (`test_<module>.lua`, each ending in `return check.run({...})`), all run
  by `make test` in one process through `runner.lua`.
- `tests/e2e/`: the 20 replays, their hashes, end states and notes.
- `worst-case/`: long games checked one at a time with `make end-state
  EVENTS=`.

## Code conventions
The rules below hold for every module under `game/`; `lua-guard`,
`lua-test-modules` and `make lua-names` check the mechanical ones, the
replays the rest.

**Naming.** An exported name, function or constant, does not repeat its
module's name or first word, which every caller already writes, and
`game/p8/` has no `p8_` left (`planet.init_instance`, `fixed.add`,
`vram.SCREEN_WIDTH`, `entity_class.STATE_NORMAL`); `make lua-names` checks it.
Four exports keep a word of their module's name, the check's listed
exceptions: `draw_functions.draw_row`, `.draw_row_text_image` and
`.draw_rows_default`, whose verb is the module's first word, and
`bindings_default.bindings`, whose stripped name would be empty. A field
whose name would be a Lua keyword takes a trailing underscore (the
slipway's `end_`), and an export whose name would be one takes a word that
says what it returns (`slipway.end_node`). A module alias that would clash
with a parameter or local takes the `_module` suffix
(`local planet_module = require("game.planet")` in `save_load.lua`).

**Modules and loading.** One module per file, required by its path under
`game/` (`require("p8.gfx")`), returning `M` (the two data modules
`p8.quarter_wave` and `p8.sprite_sheet_packed` return their table). No
globals, LuaJIT `bit` only, no FFI. Module state is module-level `local`s,
or a `g_` field of `M` (`menu.g_pause_menu_open_box`). A local function used
before its definition is declared `local name` and assigned later with `name
= function`, never `local function name`, and a table entry naming it is
stored right after that assignment (`menu.galaxy_menu_on_click_table[4]`).
Modules that need each other require each other at call time, inside the
function that needs the partner, and read a partner's constants there, never
cached at module level. A partner needed at load (a class holding its
methods by identity, a table built from its value) is required at load in
one direction per pair only: `planet` requires `planet_display` and
`planet_trade` at load, they require `game.planet` at call time.
`love.graphics` and `love.image` appear only in `game/love/`.

**Tables and objects.** An object is a table; an `init` function fills the
table its caller passes. A re-init of a table another holder may keep resets
its fields in place and keeps its identity and its sub-tables:
`pad_nav.init` creates its sub-tables and bindings table only when absent,
`input_step.init` resets `played` and `played_frame` in place,
`economy_score.set_difficulty` clears and refills `g_difficulty`. A value
table (a `vec`, a `box`, a mouse position, a constant) is built by a
constructor listing every field and never changed by a function that
receives it: operations return a new one (`engine.vec`). A copy is shallow:
the tables it refers to are shared, and a nested table the copy must own is
copied by the copier. A fresh table with every field at its zero value comes
from the owning module's zero constructor when other modules need it
(`ui_button.button_zero`, `particle.emitter_zero`, `targeting.zero`), from a
local one otherwise. An optional value is `nil` when absent, tested with `~=
nil`; no `has` bit or `has_<x>` number marks it (the mouse position, a value
table, keeps its `has` bits, `entity_class.MOUSE_POS_HAS_*`). An array field
has no fixed length: its elements are created as they are written, and every
read stops below its count. The only fixed lengths are those of data tables
and the save format's ranges (`docs/save-format.md`). `is_dead` is on every
entity. A callback that must report to its caller gets a context table
(`economy_tiers.update_tiers`' `{ value = false }`). A local copy of a few
graphics-state fields is a `graphics_copy` built field by field
(`demo_overlay`, `loading_screen`). A function that advances a value it was
given returns the new value after its own results, several in a fixed order
(`result, state = fixed.rnd_from_state(state, x)`; `pad_panel.step` returns
`at, x, y, mouse_buttons`).

**Ids.** An entity id is a number handed out by `entity_id.take` from a
counter that never resets, so it is never reused; it is resolved by
`entity_id.get` through the current table, which drops an entity when the
exit queue flushes it. `nil` is "no link"; ids are compared with `==`. A
research project id follows the same rule from `research`'s own counter and
maps, and goes stale when `research.expire_project_ids` runs.

**Lists and sharing.** A new object is a table built where it is needed. The
plain-array lists are emptied or refilled in place when several owners hold
them; a list that outlives a copy point is copied, never shared by accident
(an offer's routes are a copy of the source list, assigned back when the
offer returns); a route's id arrays are never changed once built, so route
copies share them. An id list is read through `entity_id.resolve`, which
asserts the id is alive. A walk whose body may change its list walks a copy
and says so. Strings are compared with `==`.

**Arrays.** Every array is 1-based: a table whose integer key only
says "the k-th element" (a list, a sequence, a cache's slots, a ring, a pool, a data
file's rows read by position, the cart's 1-based index) starts at 1, its loops run
`for i = 1, count`, and 2-D data is `t[y][x]`. A list without holes (the trade route lists,
the id lists, needs, particles, the registry's buckets, the exit queue) is a plain Lua
array, appended with `t[#t + 1] = x`. An array with its own count (`{ items, count }`,
`tickets_count`, `placed_items_count`, `rows_count`, the save state's pools) keeps it:
elements past the count may be stale or absent, so `#` and `ipairs` are never used on
it, reads stop at the count, and an append names its new position
(`local n = list.count + 1; list.items[n] = x; list.count = n`); an array with holes
(the type picker's `recent` and `recent_set` until the ring fills, the galaxy menu's
refresh and on-click tables) is read by position only. A table keyed by a value
that has its own meaning, 0 included, is a map and keeps its keys: a colour
(`p8.palette`, `mem.draw_palette`, the CLUT entries, `hud`'s palette blocks), a pixel
coordinate or offset (`mem.screen[y][x]`, the sprite sheet, the map cells, the glyph
rows, every pixel loop), an address or angle (`p8.sprite_sheet_packed`,
`p8.quarter_wave`), an image slot handle (from 0, like a PICO-8 sprite number; the
game's own caches compute slot `FIRST + i - 1`), a byte (`crc32_table`), a disk code
(`save_state.NODE_*` in `node_constructors` and `node_kind`), a saved state
(`entity_class.STATE_*`), a tag's bit position (the registry's tag buckets), a layer
value (the layer order's `start`, `start[0]` the empty prefix; `ui`'s hover layers),
a draw-function code, a disk rank (the save validators' `seen` sets) and a demo frame
(`demo_data.pad_buttons`). An enum that only indexes tables is numbered from 1
(`entity_registry.MESSAGE_*`, `bindings.MODE_*`, `bindings.ACTION_*`). The save state mirrors the
bytes: its arrays are 1-based but every reference it holds keeps its on-disk value, a
0-based rank, converted to a position only where the capture writes it
(`position - 1`) and where the loader and the decoder's node-slipway check read it
(`rank + 1`) (`docs/save-format.md`). A copy or comparison of part of an array is a
loop over its count. A reference to an element is its index; "none" is `nil`; an
element's index is found through a reverse map built at load (`project_index_of` in
`research.lua`).

**Integers, bytes and strings.** A truncating integer division is `cint.div`
(never `-0`, raises on 0) and its remainder `cint.mod` (the dividend's
sign). `math.floor` and Lua `%`, which round toward minus infinity, are for
a magnitude (a value made non-negative first, as `str` and `state_format` do
with fixed point), a counter wrapping at 2^32 (the registry's, the row
serial, `row_image`'s products), a pattern phase in 0..3 of any world
coordinate, a colour byte rounded back from `getPixel`, and `fixed`'s own
arithmetic. A byte is `bit.band(x, 0xFF)`; a `bit` result above 2^31 − 1 is
brought back to unsigned where it is compared, stored or printed (the save
CRC). Fixed point goes through `p8.fixed`. A character is its byte value
(`string.byte`), text is a Lua string, and a walk over text runs `1 .. #s`.

**Truth values.** Predicates and truth-valued fields are Lua booleans. A
predicate returns exactly `true` or `false`, never `nil` or one of its
operands: a comparison is returned as is, a presence test is `p ~= nil`, and
a lookup returns its boolean `found` before its value (`found, out`). A
truth-valued field holds `true`, `false` or `nil`, and `nil` reads as false.
Tests are `if x then` and `if not x then`, never `x == false` or
`x == true`; a counter adds one under `if c then`. `0` and `1` stand for a
truth value only at a format boundary, converted at that site: the save
record mirrors its bytes, so its flag fields stay `0`/`1` numbers (and
`fold_anchor`, `infraspace` the cart's `0x10000` or `0`), written by the save
builder with `(c) and 1 or 0` and read back by the loader with `x ~= 0`; the
state dump's `visible` column and the draw cost model's `fillp_transparent`
key bit build their `1`/`0` the same way. Numbers that are data (counts, bit masks, colors, fixed-point
values, divisors) keep their `== 0` and `~= 0` tests. `(c) and a or b` is
used only when `a` is never `nil` or `false`; a choice between two
functions is an `if`/`else`.

**Control flow and errors.** An invariant is `assert(c, "msg")`, a check of
data an `assert` at load. A dispatch on a value is an `if` chain or a lookup
table built once; `continue` is `goto continue`; a numeric `for` is used
only when the body changes neither counter nor bound, a `while` otherwise.
Behaviour that varies by kind is a class table of functions. A fixed list of
entries the runtime visits is a module function calling its visitor once per
entry, in order (`image.SLOTS(SLOT)`, the image slots). `fatal.raise(msg)`
raises `error(msg, 0)`; messages are built with `string.format`.

**Data tables and checks.** Every table of a data file is a module field
(LuaJIT allows 200 locals per function). A data item or row is built by a
constructor defined before the tables that call it at load
(`map_tables.make_data_item`, `map_tables.make_data_row`, aliased as locals
by `game_data`), which gives each missing field its default and raises on a
field the record lacks (`map_tables.check_fields`); a record written
positionally gets a positional constructor (`game_data`'s `vec`, `box`,
`kind_record`). A table that refers to another holds that table. Four data
modules are edited by hand (`p8.quarter_wave`, `p8.sprite_sheet_packed`,
`game.demo_data`, `game.bindings_default`); `p8.sprite_sheet_packed` and
`game.bindings_default` open with a line naming the file each is kept in
step with (`game/sprites.png`, `bindings.default.cfg`), and nothing checks
either pair agrees. `make test` runs two checks over the whole tree:
`lua-guard` (every file compiles, no undeclared global, only this file's
`love.` names, every `require` takes a literal name resolving under `game/`
as `<name>.lua` or `<name>/init.lua`, or `bit`, and every PNG named in code
is a file under `game/`) and `lua-test-modules`, which requires every module except
`main` and `conf` under plain LuaJIT and reports each one that fails to
load; it clears the sentinels a failed `require` leaves in `package.loaded`,
so each failing module reports its own missing module; `make lua-names`
(outside `make test`) checks the naming rule.
## Rules for the fork

- The e2e replays are the source of truth; no C unit test was ported to
  Lua.
- `make e2e` runs under 9 seconds (the rule of `CLAUDE.md`). The replays
  run in parallel, longest first, LuaJIT with its JIT on.

## Open questions

- Which LÖVE version does the love2d studio iOS app run? 11.x keeps everything
  aligned.
- Keyboard play on PC: not needed by the e2e, undecided.
