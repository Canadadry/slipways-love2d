---
title: "The fake LÖVE fails on an unlisted function, a colour outside the 16 and a foreign pixel on canvas 0"
description: "The fake LÖVE lets a game reach LÖVE functions it does not implement, set colours that cannot be captured and end frames with pixels outside the palette, so a Lua game can drift from the contract without one fatal error."
status: needs-triage
---

## Problem Statement

design.md promises three fatal errors, each naming the function or the
frame and pixel: a LÖVE function not on the list, a colour that is not one
of the 16, a pixel of another colour on the canvas. None of them exists
today:

- **Unlisted functions.** `love` is a plain table and so are
  `love.graphics`, `love.image`, `love.joystick` and `love.filesystem`. A
  game reading `love.timer`, calling `love.graphics.getWidth()`,
  `data:mapPixel(...)`, `joystick:getName()` or `canvas:getWidth()` gets
  Lua's generic "attempt to call a nil value" (or, worse, a silent `nil`
  in `if love.timer then`), never "this is not in the function list". A
  game defining `love.keypressed` or `love.errorhandler` is silently never
  called by the fake, while real LÖVE would call it: the two diverge
  without a word.
- **Colours outside the 16.** `setColor` accepts any opaque colour. A
  component computed any other way than `byte / 255` can lose a byte and
  only shows up, much later, as a capture mismatch.
- **Foreign pixels.** Step 4.16 writes a pixel that is not an opaque
  palette colour as colour 0 and prints one warning at the end of the run,
  which still exits 0 (`TODO step 17` in the command). A frame drawn while
  no canvas exists is 19 200 such pixels with the same warning. In step 18
  this would show up only as a sha256 mismatch on a 600-frame replay, with
  no frame or pixel named.

Two constraints pull the other way. design.md tells the game to call
`setColor(1, 1, 1)` (white, not one of the 16) before drawing an image in
its own colours, and the text rule tints white `FF` texels with a palette
colour; scratch canvases may hold transparent or non-palette bytes. And the
capture programs of `tools/love_capture/` run in the same host as
`tmp/fake_love` (through `love_capture_check`) while deliberately setting
non-palette colours and leaving transparent pixels, because they measure
real LÖVE: every `love-capture-check-*` must stay green.

## Solution

Three checks, each where it can name the cause best and where it cannot
fire on a capture program or on a game following design.md:

1. **Unlisted function, in the host (both binaries).** Reading a field of
   `love`, of a LÖVE module table or of a LÖVE object that the function
   list does not hold raises an error naming it:
   `love.graphics.getWidth is not in the function list of docs/love-port/design.md`
   (`ImageData:mapPixel`, `Canvas:getWidth`, … for objects). Creating a
   new field in `love` other than the six callbacks the fake calls, or in
   a module table, raises the same error. The fake's harness and every
   capture program only use listed functions, so the capture checks stay
   green and now also enforce the plan's rule that a capture program's
   drawing part uses listed functions only.
2. **Colour outside the 16, at `setColor`, in `tmp/fake_love` only.** The
   colour `setColor` would store (its three bytes after LÖVE's truncating
   conversion) must be one of the 16 palette triples or white `FFFFFF`;
   anything else is refused with
   `love.graphics.setColor: 4C0000 is not one of the 16 palette colours nor white FFFFFF (pass each palette byte as byte / 255)`.
   `love_capture_check` runs its programs with any colour allowed: they
   measure LÖVE's conversions, the palette rule is the game's contract.
   `clear`, `setPixel` and image texels are not checked: what they put on
   canvas 0 is caught by check 3, and on scratch canvases they are legal.
3. **Foreign pixel, at the end of each frame, in `tmp/fake_love` only.**
   After the draw step (after the "Canvas is active" check), canvas 0 must
   exist and every pixel of it must be an opaque palette colour; otherwise
   the run stops at once, exit 1, no capture and no state file left:
   `fake_love: frame f3: pixel (0, 0) of canvas 0 is FF0000FF, not one of the 16 palette colours (1 such pixels in the frame)`
   or
   `fake_love: frame f0: no canvas exists at the end of the frame (create the game canvas first, in love.load)`.
   The end-of-run warning and its `TODO step 17` disappear.
   `love_capture_check` does not run this check: it compares all four
   channels with a PNG and transparent pixels are part of what it pins.

Every fixture game that runs frames and expects to get past frame 0 gets a
canvas 0 cleared to opaque colour 0 before its own `love.load`, appended at
the end of its `main.lua` so no line number its tests assert moves. One
test game per error proves each message; a test game following every
drawing rule of design.md (white before images, white texels tinted with a
palette colour, scratch canvases holding transparent and non-palette bytes,
screen draws, nested translations) runs without error and writes the
capture expected from its drawing.

## User Stories

1. As a plan-2 developer, I want reading `love.timer`, `love.audio`, `love.window` or any other field of `love` outside the list to fail with its full name, so that I learn at once that the fake (and the contract) does not offer it.
2. As a plan-2 developer, I want `love.graphics.getWidth`, `love.graphics.print` or any unlisted `love.graphics` field to fail with its full name, so that I do not port a PICO-8 call to a LÖVE function the fake cannot check.
3. As a plan-2 developer, I want unlisted fields of `love.image`, `love.joystick` and `love.filesystem` to fail the same way, so that every module has one rule.
4. As a plan-2 developer, I want `ImageData:mapPixel`, `Joystick:getName`, `Canvas:getWidth`, `Image:getWidth` and `Quad:getViewport` to fail naming the object kind and method with a colon, so that a method call reads like the code I wrote.
5. As a plan-2 developer, I want the error raised at the access, with my file and line in front like every other refusal, so that I find the call without a traceback hunt.
6. As a plan-2 developer, I want `if love.timer then` to fail rather than quietly take the `else` branch, so that feature detection cannot hide an unlisted module.
7. As a plan-2 developer, I want defining `love.keypressed`, `love.errorhandler`, `love.run` or `love.quit` to fail with its name, so that I never write a callback the fake would never call while real LÖVE would.
8. As a plan-2 developer, I want defining `love.load`, `love.update`, `love.draw`, `love.conf`, `love.gamepadpressed` and `love.gamepadreleased` to keep working, so that the six callbacks the fake calls stay mine to set.
9. As a plan-2 developer, I want reading one of those six callbacks before I define it to give `nil`, so that a wrapper like `local old = love.load` works.
10. As a plan-2 developer, I want `love.graphics`, `love.image`, `love.joystick` and `love.filesystem` to read as `nil` in `conf.lua`, as since step 2.3, so that conf.lua behaves as in LÖVE's load order and only unknown names fail there.
11. As a plan-2 developer, I want adding a field to `love.graphics` (or any module table) to fail, so that a monkey-patch cannot replace a measured function with an unmeasured one.
12. As a plan-2 developer, I want `pairs(love.graphics)` to keep listing exactly the 13 functions, so that existing introspection and tests are unchanged.
13. As a plan-2 developer, I want `setColor` with a palette colour as `byte / 255` and `setColor(1, 1, 1)` to keep working, with or without the alpha 1, so that every rule of design.md is still accepted.
14. As a plan-2 developer, I want `setColor` to refuse a colour whose bytes are not one of the 16 triples nor white, naming the six hex digits it would have drawn, so that a component that lost a byte fails at the call that lost it.
15. As a plan-2 developer, I want the colour judged on the bytes LÖVE would draw, not on the numbers I passed, so that any expression giving the palette byte (clamped or not) is accepted and none that misses it is.
16. As a plan-2 developer, I want a refused `setColor` to leave the current colour unchanged, as every other refusal does, so that a caught error cannot leave a half-set colour.
17. As a plan-2 developer, I want the palette check to come after the existing argument, finiteness and alpha checks, so that existing messages for those mistakes are unchanged.
18. As a plan-2 developer, I want `clear` with any colour on a scratch canvas to stay legal, so that I can clear a build canvas to transparent black.
19. As a plan-2 developer, I want white points, transparent pixels, and palette-tinted coloured texels on a scratch canvas to stay legal, so that scratch canvases are free.
20. As a plan-2 developer, I want a white `FF` texel drawn under a palette colour to stay legal, so that the text rule of design.md works.
21. As a plan-2 developer, I want the run to stop at the end of the first frame whose canvas 0 holds a pixel outside the palette, naming the frame, the first such pixel in row order, its RGBA and the count in that frame, so that I find the wrong draw in one look.
22. As a plan-2 developer, I want a frame ending with no canvas at all to stop the run with a message telling me to create the game canvas in `love.load`, so that forgetting it is not a frame of zeros.
23. As a plan-2 developer, I want a transparent pixel on canvas 0 (a fresh canvas's `00000000` included) to be foreign, so that a canvas I forgot to clear opaque fails instead of reading as black.
24. As a plan-2 developer, I want a failed frame check to leave neither capture nor state file, as every other fatal error, so that a partial capture is never hashed.
25. As a plan-2 developer, I want the frame check after the "present cannot be called while a Canvas is active" check, so that leaving a canvas selected keeps its own message.
26. As a plan-2 developer, I want a foreign pixel produced without any checked call (a palette colour tinting a coloured texel onto canvas 0) still caught at the frame's end, so that the frame check is the last word.
27. As a plan-2 developer, I want design.md to state the three errors, their messages, where each is checked and which colours `setColor` accepts, so that I read the contract and not the fake's source.
28. As a plan-1 developer, I want the function-list guard in one small module used by every install, so that a function added to the list later is added in one place per table, and the guard is unit-tested once.
29. As a plan-1 developer, I want the host's own reads of `love` callbacks to bypass the guard, so that the fake never trips its own check.
30. As a plan-1 developer, I want every `love-capture-check-*` to stay green unchanged, PNGs and programs untouched, so that real-LÖVE conventions stay pinned.
31. As a plan-1 developer, I want `love_capture_check` to keep the function-list guard but allow any colour and skip the frame check, so that capture programs keep measuring LÖVE while still using listed functions only.
32. As a plan-1 developer, I want one test game per error with an exact message check, so that each error is proven the way the plan asks.
33. As a plan-1 developer, I want a test game that follows every drawing rule of design.md to run over several frames with exit 0, nothing on stderr and known capture bytes, so that the new checks are proven not to reject the game's own rules.
34. As a plan-1 developer, I want existing fixture games to get a canvas 0 by a block appended at the end of `main.lua`, so that the line numbers their tests assert do not move.
35. As a plan-1 developer, I want the fixtures whose tests fail before any frame ends left untouched, so that the churn is only where it is needed.
36. As a plan-1 developer, I want the `set_color_log` expectations updated to the palette rule (the clamped and alpha-1 cases now on palette bytes, plus refused near misses), so that the log keeps testing every `setColor` path.
37. As a step-18 implementer, I want a game breaking the contract to fail with a named error on a replay instead of reaching the sha256 comparison, so that `make love-e2e` reports causes, not just mismatches.
38. As the repository owner, I want every new check a few milliseconds, each its own `FAKE_LOVE_CHECKS` target under 100 ms, and `make test` measured with design.md's protocol, so that the fast loop stays fast.
39. As the repository owner, I want `make e2e`, the C game and every `*.sha256` untouched, so that the reference is unaffected.

## Implementation Decisions

### 1. Unlisted function

- **Concretely.** Four kinds of table are guarded:
  - `love` itself. Reading an absent key: `nil` for the six callback names
    (`conf`, `load`, `update`, `draw`, `gamepadpressed`,
    `gamepadreleased`) and the four module names (`graphics`, `image`,
    `joystick`, `filesystem`, absent while `conf.lua` runs); an error for
    anything else. Creating a new key: allowed for the six callback names,
    an error for anything else (module names included: the fake installs
    them itself).
  - The module tables `love.graphics`, `love.image`, `love.joystick`,
    `love.filesystem`: reading an absent key or creating a new one is an
    error.
  - The method tables of objects: `ImageData` (`setPixel`, `getPixel`),
    `Joystick` (`isGamepadDown`), and `Canvas`, `Image`, `Quad` (no
    methods; `Canvas` gains an index for the purpose): reading an absent
    method is an error. Assigning a field on a userdata already fails in
    Lua and stays as it is.
- **Message.** `<name> is not in the function list of docs/love-port/design.md`,
  `<name>` being `love.<key>`, `love.<module>.<key>` or `<Kind>:<key>`; a
  non-string key is shown through `tostring` in brackets (`love[1]`). The
  error is raised at the caller's level so it carries the game's
  `file:line:` like every other refusal, and the host prefixes the callback
  and frame as usual (`love.load: main.lua:3: love.timer is not in …`).
- **Catchable, like every refusal.** It is an ordinary Lua error: uncaught,
  the run fails (exit 1, no capture, no state); caught by `pcall`, the game
  continues. Rejected: a sticky host flag failing the run even when caught,
  because every other refusal of the fake is catchable and the log fixtures
  rely on that; a game swallowing errors is out of contract anyway.
- **Module `function_list`** (new, in the fake's directory), a small deep
  interface used by every install:
  - protect `love` (the global), installed by `host_open` before
    `conf.lua` runs, with the callback and module name lists above;
  - protect a module table under a prefix (`"love.graphics"`), called by
    each module install after its functions are set;
  - protect a method table under an object kind (`"ImageData"`), called
    where each metatable's `__index` is built.
  The module installs place their table in `love` with a raw set (the
  guard forbids new keys), and the host reads callbacks with raw access, so
  the fake never goes through its own guard. `pairs` and `next` are
  unaffected (the guard is a metatable): `counting`'s `love empty` in
  conf.lua and the 13 names of `love.graphics` hold.
- `love.conf(t)` is added to design.md's function list (it was only in the
  loop description): the fake calls it with `window`, `modules` and `audio`
  tables and reads nothing back.

### 2. Colour outside the 16

- **Where.** In `setColor` only, after every existing check (count, types,
  finiteness, alpha), on the three bytes LÖVE's truncating conversion
  gives. Legal: the 16 triples of design.md's Capture table and `FFFFFF`.
  The 16 triples are the ones of the `frame_capture` module (one table in
  the fake, reused, not copied).
- **Why white.** design.md requires `setColor(1, 1, 1)` before an image in
  its own colours, and white tints texels to themselves. No other
  non-palette colour has a use in the game: a palette colour tinting a
  white texel gives the palette colour; a palette colour tinting a
  coloured texel gives non-palette bytes, legal on a scratch canvas,
  caught by the frame check on canvas 0.
- **Why not `clear`, `setPixel`, texels.** `clear()` (transparent black)
  is the natural way to reset a scratch canvas; ImageData legitimately
  holds alpha 0 and loaded bytes; texels are judged when they land. All
  three reach canvas 0 only through pixels the frame check judges.
- **Message.** `love.graphics.setColor: <RRGGBB> is not one of the 16 palette colours nor white FFFFFF (pass each palette byte as byte / 255)`;
  the current colour is unchanged.
- **Mode.** The host has a switch, set before `host_open`, that allows
  any opaque colour in `setColor` (all other refusals stay). `tmp/fake_love`
  never sets it; `love_capture_check` always does. The canvas, graphics
  and colour conversion code is otherwise shared, unchanged.

### 3. Foreign pixel on canvas 0

- **Where.** In the command (`tmp/fake_love`), right after each successful
  frame, where step 16 appends the capture frame: the packing already
  counts foreign pixels and records the first, so a count above zero (or
  no canvas) is now fatal. The run stops: no later frame runs, the capture
  and the state file are removed by the existing failure path. The
  accumulated count, the first-frame bookkeeping, the end-of-run warning
  and the `TODO step 17` comment go. The `frame_capture` module keeps its
  interface (pack, count, first foreign; NULL pixels meaning no canvas)
  and its unit test.
- **Messages** (printed as `fake_love: <message>`, exit 1):
  - `frame f<N>: pixel (<x>, <y>) of canvas 0 is <RRGGBBAA>, not one of the 16 palette colours (<count> such pixels in the frame)`
    — the first pixel in row order of that frame;
  - `frame f<N>: no canvas exists at the end of the frame (create the game canvas first, in love.load)`.
- **No canvas is fatal**, not a frame of zeros: design.md already tells
  the game to create canvas 0 in `love.load`, and a game without it can
  never match a `.sha256`. Zero frames still give an empty capture and no
  error, whatever the game created.
- **Alpha `00` is foreign** (step 16's choice, confirmed): the game must
  own every pixel of canvas 0.
- `love_capture_check` never calls the command's frame check (it compares
  RGBA with the PNG, transparent pixels included).

### Fixture updates

- **The canvas-0 block.** Appended at the end of `main.lua` of every
  fixture game whose test runs at least one frame and expects to get past
  the end of frame 0, it keeps the fixture's `love.load` (possibly `nil`)
  and redefines `love.load` to create a canvas, select it, `clear(0, 0, 0)`,
  select the screen, then call the kept function with the same arguments
  and return its results. Canvas 0 is then the block's, the fixture's own
  canvases become 1 and up, and nothing else draws on canvas 0. One
  comment line on the block (the frame check needs it). Expected set (the
  implementer runs each test and adjusts it to the truth): `counting`,
  `minimal`, `state_raises`, `raises`, `no_debug_state`,
  `state_not_string`, `graphics`, `image_data_log`, `image_files_log`,
  `image_draw_log`, `points_log`, `rectangle_log`, `set_color_log`,
  `transform_log`, `pad_log`, `pad_poll`, `pad_misuse`, `disk_echo`,
  `disk_sizes`, and `disk_misuse` if one of its cases passes frame 0.
- **Left untouched**: fixtures whose tests fail in `main.lua`, `love.load`
  or before the end of frame 0 (`no_main`, `outside`, `not_function`,
  `pad_not_function`, `graphics_misuse`, the `*_uncaught` games),
  `modules` (zero frames), the `capture/` programs.
- **`capture_colours`**: frame 3 now makes its foreign pixel without
  `setColor` leaving the palette: a 1×1 image of colour 8 (`FF004D`) drawn
  at (0, 0) of canvas 0 under `setColor` of colour 9 (`FFA300`), which
  tints it to `FF0000FF`. The game is the foreign-pixel test game: on
  `capture_colours.json` (4 frames) it fails at `f3`; on
  `three_frames.json` it succeeds and its capture is the first three frames
  step 16 checks.
- **`set_color_log`**: `clamped` becomes `(2, -1, 77/255)` (clamps to
  `FF004D`, ok), `alpha 1` becomes `(1, 0, 77/255, 1)` (ok); new entries
  `white` `(1, 1, 1)` ok, `palette` `(0x1D/255, 0x2B/255, 0x53/255)` ok,
  `outside` `(0.3, 0, 0)` refused naming `4C0000`, `one byte off`
  `(1, 0, 76/255)` refused naming `FF004C`; existing messages unchanged.

### design.md edits

- "Anything outside the contract … is a fatal error": becomes a short
  paragraph **"Fatal errors"** listing the three, where each is checked,
  their messages, that they are ordinary Lua errors except the frame check
  (which no game code can catch), and that `love_capture_check` keeps the
  function list but allows any colour and does not check frames.
- **Capture**: "a frame drawn while no canvas exists has no palette
  pixel" becomes: a frame ending with a foreign pixel or no canvas is
  fatal, with the messages.
- **Colour**: the accepted colours (16 triples or white, judged on the
  drawn bytes) and the refusal; the game rule gains "only palette colours
  and white".
- **The LÖVE functions the game may use**: `love.conf(t)` added; one
  sentence on the guard (reading or creating anything else fails; the six
  callbacks are the only fields the game may set on `love`).
- **Real-LÖVE captures**: capture programs run with any colour and no
  frame check, under the function-list guard.

## Implementation Chunks

1. **Function list.**
   - `function_list` module; `host_open` protects `love` before
     `conf.lua`; graphics, image, joystick and filesystem installs set their
     table raw and protect it and their object method tables; the host's
     callback reads go raw.
   - Unit test `test_fake_love_function_list` (bare LuaJIT state, like
     `test_fake_love_image`): absent read and new key on a protected
     module table fail with the full name; a listed field reads; `love`
     reads `nil` for callbacks and module names, fails for others; setting
     a callback works, setting another name fails; a method table names
     `Kind:method`; `pairs` lists only the real fields; a non-string key.
   - Fixture `unlisted` (one game, the case chosen by `FAKE_LOVE_UNLISTED`):
     `love.timer` read in `main.lua`; `love.audio` read in `conf.lua`;
     `love.graphics.getWidth()` in `love.load`; `love.filesystem.lines`;
     `ImageData:mapPixel`; `Joystick:getName` in `love.update`;
     `canvas:getWidth()`; `Quad:getViewport`; `love.keypressed = function`
     in `main.lua`; `love.graphics.extra = 1`. CLI test
     `test_fake_love_unlisted` (own target): each case exits 1, no capture
     nor state, stderr holds the callback (and frame when in one),
     `main.lua:<line>:` (or `conf.lua:`) and the exact `… is not in the
     function list of docs/love-port/design.md`.
   - design.md function list paragraph and `love.conf(t)`.
   - Stories 1–12, 28–31 (guard part), 32.
2. **Frame check.**
   - Command: fatal on a foreign pixel or no canvas, messages above;
     warning and TODO removed.
   - Canvas-0 block appended to the fixture set above; `capture_colours`
     frame 3 changed; new fixture `no_canvas` (a `love.draw` that draws
     nothing, a `debug_state`).
   - `test_fake_love_frames`: bytes of `capture_colours` on
     `three_frames.json` (3 × 9 600, as step 16 built them, minus the
     fourth frame), `capture_colours` on `capture_colours.json` fails with
     `frame f3: pixel (0, 0) of canvas 0 is FF0000FF` and `1 such pixels`,
     `no_canvas` on `three_frames.json` fails with `frame f0: no canvas`,
     `counting` on `zero_frames.json` still an empty capture with nothing
     on stderr. `test_fake_love` keeps `counting`'s three black frames
     (now from the block's opaque colour 0).
   - design.md Capture paragraph.
   - Stories 21–26, 34, 35, 37 (frame part).
3. **Colour check and the drawing-rules game.**
   - `setColor` palette-or-white check after the existing ones; the host
     switch; `love_capture_check` sets it.
   - `set_color_log` updated as above; new fixture `set_color_outside`
     (uncaught `setColor(0.3, 0, 0)` in `love.draw`) failing with
     `love.draw f0: `, `main.lua:<line>: love.graphics.setColor: ` and
     `4C0000`, in `test_fake_love_set_color`.
   - Fixture `drawing_rules` (positive test game) on `three_frames.json`:
     `setDefaultFilter`, canvas 0 first then a scratch canvas, in
     `love.load`; a sprite ImageData of palette texels and alpha-0 texels
     and a "font" ImageData of white `FF` texels and alpha-0 texels, built
     with `setPixel(byte / 255)`; each frame: canvas 0 cleared to a palette
     colour, palette rectangles and `points(x + 0.5, y + 0.5)`,
     `setColor(1, 1, 1)` then the sprite, `setColor(colour 8)` then a
     letter from the font image, all inside `push`/`translate`/`pop` with
     a nested level; the scratch canvas cleared with `clear()`, then
     `clear(0.3, 0.3, 0.3)`, white points, and the sprite tinted by a
     palette colour (non-palette bytes); a draw with the screen selected;
     frame 1 and 2 draw over the previous frame. Run in
     `test_fake_love_frames`: exit 0, stderr empty, capture 3 × 9 600
     bytes, a handful of known bytes (a sprite texel, a tinted letter
     texel, a translated rectangle, the cleared background) checked by
     value.
   - design.md Colour, Fatal errors and Real-LÖVE captures paragraphs.
   - Stories 13–20, 27, 31 (colour part), 33, 36.
4. **Timing.** `make test` measured with design.md's protocol against the
   step's "before" (HEAD at the start of the step) and `2e7215c`; the
   medians, paired medians, spreads, rounds, cores and the slowest new
   check's own time go in the last commit message for the step note.
   Stories 38–39.

## Testing Decisions

- A good test drives a public interface and checks what comes out: exit
  status, stderr text, capture bytes, state file, or the return values of
  a module. No test reads the host's internals.
- **`function_list` gets a unit test** on a bare LuaJIT state (~1 ms):
  it is the deep module of the step and the CLI tests only cover its
  wiring.
- **Each error gets a CLI test over its own game**: `unlisted` (several
  cases, one game), `set_color_outside`, `capture_colours` frame 3, plus
  `no_canvas`. Messages are matched as exact substrings, positions
  included.
- **The positive game `drawing_rules`** proves the checks accept
  design.md's rules; its expected bytes are written from its drawing, not
  from a run of the fake.
- **Every `love-capture-check-*` stays green unchanged**: that is the
  verification that the capture programs only use listed functions and
  that the colour and frame checks do not reach them.
- **Prior art**: `test_fake_love_frames` and `test_fake_love` (capture
  bytes), `test_fake_love_graphics` and `test_fake_love_joystick`
  (env-selected misuse cases with `fake_love_cli_assert_failure`),
  `test_fake_love_image` (bare LuaJIT state unit test),
  `test_fake_love_set_color` (log game, uncaught game).
- **Budget**: new targets `test_fake_love_function_list` and
  `test_fake_love_unlisted` in `FAKE_LOVE_CHECKS`, `FAKE_SIDE_BINARIES`
  and the fake-side link list; each under 100 ms (`unlisted` runs about
  ten short processes; split it in two targets if it goes over).
  `test_fake_love_frames` grows by three short runs; check it stays under
  100 ms.
- The C game's unit tests, `make e2e` and every `*.sha256` stay
  unchanged; no capture program or PNG changes.

## Out of Scope

- `make love-e2e` and the stub games of step 18.
- Making an unlisted access uncatchable by `pcall`.
- Guarding replaced fields (`love.graphics = nil`, `love = {}`, a
  `rawset` from the game): only reads of absent keys and new keys are
  guarded.
- Checking `clear`, `setPixel` or texel colours at the call.
- Keyboard callbacks or any new function in the list.
- Any change to drawing rules, capture programs, PNGs, harnesses, CI jobs.
- The Lua game itself (plans 2–3). Audio, always.

## Further Notes

- The two binaries share one host: the function-list guard runs in both;
  the colour check is switched off in `love_capture_check`; the frame
  check lives in the command only. This is the split that keeps capture
  programs measuring real LÖVE while games are held to the contract.
- `frame_capture_pack` still writes a foreign pixel as 0; that path can
  no longer reach a capture file, and the module's unit test is unchanged.
- The fixture block is the same few lines everywhere on purpose; the
  fixtures are separate game directories and `require` only looks inside
  each, so it is not shared.
