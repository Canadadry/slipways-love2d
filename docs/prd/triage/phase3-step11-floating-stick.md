---
title: "The floating stick: base, capped knob, dead zone and 8 sectors"
description: "The virtual pad has a stick zone but no stick: nothing turns a finger's start point and current point into a knob position and the four direction bits of pad_buttons."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`, §2 Input) steers the game on a portrait iPhone
with a *floating* stick on the left half of the virtual pad: the base
appears where the thumb lands, the knob follows the thumb but never leaves
the base's edge, a small dead zone around the centre means "no direction",
and outside it the angle picks one of 8 sectors (up, down, left, right
60° wide each; the four diagonals 30° each). A diagonal must press two
direction bits at once, as a real gamepad's d-pad does, so the aim and
placement pans (`pad_nav`'s `aim_pan`) move on both axes.

Today nothing computes any of this. Step 10 gives the stick zone and the
base and knob sizes on the screen; step 12 (touch → mask) and step 16
(drawing) need, for one stick touch, the base centre, the knob centre and
the 4-bit direction mask. Without one exact rule, a thumb resting right on
a sector edge or on the dead-zone edge could flicker between two answers
depending on how the code is written, and no test would say which answer
is right.

## Solution

A new pure-Lua module, `love.pad_stick`, beside `love.pad_layout`:

1. **`start(x, y, base_size)`**: a stick whose base is centred where the
   touch starts, its knob at the centre, no direction.
2. **`follow(stick, x, y)`**: moves the knob to the touch's current
   position, capped at the base's edge, and recomputes the direction mask
   from the offset to the base centre: nothing inside the dead zone (25 %
   of the base radius), else one of the 8 sectors, a diagonal setting two
   bits.
3. **The four direction masks** `UP`, `DOWN`, `LEFT`, `RIGHT` = 1, 2, 4, 8:
   the bits of `dpup`, `dpdown`, `dpleft`, `dpright`, buttons 1–4 of
   `main.lua`'s `PAD_BUTTON_NAMES`.
4. **Lua tests** in `tests/lua/game/` (the one `lua-test-game` process)
   on both sides of every 30°/60° edge in all four quadrants, on an exact
   floating-point tie on each kind of edge, on both sides of the dead-zone
   edge, and on the knob cap, all with exact `==` comparisons.
5. design.md gains a "**Pad stick.**" paragraph after "Pad layout".

Nothing calls the module yet; the game and the 20 replays do not change.

## User Stories

1. As a player on a portrait iPhone, I want the stick's base to appear where my thumb lands on the left half of the pad, so that I never have to find a fixed stick without looking.
2. As a player, I want the knob to follow my thumb, so that I see which way I am pushing.
3. As a player, I want the knob to stop at the base's edge when my thumb goes further, so that the drawn stick never stretches across the screen.
4. As a player, I want pushing further than the base's edge to keep the same direction, so that a big thumb movement does not change what I steer.
5. As a player, I want a small dead zone around the base centre, so that a resting thumb presses no direction.
6. As a player, I want the dead zone to be a quarter of the base radius, so that a short, deliberate push is enough to steer.
7. As a player, I want up, down, left and right to be 60° wide each, so that a sloppy thumb still moves in a straight line, which jumps and menus need.
8. As a player, I want the four diagonals to be 30° wide each, so that I can still pan diagonally when I aim on purpose.
9. As a player, I want a diagonal to press two directions at once, so that aim and placement pans move on both axes as with a real d-pad.
10. As a player, I want the stick's directions to match the screen (up is up), so that pushing up never moves the cursor down.
11. As a player, I want the stick to give the same bits as the gamepad's d-pad, so that the game behaves exactly as with a controller.
12. As a player, I want a thumb resting exactly on a sector edge to give one stable answer, so that the direction never flickers between a straight and a diagonal.
13. As a player, I want a thumb resting exactly on the dead-zone edge to give one stable answer, so that a direction never blinks on and off.
14. As a player, I want the knob to start at the base centre when I touch, so that a fresh touch presses no direction until I move.
15. As a player on desktop or in landscape (overlay mode), I want the stick to scale with the pad, so that the dead zone and the cap feel the same everywhere.
16. As a developer of step 12 (touch → mask), I want a stick created from the touch's start point and the placed base size, so that ownership of the touch stays in step 12 and the geometry here.
17. As a developer of step 12, I want the direction as a mask of `pad_buttons` bits 1–4, so that I OR it with the buttons' bits and the gamepad's mask with no translation.
18. As a developer of step 12, I want the stick to follow a touch anywhere on the screen, so that a stick touch leaving its zone keeps steering, as plan §2 requires.
19. As a developer of step 16 (drawing), I want the base centre and the knob centre in LÖVE screen units, so that I draw the base and knob sprites centred on them with no conversion.
20. As a developer of step 16, I want the knob position already capped, so that the drawing code holds no geometry.
21. As a developer, I want the module to take the base size, not the window or the pad scale, so that it does one thing and `love.pad_layout` stays the only place that sizes the base.
22. As a developer, I want the module free of any `love.*` call and of state outside the stick table, so that it runs under plain LuaJIT in `lua-test-game` and `lua-test-modules`.
23. As a developer, I want the sector test done with products of the offsets, never with an angle function, so that the edges are exact and no trigonometric rounding decides a sector.
24. As a developer, I want `start` and `follow` to refuse a non-numeric coordinate or a base size that is not a positive number, with a message naming the field, so that a wiring mistake fails at the call.
25. As a developer, I want the exports to follow CLAUDE.md naming (no `pad_` prefix, verb names for functions), so that `make lua-names` stays green.
26. As a maintainer, I want the boundary rules written once in design.md, so that a later change to the stick is a decision, not a drift.
27. As a maintainer, I want tests on the nearest integer points on both sides of each edge in all four quadrants, so that a flipped comparison or a wrong sign names itself.
28. As a maintainer, I want tests on exact floating-point ties, so that the "tie goes to the straight sector" rule is proven, not assumed.
29. As a maintainer, I want tests on both sides of the dead-zone edge, on an axis and on a diagonal, so that `<` versus `<=` is pinned.
30. As a reviewer, I want every expected value exact in a double, so that the tests compare with `==` and never need a tolerance.
31. As a reviewer, I want the 20 replays' `*.sha256` and `*.end-state` unchanged, so that this step is proven to change no game behaviour.
32. As a reviewer, I want `lua-test-game`'s own time recorded before and after, so that plan §2's timing rule for steps 10–13 is followed.

## Implementation Decisions

**Hypotheses on step 10 (not yet done; its PRD is
`docs/prd/triage/phase3-step10-pad-element-layout.md`).** The implementer
checks each first; one that fails is reported, and the decision resting on
it is revisited.

- **H-10-1**: step 10 is committed and `make test-all` is green on it; its
  last commit is this step's "before" commit for the `lua-test-game`
  timing.
- **H-10-2**: `love.pad_layout` exists in `game/love/`, is pure, exports
  `STICK_BASE_SIZE = 48` and `STICK_KNOB_SIZE = 24` (reference units), and
  its `place(pad)` returns `stick_base_size` and `stick_knob_size` in LÖVE
  screen units, equal to the size times `pad.scale`, never rounded (so 117
  and 58.5 at 390×844, 120 at a scale of 2.5).
- **H-10-3**: those sizes are **diameters** (the base sprite is 48×48, the
  knob 24×24), so the base radius is `base_size / 2`.
- **H-10-4**: step 10 leaves the stick's position free ("the base's
  position is where the touch starts, step 11") and adds no hit-test; the
  stick zone check and the "a button wins over the stick zone" ownership
  rule are step 12's.
- **H-10-5**: step 10 does not change `tests/lua/game/runner.lua`,
  `check.lua`, `lua-names`' prefix rule (a module's first word, here
  `pad`, may not start an export), or `love.window_layout`.
- **H-10-6**: step 10 adds design.md's "**Pad layout.**" paragraph right
  after "**Window layout.**"; this step's paragraph goes right after it.
- **H-10-7**: step 10 adds only `tests/lua/game/test_pad_layout.lua` to
  the runner, so `lua-test-game`'s own time at H-10-1's commit is close to
  today's.

**The module, `love.pad_stick`** (in `game/love/`, beside
`love.pad_layout` and `love.pad_sheet`: it is part of the LÖVE-side
virtual pad, not of the cart; `game/game/` holds the cart's own `pad_*`
navigation modules). Pure: no `require` of `love`, no `love.*` call, no
module state; everything lives in the stick table the caller holds. It
works in LÖVE screen units (the window coordinates `love.touch` gives)
with screen y growing downward.

**Exports** (CLAUDE.md naming; no export is `pad` or starts with `pad_`):

- `UP = 1`, `DOWN = 2`, `LEFT = 4`, `RIGHT = 8`: the masks of
  `pad_buttons` bits 0–3, i.e. buttons 1–4 `dpup`, `dpdown`, `dpleft`,
  `dpright` of `main.lua`'s `PAD_BUTTON_NAMES` (bit `n − 1` for button
  `n`, as `read_pad_buttons` builds it). `main.lua`'s list is local and
  needs LÖVE, so the module writes the four values once with a comment
  naming that list; step 14 may assert them against it when it wires the
  stick.
- `start(x, y, base_size)`: returns a new stick table
  `{base_x = x, base_y = y, radius = base_size / 2, knob_x = x, knob_y =
  y, mask = 0}`. `base_size` is the placed `stick_base_size` (H-10-2,
  H-10-3). The base is not clamped to the stick zone or the window: plan
  §2 puts it where the touch starts; a base near an edge is drawn cut by
  the screen (step 16).
- `follow(stick, x, y)`: sets `knob_x`, `knob_y` and `mask` from the
  touch's current position; returns nothing (the caller reads the
  fields). Called with the start point it gives back the fresh stick's
  values. The touch may be anywhere on the screen (a stick touch that
  leaves its zone keeps steering, plan §2).
- Refusals: a non-number `x`, `y` (in either function) or a `base_size`
  that is not a number `> 0` fails an `assert` whose message starts with
  `pad_stick.start:` or `pad_stick.follow:` and names the argument (the
  style of `window_layout.compute`'s `check_size`). `follow` also refuses
  a `stick` that is not a table.

**The geometry** (offset `dx = x − base_x`, `dy = y − base_y`, both
computed once; `distance_squared = dx * dx + dy * dy`; no angle function,
no square root except for the knob cap):

- **Knob cap.** If `distance_squared <= radius * radius`, the knob is the
  touch itself. Otherwise the knob is
  `base + (dx, dy) * (radius / math.sqrt(distance_squared))`: on the
  base's edge, in the touch's direction. The knob's *centre* is capped at
  the base's edge (the knob may overhang the base by its own radius), the
  usual on-screen-stick look; plan §2's "capped at the base's edge" read
  this way.
- **Dead zone.** Its radius is `radius * 0.25`, which is `base_size / 8`
  exactly (a division by a power of two). **Rule: the dead zone is the
  open disc**: `mask = 0` when `distance_squared < dead * dead`; a touch
  exactly at 25 % of the base radius already points. Chosen to match the
  pad's other edges (step 10's half-open rectangles and the fake's
  half-open screen rule give an edge to the region beyond it), and so the
  dead zone is the smallest set the plan's words allow.
- **Sectors**, from the raw offset (not the capped knob, which has the
  same direction but went through a square root), with
  `ax = |dx|`, `ay = |dy|`:
  - `ax * ax >= 3 * ay * ay` → **horizontal only**: `RIGHT` if `dx > 0`,
    else `LEFT` (angle within 30° of the horizontal axis, since
    tan² 30° = 1/3);
  - else `ay * ay >= 3 * ax * ax` → **vertical only**: `UP` if `dy < 0`,
    else `DOWN` (within 30° of the vertical axis, i.e. beyond 60° from the
    horizontal, tan² 60° = 3);
  - else **diagonal**: the horizontal bit of `dx`'s sign OR the vertical
    bit of `dy`'s sign.
  This gives right, up, left and down 60° wide each (centred on the axes,
  edges at ±30° and ±150° from the horizontal, ±60° and ±120°) and the
  four diagonals 30° each, as plan §2 requires.
- **Boundary rule on the 30°/60° edges: a tie goes to the straight
  (60°) sector**, through the two `>=`. With exact arithmetic no point but
  the base centre lies on an edge (the edges' slopes 1/√3 and √3 are
  irrational) and the base centre is inside the dead zone, so ties come
  only from double rounding of the products; the rule makes them
  deterministic and gives them to the wider, more common sector. Both
  conditions hold together only at `dx = dy = 0`, which the dead zone
  already answered, so the order of the two tests does not matter.
- The products are exact for every offset whose squares fit in 53 bits
  (any offset of a ≤ 4096-unit window with dyadic coordinates); for
  others, rounding can move a point closer than ~10⁻¹⁶ of its distance to
  an edge to the other side, which no thumb can feel.

**Docs.** design.md gets a "**Pad stick.**" paragraph right after "**Pad
layout.**" (H-10-6): the module, `start` / `follow` and the stick fields,
the mask values and their `PAD_BUTTON_NAMES` origin, the knob cap, the
dead zone (open disc of `base_size / 8`), the sector rule with the two
inequalities and the tie rule, and "y grows downward, so `dy < 0` is up".
`docs/plan-6.md`'s step note is the coordinator's, not this step's.

## Implementation Chunks

1. `start`, the refusals, and `follow`'s knob cap (with `mask` left 0):
   the stick fields and the cap tests.
2. The direction mask in `follow`: the four masks, the dead zone and the
   sectors, with their edge, tie and quadrant tests; then design.md.

## Testing Decisions

- **A good test here** calls `start` and `follow` and reads the stick's
  fields, comparing numbers exactly with `==` (`check.expect`). It never
  reads the module's source and never uses a tolerance: every input is a
  dyadic number chosen so that every offset, product and expected knob is
  exact in a double (each test that relies on a computed offset asserts
  that offset first, so the exactness is checked, not assumed).
- **New test file** `tests/lua/game/test_pad_stick.lua`, in the one
  `lua-test-game` process (prior art: `tests/lua/game/test_window_layout.lua`,
  its `expect_rectangle` helper and its `expect_refused` pcall pattern;
  the file ends with `return check.run({...})`, as the runner requires).
  Masks are written as literals in the test (`1`, `2`, `4`, `8` and their
  sums) as well as checked against the exports once, so a wrong export
  fails.
- **Masks**: `UP, DOWN, LEFT, RIGHT == 1, 2, 4, 8` (the bits of buttons
  1–4 of `PAD_BUTTON_NAMES`).
- **Start**: `start(40, 600, 117)` gives base (40, 600), radius 58.5, knob
  (40, 600), mask 0; `follow` at the start point leaves the same values.
- **Sector edges, all four quadrants** (base (40, 600), base size 117, so
  dead-zone radius 14.625; every offset below is far outside it). The
  nearest integer points on each side of an edge come from `dx² − 3dy² =
  1` and `= −2`:
  - offset `(26, ±15)` / `(−26, ±15)` (29.98°, just inside the
    horizontal sector; `26² − 3·15² = 1`) → `RIGHT` / `LEFT` alone;
  - offset `(19, ±11)` / `(−19, ±11)` (30.07°, just inside the diagonal;
    `19² − 3·11² = −2`) → the diagonal: `(19, −11)` → `UP + RIGHT` = 9,
    `(19, 11)` → `DOWN + RIGHT` = 10, `(−19, −11)` → 5, `(−19, 11)` → 6;
  - offset `(±15, 26)` / `(±15, −26)` (60.02°, just inside the vertical
    sector) → `DOWN` / `UP` alone;
  - offset `(±11, 19)` / `(±11, −19)` (59.93°, just inside the diagonal)
    → the diagonal of their signs.
  One table-driven loop over the 16 offsets, each with its literal
  expected mask; the test name says which side of which edge.
- **Axes and the middle of a diagonal**: `(20, 0)` → 8, `(−20, 0)` → 4,
  `(0, −20)` → 1 (up, screen y down), `(0, 20)` → 2, `(20, 20)` → 10,
  `(−20, −20)` → 5.
- **Exact ties** (base (0, 0), base size 117, so offsets equal the touch
  coordinates exactly; the module is pure, so a base at the origin is a
  valid input): with `t = 20.784609690826528` (4·√27 rounded, a literal in
  the test), the test first asserts `t * t == 3 * 12 * 12` in double (432),
  then: touch `(t, 12)` → `RIGHT` alone, `(−t, −12)` → `LEFT` alone (a tie
  on a 30° edge goes to the horizontal sector); touch `(12, −t)` → `UP`
  alone, `(−12, t)` → `DOWN` alone (a tie on a 60° edge goes to the
  vertical sector). Both distances are 24, outside the dead zone.
- **Dead-zone edge on an axis** (base (40, 600), base size 117, dead
  radius 14.625): touch `(54.625, 600)` (exactly 25 % of the radius) →
  `RIGHT`; touch `(54.625 − 2^-47, 600)` (the next double below; the test
  asserts the offset is `14.625 − 2^-47`) → 0. Also `(40, 585.375)` → `UP`
  and `(40, 585.375 + 2^-43)` → 0 (the next double above, 585.375's ulp
  being 2^-43; the test asserts the offset is `−14.625 + 2^-43`).
- **Dead-zone edge on a diagonal** (base (40, 600), base size 120 — 48 ×
  2.5 — so radius 60 and dead radius 15): touch `(49, 588)`, offset
  `(9, −12)`, distance exactly 15 → `UP + RIGHT` = 9 (53.13°, a
  diagonal); touch `(49, 588 + 2^-43)` (the test asserts the offset
  `−12 + 2^-43`) → 0.
- **Knob cap** (base (40, 600), base size 120, radius 60): touch `(76,
  648)`, offset `(36, 48)`, distance exactly 60 → knob = touch (76, 648);
  touch `(112, 504)`, offset `(72, −96)`, distance 120 → knob `(76, 552)`,
  mask 9; touch `(40, 800)` → knob `(40, 660)`, mask 2; a touch inside the
  base `(50, 610)` → knob = touch. The mask of a capped touch equals the
  mask of the same direction inside the base (`(112, 504)` and `(76, 552)`
  both give 9).
- **Follow after follow**: a touch moved from a diagonal back into the
  dead zone gives mask 0 and knob = touch (no state but the base carries
  over).
- **One composition with step 10** (H-10-2): `start` with
  `pad_layout.place(window_layout.compute(390, 844).pad).stick_base_size`
  gives `radius == 58.5`, so the dead radius on the reference iPhone is
  14.625 points; if H-10-2 fails, this test follows step 10's real export.
- **Refusals**: `start` with `base_size` 0, `base_size` `"48"` and a
  missing `y`; `follow` with a string `x` and a `nil` stick: each fails
  with a message naming the argument (`expect_refused` pattern).
- **Not tested here**: which touch is the stick's and the stick zone
  check (step 12), OR with the buttons and the gamepad (steps 12, 14),
  drawing (step 16). `lua-test-modules` and `lua-names` cover loading and
  naming with no new test.
- **Timing** (plan §2, steps 10–13): only a `tests/lua/game/` file is
  added. Record `lua-test-game`'s own time before (H-10-1's commit) and
  after: median of 11 runs of the runner command itself (the
  `LUA_PATH=… luajit tests/lua/game/runner.lua …` line of the Makefile),
  not through `make`. Expected growth well under 1 ms; the full `make
  test` gate runs only if it grew by more than ~10 ms. No `make e2e`
  timing: nothing the game or the fake does per frame changes.
- **Check of the step**: the new tests pass; `make test-all` green; `make
  e2e` and `make end-state` 20/20 with every `*.sha256` and `*.end-state`
  unchanged; `make lua-names` green; design.md states the stick rules;
  the two timing numbers in the report.

## Out of Scope

- Any game change: `main.lua` does not require `love.pad_stick` (step 14
  does).
- Deciding which touch is the stick's, the stick zone hit-test, the
  "button before stick zone" rule, touches that lift (step 12).
- OR-ing the stick's mask with the buttons and the gamepad (steps 12, 14).
- Show / hide and dropping touches (step 13).
- Drawing the base and the knob (step 16), and clamping the base inside
  the pad or the window (rejected above: plan §2 puts the base where the
  touch starts).
- Analogue speed from the stick's distance: the stick only presses
  digital bits; pan speed stays `pad_nav`'s held-frames ramp.
- Any change to `pad_nav`, `pad_direction`, `input_step` or the bindings.
- Writing plan §6's step note (the coordinator does, from the report).

## Further Notes

- All expected values were checked in LuaJIT while writing this PRD:
  `(54.625 − 2^-47) − 40 == 14.625 − 2^-47` and its square is below
  `14.625² = 213.890625`; `(588 + 2^-43) − 600 == −12 + 2^-43` and
  `81 + dy²` is below 225; `t = 4·√27 = 20.784609690826528` has
  `t * t == 432 == 3 * 12 * 12`; `√(72² + 96²) = 120` and `72 · (60 /
  120) = 36`, `−96 · (60 / 120) = −48`; `117 / 8 = 58.5 · 0.25 = 14.625`.
- The Pell-type pairs `(26, 15)`, `(19, 11)` lie at 29.98° and 30.07°,
  their swaps at 60.02° and 59.93°: the closest integer points to the
  edges with offsets under 30, so a wrong constant (2 or 4 instead of 3)
  or a flipped inequality fails at least one of them.
- The dead radius on the reference iPhone is 14.625 points (6 reference
  units, a quarter of the 24-unit base radius); the base radius is 58.5
  points.
