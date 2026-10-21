---
title: "The port checklist script"
description: "Nothing lists which C function, data table and file of the game each Lua port step must carry, so no step can prove it is complete."
status: needs-triage
---

## Problem Statement

Plan 2 ports the whole C game to Lua as a faithful copy: same files, same functions, same names, same data tables. Every later step of the plan (1.2 fixed point, 2.1 runtime, 3.1 to 3.15 engine and game) is verified by "the checklist": every C file assigned to the step has its Lua file, and every C function its Lua function. Nothing in the repository does this today. There is no list of the roughly 1 560 C functions (about 1 525 in `.c` files, plus the `static inline` functions of the headers) and a few dozen file-level data tables. Nothing says which Lua file and name each one maps to, or which ones are deliberately not ported and why. There is also no assignment of the C files to the Phase 3 steps, even though the plan makes that assignment authoritative. Without it, an agent working on step 3.6 cannot tell which files are its own, whether it missed a function, or whether the modules it writes can `require` modules that already exist.

## Solution

Add a checklist script under `tools/` that reads the C sources and one small committed data file, and reports:

- every C function of `src/p8/`, `src/engine/`, `src/game/` and `src/devtools/state_format.c`, header `static inline` functions and the two generated units (default bindings, demo) included, with its Lua file and Lua name, marked present or MISSING, or marked not ported with the reason;
- every file-level data table of the same files, with its Lua table, present or MISSING;
- every C file of `src/engine/` and `src/game/` (headers and generated units included) with its Phase 3 step, plus the `src/p8/` files and `state_format.c` with the step that ports them.

`make checklist` runs it. Today it reports every function and table as MISSING, every file as having a step, and exits 0. With `STEP=<id>` it fails while anything assigned to that step or an earlier one is missing. Later steps use this as their completeness gate.

## User Stories

1. As a plan-pilot agent closing step 3.6, I want `make checklist STEP=3.6` to fail while any function of a file assigned to 3.1–3.6 has no Lua function, so that I cannot close the step incomplete.
2. As an agent starting a Phase 3 step, I want the listing to show exactly which C files belong to my step, so that I know my scope without re-deriving the include graph.
3. As an agent writing a Lua module, I want the listing to give the Lua file and Lua name of each C function, so that I name it as the faithful-copy rule requires.
4. As an agent, I want static C functions listed too, so that private helpers are not forgotten in the copy.
5. As an agent, I want header `static inline` functions listed under the header, mapped to the module's Lua file, so that `vec.h`, `fixed.h` and `entity_geom.h` inlines are ported once, in the right file.
6. As an agent, I want the file-level data tables listed with their Lua table, so that the data-table checks of Phase 3 know what to compare.
7. As an agent, I want the generated default bindings unit (`bindings_default`) and the generated demo unit (`demo_data_*`) covered, so that the Lua game has the same tables even though their C is produced at build time.
8. As an agent, I want the items that plan 0 left dead (`bindings_compiled_personal`, `src/p8/format.c`, `GPU_FRAME_BUFFER_BYTES` and the GPU budgets of `gpu_sizes.h`, the width-multiple-of-16 rule of `image.h`) listed as not ported with their reason, so that nobody ports dead code and nobody wonders why it is absent.
9. As an agent deciding in a later step that a C function is not ported, I want to record that with a reason in one data file, so that the checklist stays the single place for such decisions.
10. As an agent, I want a new C function, added by a bug fix while the C game is still the reference, to show up as MISSING automatically, so that the checklist cannot fall behind the C sources.
11. As an agent, I want a not-ported entry or a name exception that refers to a C function, table, macro or file that no longer exists to fail the run, so that stale entries do not pile up.
12. As an agent, I want a C file of `src/engine/` or `src/game/` without a step to fail the run, naming the file, so that a new C file is assigned when it appears.
13. As an agent, I want the run to fail when a file assigned to a step includes a file assigned to a later step, unless both are in the same include cycle, so that every step's Lua modules only `require` modules already written.
14. As an agent working on the 18-file include cycle (steps 3.6 to 3.9), I want includes inside the cycle to be allowed across those steps, so that the cycle can be cut into consecutive steps as the plan says.
15. As an agent, I want the full listing written to one text file with one entry per line, so that I can `grep` it for a file, a step or `MISSING`.
16. As an agent, I want a short summary on the terminal (per-step counts and one totals line), so that the run's result fits in a tool output.
17. As an agent, I want the totals line to give ported, missing and not-ported counts for functions and tables, so that progress across Phase 3 can be read at a glance.
18. As a developer, I want `make checklist` to stay outside `make test`, `make e2e` and `make test-all`, so that the fast loop is not slowed.
19. As a developer, I want the script to parse the C files directly, without a compiler, ctags or Python, so that it runs wherever LuaJIT runs (installed by `tools/love_install.sh`) and finishes in well under a second.
20. As a developer, I want a parse the script does not understand to stop the run with the file and line, so that a silent miss of a function is impossible.
21. As a developer, I want the step of each file kept in a committed data file, so that the assignment is reviewed in diffs and is authoritative, as the plan requires.
22. As a plan-pilot agent at step 1.2, I want `make checklist STEP=1.2` to cover `fixed.h`, `fixed.c` and `quarter_wave.h`, so that step's "the checklist covers fixed.h" is a single command.
23. As a plan-pilot agent at step 2.1, I want `make checklist STEP=2.1` to cover `src/p8/`, so that step's "the checklist covering src/p8/" is a single command.
24. As an agent at step 3.15, I want `state_format.c` assigned to 3.15, so that `debug_state()`'s copy is checked by the same gate.
25. As an agent, I want a Lua function to count as present whatever the module convention step 2.1 settles (`local function f`, `function M.f`, `M.f = function`), so that the checklist does not have to change when the convention is fixed.
26. As a reviewer, I want the script's behaviour covered by tests on small fixture trees, so that a change to the parser cannot silently drop functions.

## Implementation Decisions

**Modules.**

- A checklist script in Lua, run by LuaJIT (`luajit`, already required by the project and installed by `tools/love_install.sh`): `tools/checklist.lua`. It is a check script, so it lives in `tools/` as CLAUDE.md requires. Lua and not sh/awk because it tokenizes C (multi-line signatures such as `allocator_grow` defeat a line regex), computes strongly connected components and checks Lua files. LuaJIT and not Python, clang or ctags: ctags is not installed, clang's AST dump took about 15 s over the tree, and LuaJIT is already a project dependency.
- A committed data file, `tools/checklist.txt`. It holds only what cannot be derived from the sources: file-to-step assignments, generated units, not-ported entries and Lua-name exceptions.
- A public make target `checklist` (help line ≤ 20 words). It depends on the two generated units (`tmp/bindings/core.c`, `tmp/demo/demo.c`) and on the existing LuaJIT version check, runs the test binary, then the script with the listing at `tmp/checklist.txt`. It passes `--step $(STEP)` when `STEP` is set. It does not take `tmp/make.lock`: it builds nothing that `test` builds concurrently except the generated units, which make already serializes within one invocation. It is not a prerequisite of `test`, `e2e`, `test-all` or CI.

**Interface of the script.** `luajit tools/checklist.lua [--root <dir>] [--listing <file>] [--step <id>]`.

- `--root` (default `.`) is the tree read: `<root>/src/...`, `<root>/tmp/...` for the generated units, `<root>/game/...` for the Lua files, `<root>/tools/checklist.txt`. The tests point it at fixture trees.
- `--listing` writes the full listing there (default: no listing file).
- `--step <id>` turns MISSING into a failure for every file assigned to `<id>` or an earlier step.
- Exit status: 0 when consistent (and, with `--step`, nothing missing up to that step); 1 on a consistency error or, with `--step`, a missing item; 2 on usage errors and C it cannot parse. Every error is one line on stderr starting `checklist: ` and naming the file (and line for parse errors). All errors are reported before exiting, not just the first.

**What is scanned.** Every `.c` and `.h` of `src/p8/`, `src/engine/`, `src/game/`, plus `src/devtools/state_format.c`, plus the generated units named by `unit` lines of the data file. Each file is parsed raw, on its own, without preprocessing. Comments, string and character literals are skipped. Preprocessor lines, with their `\` continuations, are skipped. Brace depth is tracked. At depth 0, tokens accumulate into a statement that ends at `;` or at a `{`:

- A `{` after `struct`, `union` or `enum` (with an optional tag) is a type body: it is skipped and the statement continues.
- A `{` after `=` is an initializer: the statement is a data definition, and it continues to `;`.
- Any other `{` ends a function definition. The parameter list is the last balanced `( … )` before the `{`, and the function's name is the identifier just before it. This survives leading attributes such as `__attribute__((…))` and `P8_NORETURN`.
- A `{` at depth 0 that fits none of these is a parse error (exit 2).
- `extern` statements and declarations without a body are ignored.

**Data table.** A file-scope object definition (not `extern`, not inside a function) whose initializer is a brace list or a string literal, const or not. Examples: `p8_quarter_wave`, `bindings_default`, `demo_data_captions`, `main_camera_placeholder_class_def`. Scalar initializers (`static int g_entity_up = 0;`) and uninitialized state are not tables. Function-local `static const` tables (`names` in `entity.c`) are not listed; they belong to their function's entry.

**Default Lua mapping (derived, not stored).**

- `src/<dir>/<base>.c` and `src/<dir>/<base>.h` both map to `game/<dir>/<base>.lua`.
- The C name is kept for the Lua function or table.
- `state_format.c` follows the same rule (`game/devtools/state_format.lua`); `debug_state()` in `main.lua` will call it.
- A generated unit maps to the Lua file its `unit` line names. Seeded: `tmp/bindings/core.c` maps to `game/game/bindings_default.lua` at step 3.4, and `tmp/demo/demo.c` maps to `game/game/demo_data.lua` at step 3.13.
- The "default bindings" unit is `tmp/bindings/core.c`: it defines `bindings_default`, which `input_step.c` reads and which is in the game's `ENGINE_SRCS`. `tmp/bindings/default.c` (`bindings_compiled`) is the replay tool's unit, read only by `src/cmd/replay.c`, and is not scanned.
- A `lua` exception line overrides file and name for one C function or table. None is seeded.

**Presence in Lua.** The Lua file must exist. Lines starting with `--` (after spaces) are ignored.

- A function is present when a line matches, with optional leading spaces: `function <name>(`, `function <id>.<name>(`, `function <id>:<name>(`, `local function <name>(`, or `[local ]<name> = function` / `<id>.<name> = function`. Spaces before `(` are allowed.
- A table is present when a line matches `[local ]<name> =` or `<id>.<name> =`.

This keeps the checklist independent of the module convention that step 2.1 settles.

**Data file format.** One entry per line, fields separated by spaces, a reason after ` -- `. No comment lines. Blank lines are allowed.

- `step <id> <C path>`: assigns one C file.
- `unit <id> <generated path> <Lua path>`: one generated unit, scanned and assigned.
- `not-ported function|table <C path> <name> -- <why>`
- `not-ported file <C path> -- <why>`: every function and table of the file, which then needs no `step` line.
- `not-ported declaration|macro <C path> <name> -- <why>`: a name that is neither a function nor a table. It is checked only to still appear in the file as an identifier.
- `not-ported rule <C path> <label> -- <why>`: a rule with no symbol. It is checked only for the file existing.
- `lua <C path> <C name> <Lua path> <Lua name>`

Valid step ids are `1.2`, `2.1` and `3.1` to `3.15`, ordered in that sequence.

**Consistency errors (exit 1, with or without `--step`).**

- A scanned C file of `src/engine/` or `src/game/` with no `step` line and not wholly not ported.
- The same for `src/p8/` and `state_format.c`: the plan requires only engine and game, but `--step 1.2` and `--step 2.1` need the p8 files too.
- A `step` line for a file that does not exist, or a file with two `step` lines.
- An unknown step id.
- A not-ported or `lua` entry naming a file, function, table, macro or declaration that no longer exists.
- A not-ported entry for something that is also given a `lua` exception.
- An include order violation: file A includes `"<dir>/<base>.h"` of a scanned, assigned file B whose step comes after A's, and A and B are not in the same strongly connected component of the include graph. The graph is built from the `#include "…"` lines of the scanned files, between scanned files only. The component exemption is what lets the 18-file cycle span 3.6 to 3.9, and lets `zone`/`zone_sizes` and `entity_geom`/`entity_registry` sit together.

MISSING items are never a consistency error. They fail only under `--step`.

**Listing format** (`--listing`). One line per entry, sorted by step, then C path, then source line:

- `<step> function|table <status> <C path>:<line> <C name> -> <Lua path> <Lua name>[ -- <why>]`, with status one of `ported`, `MISSING`, `not-ported`.
- `<step> file <present|MISSING> <C path> -> <Lua path>` for each assigned file. A header with no function and no table needs no Lua file and is shown as `no-lua`.
- Not-ported declarations, macros and rules also get one line each.
- Files wholly not ported show `-` as step.

**Terminal summary.** One line per step: `<id>: <f> files, <p>/<n> functions, <tp>/<tn> tables`. Then the totals line: `checklist: <p> ported, <m> missing, <x> not ported, of <n> functions; <tp> ported, <tm> missing, <tx> not ported, of <tn> tables; <files> files, each with a step`. Then any errors.

**Seeded not-ported entries** (from the plan's step notes, each with the note's reason: dead since plan 0, only deleted binaries used it):

- `bindings_compiled_personal` (declaration, `src/game/bindings.h`).
- `src/p8/format.c` (file).
- `src/p8/format.h` (file).
- `GPU_FRAME_BUFFER_BYTES`, `GPU_SHIPPED_FRAME_BUFFER_BYTES`, `GPU_BUILD_BUFFER_BYTES`, `GPU_CLUT_SLOTS`, `GPU_PATTERN_SLOTS` (macros, `src/p8/gpu_sizes.h`). The reason also says "still measured by draw_meter.c and state_format.c".
- `width-multiple-of-16` (rule, `src/p8/image.h`).

Nothing else is seeded as not ported. Every other function and table starts with its default Lua mapping and is reported MISSING. A later step that decides not to port something records it here, with why.

**Seeded file assignment.** Rule: a file goes to the step the plan's step list names for it. When the include order forbids that (the plan says each step's modules only require modules already written, and the checklist's assignment is authoritative), it moves to the nearest step that satisfies the order. A file the plan does not name goes to the step of its nearest relative. Each `.h` shares its `.c`'s step. This draft passes the order check (checked by a prototype over the current include graph):

| step | modules (`.c` and `.h` together) |
|---|---|
| 1.2 | p8 `fixed`, `quarter_wave` |
| 2.1 | every other `src/p8/` file except `format` |
| 3.1 | `zone`, `zone_sizes`, `allocator`, `game_zone`, `frame_zone`, `value_list`, `pointer_list`, `slot_tally`, `slot_group`, `vec`, `geom`, `collect`, `dispatch_context` |
| 3.2 | `entity_id_type`, `entity_class`, `entity_id`, `entity_id_list`, `entity_list`, `exit_queue` |
| 3.3 | `entity_registry`, `entity_geom`, `entity`, and game `map_tables`, `game_data` (moved from 3.4: `entity_registry.c` includes `game/game_data.h`) |
| 3.4 | `gen`, `bindings` + generated `tmp/bindings/core.c`, `mouse_button`, `mouse`, `image_slot`, `button_image`, `text_image`, `targeting`, `grab_arena`, `visual_random` |
| 3.5 | `hud`, `layout`, `tech_gate`, `research`, `data_init`; `row_image`, `draw_functions` (moved from 3.4: they include `hud`/`layout`); `economy_formulas`, `main_economy` (moved from 3.7: `tech_gate` needs them); `trade` (moved from 3.10: `research` includes it) |
| 3.6 | cycle: `galaxy`, `planet`, `planet_family`, `planet_trade`, `planet_display`; `ui_zone` (moved from 3.9: `planet_display` includes it) |
| 3.7 | cycle: `economy`, `economy_score`, `economy_tiers` |
| 3.8 | cycle: `slipway`, `probe`, `wormhole`; `particle_zone` (moved from 3.9: `slipway` includes it) |
| 3.9 | cycle: `ui`, `ui_button`, `message_box`, `particle`, `background`, `scroll`, `research_display`; engine `bootstrap` (moved from 3.3: it includes `game/mouse.h`, `game/scroll.h`, `game/ui.h`) |
| 3.10 | `planet_family_trade`, `slipway_display`, `wormhole_display`, `structure_icon`, `score_panel`, `score_panel_display`, `help` |
| 3.11 | `pad_direction`, `pad_cycle`, `pad_panel`, `pad_target`, `pad_back`, `pad_build`, `pad_jump`, `pad_select`, `pad_slipway`, `cheat` |
| 3.12 | `save_storage`, `save_state`, `save_load` |
| 3.13 | `menu`, `attract`, `demo_data` + generated `tmp/demo/demo.c`, `demo_overlay`, `loading_screen`; `pad_nav` (moved from 3.11: it includes `game/menu.h`) |
| 3.14 | `cart`, `input_step` |
| 3.15 | `src/devtools/state_format.c` |

If the script's order check disagrees with this table on the real tree, the order check wins. The implementer moves the file by the same rule and lists the move in the commit message. The moves above go in the step note that plan-pilot records for this step, so that the Phase 3 steps read their real scope from the checklist and not from their titles.

## Implementation Chunks

1. **Scanner and listing.** The script with `--root` and `--listing`: the tokenizer, function and table detection, the default Lua mapping, presence detection in Lua files, the listing and the totals line. No data file yet: every file is shown with step `-`. Tests on fixture trees. Parse errors exit 2.
2. **Data file and consistency.** `tools/checklist.txt` parsing (`step`, `unit`, `not-ported …`, `lua`), the step column, per-step summary, every consistency error except include order. The seeded `tools/checklist.txt`: the full assignment table above and the step-note entries. The `make checklist` target. Running it on the real tree reports every function and table MISSING and every file with a step.
3. **Include order and the step gate.** The include graph, its strongly connected components, the order-violation error, and `--step` (`STEP=` in make). The real tree passes the order check with the seeded assignment, or the assignment is corrected by the rule above.

## Testing Decisions

- A good test drives the script as a black box: it writes a small fixture tree (a few C files, optionally Lua files and a data file) under `tmp/`, runs the script, and asserts on the exit status, the stderr error lines and lines of the listing. No test reaches into the script's internals.
- One C test, `tests/test_checklist.c`, is modelled on `tests/test_button_guard.c` (fixture tree under `tmp/test_checklist_work`, `system()` with output and status captured). It is built like the other guard tests but is **not** in `UNIT_RUNS` and not a prerequisite of `test`: `make checklist` runs it before the real run. CLAUDE.md forbids slowing `make test`, and every new process in `make test` would need the design's timing protocol.
- Behaviours to cover:
  - a multi-line signature;
  - a static function;
  - a `static inline` header function mapped to the module's Lua file;
  - leading attributes;
  - comments and strings holding braces or fake signatures;
  - preprocessor lines with continuations;
  - `typedef struct {…}` and a struct variable with and without initializer;
  - brace and string tables versus scalar and uninitialized state;
  - `extern` and prototypes ignored;
  - an unparseable top-level `{` (exit 2, file and line);
  - each Lua presence form and a commented-out definition;
  - a missing Lua file;
  - a generated unit;
  - each consistency error;
  - an include order violation and the same edge inside a cycle accepted;
  - `--step` failing on a missing item at an earlier step and passing when only later steps miss items;
  - the totals line.
- The real-tree run (`make checklist`) is the step's acceptance: exit 0, every function and table MISSING except the not-ported entries, every engine and game file (headers and generated units included) with a step, the order check green.

## Out of Scope

- Writing any Lua game code: steps 1.2 to 3.15.
- Putting the checklist in `make test`, `make test-all` or CI. A later step may add it under the design's timing protocol.
- Function-like macros (`P8_FIXED_LIT`, the `POISON` macros of `zone.c`): not functions, not listed. Function-local static tables: covered by their function's entry. Typedefs, enums and struct layouts: not listed.
- `src/platform/`, `src/cmd/`, and `src/devtools/` except `state_format.c`, which the plan does not port. `tmp/bindings/default.c` (the replay tool's `bindings_compiled`).
- Checking that a Lua function's body is a faithful copy: the checklist checks presence only. The data-table equality checks are Phase 3's.
- Deciding new not-ported items beyond the step notes: later steps record those in the data file.

## Further Notes

- `GPU_FRAME_BUFFER_BYTES` is listed as not ported, as the step note says, but `state_format.c` still prints it as the limit of its `gpu_frame` zone line. Step 3.15's `debug_state()` must still print the same text as the C, so the number it prints has to come from somewhere. That is for 3.15 to settle, not this step. The listing shows the entry with its reason so the question is visible there.
- The include graph today: 93 engine and game modules, three include cycles (`zone`/`zone_sizes`, `entity_geom`/`entity_registry`, and the 18-module game cycle of steps 3.6–3.9).
- The script's output names steps by their plan-2 numbers; if plan 2's step list is renumbered, `tools/checklist.txt` changes with it.
