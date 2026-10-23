# Plan 5 — Clean the Lua into natural Lua

## 1. Goal

Turn the faithful copy of the C (plan 2) into Lua that reads as Lua, with the
20 e2e as the safety net: every step keeps every capture sha256 and every end
state unchanged. Read `docs/love-port/design.md` first.

## 2. Rules

- Every step keeps `make test-all` green and passes `make e2e end-state`,
  with every `.json.sha256` and `.end-state` unchanged. End states are outside
  `test-all` and check-e2e: `plan-pilot` runs `make e2e end-state` after
  check-e2e is green and before the step note. A red result goes through
  `fix-bug` as check-e2e does; the step is reverted only when that path
  stops. Never re-baselined.
- Each step note records how long `make e2e` took. Over 9 s is red, like a
  changed hash: fixed in the step, reverted only when that fails.
- A step that finds a phase-0 decision cannot be carried out as written stops
  `plan-pilot`, which reports instead of asking `guideline`. So do a reverted
  step (the report says why) and a phase-end `warn-review` warning that bears
  on a decision for a later step; other warnings go to `plan-review`. The
  decision is reopened with the user, the plan goes through `/plan-converge`
  again, and the run restarts at that step.
- Each step updates or deletes, in the same step, the parts of
  `docs/love-port/design.md` and of `CLAUDE.md` § The game that it makes
  false: later steps are planned from them.
- One module (or one kind of C-ism) per step, small enough to fail fast. A
  step too large for that is split into its PRD's Implementation Chunks, one
  per module group: a module's definitions together with all their callers.
  Each chunk's verifier sees `make e2e` green with every `.json.sha256`
  unchanged and the step's check, restricted to that group, holding before
  the chunk is committed; the last chunk runs the full check.
- The fixed-point format and the random generator stay: they are what makes
  the hashes match, not a C-ism.
- Not this plan: new features, iOS.

## 3. Phases and steps

### Phase 0 — Decide what "natural" means

Attended: run in its own session with the user, before `plan-pilot`. Once
phase 1 is written, rerun `/plan-converge` on this plan, then start
`/plan-pilot` at phase 1. If phase 1 still reads "(written by step 1)",
`plan-pilot` stops and reports instead of running phase 0.

1. A grilling (`/grill-me`) with the user over a list of the C-isms found in
   `game/` — for example manual memory zones and allocators, registries and
   id lists, the output queue, components held by pointer, `_t` names and
   module prefixes, fixed-size arrays — and the checks that freeze them
   (`lua-test-modules` and its reference, which can no longer be
   regenerated, `lua-guard`, the `tests/lua/game/` tests), deciding for
   each: keep, replace with what, in which order. The proposed order follows
   dependencies, then puts first the steps whose decision may not keep the
   hashes; a check that freezes a structure is dealt with no later than the
   step that breaks it. Each later step states how its result shows in the
   repository, false before the step and true after; its note records that
   check. The steps are grouped into as many phases as useful, each ending
   where a `warn-review` of the code touched so far could still change the
   steps after it. The decisions are written
   into this plan's phases below before any code change, and the step ends,
   in the same attended session, by writing its `### Step 0.1` note: the
   decisions and the user's agreement. Verified by that note.

### Decisions of phase 0

- The Lua no longer mirrors the C: files, functions, order and names may
  diverge freely. Comparing with the C side by side is given up; the hashes
  and end states are the only net.
- Memory zones and allocators go entirely, with no Lua stand-in, and their
  `SIZEOF` entries and the `cap` lines of the state dump with them (the
  `.end-state` files filter `cap` out). A capacity limit stays only where it
  is a game rule (the save format's 255 nodes and slipways).
- Entity ids become plain numbers, never reused, resolved through one
  `id → entity` table that drops an entity at its death. The
  `{ index, generation }` tables, `entity_id_none()` and `entity_id_equal`
  go.
- `entity_list`, `pointer_list`, `entity_id_list` and `value_list` become
  plain 1-based Lua arrays. A traversal that tolerates changes says so (a
  copy, or a removal deferred to the exit queue) instead of the `last_count`
  iterator. `collect` becomes local loops; the exit queue stays, as a plain
  array.
- Every array is 1-based, data tables included.
- Predicates return booleans; `x ~= 0` tests and `and 1 or 0` on truth
  values go.
- An entity's optional parts are fields that are `nil` when absent; the `has`
  bits go, and so do the `has_<field>` 0/1 numbers paired with a value
  (`has_production_bonus`, `has_net_income`, `has_resource`, ...): the value
  is `nil` when absent.
- Research project ids `{ index, epoch }` become plain numbers like entity
  ids.
- The capacity limits left after phase 1 that are not a game rule (message
  box, layout, text, row image, score panel, help, bindings,
  `SLIPWAY_SERVED_PLANETS_CAP`, ...) go.
- Exported names drop the repeated module prefix (`planet.init_instance`,
  `p8_` inside `p8/` too); `CLAUDE.md`'s naming rule changes with them.
- `lua-test-modules` keeps only "every module loads": the comparison with
  `tests/lua/table_reference.txt` and the file go. `lua-guard` stays. Each
  `tests/lua/game/` test changes with the module it tests.
- The `-- Copy of src/...` headers go. `cint.div` and `cint.mod` stay (LuaJIT
  has no integer division), as do `goto continue` and the call-time
  `require` of include cycles.
- `design.md` § Conventions inherited from the C is replaced by the
  conventions the code follows.

### Phase 1 — The engine foundation

1. `lua-test-modules` checks only that every module loads. Seen when
   `tests/lua/table_reference.txt` is gone and `make test` is green.
2. Zones and allocators removed. Seen when
   `grep -rE 'require\("engine\.(zone|zone_sizes|frame_zone|game_zone|allocator|slot_group|slot_tally)"\)|SIZEOF' game/`
   finds nothing, those modules and the game-side pools (`ui_zone.lua`,
   `particle_zone.lua`, `grab_arena.lua`) are gone, and the state dump
   writes no `zone`, `budget` or `cap` line. The game-side pools go first.
3. Entity ids as plain numbers. Seen when
   `grep -rE 'index *=[^}]*generation|generations|entity_id_equal|entity_id_none' game/`
   finds nothing (the registry's change counter `entity_registry_generation`
   is not an id and stays).
4. The four list kinds as plain arrays. Seen when `entity_list.lua`,
   `pointer_list.lua`, `entity_id_list.lua`, `value_list.lua` and
   `collect.lua` are gone and `grep -r last_count game/` finds nothing.

### Phase 2 — The entity model

1. Optional parts as `nil`-able fields, `has_<field>` numbers included. Seen
   when
   `grep -rnE '_HAS_|entity_has\(|\.has\b|\bhas_[a-z_]+ *([~=]?= *[01]\b|= *[a-z_.]*has_[a-z_]+)|has_[a-z_]+ or [01]\b' game/`
   finds only bit sets that are not an entity's optional parts, and `has_`
   names that are not a 0/1 flag paired with a value (a method such as
   `has_refresh`, a local boolean), each listed in the step note.
2. Research project ids as plain numbers. Seen when `grep -rn epoch game/`
   finds nothing.
3. Capacity limits that are not a game rule removed. Seen when every
   remaining constant or local in `game/` that bounds a growing collection
   or text (by a fatal, an assert, a cut or a refusal), whatever its name
   (`_CAP`, `_MAX`, `_LENGTH`, `_SIZE`, `_LIMIT`...), is a game rule, each
   listed with its reason in the step note; a constant that is only the size
   of a data table is not a limit.

### Phase 3 — The surface

One kind per step.

1. Booleans. Seen when
   `grep -rE 'and 1 or 0|[~=]= [01]( *(then|and|or|do|\))|$)' game/` finds
   only tests on numbers that are not truth values and 0/1 numbers written
   to a format (the save's flag bytes), and no function whose returns are
   all `0`/`1` (`grep -rnE 'return [01]$' game/`) is still used as a truth
   value, whatever its name, each exception listed in the step note. A predicate's `return 0` is truthy in
   Lua: a missed one is a bug the replays may not see.
2. 1-based arrays. Seen when
   `grep -rE '\[0\]|for [a-z_]+ = 0,|\[[a-z_.]+ *\+ *1\]|while [a-z_]+ < |\[[a-z_.]*count\] *=' game/`
   finds only plain numbers or genuine element shifts, not 0-based indices,
   each listed in the step note. An array with holes keeps an explicit count
   or index loop; never `#` or `ipairs` on it.
3. The C headers. Seen when
   `grep -rnE -- '--.*(Copy of|\bC\b|memset|\b[a-z0-9_]+_t\b|\{0\})' game/`
   finds only lines that are not about the C, each listed in the step note.
   A comment that states a trap through the C is reworded to state the trap
   without it, not deleted.
4. Names without the module prefix, with `CLAUDE.md`'s naming rule. Seen
   when no exported name (`M.<name>`) of a module starts with the module's
   name or with its first word followed by `_` (`entity_get` in
   `engine.entity_id`) or, in `p8/`, with `p8_`, each exception listed in the
   step note.
5. `design.md` § Conventions inherited from the C replaced. Seen when the
   section is gone and no convention left in `design.md` names a C construct.

## 4. Done when

- Every C-ism listed in phase 0 is handled as decided: every step after
  phase 0 has its own check holding.
- `make test-all` green and `make e2e end-state` passing, every hash and end
  state unchanged.
- `docs/love-port/design.md` and `CLAUDE.md` describe the code as it stands.
- `make e2e` under 9 s.

## 5. Open questions

- None.

## 6. Step notes

### Step 0.1

Grilling held with the user on 2026-10-22/23. The user wants the code clean
for its own sake (no feature or port depends on it) and agreed to every
decision under "Decisions of phase 0" and to the order of phases 1–3.
Checked during the grilling: the save format depends on no memory layout;
`end_state.sh` filters the `cap` lines, so removing the zones changes no
`.end-state`.

### Step 1.1

`tests/lua/test_modules.lua` only requires every module of `game/` (116
loaded) and clears the sentinels of a failed `require`;
`tests/lua/table_reference.txt`, `--defer` and `LUA_DEFERRED_MODULES` are
gone, `tests/test_lua_modules.c` covers loading only. `design.md` describes
the new check; its `SIZEOF` sentence stays for step 1.2. Seen: the reference
is gone, `make test` green (0.58 s). `make e2e` 20/20 in 5.8 s, `make
end-state` 20/20. The PRD agent dropped `--defer` without asking `guideline`:
the list was empty and the step keeps "only" the load check.

### Step 1.2

Done in 7 chunks, game-side pools first: UI pool, particle pool with slot
groups and slot zones, grab arena and arenas, frame zone, game and load
zones, engine lists and helpers, then the game modules with the allocator,
`zone` and `zone_sizes` deleted. Ten modules gone; every allocation is a
plain table. Kept to hold the hashes: a growing list still gets a fresh
items table holding a copy at the same moments (copies of a list header
share it), every deep copy, every call order. Real lengths read from
`SIZEOF` became named constants with the same values. The entity table cap
(512) now lives in `engine.entity_id`; the exit queue keeps its cap. The dump
writes no `zone`, `budget` or `cap` line. Seen: the step's grep finds
nothing, the ten modules are gone, no `make e2e` state holds such a line.
`make e2e` 20/20 in 5.3 s, `make end-state` 20/20, `make test` 0.6 s. The
e2e notes quoting dump peaks were rewritten; notes quoting per-pool live
counts as game state (`pad_message_box.txt`, `pad_pause_restart.txt`,
`pad_game_end.txt`, `pad_bankrupt.txt`, `pad_planet_projects.txt`) were left
as they are.

### Step 1.3

Two chunks: the id representation, then the old API. An entity id is a
number from a counter never reset, looked up in one `id → entity` table
that drops the entity when the exit queue flushes it, before its class
release; "no link" is `nil`. `engine.entity_id_type`, the
`{ index, generation }` tables, the 512 cap with the "entity table full" and
"layer order full" fatals, `entity_id_live` and every `entity_id_none()`
helper are gone. Predicates kept their 0/1 results for step 3.1. No unit
test added: ids reach no capture, save or end state; the replays are the net.
Seen: the step's grep finds nothing. `make e2e` 20/20 in 5.2 s, `make
end-state` 20/20, `make test` 0.65 s.

### Step 1.4

Five chunks: `value_list` with the trade route lists, `pointer_list` with
`collect`, `entity_id_list`, `entity_list` with the registry buckets, then
the exit queue. Every list of these kinds is a plain 1-based array; lists
held by several owners are emptied or refilled in place, an offer's lists
are copies taken at the same moment and assigned back. Reserves went with
copy-on-grow. Every `last_count` walk became a numeric `for` (one class-2
walk: remove then break); none needed a copy or a deferred removal. Each
chunk ran a temporary check after every walk body (length and current
element unchanged) once over the 20 replays: it fired on none of about 55
walks, then was removed. The bootstrap update walk and the save loader's
reverse walk keep their live-length forms. `collect_format_signed` moved to
`economy_formulas`. The `{ items, count }` data tables stay 0-based for step
3.2. Seen: the five modules are gone, `grep -r last_count game/` finds
nothing. `make e2e` 20/20 in 4.9 s, `make end-state` 20/20, `make test`
0.65 s.

### Phase 1 review

The end-of-phase-1 `warn-review` raised three warnings bearing on phase-0
decisions; `plan-pilot` stopped and the user decided, on 2026-10-23: the
`has_<field>` numbers are optional parts (step 2.1), research project ids
become plain numbers (new step 2.2), and the capacity limits that are not a
game rule go (new step 2.3). `plan-review` fixed the checks of steps 3.1,
3.2 and 3.4 in the same review.

### Step 2.1

Ten chunks by module group: display rows, resources, research projects,
the planet's parts, particles/probes/targeting, the economy, UI/buttons/icons,
boxes and panels, positions, then the entity header with the docs and
skills. Every `has` bit set of an entity and every `has_<field>` 0/1 number
paired with a value is gone: an absent part or value is `nil`, tested with
`~= nil`; `is_dead` stays on every entity. Each chunk ran a temporary
instrument at every read or copy not behind its flag over one `make e2e`:
no stale value was ever read, every unguarded read found the 0 placeholder;
the reads that needed it became `x or 0` (row signature output, saved fold
anchor and infraspace). Save bytes stayed identical (102 distinct records)
and the decoder's results unchanged under byte fuzzing. Exceptions left by
the check, none an entity's optional part: `SAVE_NODE_*` (the save's node
flag word), `MOUSE_POS_HAS_*` with `mouse.has` (a by-value struct, not an
entity), `ROW_SIGNATURE_HAS_OUTPUT` and `ROW_NEED_HAS_RESOURCE` (cache-key
bits), `has_refresh` (a class method). `main_economy.lua` now holds only a
shapes comment and is required by nothing. warn-review rule 13 and the tdd
skill were inverted to match. `make e2e` 20/20 in 4.9 s, `make end-state`
20/20, `make test` 0.65 s.

### Step 2.2

One unit. A research project id is a number from `research`'s own counter,
never reset and never reused, resolved through two module-local maps (id →
project, project → id); `research_expire_project_ids` gives every current
project a fresh number, so an id kept past it resolves to `nil` as before.
`RESEARCH_PROJECT_ID_NONE`, both `research_project_id_none()` helpers, the
16-bit epoch and its masks are gone; "no project" is `nil`. A shadow-epoch
instrument over one `make e2e` compared old and new lookups: 552 live,
11839 `nil`, 0 stale, never different. Seen: `grep -rn epoch game/` finds
nothing. `make e2e` 20/20 in 5.0 s, `make end-state` 20/20.

### Step 2.3

Five chunks: startup/devtools/storage, layout and image caches, boxes and
panels, planets/slipways/economy, research and tech gate. 31 limits went,
each the size of a deleted C array or buffer; inline arrays are created as
written and read below their count. Kept as game rules, each named in the
code: the save format's 8192-byte record, its 255 nodes and slipways, its
field ranges (planet id 7 bytes, 8 needs, magic, 3 recent types), its pool
caps and u8/u16 field widths, and the weighted picker's 66 tickets; the
cart's 3 slipways per slipgate; `draw_meter`'s PSX GPU slots (a measured
machine). Not limits: data table sizes, the image slot table and its
evicting caches, saturating counters, flushed batches, offsets, contract
asserts, cuts at a NUL or delimiter. Instruments counted 0 hits of every
removed cut or refusal; the planet row signature's more-than-4-needs
refusal was never reached (at most 4 needs in any replay). `make e2e` 20/20
in 4.8 s, `make end-state` 20/20, `make test` 0.65 s.

### Step 3.1

Nine chunks by module group: runtime, engine (with `is_dead`), class
callbacks, data lookups, planets/slipways/wormholes, economy/research/
targeting, UI, input/cart/save, pad layer. Every predicate returns exactly
`true`/`false`, every truth-valued field and local is a boolean, `nil`
reads as false, no `== true`/`== false` is written; counters became
`if c then n = n + 1 end`. A predicate still used across a later chunk got
a temporary `and 1 or 0` wrapper, removed by its own chunk. 0/1 stays only
at format boundaries: the save record's flag fields (with `fold_anchor` and
`infraspace` saved as `0x10000`/`0`) in the builder and loader, the dump's
`visible` column, the draw-cost key's transparency bit. `fold_anchor` and
`infraspace` are `true` or `nil` in the game. Each chunk ran a static scan
and a type assert on every converted return, write and read over one
`make e2e`: none fired; the save records stayed byte-identical (133
encoded) and the decoder's accept/reject unchanged under fuzzing. Remaining
grep hits, by kind (none a truth value): bit-mask tests, counts and lengths,
divisors and arithmetic, colors and pixels, fixed-point values (`satisfied`,
a route's resource), save format fields, the layer key, the dump and
cost-key formats; every `return 0/1` left is in a function returning
numbers. design.md § Truth values and warn-review rule 20 match. `make e2e`
20/20 in 4.9 s, `make end-state` 20/20, `make test` 0.7 s.

### Step 3.2

Ten chunks by module group: runtime lists, registry, `game_data`
sequences, `map_tables` lookups, data rows, layout values and image caches,
message box/menus/panels, research/tech gate/economy/slipway lists, input
and pad, save. Every array runs from 1; a counted array keeps its count and
appends with `local n = count + 1`; a removal shifts down and clears the
vacated slot; an array with holes is walked by index, never `#`/`ipairs`.
Enums that index a table (`ENTITY_MESSAGE_*`, `BINDINGS_MODE_*`,
`BINDINGS_ACTION_*`) are numbered from 1; "not found" is `nil` instead of
`-1`. Kept as maps keyed by a value where 0 is real: colours, pixel
coordinates and offsets, glyph rows, byte addresses, angles, image slot
handles, the CRC table, disk node kinds, tag bit positions, layer values,
the demo's frame clock. The save writes the same bytes: ranks on disk stay
0-based, converted only in the builder (`position - 1`) and loader
(`rank + 1`). Each chunk ran a zero-key trap and a past-count counter over
one `make e2e` (never fired, counts equal before and after) and compared
every shifted data table with its HEAD version at load; the 133 encoded
saves stayed byte-identical, every record and the demo record round-trip,
the decoder's accept/reject is unchanged under fuzzing. Remaining grep
hits, by kind: the value-keyed maps above, genuine element shifts (registry
insert/remove, ticket removal), neighbour reads, save-rank conversions,
plain numbers. warn-review rule 21 matches. `make e2e` 20/20 in 4.9 s,
`make end-state` 20/20, `make test` 0.65 s.

### Step 3.3

Seven chunks by module group, comments only (plus `game.main_economy`
deleted: it held only a shapes comment and nothing required it). Every
`-- Copy of` header, "Shapes, with the C field names" block, `-- Lua only`
marker, `{0}`/memset note and port-status claim is gone; a shape stays only
where no constructor lists the fields (the entity header and class table).
Contracts and traps stay, reworded in Lua terms next to their code; two
"copy on the stack" warnings went, their tables being fresh per iteration.
The two hand-kept data modules (`sprite_sheet_packed`, `bindings_default`)
name the file they follow. Proof per file: the `-U0` diff holds only comment
and blank lines, and the stripped bytecode of HEAD and of the edit share a
byte-identical compile (this LuaJIT seeds its string hash at random, so a
single compile can differ even for an unchanged file). Seen: the step's
grep finds nothing in `game/`; the wider grep finds only three "structure"
words. Left byte for byte, being code: assert and error strings naming
`NULL`, `_t` types, `struct`, "dereferences" or the C (`entity_id`,
`slipway`, `slipway_display`, `bindings`, `pad_nav`, `probe`, `layout`,
`hud`, `map_tables`, `gfx`, `cart`, `game_data`, `draw_functions`,
`pad_cycle`, `fixed`, `trade`, `planet_family`, `mouse_button`,
`loading_screen`), and attract's assert citing a `test_demo_data` that no
longer exists. design.md's `-- Lua only` and generated-header sentences
match; warn-review rule 22 added. `make e2e` 20/20 in 4.5 s,
`make end-state` 20/20, `make test` 0.66 s.

### Step 3.4

Fourteen chunks: the `make lua-names` check (`tools/lua_names.lua`, its
fixture self-test, outside `make test`, `test-all` and CI, 0.45 s), then
one module group each with all its callers. 930 exports lost their module's
name or first word, and `p8_` is gone from `game/p8/` (`planet.init_instance`,
`fixed.add`, `vram.SCREEN_WIDTH`, `entity_class.STATE_NORMAL`). Special
cases: an empty stripped name takes a role name (`fatal.raise`,
`state_format.dump`, `data_init.init`, `bindings_default.bindings`); the
keyword case `slipway.slipway_end` became `slipway.end_node`, with
`slipway.start_node`. Kept: kind prefixes that are not the module's own,
instance fields, `g_*` fields, module locals outside `p8/`, the dump's
`stopped p8_fatal frame` token and `fixed_reference.txt` (`test_fixed` maps
its record names). Exceptions to the check, listed in the tool:
`draw_functions.draw_row`, `.draw_row_text_image`, `.draw_rows_default`
(the verb is the module's first word) and `bindings_default.bindings` (the
stripped name would be empty). Proof per chunk: the rename map regenerated
at HEAD reproduces the chunk's files except listed hand edits (require
paths and file paths the script had touched, wildcard mentions), the
old-name grep's hits all explained, `lua-names` clean for the group.
Seen: unrestricted `make lua-names` reports 0 prefix and 0 field findings.
CLAUDE.md's naming rule, design.md's Naming paragraph and warn-review rule
23 match. `make e2e` 20/20 in 4.8–5.1 s, `make end-state` 20/20,
`make test` 0.63 s.

### Step 3.5

One unit, docs only. design.md § "Conventions inherited from the C" is
replaced, in the same place, by § "Code conventions": Naming, Modules and
loading, Tables and objects, Ids, Lists and sharing, Arrays, Integers bytes
and strings, Truth values, Control flow and errors, Data tables and checks.
Claims found false against the code were corrected rather than carried over
(the context box is `{ value = false }`; the data constructors are
`map_tables` exports; `math.floor` and `%` are described by what they do;
module state also lives in `M.g_*` fields), and three stale claims outside
the section were fixed (§ Layout of the fork's `tests/lua/`, § How the Lua
game draws, § Numbers and randomness). Each paragraph was checked against
the code by a grep or a read; every backticked name in design.md exists.
Seen: "inherited from the C" is gone; the C mentions left are true facts,
not conventions (the C game set the hashes, the fake LÖVE is C with C
tests, the fixed-point reference was frozen from the C library, no C unit
test was ported). `make e2e` 20/20 in 4.8 s, `make end-state` 20/20.
