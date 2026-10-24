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
- **Timing gate per step.** Every step that adds a check to `make test`
  passes design.md's timing gate (same section, rule 4) before it closes,
  and records the medians in §6.
- **`make e2e` time per step.** Every step that changes the fake's
  per-frame work or what the game draws each frame (steps 4, 5, 7, 16)
  measures `make e2e` wall time and records it in §6 (before the plan:
  ~6–7.4 s on 4 cores, 20 replays): the median of 5 runs, beside the step's
  "before" commit in the same session. The step fails if the median plus
  the touch replays' estimated cost (their frame count, twins included,
  estimated once at step 4 and recorded in §6 as a cap step 15 keeps or
  re-measures, times the per-frame cost measured at step 4) leaves less than 1 s under 9 s; it then reworks
  its cost before it closes.
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
   the game rectangle and the pad rectangle. Verified: Lua tests for
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
    field the fake accepts is named in design.md, and the reverse.
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
