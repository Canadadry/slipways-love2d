---
title: "One checked function list for the fake and lua-guard, and a final read of design.md's fake contract"
description: "The fake LÖVE's function list exists in three places (the fake's C installs, tools/lua_guard.lua, design.md) that nothing checks against each other, and design.md has already drifted from the fake in a few places."
status: needs-triage
---

## Problem Statement

Plan 6 (`docs/plan-6.md`) grew the fake LÖVE's contract in steps 1.1–1.5:
`love.resize`, `getDimensions`, `love.touch`, `love.mouse`, the screen,
scaled draws, and the replay fields `window`, `resize`, `touches`, `mouse`,
`screens` and `pad_sheet`. Each step wrote design.md by hand and extended
`tools/lua_guard.lua` by hand. Step 18 is the plan's last look at that
contract before the comment rework of step 19.

The maintainer has three copies of the same list and no way to know they agree:

- **The fake** decides what the game may touch. The callback and module
  names are two arrays in `function_list.c`. The functions of each module
  are whatever the module's install function (`graphics_install`,
  `touch_install`, `joystick_install`, `filesystem_install`,
  `image_install`) happens to set. The methods are whatever each metatable
  holds.
- **`tools/lua_guard.lua`** keeps its own `CALLBACKS` and `FUNCTIONS`
  tables. It refuses a `love.` name in `game/` before the fake ever runs it.
- **design.md** ("The LÖVE functions the game may use", with the
  "exactly" sentences of its paragraphs) is the contract a human reads.

Warn-review flagged the second copy at the end of phase 1 and again at the
end of phase 2: nothing checks that `lua_guard.lua` and the fake agree. A
function added to the fake and not to `lua_guard.lua` is refused by
`make test` for a game that would run. The other way round is worse: a name
added to `lua_guard.lua` and not to the fake passes `lua-guard` and fails
only at run time. Review item 10 adds that a one-time sweep fixes today's
drift but does nothing about the next one.

The drift is not only possible, it is already there. This PRD's own read of
the committed code (HEAD `d1036e5`) found:

- **D1.** design.md's "Images." paragraph says `love.graphics` "holds
  exactly" 13 names. The fake installs 14, `getDimensions` included
  (step 1.2). The function-list section has all 14, so design.md
  contradicts itself.
- **D2.** The replay rules the fake enforces and design.md does not state:
  - the document must be an object;
  - `frames` is required and must be an array of objects;
  - `input` is required and must be exactly `"pad"`;
  - a button in `pad` must be an integer from 1 to 12, given once in the
    frame. design.md only says "the replays hold button numbers 1–12".
- **D3.** design.md says "Images and Quads have no methods". The fake also
  gives Canvases an empty, guarded method table
  (`test_fake_love_unlisted` already refuses `Canvas:getWidth`). The
  general rule covers it, but the sentence reads as complete and it is not.
- **D4.** The `lua-guard` description under "Code conventions" says it
  allows "only this file's `love.` names". design.md is not what it reads:
  it reads its own copy.

Everything else matched:

- the seven callbacks and the six modules;
- the 14 graphics functions, `newImageData`, `getJoysticks`, the touch,
  mouse and filesystem functions;
- the methods `ImageData:setPixel/getPixel` and `Joystick:isGamepadDown`;
- the replay members `input`, `frames`, `initial_save`, `bindings`,
  `pad_sheet`, `attract_after`, `window` and `screens`, with `start`
  refused and other members ignored;
- the frame keys `pad`, `resize`, `touches` and `mouse`.

Step 16 (drawing the pad) and step 17 (check-e2e) come before this step and
are not done yet. Their effect on design.md and on the fake is stated below
as hypotheses.

The step must also close the plan's §4 budget line: "`make test` no slower
than before the plan", meaning the timing gate against the plan's start
commit `8c830c6`.

## Solution

**One machine-readable function list, checked against the fake in both
directions, used by `lua-guard`, and mirrored in design.md:**

- A new pure-data Lua file, `tools/love_function_list.lua`, holds the list:
  - the callbacks the game may set on `love`;
  - per module, the functions it holds;
  - per object kind (Canvas, Image, Quad, ImageData, Joystick), its
    methods.
- `tools/lua_guard.lua` drops its own `CALLBACKS` and `FUNCTIONS` and reads
  that file instead. Its behaviour, messages and output stay byte for byte
  the same.
- `test_fake_love_function_list`, an existing `make test` check, gains one
  test. It boots the real host on a tiny fixture game, collects what the
  fake actually installs, and compares it with the data file in both
  directions:
  - the callbacks and module names come from `function_list.c`'s arrays;
  - the functions are each module table's raw keys;
  - the methods are each kind's metatable `__index`.

  It reports every name missing on either side. Adding a function to the
  fake, or to the list, without the other turns `make test` red. No new
  process: the check runs inside a binary that `make test` already runs.
- design.md's function-list section says that this file is its
  machine-readable copy, which side checks it, and what an extension must
  touch.

**A by-hand final read of design.md's fake contract**, made by the
implementer on the code as it stands after steps 16 and 17. It fixes D1–D4
and any other drift that read finds. The design.md ↔ code comparison
stays a by-hand read, not a check. The reasons are under Implementation
Decisions.

**The §4 timing gate** runs against `8c830c6`, beside design.md's two
baselines (this step's "before" commit and `8dff812`).

## User Stories

1. As a maintainer, I want one machine-readable list of the LÖVE names the game may use, so that the fake, `lua-guard` and design.md cannot each hold their own version.
2. As a maintainer, I want `make test` to fail when the fake installs a function the list lacks, so that a function added to the fake without its entry is caught at once.
3. As a maintainer, I want `make test` to fail when the list names a function the fake does not install, so that `lua-guard` never lets through a call the fake would refuse at run time.
4. As a maintainer, I want the same two-way check for the callbacks the game may set on `love`, so that a new callback in `function_list.c` cannot be forgotten in `lua-guard`.
5. As a maintainer, I want the same two-way check for the module names, so that a seventh module cannot be added on one side only.
6. As a maintainer, I want the same two-way check for each object kind's methods, so that `ImageData`, `Joystick`, `Canvas`, `Image` and `Quad` cannot gain or lose a method silently.
7. As a maintainer, I want the failure to name every missing name on each side, so that I fix the drift in one pass instead of one name per run.
8. As a maintainer, I want the check to read what the real host installs, through `host_open`, so that a module installed but never registered, or registered under the wrong table, is caught too.
9. As a maintainer, I want the check inside an existing `make test` binary, so that `make test` gains no process and stays as fast as the plan requires.
10. As a maintainer, I want `lua-guard` to read its list from the same file, so that the guard and the fake are checked against one source rather than three.
11. As a maintainer, I want `lua-guard`'s output and messages unchanged, so that `test_lua_guard` and every skill that reads its output keep working.
12. As a maintainer, I want `lua-guard` to fail clearly when the list file is missing or malformed, so that a broken list is never read as an empty one that refuses every `love.` name.
13. As a maintainer, I want the list file to be plain data with no logic, so that reading it is the same in `lua-guard` and in the C test.
14. As a maintainer, I want design.md's function-list section to name the list file and its check, so that the next person extending the fake knows the three places to touch.
15. As a maintainer, I want design.md's "Images." paragraph to stop enumerating graphics functions that the function list already holds, so that this sentence cannot drift again (D1).
16. As a maintainer, I want design.md to state that the replay document is an object, `frames` a required array of objects, and `input` a required `"pad"`, so that a replay author learns the rules from the contract and not from an error (D2).
17. As a maintainer, I want design.md to state that a frame's `pad` buttons are integers from 1 to 12, each given once, so that the refusal of a repeated button is in the contract (D2).
18. As a maintainer, I want design.md to say Canvases have no methods, like Images and Quads, so that the sentence is complete (D3).
19. As a maintainer, I want the `lua-guard` description in design.md's code conventions to say where its list comes from, so that the description matches the tool (D4).
20. As a maintainer, I want every function, method, callback, module, replay member and frame key the fake accepts to be named in design.md, so that design.md is the whole contract.
21. As a maintainer, I want everything design.md names as accepted to be accepted by the fake, so that the contract promises nothing the fake refuses.
22. As a maintainer, I want the by-hand read done on the code as steps 16 and 17 left it, so that whatever they changed is read too.
23. As a maintainer, I want design.md's description of `test_game_screens` to match what step 16 made of it, so that the screen section's list of checks is true.
24. As a maintainer, I want the plan's §4 line about iOS gone from design.md, so that the plan's "done when" holds at its last design.md step.
25. As a maintainer, I want the `make test` timing gate against `8c830c6` run and recorded, so that the plan closes with proof that `make test` is no slower than at its start.
26. As a maintainer, I want the gate also run against this step's "before" commit and `8dff812`, so that design.md's two-baseline protocol holds for this step's own change.
27. As a maintainer, I want `make e2e`'s wall time recorded once more, so that the plan's "`make e2e` under 9 s" line has a final number.
28. As a reviewer, I want mutation proof that the new check catches each kind of drift, so that the check is not a test that always passes.
29. As a reviewer, I want the 28 replays' hashes and end states unchanged, so that this clean-up provably changes no game behaviour.
30. As a future plan author, I want the list file to be where a new LÖVE function is first written, so that extending the fake starts from the checked copy.
31. As an agent running `rework-comments` next (step 19), I want the function-list code and design.md settled first, so that the comment rework does not churn on a list about to move.

## Implementation Decisions

**Hypotheses on steps 16 and 17 (not yet done; step 16's PRD is
`docs/prd/triage/phase3-step16-draw-virtual-pad.md`, step 17 is the
`check-e2e` skill).** Before anything else, the implementer checks each one
against the committed code and the plan's §6. If one fails, the
implementer reports it and revisits the decision that rests on it. The real
code of steps 16 and 17 wins over this PRD's wording, and that includes the
drift list D1–D4, which is re-read and not trusted.

- **H-16-1**: steps 16 and 17 are committed, and `make test-all`, `make e2e`
  and `make end-state` are green on the last of their commits. That commit
  is this step's "before" commit for every timing.
- **H-16-2**: step 16 changed neither the fake (`tools/fake_love/`), nor its
  replay format, nor `tools/lua_guard.lua`. Its PRD puts all three out of
  scope: every call the pad makes was already listed. If it changed any of
  them, the by-hand read covers the change, and the new data file and its
  check list the post-step-16 contract.
- **H-16-3**: step 16 updated design.md: a "Pad drawing" paragraph, the
  present's rule reworded from "last in `love.draw`" to "after every draw
  onto canvas 0, only the pad's sprites after it", in "The present" and in
  the function-list section, `virtual_mask` among the virtual pad's
  readable fields, and the intro sentence that says nothing in these plans
  targets iOS removed or rewritten. If the iOS sentence is still there,
  this step removes it (plan §4), and if the present's rule is still
  stated two different ways, this step makes them one.
- **H-16-4**: step 16 extended `test_game_screens` to one boot of
  `tests/fixtures/game_screens.json` with eight frames and six `screens`
  and `pad_sheet: pad_flat.png`, under 100 ms. It passed the full timing
  gate and recorded its headroom against `8c830c6` in §6. The sentence in
  design.md's "The screen" naming `test_game_screens` "(390×844, then a
  `resize` to 640×480)" may be stale: if it is, this step corrects it.
- **H-16-5**: step 17 (check-e2e) changed nothing in the fake, its replay
  format or `lua_guard.lua`. Any fix-bug commits it made touched `game/` or
  tests only. `tests/e2e/` holds 28 replays (the 20 originals, four
  `touch_*`, four `pad_twin_*`), all matching.
- **H-16-6**: the game still calls only listed names (`lua-guard` is green),
  and no listed function became unused in a way the plan asks to prune.
  This step prunes nothing (Out of Scope).

**Decision: a permanent check for code against code, a by-hand read for
prose against code.** The two comparisons differ in kind:

- **Fake ↔ `lua_guard.lua`** is two pieces of code that must hold the same
  set. A mechanical comparison is exact and cheap, and it is the drift
  warn-review flagged twice. It becomes a permanent check.
- **design.md ↔ fake** is prose against code. Names appear in sentences,
  partly qualified (`setCanvas` after `love.graphics.newCanvas`), with
  forms and arguments, and with refused names as examples
  (`love.graphics.getWidth`, `ImageData:mapPixel`). A parser of that prose
  would be fragile, and a check that breaks when a sentence is reworded is
  worse than none. It would also miss D1, which sits in an "exactly"
  sentence outside the list. The plan freezes the fake's format after
  phase 1, and its rule "design.md follows each step" already makes every
  contract change touch design.md. So design.md is read by hand now. The
  new section text tells the next extender the three places to touch, and
  two of the three are now checked against each other.
- **Replay fields** stay out of the data file and out of `lua-guard`: the
  guard reads game code and never sees a replay. The replay members and
  frame keys are compared by hand with design.md (the member table and
  frame key list in `replay.c`, plus `input`, `frames` and `start`).
- The plan's wording "named in design.md and in `tools/lua_guard.lua`"
  therefore covers, for `lua_guard.lua`, the names it can see: callbacks,
  modules and module functions. Methods sit in the data file, which
  `lua_guard.lua` reads, and the C check covers them. The guard does not
  use them, because it cannot know an object's type statically.

**The data file `tools/love_function_list.lua`.** Plain Lua data, no logic,
no `require`. It returns one table:

```lua
-- shape decided here; names as of HEAD d1036e5
return {
    callbacks = { "conf", "load", "update", "draw", "gamepadpressed", "gamepadreleased", "resize" },
    modules = {
        graphics = { "clear", "draw", "getDimensions", "newCanvas", "newImage", "newQuad", "points",
                     "pop", "push", "rectangle", "setCanvas", "setColor", "setDefaultFilter", "translate" },
        image = { "newImageData" },
        joystick = { "getJoysticks" },
        touch = { "getPosition", "getTouches" },
        mouse = { "getPosition", "isDown" },
        filesystem = { "getInfo", "read", "write" },
    },
    methods = {
        Canvas = {}, Image = {}, Quad = {},
        ImageData = { "getPixel", "setPixel" },
        Joystick = { "isGamepadDown" },
    },
}
```

Sequences, not sets, so that both readers can sort and print them, with
each name written once per sequence. Method kinds are the metatable names
the fake registers.

**`tools/lua_guard.lua`** loads the file from its own directory (the
directory of `arg[0]`, so the `lua-guard` target and `test_lua_guard`, both
run from the repository root, resolve it alike). It builds its `CALLBACKS`
and `FUNCTIONS` sets from `callbacks` and `modules`, and ignores `methods`.
`GLUE_ONLY`, every message and the output are unchanged. A missing or
non-table file fails with exit 2 and a `lua-guard: ` message naming the
file, like its usage failures. The `lua-guard` target and the
`test_lua_guard` binary gain the file as a prerequisite.

**The fake ↔ list check, in `test_fake_love_function_list`.**

- `function_list.h` exports the callback and module name arrays and their
  counts. The C name follows the header's existing `function_list_` prefix;
  the implementer picks the exact form. Today they are `static` in
  `function_list.c`, and the guard keeps using them unchanged.
- A new fixture game (`tests/fixtures/fake_love/function_list/main.lua`),
  at its top level and before it sets any callback, walks `pairs(love)`
  (raw: the guards are metatables, so `pairs` and `next` see the installed
  tables) and each module's `pairs`. It creates one object of each kind (a
  160×120 canvas, a 1×1 ImageData, its Image, a 1×1 Quad, the joystick of
  `getJoysticks()`) and walks `pairs(getmetatable(obj).__index)`. Its
  `debug_state()` returns the sorted lines `module <name>`,
  `function love.<module>.<name>` and `method <Kind>:<name>`. Only
  functions on the list are used to do this.
- The test links the host sources (the fake minus its `main`) besides
  `function_list.c`. It calls `host_new`, `host_open` on the fixture and
  `host_debug_state`, with no frame and no `love.load` if `host_debug_state`
  allows it, otherwise `host_load` with no argument. It loads the data file
  with `luaL_dofile` into a plain state and renders the same lines from it,
  sorted. It renders the exported C arrays as `callback <name>` and
  `module <name>` and compares them with the file's `callbacks` and
  `modules` keys.
- On a mismatch it prints, for each side, every line present only there
  (`in the fake, not in tools/love_function_list.lua: …` and the reverse),
  then fails its `assert`.
- The test does not compare with design.md (decision above).
- The binary's Makefile rule gains the host sources, the data file and the
  new fixture as prerequisites. The fixture also matches the existing
  `FAKE_LOVE_FIXTURES` wildcard. It stays in `FAKE_LOVE_CHECKS`, in the
  same place.

**design.md, the by-hand read.** The implementer lists, from the code after
steps 16–17:

- every name the five install functions set;
- the arrays of `function_list.c`;
- every metatable's methods;
- the replay members of `replay.c`'s start-state table, plus `input`,
  `frames` and `start`;
- the frame keys and every refusal rule of a member or frame key.

Each item is checked against design.md, and design.md's fake sections are
checked back against the list. The implementer may script this in the
scratchpad (not committed). The fixes:

- **D1**: the "Images." paragraph no longer enumerates `love.graphics`.
  It says `love.graphics` holds exactly the drawing functions of "The LÖVE
  functions the game may use", from `main.lua` on. The function-list
  section is the one enumeration.
- **D2**: the "Fake disk and arguments" paragraph states that the document
  is an object, `frames` a required array of objects, `input` a required
  `"pad"`, and `pad` integers 1–12 each at most once per frame, with the
  fake's refusal messages quoted as the paragraph does for the other
  members.
- **D3**: "Canvases, Images and Quads have no methods".
- **D4**: the `lua-guard` sentence of "Code conventions" says it allows the
  `love.` names of `tools/love_function_list.lua`.
- **The function-list section** gains one short paragraph:
  - `tools/love_function_list.lua` is this list in machine-readable form
    (callbacks, module functions, methods);
  - `lua-guard` reads it;
  - `test_fake_love_function_list` checks it, both ways, against what the
    fake installs;
  - extending the fake means its install function, that file and this
    section.
- The "Layout of the fork" bullet for `tools/` names the new file.
- Anything else the read finds: H-16-3's present rule and iOS line,
  H-16-4's `test_game_screens` sentence, and any drift step 16 or 17
  introduced.

No behaviour of the fake changes, and no message changes. A drift resolved
by changing the fake instead of design.md is out of scope; the implementer
reports it.

**Timing.** The step adds work to an existing `make test` check (one
in-process boot of a tiny fixture, about 5 ms measured for a whole fake
process on such a fixture in this session, and one `dofile`) and changes
`lua-guard` by one `dofile`. design.md's full gate (rule 4) applies:

- 11 rounds against the "before" commit (H-16-1), `8dff812` and `8c830c6`;
- each paired median must be ≤ 3 %, with 11 more rounds pooled if one is
  over;
- the step note records the plain and paired medians, the spreads, the
  rounds, the core count, and `test_fake_love_function_list`'s own time
  before and after (median of 11 runs of the binary).

The `8c830c6` result is the plan's §4 closing number. If it fails, the
first lever is this step's own addition. The next is the plan's listed
reserves (step 2.6's budget plan); splitting a check is the last resort.

`make e2e` is not in this step's per-step list (no per-frame change). For
§4, the step records one median of 5 runs and checks it is under 9 s.

## Implementation Chunks

1. **The data file and `lua-guard` on it.** Write
   `tools/love_function_list.lua` from today's `lua_guard.lua` tables, add
   `methods`, switch `lua_guard.lua` to it, and add the missing-file
   failure with its `test_lua_guard` case (red first). `make lua-guard` and
   `test_lua_guard` are byte-identical otherwise.
2. **The fake ↔ list check.** Export the C arrays, write the fixture and the
   new test in `test_fake_love_function_list`, and run the mutations (see
   Testing Decisions). Measure the binary's own time.
3. **The by-hand read and design.md.** Fix D1–D4 and whatever the read
   finds, write the new function-list paragraph, then run the full timing
   gate and `make e2e`.

## Testing Decisions

- **A good test here checks a contract through its public surface.** The
  new test reads what the real host installs (`host_open`, the game's own
  view through `pairs`), not the C install functions' source. It compares
  that with what the guard reads (the data file), never with a third
  literal copy in the test.
- **`test_fake_love_function_list`** gets the new test. Prior art:
  - its own tests (guarded `love`, `pairs` over a guarded module);
  - `tests/fixtures/fake_love/graphics/main.lua` and
    `test_fake_love_graphics`, which already list `pairs(love.graphics)`
    in a debug state;
  - `test_fake_love_image`, which links fake modules into a unit test.

  **Mutations** the implementer runs and reverts, each turning the test
  red with the right name in the message:
  1. a function added to `graphics_install` only;
  2. a name added to the data file's `touch` only;
  3. `"keypressed"` added to the C callbacks only;
  4. a callback removed from the data file only;
  5. `getPixel` removed from the data file's `ImageData`;
  6. a method added to the `Joystick` metatable only;
  7. a seventh module name added to the C modules only.
- **`test_lua_guard`**: unchanged, and it still passes, which proves the
  guard reads the same names. One new case: the guard pointed at a missing
  list file fails with exit 2 and its message. A temporary copy of the
  script in the test's work directory, without the list file beside it, is
  one way. The implementer picks the cheapest form that keeps one luajit
  run.
- **The existing literal lists stay**: the graphics fixture's
  `main graphics …` line, `disk_echo`, `image_files_log`, and the names in
  `test_fake_love_function_list`'s guard tests. They test behaviour. Making
  them read the data file is not needed.
- **design.md**: no automated test (decision above). The step's check
  includes the implementer's by-hand comparison, listed in the report: each
  name and each replay field on both sides, and each drift fixed.
- **Replays**: `make e2e` and `make end-state` all matching (28 of 28 if
  H-16-5 holds), with no file of `tests/e2e/` changed.
- **Check of the step**:
  - `make test-all` green;
  - the mutations red;
  - `git diff --stat tests/e2e game` empty;
  - design.md states D1–D4's fixes and the list file;
  - the three-baseline gate passed and recorded;
  - `make e2e` under 9 s.

## Out of Scope

- Any change to the fake's behaviour, messages or replay format, and any
  new LÖVE function. A drift that would need one is reported, not fixed.
- Pruning listed functions the game no longer calls.
- A mechanical check of design.md's prose against the code.
- Generating the fake's C installs or `function_list.c`'s arrays from the
  data file.
- The other phase-1 and phase-2 warn-review items: duplicated limits and
  constants, `replay.c`'s size, the twin touch types, hand-listed Makefile
  checks, `test_fake_love_screen` against `_screens`, the `love-file`
  hash comparison, `check.run`'s `arg[0]`.
- `game/`, `tests/e2e/`, the plan's §6 step note (the coordinator's),
  `CLAUDE.md`, and the comment rework (step 19).

## Further Notes

- D2's rules date from before plan 6 (the replay parser of plan 2). They
  are fixed here because this step is the contract's final read, not
  because plan 6 introduced them.
- The data file adds no copy: it replaces `lua_guard.lua`'s copy, so there
  are still two machine copies (the fake's installs and the list) and one
  prose copy, and the two machine copies are now checked against each
  other.
- If H-16-2 fails, because step 16 added a function to the fake (for
  instance `dpiscale` for the `highdpi` risk noted at step 2.7), the data
  file and design.md take it like any other name. The new check then
  proves `lua-guard` has it.
