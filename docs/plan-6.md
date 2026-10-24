# Plan 6 — The game on a portrait iPhone, through Love2D Studio and a virtual pad

## 1. Goal

The game runs on an iPhone held in portrait, in the iOS app Love2D Studio
(https://apps.apple.com/fr/app/love2d-studio/id6474188075), which imports and
runs `.love` files. With no controller, it is played with a virtual pad
drawn on the touch screen. On desktop nothing is lost: the real gamepad still
plays, the window becomes resizable, and a mouse click shows the virtual pad.

The goals behind every rule below:

- **The game logic does not change.** The virtual pad produces the same 12
  bits of `pad_buttons` as a real gamepad (`main.lua`'s `read_pad_buttons`,
  `PAD_BUTTON_NAMES`); the cart, `input_step`, `pad_nav` and the bindings see
  no difference. The 20 replays of `tests/e2e/` keep their `*.sha256` and
  `*.end-state` byte for byte.
- **Everything is verified headless, by an agent, exactly.** No human looks
  at a frame, no window opens, and real LÖVE is not a verification tool
  (that is why the fake LÖVE exists). Every visual requirement is a test
  that compares a frame with an expected frame **pixel for pixel, with no
  tolerance**: a tolerance opens the door to flaky tests.
- **The look of the pad changes without breaking the tests.** The pad is
  made of sprites; tests use a flat-colour sheet, the game uses the real art.
- `make test` stays as fast as today and `make e2e` under 9 seconds
  (`CLAUDE.md`).

## 2. Rules

### Input

- The virtual pad's mask is OR-ed with the real gamepad's mask in
  `love.update`, before `input_step.run`. No other input path is added (no
  direct "tap a planet"): rejected, because it would be a second way to play
  that every replay would have to cover.
- **Stick, not d-pad.** A *floating* stick on the left half of the pad
  rectangle: the base appears where the touch starts; a touch that starts in
  that zone stays the stick's touch until it lifts, even if it leaves the
  zone. The knob follows the touch, capped at the base's edge. **Dead zone:
  25 % of the base radius** (no direction). Outside it, the direction is the
  angle from the base centre, cut into **8 sectors: up, down, left, right
  60° wide each, the 4 diagonals 30° each**. A diagonal sets two direction
  bits (button 1–4). Aim and placement pans combine both axes
  (`pad_nav.lua` `aim_pan`); jumps and menus read one direction.
- **The 8 other buttons** (5–12: A, B, X, Y, L, R, Start, Back, per
  `PAD_BUTTON_NAMES`) are tested every frame against each touch's *current*
  position: sliding a finger from A to B releases A and presses B. Several
  fingers press several buttons (5 + 6 is the developer cheat chord, fixed
  in `input_step`; it stays reachable).
- Touches are read with `love.touch.getTouches` / `getPosition`; on desktop,
  the mouse with its left button down counts as one more touch
  (`love.mouse.getPosition` / `isDown`). Polling, not callbacks.
- **Shown / hidden.** The pad starts hidden. The first touch or click while
  it is hidden shows it and **presses nothing**, for as long as it is held
  (else a tap would press an invisible button). Any real gamepad button
  pressed hides it and drops the held touches (no button stays stuck); the
  next touch shows it again under the same rule. Once shown it stays shown
  (no fade). In the attract demo the first touch only shows the pad; the
  next press leaves the demo like any button.

### Layout

- The game stays a 160×120 canvas (`vram.SCREEN_WIDTH/HEIGHT`, canvas 0).
  `screen.present` draws it onto the screen **scaled by a fractional factor
  and centred** in its rectangle, nearest filtering. Whole-number scaling
  was rejected (black bars).
- The pad is designed once in a **160×120 unit reference rectangle (4:3, as
  the game)**: every element has fixed coordinates in it. It is drawn by
  scaling that rectangle uniformly, as the game. Layout tests therefore only
  vary scale and offset.
- **Two modes**, from the window size, recomputed every frame:
  - **tall** when `height > 2 * width`: the window is cut in two halves; the
    game fits at maximum size centred in the top half, the pad fits at
    maximum size centred in the bottom half. The game never moves when the
    pad shows or hides. On a 390×844 iPhone the fit rule gives scale
    390 / 160 = 2.4375: game 390×292.5 at y 64.75–357.25 (below the Dynamic
    Island, ~59), pad 390×292.5 at y 486.75–779.25 (above the home indicator,
    ~34). Rectangles are never rounded: the present and the pad draw at the
    exact fractional values. No safe-area API: the halves make it unnecessary.
  - **overlay** otherwise (desktop, iPad, landscape iPhone): the game fits
    at maximum size centred in the window; the pad is drawn over the game's
    rectangle, exactly. Spreading it to the window's edges was rejected.
- Pad elements in the reference rectangle: the stick zone is the left half;
  A/B/X/Y in an Xbox diamond on the right (A bottom, B right, X left, Y top);
  L and R in the top corners; Back and Start small, centred at the bottom.
  Exact coordinates are the implementer's choice, written once in a table;
  each button at least 44 points on a 390-point-wide iPhone.
- The pad is drawn on the screen (no canvas selected), after the present, so
  it never touches canvas 0 and the capture hashes cannot see it.

### Look

- Everything the pad draws is a **sprite** from `game/pad.png`, cut by a Lua
  quad table: stick base, stick knob, and for each of the 8 buttons a
  **released and a pressed sprite** (pre-rendered; no tint or light effect).
  Each sprite is drawn stretched into its layout rectangle, nearest
  filtering. Changing the look means redrawing `pad.png` (and the quad table
  if sizes change), never the layout code. Circles drawn with
  `love.graphics.circle` were rejected in favour of sprites.
- Labels are part of the sprites.

### Fake LÖVE (`tools/fake_love/`)

The fake's function list is a contract recorded in `docs/love-port/design.md`
("The LÖVE functions the game may use"); this plan extends it and the
section is updated with each extension:

- `love.touch.getTouches`, `love.touch.getPosition`,
  `love.mouse.getPosition`, `love.mouse.isDown`: answered from the replay
  (below); without replay data, no touch and no click.
- `love.graphics.getDimensions`: the replay's window size.
- the `love.resize(w, h)` callback, called by the fake before `love.update`
  on a frame that carries `resize`.
- `draw(canvas, x, y, 0, s, s)` with any offset and any positive scale
  (today: `(0, 0)` and `screen.SCALE` only, "The present" section), and
  `draw(image, quad, x, y, 0, sx, sy)` onto the screen.
- **`setColor`'s palette and alpha check is dropped**, everywhere. The
  end-of-frame check that every pixel of canvas 0 is a palette colour
  **stays**: the capture packs canvas 0 into 4-bit palette indices, and
  without the check a stray colour would be packed wrong silently.
- **The fake rasterises the screen**: an RGBA framebuffer of the window size,
  black at each frame start, receiving the present and the pad's sprites.
  The rasterisation rule is stated once in `design.md` so a test can compute
  an expected frame from it: a screen pixel belongs to a draw if its centre
  lies inside the destination rectangle, half-open (`x0 <= cx < x1`, same
  for y, so a centre on an edge belongs to exactly one draw), and samples the source texel that
  centre maps to (nearest). Alpha blending is LÖVE's default ("alpha" blend
  mode).
- Replay format, every new field optional so the 20 replays are unchanged:
  - top level: `"window": [w, h]` (default 640×480), `"pad_sheet": "<file>"`
    (resolved next to the replay like `bindings`; the fake serves it in
    place of `game/pad.png`; default the real one), `"screens": [frame
    numbers]` (the fake writes the whole screen of those frames as PNG next
    to the capture);
  - per frame: `"touches": [[id, x, y], …]` (the touches held that frame,
    window coordinates), `"mouse": [x, y, left 0/1]` (position kept between
    frames), `"resize": [w, h]`.

### Tests

- **Touch e2e replays**: a few short `tests/e2e/touch_*.json`, each the
  touch version of a pad scenario, with its `.txt` note. Their capture hash
  must equal the pad equivalent's (same frames on canvas 0). Planned: title
  to first slipway; aim pan on a diagonal; Back held to end the year; show,
  hide by a gamepad button, show again. They must fit in `make e2e`'s 9 s
  (measure; today ~7 s).
- **Screen tests** in `make test` (parallel, one target each, as the fake's
  other checks): a short replay with the flat sheet
  (`tests/fixtures/pad_flat.png`: each sprite a solid square of its own
  colour, pressed ≠ released) and `screens`; the test compares each PNG
  exactly with the expected frame it builds itself. **Each test builds its
  expected frame the way that suits it** (a few lines of Lua filling
  rectangles, an SVG rasterised by ImageMagick, a committed PNG…): no
  universal tool is built. The expected frame is written before the code
  (TDD). An expected frame made with an outside tool (ImageMagick…) is made
  once and committed as a PNG: a check of `make test` never runs an outside
  tool (design.md, "`make test` and the fake's checks", rule 2).
- **Timing gate per step.** Every step that adds a process or a fake check
  to `make test` (a C check, a capture program, or new work in an existing
  fake check: steps 6, 7, 9, 16) first adds up its new
  checks' own times (from step 1.4: one ~80 ms check ≈ 2.5 % of
  `make test`) and groups them to fit, then passes design.md's timing gate
  (same section, rule 4) before it closes, and records the medians in §6.
  From step 5 on, the gate also measures against **the plan's start
  commit, 8c830c6**, beside the two design.md baselines: the paired median
  against it must stay ≤ 3 % (§4's "no slower than before the plan");
  a step over it reworks or merges checks (its own or earlier plan
  checks) before it closes. A step that only adds `tests/lua/game/` files
  (one `lua-test-game` process since step 6: steps 10–13) records
  `lua-test-game`'s own time before and after in its note, and runs the
  full gate only if it grew by more than ~10 ms.
- **`make e2e` time per step.** Every step that changes the fake's
  per-frame work or what the game does each frame (steps 4, 5, 7, 14,
  15, 16) measures `make e2e` wall time and records it in §6 (before the
  plan: ~6–7.4 s on 4 cores, 20 replays): the median of 5 runs, beside the
  step's "before" commit in the same session. The step fails if the median
  plus the touch replays' estimated cost (their frame count, twins
  included, capped at 720 frames by step 4, times the per-frame cost
  measured at step 4) leaves less than 1 s under 9 s; it then reworks its
  cost before it closes. From step 15 on, the committed touch replays are
  measured instead of the cap, and step 15 still leaves the 1 s margin for
  step 16's drawing. When the "before" commit's own projection already
  exceeds the limit in the same session (a slow session), the step is
  judged on its paired difference from "before" (median of the per-run
  differences ≤ 2 %), and its note records both numbers and flags the
  budget for the next step that adds e2e cost.
- **design.md follows each step.** Every step that changes the fake's
  contract updates `docs/love-port/design.md` in the same step; its check
  includes "design.md states the new behaviour". The contract's sentence
  "extending it is a decision recorded here, with its real-LÖVE capture"
  is rewritten by the first such step: this plan's extensions are verified
  by fake C tests and screen tests, not by real-LÖVE captures.
- **One static test of the real sheet**: every sprite of `pad.png` drawn at a
  fixed position, compared with a committed PNG. It breaks when the art
  changes, on purpose: regenerate it then.
- **Pure-Lua tests** (`tests/lua/game/`): layout per window size (mode,
  game and pad rectangles), stick sectors and dead zone, touch → mask,
  show/hide.

### Packaging

- `make love-file` (outside `make test-all`, with a `## ` help line) zips the
  content of `game/` with `main.lua` at the zip's root into
  `tmp/slipways.love`, real `pad.png` included. A check in `make test`
  verifies every module the game requires and every PNG it loads is under
  `game/`.
- Delivery is the user's: import the `.love` into Love2D Studio (AirDrop,
  Files). The app's git sync was rejected: `main.lua` is not at the repo
  root.
- `conf.lua`: `resizable = true`, `highdpi = true`, window 640×480 (the
  game's shape; tall mode is for the phone). The layout works in LÖVE units.

### Out of scope

- iPad-specific and landscape-specific layouts (they get overlay mode).
- A layout that follows rotation beyond what the mode rule gives.
- Audio (`CLAUDE.md`).
- Haptics, a settings screen, remappable touch buttons.
- Fading the pad, a light effect on press.

## 3. Phases and steps

Every step's check includes, where it applies, the two rules of §2: the
timing gate for a new `make test` check, and design.md updated for a change
to the fake's contract.

### Phase 1 — The fake LÖVE learns windows, touches and the screen

1. `setColor` accepts any colour and alpha in `tmp/fake_love`; the canvas 0
   end-of-frame check is unchanged. Verified: a fake check that a non-palette
   `setColor` then drawing on the screen passes, and one drawing it on canvas
   0 still fails with the frame error; the 20 replays unchanged.
2. Replay fields `window` and per-frame `resize`; `love.graphics.getDimensions`
   and the `love.resize` callback in the function list. Verified: fake C
   tests reading the size before and after a resize, and the callback order
   (resize before update).
3. Replay fields `touches` and `mouse`; `love.touch.getTouches/getPosition`,
   `love.mouse.getPosition/isDown` in the function list. Verified: fake C
   tests over a replay with touches appearing, moving, lifting; and a replay
   using the new fields (touch-only frames included) passes
   `tools/e2e_limits.sh` with the right frame count, the checker adapted if
   needed.
4. The screen framebuffer, the present at any offset and scale, and
   `screens`, with the rasterisation rule of §2 written in design.md.
   Verified: fake C tests comparing screen PNGs with expected frames computed
   by the test: fractional scale, offset, and the 390×844 present
   (scale 2.4375, where source edges land exactly on pixel centres, e.g.
   x = 19.5), which exercises the half-open rule.
5. Scaled quad draws onto the screen, alpha blending, and `pad_sheet`.
   The fake's `newImage` refusal of translucent ImageData is lifted for
   images drawn on the screen (or the plan's sprites are proven opaque —
   whichever the step decides, written in design.md). Verified: fake C
   tests: a quad stretched by a fractional scale and a replay whose
   `pad_sheet` replaces `pad.png`; plus, if the refusal is lifted, a PNG
   with translucent pixels loaded by name, made an image and blended onto
   the screen; if the sprites stay opaque, step 9's check gains a test that
   both sheets load as opaque images, and design.md's asset-PNG rules are
   updated there.

### Phase 2 — Fractional, centred present and resizable window

6. A layout module computes, from a window size, the mode (tall / overlay),
   the game rectangle and the pad rectangle. First, the step plans the rest
   of the `make test` budget: it measures at `-O2` one Lua game-test process
   and one short fake run that boots the game with `screens`, lists every
   check steps 6–16 will add with its estimated cost against the headroom
   left versus 8c830c6, decides their grouping (e.g. step 9's sheet test as
   a `screens` frame of a screen replay, steps 10–13 in as few processes as
   fit, step 16's cases inside step 7's replays), and records the list in
   §6; each later step's note updates the remaining headroom. Verified: Lua tests for
   390×844 (tall, the exact values of §2), 640×480, 844×390, 820×1180, and
   the boundary `height == 2 * width` (overlay).
7. `screen.present` draws the canvas into the game rectangle; `conf.lua`
   gets `resizable`, `highdpi`, 640×480. Verified: screen tests at
   640×480 and 390×844 compare the whole screen exactly; the 20 replays
   unchanged. These are the first `make test` checks that boot the game:
   the step measures one's cost, projects the timing gate for every screen
   test the plan adds (step 16's six cases included), and records in §6 how
   step 16 groups its cases into replays (several `screens` frames per
   replay) to stay within the gate.
8. `make love-file` and the completeness check (it does not depend on the
   pad, and a build can be tried on the phone this early). Verified: the
   check in `make test`; `unzip -l tmp/slipways.love` shows `main.lua` at
   the root; and `make love-file` runs the fake on the unpacked archive
   (`LOVE_E2E_GAME`) for at least one replay, matching its `.sha256`.

### Phase 3 — The virtual pad

9. `game/pad.png` (first art), its quad table, `tests/fixtures/pad_flat.png`,
   and the static sheet test. Verified: that test.
10. Pad element table in reference units and their screen rectangles.
    Verified: Lua tests per window size; each button ≥ 44 units on 390×844.
11. Stick: floating base, knob cap, dead zone, 8 sectors. Verified: Lua
    tests on the sector edges (30°/60° boundaries) and the dead zone edge.
12. Touch → mask: stick touch ownership, buttons hit every frame, mouse as a
    touch, OR with the gamepad. Verified: Lua tests (slide A → B, two
    fingers, stick touch leaving its zone).
13. Show / hide state: hidden at start, the revealing touch presses nothing,
    a gamepad button hides and drops touches. Verified: Lua tests.
14. Wire touch → mask and show / hide into `main.lua`, drawing nothing.
    Verified: the 20 replays unchanged.
15. The touch e2e replays of §2, each with its `.txt` and a `.sha256` equal
    to its pad equivalent's (the pad is drawn on the screen, never on
    canvas 0, so they do not wait for the drawing). Each pad equivalent is a
    pad twin with the same frames, written by this step (the existing pad
    replays are long full scenarios); the touch replay's `.sha256` is the
    twin's hash, never a baseline of the touch replay itself. Committed twins
    count toward the e2e time. Verified: each touch hash equals its twin's;
    `make e2e` green and under 9 s.
16. Draw the pad on the screen with released and pressed sprites. Verified:
    screen tests with `pad_flat.png` — hidden, shown in tall mode, shown in
    overlay mode, a button pressed, the stick pushed on a diagonal, after a
    `resize` — each against an exact expected frame; the 20 replays and the
    touch replays unchanged; `make love-file` still passes.
17. skill: check-e2e — the whole e2e suite after the pad is wired.

### Phase 4 — Clean-up

18. A final read of design.md's fake contract against the fake's function
    list and replay fields, fixing any drift. Verified: every function and
    field the fake accepts is named in design.md and in
    `tools/lua_guard.lua`, and the reverse; and
    the `make test` timing gate against the plan's start commit 8c830c6
    passes (§4).
19. skill: rework-comments — the files this plan created or changed under
    `game/` and `tools/fake_love/`.

## 4. Done when

- `make test-all` green, `make test` no slower than before the plan,
  `make e2e` under 9 s.
- The 20 original replays keep their `*.sha256` and `*.end-state`.
- Every touch replay's hash equals its pad equivalent's.
- Every screen test passes with an exact comparison.
- `make love-file` produces `tmp/slipways.love` with `main.lua` at its root.
- `docs/love-port/design.md` describes the extended fake contract, the
  layout and the pad; the "nothing in these plans targets iOS" line is gone.

## 5. Open questions

- Which LÖVE version Love2D Studio embeds is unknown (the store page does
  not say). The APIs used here are stable since 11.0; if the game misbehaves
  on the phone, check that first.

## 6. Step notes

### Step 1.1

`setColor` takes 3 or 4 finite numbers in both binaries; alpha becomes a
byte like r, g and b (alpha 2 stores `FF`). The palette-or-white and
alpha-must-be-1 refusals and the "any colour" switch
(`host_allow_any_color`) are gone. Scope decision of the PRD: a
translucent colour drawn by `points`, `rectangle` or `draw(image, quad)`
onto a canvas is refused at the call (blending on a canvas is not
modelled); screen draws take anything. The present still requires opaque
white; the canvas 0 frame check is unchanged. No `make test` check added:
the two set_color failure fixtures became `set_color_screen` (passes) and
`set_color_canvas_zero` (frame error), so the timing gate did not apply;
the set_color check runs in 32–41 ms. design.md updated, including the
"with its real-LÖVE capture" sentence. `make e2e`: 20/20.

### Step 1.2

Replays take `"window": [w, h]` (default 640×480) and a per-frame
`"resize": [w, h]`, integers 1–4096; a resize to the size in force is
refused at parse time; a frame holds only `pad` (still required) and
`resize`. The host holds the size; `getDimensions` answers it whatever
canvas is selected; on a resize frame `love.resize(w, h)` runs first,
before the pad callbacks and `update`, never for the start size.
`host_frame` takes a frame-input struct (for step 3's touches). PRD
decisions: start-state members are read before the frames (the resize
check needs the window); `lua-guard` extended now; `t.window` stays
ignored; `highdpi` not modelled. New check `test_fake_love_window`,
43–54 ms. Timing gate (11 rounds): paired median +1.07 % vs 54cd5d8,
−87 % vs 8dff812 (plain medians 0.893 / 0.892 / 6.848 s): pass.
`make e2e`: 20/20.

### Step 1.3

A frame may hold `pad`, `resize`, `touches` and `mouse`, each optional
and at most once; a missing `pad` holds nothing, so touch-only frames and
`{}` are valid. `touches`: at most 10 `[id, x, y]`, ids 1–2147483647
unique in the frame, inside the window after the frame's own resize.
`mouse`: `[x, y, left]`, kept until the next one, (0, 0) up before the
first. The fake answers `love.touch.getTouches/getPosition` (ids are light
userdata equal to the replay id; a lifted id raises `Invalid active touch
ID`) and `love.mouse.getPosition/isDown(1)`, made current with the pad
after `love.resize`; touch and mouse callbacks stay refused.
`tools/e2e_limits.sh` counts the objects of `frames` (one awk for all
replays: ~280 → ~185 ms). Shared fixture `tests/fixtures/touch_frames.json`.
New check `test_fake_love_touch`, ~55–80 ms. Timing gate: first 11
rounds +3.14 % vs 6cd6898, so 22 pooled: +2.78 % vs 6cd6898, −86.67 % vs
8dff812 (plain medians 0.894 / 0.877 / 6.572 s): pass, with little margin
left for later checks. `make e2e`: 20/20.

### Step 1.4

The fake has a screen: an RGBA framebuffer of the window size, opaque
black at each frame start (after the resize), rasterised only on frames
a replay lists in `"screens"` (at most 16, ascending), written to
`<capture>.screen-f<N>.png` and removed on a later failure. The rule
(design.md "The screen"): a pixel belongs to a draw if its centre lies in
the destination, half-open; texel `floor((centre − x) / s)`, clamped, in
double; alpha `00` leaves the pixel. The present takes any finite
position below 2^20 and any equal scale in (0, 1024]; a translucent
canvas is refused until step 5. The PNG writer moved into the fake
(`png_file_write`), with table-driven CRC-32 and a faster Adler-32
(checked against zlib) and `-O2` for `png_file.o`, to keep the new
checks cheap. New: unit test `test_fake_love_screen` (~22 ms), check
`test_fake_love_screens` (~70–85 ms, close to the 100 ms budget; its
malformed-`screens` refusals run once, the reader's test covers each).
`make test` gate (11 rounds): +2.49 % vs 1a7033b, −86.8 % vs 8dff812:
pass. `make e2e`: median 6.756 s (1a7033b: 6.630 s); per-frame cost
1.542 ms (user + sys / 4 072 frames / 4 cores). **Touch-replay cap for
step 15: 720 frames, twins included.** Projection 6.756 + 720 × 1.542 ms
= 7.87 s, under 8 s by 0.13 s: little margin. `make e2e`: 20/20.

### Step 1.5

The screen draw takes a quad, two scales and a tint, and blends in
LÖVE's "alpha" mode with llvmpipe's `mul` (`min(255, mul(s,a) +
mul(d,255−a))`), the screen's alpha staying `FF`; the present goes
through it. The PRD lifted `newImage`'s refusal of translucent ImageData
(not "sprites proven opaque"): an Image remembers its first translucent
texel and is refused only when drawn onto a canvas; so step 9 needs no
opacity test. The 4-argument quad draw reaches the screen;
`draw(image, quad, x, y, 0, sx, sy)` draws onto the screen only. Replay
member `"pad_sheet"` (a `.png` next to the replay, ≤ 1 MiB) is served as
`pad.png`, shadowing the game's. No new `make test` process: the screen
cases (390×844, `pad_sheet`, two `screens`, exact frames) and the
missing-sheet refusal run inside `test_fake_love_image_draw` (~25 → ~55
ms); expected-frame helpers shared in `tests/expected_screen.{c,h}`.
**Gate against the plan's start failed**: +5.63 % vs 8c830c6 (22 rounds,
accumulated over steps 1.2–1.4; this step +0.02 % vs 8f4c7f6). Guideline
decision: default `CFLAGS` gain `-O2` for the fake and every check (no
`-ffast-math`). Then, 11 rounds: −1.55 % vs 8f4c7f6, +1.29 % vs 8c830c6,
−87.03 % vs 8dff812: pass; **headroom left vs 8c830c6: ~1.7 points.**
`make e2e`: 5.202 s (8f4c7f6: 6.007 s), projection 5.202 + 720 × 1.542
ms = 6.31 s (the 1.542 ms per-frame cost predates `-O2`, so it is
cautious; step 7 re-measures it). `make e2e` and `make end-state` 20/20.

End of phase 1 — warn-review flagged 23 design smells in the fake
(duplicated limits and constants, `replay.c`'s size and JSON parser,
twin touch types, `touch.h` ↔ `host.h`, `lua_guard.lua` a second copy of
the function list, `graphics.c` without a unit test, hand-listed Makefile
checks, `test_fake_love_screen` vs `_screens`). plan-review accepted two
method changes (step 6 plans the remaining `make test` budget; step 18
also compares `tools/lua_guard.lua`); the other warnings are left open,
the fake's format being frozen after phase 1.


### Step 2.6

`love.window_layout.compute(width, height)` (in `game/love/`, beside
`love.screen`; `game.layout` was taken) returns the mode (`MODE_TALL` when
`height > 2 * width`, else `MODE_OVERLAY`) and the game and pad rectangles
`{x, y, width, height, scale}`, fitted at max scale, centred, never
rounded; exact tests for 390×844, 640×480, 844×390, 820×1180, 390×780,
390×781. Nothing calls it yet. All `tests/lua/game/` tests now run in one
LuaJIT process (`tests/lua/game/runner.lua`, target `lua-test-game`; a
file ends with `return check.run({...})`), so later test files add no
process; the skills' per-file commands follow.

**Budget plan** (`-O2`, 4 cores): 1 point ≈ 32 ms of a check's own time;
game boot in the fake ~43 ms + ~1.6 ms per frame; a 390×844 screen PNG
~4.7 ms + ~5.4 ms to compare (640×480: ~5 + ~9.5 ms); a Lua test process
~5 ms. Checks still to come: step 7 one target, one boot, 390×844 and a
resize to 640×480, ~67–73 ms; step 8 inside `lua-guard`, ~1–2 ms; step 9
a non-game run inside an existing fake check, window ≤ 256×128,
~8–10 ms; steps 10–13 files of the runner, ~2 ms; step 16 ~6 frames and
`screens` appended to step 7's replay with small windows (160×322 or
200×420 tall, 160×120 or 320×240 overlay), ~+17 ms (step 7+16 target
~85–90 ms, under 100 ms). Total ~3.2–3.3 points. Reclaims: (a) fold
`test_fake_love_touch`'s misuse runs (done in this step, see below);
(b) `test_fake_love_screens`: frames 2–4 at ~200×150, a tiny replay for
its failing run, drop the malformed-`screens` CLI refusal (≈ −25–30 ms,
step 7); reserve (c) `test_fake_love_window` misuse via pcall, drop
`window_invalid` (≈ −11 ms). Biggest levers outside the plan:
`test_e2e_limits` ~0.70 s of own time, `test_love_e2e` ~255 ms.

Gate: first run failed against 8c830c6 (+3.34 %, 22 rounds; this step
+0.23 % vs c67b0b4). Guideline decision: reclaim (a) pulled into this
step — seven misuses in one pcall-logged run, compared whole; the
`touch_invalid` CLI refusal dropped (its message asserted in
`test_fake_love_replay`); 11 fake runs → 4, ~61 → ~27 ms. Rerun, 11
rounds: −1.26 % vs c67b0b4, +2.32 % vs 8c830c6, −87.51 % vs 8dff812:
pass. **Headroom vs 8c830c6: ~0.7 point.** `make e2e`: 20/20.

### Step 2.7

`screen.present` draws canvas 0 into the game rectangle of
`love.window_layout` for `love.graphics.getDimensions()`, every frame,
unrounded, with no `love.resize` handler; `screen.SCALE` is gone; at
640×480 the draw is the old one, so the 20 replays keep their hashes.
`conf.lua`: `resizable`, `highdpi`, 640×480 (tested by a runner file).
New check `test_game_screens` (first among the fake checks, ~64–72 ms):
one boot of `game/` on `tests/fixtures/game_screens.json`, 390×844 then a
resize to 640×480, both whole screens compared exactly with frames built
from the run's own capture at the literal rectangles (0, 64.75, 2.4375)
and (0, 0, 4).

Reclaims, each keeping its assertions (dropped CLI refusals are asserted
in `test_fake_love_replay`, the CLI refusal path by `pad_invalid`):
(b) `test_fake_love_screens` frames 2–4 at 200×150, a 40×30 failing
replay, ~71 → ~36 ms; (c) `test_fake_love_window` `getDimensions(1)` as a
pcall-logged line, `window_invalid` dropped, ~40 → ~29 ms; (d)
`tools/e2e_limits.sh`'s five greps per replay folded into its one awk,
102 → 2 processes, `test_e2e_limits` (unchanged, on `make test`'s
critical path) ~800 → ~415 ms. Reserves (e)–(g) not needed.

Gate, 11 rounds: −10.36 % vs ee5e66a, −12.31 % vs 8c830c6, −88.53 % vs
8dff812: pass; **headroom vs 8c830c6: ~15 points** (was ~0.7).
**Step 16's grouping**, projected on a scratch replay: one replay
(`game_screens.json`), one boot, nine frames, six `screens` — 390×844,
640×480, then 200×420 (tall, (0, 30), scale 1.25) with the touches, then
320×240 (overlay, scale 2) — ~90 ms, under 100 ms with ~10 ms to spare,
pad drawing not included: step 16 adds no frames beyond these nine.
`make e2e`: median 4.836 s (ee5e66a: 5.058 s); **per-frame cost at
`-O2`: 1.124 ms** (replaces 1.542 ms); projection 4.836 + 720 × 1.124 ms
= 5.65 s ≤ 8 s. `make e2e` 20/20.

Risk noted by the PRD, for the phone: with `highdpi`, LÖVE 11's
`newCanvas` takes the screen's DPI scale, so the 160×120 canvas may be
3× dense on a Retina screen and 1-pixel points may leave gaps; the fix
would be `dpiscale = 1`, a function-list extension. Not acted on.

### Step 2.8

`lua-guard` now also refuses a `require` whose argument is not a literal,
a module resolving neither to `<root>/<name>.lua` nor to
`<root>/<name>/init.lua` (`bit` built in), and a string literal naming a
`.png` that is not under the root (comments and longer messages
ignored): the archive of `game/` is complete by construction, no new
process. `make love-file` replaces `make love` (outside `make test-all`):
it zips `game/` into `tmp/slipways.love` (`main.lua` at the root,
checked), unpacks it into `tmp/love-file/game` and plays
`tests/e2e/pad_title.json` from there, which must match its `.sha256`;
missing zip/unzip, no `main.lua`, a failed run or another hash each fail
with their own message. `tools/love_install.sh` installs `zip` and
`unzip`. README: import into Love2D Studio, gamepad still needed, the
`highdpi` risk. Gate (22 pooled; first 11 had +4.56 % vs 30ec5c5): +2.50 %
vs 30ec5c5, −5.56 % vs 8c830c6, −88.33 % vs 8dff812: pass; headroom vs
8c830c6 ~8.6 points (this session). `make e2e` and `make end-state` 20/20.

End of phase 2 — warn-review flagged: a new test binary named by hand in
four Makefile lists; `love-file` re-implementing the hash comparison of
`tools/love_e2e_report.sh` and repeating the zip check in two recipes;
`lua_guard.lua` a copy of the function list with nothing checking it
against the fake (step 18 now compares them); `check.run` depending on the
global `arg[0]` the runner rewrites; `window_layout` returning a `pad`
rectangle nothing reads yet (step 10 will); `PAD_WIDTH/HEIGHT` literals
equal to `vram.SCREEN_WIDTH/HEIGHT`.

### Step 3.9

The art is generated: `tools/pad_art.c`, run by `make pad-art` (outside
`make test`), writes byte for byte reproducibly `game/pad.png` (240×72
RGBA, PICO-8 palette: stick base 48 and knob 24; A, B, X, Y, L, R,
Start, Back as 24×24 sprites, released and pressed, labels from
`font.png`, translucent bodies), `tests/fixtures/pad_flat.png` (same
rectangles, each one distinct opaque colour) and the static test's
expected frame, composed with the screen's multiply from the generator's
own table. `love.pad_sheet` (`game/love/`) holds the rectangles as data —
buttons keyed by LÖVE gamepad name (`a`, `b`, `x`, `y`, `leftshoulder`,
`rightshoulder`, `start`, `back`), `STICK_BASE`, `STICK_KNOB` — and
`load()` returns the Image and one Quad per sprite; nothing requires it
yet. The static sheet test is one more fake run inside
`test_fake_love_image_draw` (~58 → ~66 ms): all 18 sprites drawn through
the module, compared exactly with the generator's frame, plus
`pad_flat.png`'s solid distinct colours. Step 1.5 had lifted the
translucent-image refusal, so no opacity test. Gate, 11 rounds: −1.27 %
vs 102b0ab, −11.38 % vs 8c830c6, −88.64 % vs 8dff812. `make love-file`
ships `pad.png`. `make e2e` and `make end-state` 20/20.

### Step 3.10

`love.pad_layout` (`game/love/`, pure) holds the reference table:
`STICK_ZONE` (0, 0, 80, 120), `STICK_BASE_SIZE` 48, `STICK_KNOB_SIZE` 24,
`BUTTONS` keyed by gamepad name, each 24×24 — Y (108, 36), X (84, 60),
B (132, 60), A (108, 84) (diamond centred at (120, 72)), L (4, 4),
R (132, 4), Back (54, 94), Start (82, 94). Rectangles half-open; a touch
that starts on a button belongs to it, never to the stick zone (L and
Back lie inside it: rule for step 12). `place(pad)` gives exact
unrounded screen rectangles: 58.5 points per button at 390×844 (≥ 44).
Back and Start are "small" by their drawn pill, not by a smaller touch
area. 16 tests in `tests/lua/game/test_pad_layout.lua` (390×844, 640×480,
844×390, 820×1180, 200×420). `lua-test-game` own time: no measurable
growth, so no full gate (§2). `make e2e` 20/20.

### Step 3.11

`love.pad_stick` (`game/love/`, pure): `start(x, y, base_size)` returns
`{base_x, base_y, radius = base_size / 2, knob_x, knob_y, mask = 0}`
with the base where the touch starts, unclamped; `follow(stick, x, y)`
moves the knob to the touch, its centre capped at the base's edge, and
sets `mask`. Dead zone: the open disc of 25 % of the radius (a touch on
its edge already points). Sectors without angles: horizontal when
`dx² ≥ 3·dy²`, vertical when `dy² ≥ 3·dx²` (exactly 60° each, ties go
straight), otherwise a 30° diagonal with two bits. `UP`, `DOWN`, `LEFT`,
`RIGHT` = 1, 2, 4, 8 (dpad bits of `PAD_BUTTON_NAMES`), screen y down.
15 tests, both sides of every edge in all quadrants, exact double ties,
the dead-zone edge; six mutations each caught. `lua-test-game` 74 →
79 ms (noise), no full gate. `make e2e` 20/20.

### Step 3.12

`love.pad_touch` (`game/love/`, pure, requires `bit`, `p8.input`,
`love.pad_stick`): `BUTTON_MASKS` (a = 16 … back = 2048, from
`p8.input`), `init(touch_state)`, `update(touch_state, touches, mouse,
placed)` → virtual mask, `combine(gamepad, virtual)` = OR. Touches are
`{id, x, y}`; the mouse is nil or `{x, y, down}`, counted as one more
touch while down. Per frame: lift (the stick's touch gone → no stick),
follow (the stick follows its touch anywhere, base fixed), claim (with
no stick, the first touch new this frame, in the stick zone, on no
button), buttons (every other touch at its current position,
half-open), result = stick mask | button bits. A touch missing from a
call counts as lifted (step 13 relies on it). 24 tests (slide A → B, two
fingers = the 5+6 chord, stick leaving its zone, L/Back win, same-frame
hand-over, resize); four mutations caught. `lua-test-game` 92 → 86 ms
(noise), no full gate. `make e2e` 20/20.
