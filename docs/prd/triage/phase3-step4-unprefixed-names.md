---
title: "Names without the module prefix"
description: "930 exported names of game/ repeat their module's name (planet.planet_init_instance, fixed.p8_fixed_add); drop the prefix everywhere with no caller missed and every hash unchanged, and change CLAUDE.md's naming rule with them."
status: needs-triage
---

## Problem Statement

Plan 5, phase 3, step 4, verbatim from `docs/love-port/plan-5.md`:

> 4. Names without the module prefix, with `CLAUDE.md`'s naming rule. Seen
>    when no exported name (`M.<name>`) of a module starts with the module's
>    name or with its first word followed by `_` (`entity_get` in
>    `engine.entity_id`) or, in `p8/`, with `p8_`, each exception listed in the
>    step note.

The phase-0 decision: "Exported names drop the repeated module prefix
(`planet.init_instance`, `p8_` inside `p8/` too); `CLAUDE.md`'s naming rule
changes with them."

Every call in the game repeats the module's name twice. A caller already
names the module (`local planet = require("game.planet")`), then each
export names it again: `planet.planet_init_instance(...)`,
`fixed.p8_fixed_from_int(0)`, `vec.vec_make(...)`,
`entity_registry.entity_registry_set_layer(...)`, `fatal.p8_fatal(msg)`,
`map_tables.map_tables_make_data_row(...)`. This was C's way of putting a
namespace into one global symbol table. In Lua the module table already is
the namespace. So every line that uses the game's API carries
noise, the long names push lines over the width limit, and `CLAUDE.md`
still tells the next agent to write `module_verb[_object]` and `p8_`
names.

At HEAD `fba2bd1` (step 3.2 done; step 3.3, comments only, in progress at
chunk 3 of 7), the 97 modules of `game/` that export anything define 1645
exported names (`M.<name> =` or `function M.<name>(` at line start). 930 of them, in 93 modules, break the step's check. They are used from
every module of `game/`, from `main.lua` and `conf.lua`, from the Lua
tests (`tests/lua/test_fixed.lua`, `test_runtime.lua`,
`tests/lua/game/test_economy_formulas.lua`), from the runtime fixtures
(`tests/fixtures/love_runtime/main.lua` and its `_slots` twin), and are
quoted in `docs/save-format.md`, `docs/love-port/design.md`, `CLAUDE.md`,
`README.md`, the `tdd` and `rework-comments` skills and eight e2e notes.

A rename looks mechanical, but a missed caller is silent in Lua. A field
read of an export that no longer exists returns `nil`. Called, it raises an
error, but only on the paths the 20 replays happen to run. Compared or
cached, it changes nothing visible: `state == entity_class.ENTITY_STATE_NORMAL`
becomes `state == nil`, and `local NONE = entity_class.ENTITY_STATE_NONE`
quietly fills a data table with `nil`. The replays cannot prove a rename
complete. A check has to do that.

## Solution

Every exported name of `game/` loses its module prefix, by one mechanical
rule with four listed exceptions. Every reader of every renamed name is
updated in the same chunk: game code, tests, fixtures, tools, docs,
skills, e2e notes. A new static check, `make lua-names`, proves both
halves of the step. First, no export of a module repeats its prefix (the
step's check, with its exceptions in the tool). Second, every read of a
field of a required module, anywhere in the repository's Lua, names a
field that module has. `CLAUDE.md`'s naming rule, `design.md`'s
**Naming** paragraph and a new `warn-review` rule change to match. No
behaviour changes: every capture hash, end state and save byte stays the
same, because module tables are never iterated, indexed by a computed key
or used as metatables (checked at `fba2bd1`), so a renamed key resolves to
the same function or value it did.

After the step, `planet.init_instance(...)`, `fixed.from_int(0)`,
`vec.make(...)`, `entity_registry.set_layer(...)`, `fatal.raise(msg)`,
`gfx.rectfill(...)`, `screen.frame_begin()`, `save_state.encode(...)` and
`entity_class.STATE_NORMAL` are the API.

## User Stories

1. As a maintainer reading a call, I want `planet.init_instance(p)` rather than `planet.planet_init_instance(p)`, so that each line says the module once.
2. As a maintainer of the runtime, I want `gfx.rectfill`, `gfx.print`, `fixed.mul`, `vram.SCREEN_WIDTH`, so that `p8/` reads like the PICO-8 API it emulates, without `p8_` on every name.
3. As a maintainer, I want constants to lose the prefix too (`entity_class.STATE_NORMAL`, `image_slot.ROW_FIRST`, `save_state.NODE_PLANET`), so that one rule covers every exported name whatever its case.
4. As a maintainer of a `_display` or `_trade` module, I want the sibling prefix stripped too (`score_panel_display.new_year`, `planet_display.draw`, `planet_trade.offer_trade`), so that a module named after a family does not keep its family's name on every export.
5. As a maintainer, I want one stripping rule I can apply in my head (drop `p8_` in `p8/`, then the longest leading run of the module's own name words), so that I can predict every new name without a table.
6. As a maintainer, I want an export whose stripped name would be empty to get a named role (`fatal.raise`, `state_format.dump`, `data_init.init`, `bindings_default.bindings`), so that no module is called as `fatal.fatal`.
7. As a maintainer, I want an export whose stripped name would be a Lua keyword (`slipway.slipway_end` → `end`) to get a word that says what it returns (`slipway.end_node`, with `slipway.start_node` beside it), so that no call is written `slipway["end"](sw)`.
8. As a maintainer, I want an export whose module's first word is the verb of the name (`draw_functions.draw_row`) to keep that verb, so that the stripped name is not a bare noun (`draw_functions.row`) that has lost its action.
9. As a maintainer, I want those three `draw_functions` names listed as the check's only exceptions, with their reason, so that the step note can list them and nobody "fixes" them later.
10. As a maintainer, I want no two exports of a module to end up with the same name, so that no rename silently overwrites another export (checked: none at `fba2bd1`, the tool refuses one).
11. As a maintainer, I want names that differ only in case (`layout.ROW_CELL_FIXED` the kind, `layout.row_cell_fixed(x)` its constructor) accepted, so that the constant/constructor pairs keep their pairing, as `entity_class.MOUSE_POS_HAS_SCREEN` / `mouse_pos_has_screen` already do.
12. As a maintainer, I want names that do not start with the module's own prefix left alone (`ui_button.button_new`, `scroll.camera_update`, `planet_family.burst_new`, `probe.targeting_track`, `g_*` fields, data tables), so that this step does exactly what its check measures and kind prefixes inside multi-kind modules keep telling kinds apart.
13. As a maintainer, I want instance fields that happen to start like a module (`planet.planet_type`, `planet.planet_id` on a planet entity) untouched, so that a rename of exports never touches entity data.
14. As a maintainer, I want `p8_` gone from the runtime's module locals and their aliases too (`p8_quarter_wave`, `p8_sprite_sheet_packed_bytes`, `P8_RND_*`), so that the new rule, "no prefix", has no exception left in `p8/`.
15. As a maintainer, I want assert and error messages that name a renamed export to name it as callers now write it (`"cart.load: ..."`, `"planet_family.init() must run first"`, `"gfx.lines: ..."`), so that a message never sends me to a name that does not exist.
16. As a maintainer, I want comments that name a renamed export updated the same way, so that the traps step 3.3 kept still point at real functions.
17. As a maintainer of the state dump, I want the dump's `stopped p8_fatal frame <n>` line kept byte for byte, so that `tools/end_state.sh` and its test, which read that line as format, keep working; it is a format token, not a Lua name.
18. As a maintainer of the fixed-point test, I want `tests/lua/fixed_reference.txt` left byte for byte (it is frozen from the deleted C and cannot be regenerated), with `test_fixed.lua` mapping each reference record name to the export it checks, so that the frozen reference keeps its meaning.
19. As a maintainer, I want the backend interface keys (`cls`, `pset`, `image_begin`, ... of the software backend, the meter and the LÖVE backend) and class-table keys (`draw`, `refresh`, `left_press`, ...) left as they are, so that the rename touches module exports and not the tables that dispatch by those keys.
20. As a maintainer, I want callers' aliases kept (`gfx_module`, `planet_module`, `input_module`), so that the diff is only the renamed names.
21. As a reviewer, I want each chunk's Lua diff to be exactly what its rename map yields when applied to HEAD, plus a short list of hand edits, so that I review a map, not 2000 changed lines.
22. As a reviewer, I want each chunk to cover one module group with every caller of its exports everywhere, so that every commit leaves the repository consistent.
23. As a reviewer, I want a static check that every `alias.field` read of a required module names an existing field, so that a missed caller on a path no replay runs is still caught.
24. As a reviewer, I want a grep for every old name of the chunk over the whole repository finding only listed, explained hits, so that docs, skills and notes are covered as well as code.
25. As a reviewer, I want `lua-guard` to keep passing untouched, so that the step proves it adds no global and no LÖVE name.
26. As the plan pilot, I want `make e2e` green with every `.json.sha256` unchanged after every chunk, and `make test-all` plus `make e2e end-state` after the last, so that the plan's rules hold.
27. As the plan pilot, I want `make test` no slower, so that the new check lives in its own target, outside `make test` and `make test-all`.
28. As the plan pilot, I want the step's check runnable as one command (`make lua-names`), so that the step note records it rather than a hand-made grep.
29. As the plan pilot, I want the step's check restrictable to a chunk's module group, so that each chunk's verifier sees its own group pass before later groups are done.
30. As the plan pilot, I want every assumption about step 3.3's outcome stated as a labelled hypothesis, so that a false one is detected rather than built upon.
31. As a future agent, I want `CLAUDE.md`'s naming rule to state the new convention with examples, so that I write `slipway.drag`, never `slipway.slipway_drag`.
32. As a future agent, I want `design.md`'s **Naming** paragraph to describe the names as they are, so that step 3.5 replaces a true section.
33. As a phase-end reviewer, I want a `warn-review` rule flagging an export that repeats its module's prefix, so that the next wave does not bring the prefix back.
34. As a writer of tests, I want the `tdd` skill's examples to use the new names (`fixed.from_int`, `gfx.rectfill`, `vram.init`), so that the skill teaches the API that exists.
35. As a reader of `docs/save-format.md`, I want its references to the save code to use the new names (`save_state.capture`, `save_state.encode`, `save_state.MAX_BYTES`, `visual_random.state`), so that the document still points at real code.
36. As a reader of an e2e note, I want the eight notes that name a function (`demo_data_record`, `slipway_drag`, `economy_spend`, ...) to name it as `module.name`, so that the note can still be followed.
37. As a maintainer of the `lua-names` check, I want it to test itself on small fixtures before it checks the game, so that a broken checker cannot report a false green.
38. As a maintainer, I want the check's allowlist of field reads limited to exact (file, alias, field) triples with a reason, so that it cannot hide a missed rename in the same file.

## Implementation Decisions

### The naming rule

- **Stripping.** For an export `M.<name>` of module `<dir>.<file>`:
  1. in `p8/`, drop a leading `p8_` (any case);
  2. then drop the longest leading run of the file name's words, from the
     whole name down to its first word, when followed by `_`. The test
     ignores case and the result keeps its own case.

  Examples: `planet.planet_init_instance` → `init_instance`;
  `score_panel_display.score_panel_new_year` → `new_year` (the run
  `score_panel`); `planet_display.planet_draw` → `draw` (the first word);
  `fixed.p8_fixed_add` → `add`; `draw_cost.p8_draw_cost_rectfill` →
  `rectfill`; `gfx.p8_draw_backend_get` → `draw_backend_get` (not a run of
  `gfx`); `vram.P8_SCREEN_WIDTH` → `SCREEN_WIDTH`;
  `entity_class.ENTITY_STATE_NORMAL` → `STATE_NORMAL`;
  `entity_id.entity_get` → `get`; `image_slot.IMAGE_SLOT_ROW_FIRST` →
  `ROW_FIRST`; `save_state.SAVE_NODE_PLANET` → `NODE_PLANET`.
- **A bare verb is fine.** `entity_id.get`, `exit_queue.put`,
  `planet_display.draw`, `image_slot.COUNT`: the module's name holds the
  object the stripped prefix held. No name is lengthened for taste.
- **Empty result** (the export is the module's name), each named by its
  role:
  - `fatal.p8_fatal` → `fatal.raise` (it raises the error).
  - `state_format.state_format` → `state_format.dump` (it writes the state
    dump; its siblings `save_summary`, `stopped`, `cheat_activations` are
    named by what they write).
  - `data_init.data_init` → `data_init.init` (pairs with
    `data_init.shutdown`).
  - `bindings_default.bindings_default` → `bindings_default.bindings` (the
    default bindings table).
- **Keyword result.** `slipway.slipway_end` would be `end`: it becomes
  `slipway.end_node` (it returns the end planet or the slipway's own mouse
  point), and its pair `slipway.slipway_start` becomes
  `slipway.start_node`, so the pair stays symmetric. The `end_` field of a
  slipway is a field, not an export, and stays.
- **The verb is the first word.** In `draw_functions`, whose first word is
  a verb, stripping the whole name leaves `draw_row`,
  `draw_row_text_image` and `draw_rows_default`. These start with the
  module's first word followed by `_`, but stripping again would leave a
  noun with no action (`draw_functions.row`). They stay as
  `draw_functions.draw_row`, `.draw_row_text_image` and
  `.draw_rows_default`, and they are the check's only exceptions. The
  tool lists them, each with this reason, and the step note lists them.
  `draw_functions.draw_functions_by_id` → `by_id`.
- **Collisions.** At `fba2bd1`, stripping makes no two exports of a module
  equal and produces no other keyword. A name that differs from another
  only in case is accepted: a kind constant and its constructor
  (`layout.ROW_CELL_FIXED` / `layout.row_cell_fixed`), a backend constant
  and its getter (`gfx_software.SOFTWARE_BACKEND` / `gfx_software.backend`),
  `gfx_software.SLOT_PIXELS` / `gfx_software.slot_pixels`. Lua tells them
  apart, and `entity_class` already has such a pair. If a collision appears
  at the implementer's HEAD, the second name keeps one more word of its
  prefix, and the commit message names it.
- **Not renamed.** Names that do not start with their own module's prefix
  (kind prefixes in multi-kind modules: `ui_button.button_*`,
  `scroll.camera_*`, `planet_family.burst_*` / `structure_*` / ...,
  `probe.targeting_*` / `main_economy_*`, `menu.galaxy_menu_*`; the `g_*`
  fields; data tables of `game_data` and `map_tables` such as
  `DRAW_FUNCTION_*` and `icon_tooltips_*`). Also not renamed: module
  locals outside `p8/`, callers' aliases (`gfx_module`, `planet_module`),
  instance and class-table fields, the backend interface keys, file and
  module names.
- **`p8_` in `p8/` locals.** The runtime's module-level locals and the
  aliases bound to their modules lose `p8_` too, because `CLAUDE.md`'s
  "`p8_` for the runtime only" goes. `p8_quarter_wave` becomes
  `quarter_wave` (its module's local and the alias in `p8.fixed`).
  `p8_sprite_sheet_packed_bytes` becomes `packed_bytes` (the module's local
  and the alias in `p8.sprite_sheet`). The `P8_RND_*` locals of
  `p8.fixed` become `RND_*`.

### What changes with a name

- **Code.** Every read and write of a renamed export: `alias.old` /
  `alias:old` where `alias` is bound to the module in that file, at load
  or at call time; `require("<module>").old`; `M.old` inside the module.
  The rename is applied by exact old name per module, never by a prefix
  pattern. A prefix pattern would also rename entity fields that start
  like a module (`planet.planet_type`, `planet.planet_id`, `slipway.start`).
  At `fba2bd1`, no old export name is also read as a field of anything
  other than its module.
- **Strings.** An assert or error message naming a renamed export names it
  as a caller writes it, `module.new` (the module's file name):
  `"save_state_capture: ..."` → `"save_state.capture: ..."`,
  `"planet_family_init() must run first"` →
  `"planet_family.init() must run first"`, `"p8_lines: ..."` →
  `"gfx.lines: ..."`. The rest of the message stays byte for byte, C
  vocabulary included (step 3.3 leaves strings alone; H-3.3-3). No message
  is raised in any replay, so no capture or end state sees one.
- **Kept strings.** The state dump's `stopped p8_fatal frame %d` line is a
  format token read by `tools/end_state.sh` and `tests/test_end_state.c`.
  It stays. `tests/lua/fixed_reference.txt` stays byte for byte, and so do
  `tests/test_lua_fixed.c`'s records and expected messages, which use the
  reference's record names.
- **`test_fixed.lua`.** It dispatches by string: `fixed[name]`, with
  `name` read from the reference. Its signature table and adapters keep
  the reference's record names as keys, and one function maps a record
  name to the export: drop `p8_fixed_` / `P8_FIXED_`, ignoring case. If an
  export goes missing, the test already reports "missing function" and
  fails. That is its own proof.
- **Comments.** A comment naming a renamed export names it as
  `module.new`, keeping the comment otherwise. A comment where the old
  word is a module or file name (`bindings_default`, `state_format`,
  `data_init` as modules) is left.
- **Docs, skills, notes.** Every mention of a renamed export in
  `docs/save-format.md`, `docs/love-port/design.md`, `README.md`,
  `CLAUDE.md`, the `tdd` skill (`tests.md`, `mocking.md`,
  `interfaces-design.md`), the `rework-comments` skill (`SKILL.md`,
  `worked-example.md`) and the e2e notes (`attract`, `pad_bankrupt`,
  `pad_colonise`, `pad_message_box`, `pad_planet_projects`, `pad_research`,
  `pad_tech_costs`, `pad_title`) becomes `module.new`. The `.txt` notes are
  not hashed. Not edited: `docs/love-port/plan-5.md` (the plan and its
  step notes are history and quote the check's own example `entity_get`)
  and `docs/prd/` (PRDs are deleted once done).
- **`CLAUDE.md` § Layout and naming.** The `p8/` description loses
  "(symbols prefixed `p8_`, file names unprefixed)". The naming bullet
  becomes: "Names are full English words, with no prefix: an exported
  name does not repeat its module's name, which every caller already
  writes (`planet.init_instance`, `vec.add`, `fixed.mul`, `gfx.rectfill`,
  `entity_class.STATE_NORMAL`). An exported function is named
  `verb[_object]`. One- or two-letter names are only for a local or
  parameter whose scope fits in 40 lines." The `p8/` clause changes in the
  runtime chunk. The bullet changes in the last chunk.
- **`design.md`.** The **Naming** paragraph of § Conventions inherited from
  the C is replaced by the rule above in at most five sentences: no module
  prefix, constants included, `p8_` gone; the `draw_functions` exception;
  a keyword-named field takes a trailing underscore (`end_`); a module
  alias that would clash with a parameter takes the `_module` suffix.
  The rest of the section is step 3.5's. Every other `design.md` mention
  of a renamed export (`fatal.p8_fatal(msg)`,
  `p8_fixed_rnd_from_state(state, x)`, `pad_panel_step`, ...) is renamed.
  § `make test` and the fake's checks gains one sentence on
  `make lua-names` and why it is outside `make test`.
- **`warn-review`.** One rule after the last (23 under H-3.3-6): "An
  exported name that repeats its module's name or first word, or starts
  with `p8_` in `game/p8/` (`planet.planet_draw`)."

### The check: `make lua-names`

A new LuaJIT tool, `tools/lua_names.lua`, and a public target `lua-names`
(`## fails when a game/ export repeats its module prefix or a Lua file
reads a module field that does not exist`). It is not a prerequisite of
`test`, `test-all`, `e2e` or CI (`CLAUDE.md`: never make `make test`
slower; the prototype took 0.26 s). It runs two checks and prints one line per finding, then a
count line. It exits 0 only when both are empty.

- **Prefix check (the step's check).** It applies to every module of
  `game/` except `main` and `conf`, or only to the modules named in
  `NAMES_MODULES` (space-separated module names, a trailing `.*` for a
  whole directory: `make lua-names NAMES_MODULES='p8.* love.*'`). Every
  name defined as `M.<name> =` or `function M.<name>(`, at any
  indentation, must not start with `p8_` (in `p8/`, any case), must not
  equal the module's file name or its first word, and must not start with
  the first word followed by `_` (any case). A table in the tool lists the
  exceptions, each as (module, name, reason): the three `draw_functions`
  names, and nothing else unless the commit message justifies it. The
  check also reports two exports of a module that become equal.
- **Field check (proof that no caller is missed).** It always runs over
  the whole repository's game Lua, whatever `NAMES_MODULES` says: every
  `.lua` under `game/`, `tests/lua/` and `tests/fixtures/love_runtime*/`.
  It loads each game module under plain LuaJIT (`game/` as the module
  path, as `lua-test-modules` does). It strips comments and strings with
  the algorithm `tools/lua_guard.lua` uses, copied, so that `lua-guard`,
  which `make test` runs, stays untouched. Then it reports every read
  that names a field the module table does not hold after loading and
  that no scanned file assigns (`alias.name =`, `M.name =`, for fields
  set at run time such as `M.g_slipway_too_far`). The reads it checks:
  - `alias.name` or `alias:name`, where `alias` is bound in that file by a
    statement that is only `local alias = require("<game module>")`, at
    any indentation;
  - `require("<game module>").name`;
  - `M.name` inside a module.

  An alias bound to two modules in one file is a finding.
  An allowlist in the tool holds exact (file, alias, field) triples with a
  reason. At `fba2bd1` there are 21 such reads in four files
  (`economy_tiers`, `economy_score`, `planet_trade`,
  `planet_family_trade`): a parameter or local named `planet` or
  `slipway` shadows a call-time `require` alias of the same name, and
  reads entity fields (`planet.needs`, `slipway.class`). Only those
  triples go in. A renamed export read through such an alias is not on
  the list, so it is still reported.
- **Self-test.** `tests/lua/test_lua_names.lua` runs the tool on small
  fixture game directories under `tests/fixtures/lua_names/`: one clean;
  one with a prefixed export, a prefixed constant and a `p8_` export in
  `p8/`; one with a misspelled field read at load time, one at call time,
  one through an inline `require`, and one cached in a local; one with an
  allowlisted shadowed read beside a non-allowlisted one in the same file.
  It checks each finding line and exit status. `lua-names` runs it first,
  so a broken tool fails before it checks the game. It is not in
  `UNIT_TESTS`, `LUA_GAME_TESTS` or any `make test` prerequisite.
- **Expected at `fba2bd1`.** The field check prints nothing. A prototype
  found 0 findings over 5313 checked reads once the 21 shadowed reads were
  set aside, and it reported an injected `fixed.p8_fixed_from_intx` read
  three times. The prefix check prints the 930 names, which is the work.
- The tool and its target stay after the step: any later rename needs the
  same proof.

### Why the hashes cannot move

At `fba2bd1` no module table is iterated with `pairs`/`next`, indexed with
a computed key (`M[...]`, `alias[...]`), given a metatable, or passed
around as a value, except through an alias shadowed by an entity
parameter, which the field check allowlists, and `test_fixed`'s
dispatch. So a key's name affects nothing but the lookups that spell
it, and the field check proves every lookup spells an existing key.
Function bodies, call order, random draws and save bytes do not change.
`lua-guard` sees no new global and no LÖVE name: no stripped name is
`love`, and module fields are not globals.

### No `guideline` question

The phase-0 decision, the step's check and the inventory at `fba2bd1`
(no collision, one keyword, four empty results, three verb-first names)
settle every case above.

### Hypotheses about step 3.3 (not done when this PRD was written)

This PRD was written from HEAD `fba2bd1`, where step 3.3 (the C headers,
comments only; `docs/prd/triage/phase3-step3-c-headers.md`) had committed
its chunks 1–3 of 7, while an implementer was editing comments under
`game/`. Only committed code was read (`git show HEAD:` / `git grep HEAD`).
Each chunk regenerates its rename map at its own HEAD with the rule above.
The counts here are for orientation, not a contract. A false hypothesis is
handled by applying the rule to what is there. It stops the run (plan
§ Rules) only if it makes a decision above impossible.

- **H-3.3-1.** Step 3.3 is committed whole (all 7 chunks and its step
  note) before this step starts. Every rename-only diff below is taken
  against that HEAD.
- **H-3.3-2.** Step 3.3 changed comment lines only. It added, removed or
  renamed no export, local, string or file, apart from deleting
  `game.main_economy`, which exports nothing and is required by nothing.
  So the export inventory (1645 exports, 930 to rename, no collision)
  holds at that HEAD. If `main_economy` still exists, it changes nothing
  here.
- **H-3.3-3.** Step 3.3 left assert and error messages byte for byte,
  including those naming the C (`"... NULL ..."`, `"... extends entity_t"`).
  This step changes only the export names inside them.
- **H-3.3-4.** Some comments that survive 3.3 still name exports by their
  old names, as traps or contracts (`bindings`' "raise through
  `fatal.p8_fatal`", `draw_meter`'s "Zero until the first
  `p8_draw_meter_frame_end`"). They are renamed here like code. The
  "Shapes" blocks and the "Declared in X.h, defined elsewhere" lists that
  quoted many old names are gone, so fewer comment lines are hit than at
  `fba2bd1`.
- **H-3.3-5.** In `design.md`, step 3.3 changed only the `-- Lua only`
  clause of the **Naming** paragraph and the "Data tables and checks"
  sentence. This step replaces the **Naming** paragraph whole, whatever
  3.3 left in it.
- **H-3.3-6.** Step 3.3 added `warn-review` rule 22. This step's rule is 23,
  or the next free number.
- **H-3.3-7.** Step 3.3 reworded `CLAUDE.md` § The game's pointer to the
  entity header and touched no line of § Layout and naming. This step
  edits only § Layout and naming (the `p8/` clause and the naming
  bullet).
- **H-3.3-8.** Step 3.3 wrote no new comment or message using an old export
  name beyond those it already had. Any it did write is a hit of the
  old-name grep, handled like any other.
- **H-3.3-9.** Step 3.3's step note and PRD are left as written. A note
  that quotes an old name is history.

## Implementation Chunks

**Each chunk, before its commit** (the verifier's proof):

1. **Rename map.** The chunk's map (`module old new`) is generated from its
   HEAD by the rule, and the chunk's special cases are applied by hand. It
   is written to `tmp/` and not committed, and the commit message carries
   it in short form (module, count, special cases).
2. **Rename-only diff.** The map is applied by a script to HEAD's version
   of every changed `.lua` file: qualified code reads and writes, `M.old`,
   inline `require`, and `module.new` in messages and comments. The result
   equals the working file, except for hand edits listed in the commit
   message (`test_fixed.lua`'s mapping, the slipway pair, comments where
   the old word is a module name, the tool's exception table).
3. **`make lua-names NAMES_MODULES='<the chunk's modules>'`.** Green: no
   prefix finding in the group except listed exceptions, and the field
   check over the whole repository empty.
4. **Old-name grep.** `git grep -nw` of every old name of the group over
   the whole working tree, excluding `docs/love-port/plan-5.md`,
   `docs/prd/`, `tests/lua/fixed_reference.txt` and `tests/test_lua_fixed.c`.
   It finds only hits listed in the commit message, each with its reason:
   a module or file name equal to an old name (`state_format`,
   `data_init`, `bindings_default`), `test_fixed.lua`'s reference-name
   keys, the dump's `stopped p8_fatal` line and its test.
5. **Gates.** `make test` and `make e2e` green, every `.json.sha256` and
   `.end-state` unchanged (`git status` clean under `tests/e2e/` but for
   notes), `make e2e` time noted; `make test` time not above the step's
   base.

**Chunks, in dependency order.** The tool comes first. Each later chunk is
independent of the others' code, and the order puts the most-called
modules first, so later diffs read against already-renamed calls:

1. **The check.** `tools/lua_names.lua` with its exception table (the
   three `draw_functions` names) and its allowlist (the 21 shadowed reads),
   the self-test and its fixtures, the `lua-names` target, and
   `design.md`'s sentence on it. Proof: the self-test passes. At HEAD the
   field check is empty and the prefix check prints exactly the inventory
   (930 at `fba2bd1`). An injected misspelled read in a game file is
   reported, then reverted. `make test` time is unchanged (the target is
   outside it).
2. **Runtime.** All of `game/p8/` (`cint` has none): `fixed` 29, `gfx` 30,
   `draw_cost` 29, `input` 17, `image` 10, `vram` 9, `draw_meter` 4,
   `gfx_software` 4, `str` 3, `sprite_sheet` 3, `palette` 2, `fatal` 1
   (→ `raise`), and the `p8_` locals and aliases. Every caller, about two
   thirds of `game/` (`fixed.from_int` alone is everywhere). Also
   `tests/lua/test_fixed.lua` (record-name mapping), `test_runtime.lua`,
   both `love_runtime*` fixture `main.lua` files,
   `tests/lua/game/test_economy_formulas.lua`'s `fixed` calls, the `tdd`
   and `rework-comments` skill examples, `design.md` mentions, and the
   `CLAUDE.md` `p8/` clause. Extra proof: `lua-test-fixed`,
   `lua-test-runtime` and the `love_runtime` checks are green. They cover
   the runtime paths the replays may not.
3. **LÖVE glue and devtools.** `love.backend` 1, `pattern` 3, `screen` 8,
   `sheet` 1, `slot` 2, `text` 1, `devtools.state_format` 4 (→ `dump`,
   `save_summary`, `stopped`, `cheat_activations`; the dump line text
   kept); callers in `main.lua`, `conf.lua`, `love/`, the fixtures.
4. **Engine.** `vec` 12, `geom` 3, `entity_class` 17 (the `ENTITY_STATE_*`
   constants → `STATE_*`), `entity_id` 8, `entity_registry` 19
   (`ENTITY_MESSAGE_*` → `MESSAGE_*`, `entity_register` → `register`),
   `entity` 7, `entity_geom` 8, `exit_queue` 2, `dispatch_context` 6,
   `bootstrap` 1. Note: `game_data`'s cached locals
   (`local NONE = entity_class.ENTITY_STATE_NONE`) are field reads the
   check covers.
5. **Data and generation.** `game_data` 3, `map_tables` 4 (including the
   cached `check_fields`, `data_item`, `data_row` locals in `game_data`),
   `data_init` 2 (→ `init`, `shutdown`), `demo_data` 6, `visual_random` 4,
   `background` 11, `galaxy` 5 (`gen` has none); `docs/save-format.md`'s
   `visual_random` mentions; the `attract` note.
6. **Layout and images.** `layout` 31, `image_slot` 12, `row_image` 2,
   `text_image` 3, `button_image` 5, `draw_functions` 4 (`by_id`, the
   three listed exceptions kept).
7. **Planets.** `planet` 37, `planet_display` 13, `planet_family` 5,
   `planet_family_trade` 2, `planet_trade` 5; the `pad_planet_projects`
   note; the cart's "`planet_init()` / `planet_family_init()` must run
   first" messages.
8. **Slipways, wormholes, trade, probes.** `slipway` 25 (`end_node`,
   `start_node`), `slipway_display` 5, `wormhole` 16, `wormhole_display` 2,
   `trade` 10, `particle` 3, `probe` 11, `targeting` 6; the `pad_colonise`
   and `pad_research` notes.
9. **Economy and research.** `economy` 15, `economy_formulas` 4 (and
   `test_economy_formulas.lua`), `economy_score` 9, `economy_tiers` 4,
   `research` 18, `research_display` 5, `tech_gate` 6; the `pad_bankrupt`,
   `pad_tech_costs` and `pad_title` notes; `docs/save-format.md`'s
   `research` mention.
10. **UI core.** `ui` 33, `scroll` 11, `mouse` 8, `mouse_button` 1,
    `structure_icon` 19, `hud` 10 (`ui_button` has none).
11. **Boxes and panels.** `message_box` 20, `menu` 3, `score_panel` 20,
    `score_panel_display` 7, `help` 13, `loading_screen` 3,
    `demo_overlay` 2; the `pad_message_box` note.
12. **Input and cart.** `bindings` 52, `bindings_default` 1 (→ `bindings`;
    `README.md`'s file mention left), `input_step` 11, `cheat` 4,
    `attract` 12, `cart` 16.
13. **Save.** `save_state` 37 (`SAVE_STATE_*` → `MAX_BYTES`, `NODES_CAP`,
    ...; `SAVE_NODE_*` → `NODE_*`), `save_load` 4, `save_storage` 3; the
    rest of `docs/save-format.md`.
14. **Pad layer, the rule, the full check.** `pad_back` 3, `pad_build` 7,
    `pad_cycle` 10, `pad_direction` 5, `pad_jump` 21, `pad_nav` 28,
    `pad_panel` 12, `pad_select` 12, `pad_slipway` 13, `pad_target` 3.
    Then `CLAUDE.md`'s naming bullet, `design.md`'s **Naming** paragraph,
    the `warn-review` rule. Then the full check: `make lua-names` with no
    restriction (only the three listed exceptions), the old-name grep of
    all 930 names over the repository, `make test-all` and
    `make e2e end-state` with every hash and end state unchanged. The
    commit message carries the step note's material: the exceptions, the
    allowlist, the kept format strings and reference names, the `make e2e`,
    `make test` and `make lua-names` times.

Every module of `game/` that exports a prefixed name is in exactly one
chunk (93 at `fba2bd1`). A module the implementer finds that is not
listed (H-3.3-2 false) joins the chunk of its group.

## Testing Decisions

- No game behaviour changes, so no game test is added. The proof is the
  rename-only diff, the field check, the old-name grep and the replays.
- Tests that change with their module: `test_economy_formulas.lua` (new
  `economy_formulas` and `fixed` names), `test_fixed.lua` (record-name
  mapping, adapters on the new names), `test_runtime.lua` and the
  `love_runtime*` fixtures (new `p8` and `love` names). Each still
  checks the same behaviour. `tests/lua/game/check.lua` names nothing.
- The new tool gets its own fixture-driven test, `test_lua_names.lua`,
  run by `make lua-names` before the check. A good test here drives the
  tool as a user does (a game directory in, finding lines and an exit
  status out) and covers every read form the field check claims, plus the
  allowlist's exactness. It does not unit-test the tool's internals.
- Prior art: `tests/test_lua_guard.c` (a static Lua check driven by
  fixtures), `tests/lua/test_modules.lua` (loading every module under plain
  LuaJIT), the per-chunk instruments of steps 1.4–3.2, and step 3.3's
  mechanical diff-shape proof.
- `make test` gets no new prerequisite. `lua-guard` and
  `lua-test-modules` run unchanged and must stay green after every chunk.

## Out of Scope

- Kind prefixes inside a module that are not the module's own
  (`ui_button.button_*`, `scroll.camera_*`, `planet_family.burst_*`,
  `probe.targeting_*`, `menu.galaxy_menu_*`), the `g_*` fields, and data
  table names: not measured by the step's check.
- Module-local names outside `p8/` (`slipway_copy_infraspace`), callers'
  aliases and their `_module` suffixes, instance and class-table fields
  (`end_`, `planet_type`), backend interface keys, module and file names.
- Splitting or merging modules, or moving a function to another module
  because its stripped name reads better elsewhere.
- C vocabulary in messages (`NULL`, `entity_t`): step 3.3 reports them
  and they are left.
- `design.md` § Conventions inherited from the C beyond the **Naming**
  paragraph (step 3.5).
- `tests/lua/fixed_reference.txt` and the dump's format strings.
- Making `make lua-names` part of `make test`, `make test-all` or CI.

## Further Notes

- Inventory at `fba2bd1`: 97 modules export 1645 names; 930 in 93 modules
  break the check. By module group, roughly: runtime 141, LÖVE and
  devtools 20, engine 83, data 35, layout and images 57, planets 62,
  slipways to probes 78, economy and research 61, UI core 82, boxes and
  panels 68, input and cart 96, save 44, pad 114.
  Case-only pairs created: `layout`'s six `ROW_CELL_*` / `ROW_SCALAR_*`
  constants beside their constructors, `gfx_software.SLOT_PIXELS` /
  `slot_pixels`.
- Checked at `fba2bd1`: no `pairs` over a module, no `M[...]` or
  `alias[...]` on a module, no `setmetatable`, no re-export of another
  module's function (only `image`'s two constants copied from `vram`),
  and no old export name read as a field of anything other than its
  module. 75 load-time local caches of module fields exist
  (`local CHORD_FIRST = input.P8_PAD_BUTTON_5`). The field check covers
  them.
- The step note records: the three exceptions, the allowlisted shadowed
  reads, the kept format string and reference names, any collision found
  and how it was resolved, the `make e2e` time, `make e2e end-state`, and
  `make test` and `make lua-names` times.
