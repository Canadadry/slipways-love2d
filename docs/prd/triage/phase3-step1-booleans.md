---
title: "Booleans: truth values as Lua booleans"
description: "Every predicate and every truth-valued field of game/ still uses the C's 1/0 numbers, tested with ~= 0 and built with `and 1 or 0`; turn them into Lua booleans without changing a hash, an end state or a save byte."
status: needs-triage
---

## Problem Statement

Plan 5, phase 3, step 1 (Booleans), verbatim from `docs/love-port/plan-5.md`:

> 1. Booleans. Seen when
>    `grep -rE 'and 1 or 0|[~=]= [01]( *(then|and|or|do|\))|$)' game/` finds
>    only tests on numbers that are not truth values and 0/1 numbers written
>    to a format (the save's flag bytes), and no function whose returns are
>    all `0`/`1` (`grep -rnE 'return [01]$' game/`) is still used as a truth
>    value, whatever its name, each exception listed in the step note. A predicate's `return 0` is truthy in
>    Lua: a missed one is a bug the replays may not see.

The Lua game is a faithful copy of the C, and the C has no booleans. Every
predicate returns `1` or `0` (`planet_is_family`, `entity_geom_is_on_screen`,
`tech_gate_is_unlocked`, the save validators, ~80 exported and many more
local ones), every truth-valued field holds a number (`is_dead` on every
entity, the class flags `skip_draw_off_screen`, `is_planet`,
`is_structure`, the `stop` of a left-press or accept-drop result, a button's
`pressed`, `released`, `is_hovered`, a panel's `is_shown`, an icon's
`is_revealed`, a need's `satisfied`, module flags such as `g_initialized`,
`save_pending`, the weighted picker's `recent_set`), every call site reads
`if f(x) ~= 0 then` or `== 0`, and every comparison used as a value is
wrapped in `(c) and 1 or 0`. Lookups return a `1`/`0` "found" flag before
their value (`return 1, out`). The step's grep finds about 690 lines in 75
files, and `return [01]$` about 175.

For a maintainer this is noise on every line, and it is a trap: in Lua `0`
is truthy, so `if f(x) then` on a predicate that still returns `0` is always
taken, and `if f(x) ~= 0 then` on a predicate that already returns `false`
is always taken too. Half a conversion is worse than none, and the replays
may not reach the branch that shows it. Step 2.1 also left
`x ~= nil and x ~= 0` forms on truth-valued fields that may be `nil`
(`is_shown`, `is_revealed`, `satisfied`, `fold_anchor`, `infraspace`).

## Solution

Every truth value of `game/` becomes a Lua boolean: predicates return
`true` or `false`, truth-valued fields hold `true`, `false` or `nil`,
callers test `if f(x) then` / `if not f(x) then`, and `and 1 or 0` is left
only where a truth value is written into a format (a save byte, the state
dump's `visible` column, a cost or cache key). Numbers that are data stay
numbers: counts, colors, bit masks, fixed-point values, divisors, the save
record's flag bytes.

The work is split by module group, bottom-up (runtime, engine, class
callbacks, data lookups, planets, economy, UI, input and save, pad layer),
so that a chunk converts the predicates and fields its group defines
together with every call site of them anywhere in the repository. Each
chunk proves it missed nothing with a temporary static scan and a temporary
run-time instrument, then removes both. The game plays exactly as before:
every `.json.sha256`, every `.end-state`, every save byte unchanged.

## User Stories

1. As a maintainer, I want every predicate to return `true` or `false`, so that I read `if planet_is_family(e) then` instead of `if planet_is_family(e) ~= 0 then`.
2. As a maintainer, I want no predicate left returning `0`, so that a plain `if f(x) then` can never be silently always true.
3. As a maintainer, I want no caller left testing `f(x) ~= 0` or `f(x) == 0` on a converted predicate, so that no branch becomes always taken or never taken.
4. As a maintainer, I want no caller left testing `f(x) == 1` on a converted predicate, so that no branch becomes never taken.
5. As a maintainer, I want every truth-valued field (`is_dead`, `skip_draw_off_screen`, `is_planet`, `is_structure`, `stop`, `pressed`, `released`, `is_hovered`, `is_shown`, `is_revealed`, `satisfied`, module flags) to hold a boolean, so that its tests read as Lua.
6. As a maintainer, I want the `x ~= nil and x ~= 0` forms left by step 2.1 on truth-valued fields to become `if x then`, so that one test states one thing.
7. As a maintainer, I want a lookup's "found" result (`found, value = lookup(...)`) to be a boolean, so that multi-return predicates follow the same rule as single-return ones.
8. As a maintainer, I want a predicate defined in one module and called from fifteen others converted with all its callers in one commit, so that the tree is never half-converted for any one predicate.
9. As a maintainer, I want entity class callbacks (`hit_test`, `has_refresh`, `keep_open`, `takes_slipway`, the `stop` of `left_press` and `accept_drop`) converted with every implementation and every dispatch site at once, so that a dispatcher never tests a boolean against `0`.
10. As a maintainer, I want the runtime's own flags (`p8_palt`'s transparency argument, `fillp_transparent`, `p8_gfx_is_transparent`, `p8_font_has_glyph`) to be booleans, so that the runtime reads like PICO-8's own API.
11. As a maintainer, I want counters that used `n + ((c) and 1 or 0)` to read `if c then n = n + 1 end`, so that no truth value is used as a number.
12. As a maintainer, I want a truth value that goes into a format (a save byte, a state dump column, a cost or cache key) converted to `1`/`0` exactly at that site, so that the format is unchanged and the conversion is visible.
13. As a maintainer, I want the save record's flag fields (`recent_set`, `resource_is_bool`, the probe and need flag bytes, the node flag word) to stay 0/1 numbers, so that the record still mirrors the bytes it encodes.
14. As a maintainer, I want the boundary between the game's booleans and the save record's 0/1 bytes to sit in the save builder and the save loader only, so that each conversion is in one place.
15. As a maintainer, I want `fold_anchor` and `infraspace`, truth values held as the cart's fixed-point `1.0`, to become booleans in the game while the save still writes `0x10000` or `0`, so that the save bytes are unchanged.
16. As a player, I want every replay to play exactly as before, so that the cleanup changes nothing I can see.
17. As a player, I want a save written before this step to load after it and a save written after it to be byte-identical to one written before, so that my progress is safe.
18. As a reviewer, I want each chunk to show `make e2e` green with every `.json.sha256` unchanged before it is committed, so that a wrong conversion is caught in the chunk that made it.
19. As a reviewer, I want each chunk to run the step's two greps restricted to its own files and show that every line left is an exception or a call into a later chunk's predicate, so that progress is measurable.
20. As a reviewer, I want each chunk to prove, with a temporary static scan, that no converted name is compared with `0` or `1`, assigned `0` or `1`, indexed by, concatenated or `tostring`ed anywhere, so that the silent cases the replays may not reach are covered.
21. As a reviewer, I want each chunk to prove, with a temporary instrument over one `make e2e`, that every converted predicate returned a boolean every time it ran, so that a surviving `return 0` is caught.
22. As a reviewer, I want each chunk's commit message to name the converted predicates the replays never reached, each checked by reading, so that the gap in the dynamic proof is explicit.
23. As a reviewer, I want the instrument and the scan never committed, so that `make test` and `make e2e` keep their speed.
24. As a reviewer, I want the last chunk to run the full check over all of `game/` and produce the classified exception list, so that plan-pilot can write the step note from it.
25. As a reviewer, I want every exception listed by kind (bit-mask tests, counts and lengths, divisors, colors and pixel values, fixed-point values, save format fields, dump and key formats), each kind naming its files, and every line outside those kinds named on its own, so that the step note's list is complete and checkable.
26. As a reviewer, I want the `return [01],` multi-return forms checked as well as `return [01]$`, so that the "found" flags are not missed because the plan's grep does not see them.
27. As a maintainer, I want `design.md`'s "Truth values" convention rewritten to state the boolean rule, so that later steps are planned from what is true.
28. As a reviewer running `warn-review`, I want a rule flagging a truth value held as a 0/1 number, so that the C-ism is not reintroduced.
29. As a maintainer, I want the unit tests that froze 0/1 results (`p8_gfx_is_transparent`, `fillp_transparent`, `resource_map_get`'s found flag) updated to expect booleans, so that they test the new contract.
30. As a maintainer, I want the runtime fixture that drives `p8_palt` from scripted numbers to convert its number to a boolean at the call, so that it keeps working under the new runtime contract.
31. As a maintainer, I want every converted predicate to return exactly `true` or `false`, never `nil` and never one of its operands, so that a stored result compares and prints predictably.
32. As a maintainer, I want no test of the form `x == false`, so that a field that is `nil` when unset is never mistaken for a set one.
33. As a developer, I want the end states, which filter the dump's `visible` column, and the dump text itself unchanged, so that `make state` output stays comparable across the step.
34. As a developer, I want `make e2e` to stay under 9 seconds and `make test` not slower, so that the loop stays fast.

## Implementation Decisions

- **What is a truth value.** A number is a truth value when the code only ever tests it against `0` (or `1`) or produces it from a comparison, a predicate or a `1`/`0` literal meaning yes/no. It becomes a boolean. A number is data when it is counted, summed, used as an index, shifted, masked, written as bytes, drawn, or is a count, a length, a color, a fixed-point quantity or a divisor. It stays a number, and its `== 0` / `~= 0` tests stay.
- **Predicate contract.** A converted predicate returns exactly `true` or `false`. A comparison is returned as is (`return a < b`); a presence test is written `x ~= nil`, never `x` alone, when its result is stored or returned. A multi-return lookup returns `found` as a boolean first, its value after, unchanged otherwise.
- **Field contract.** A truth-valued field holds `true` or `false`; a field that step 2.1 made `nil` when unset holds `nil` or a boolean, and `nil` reads as false. Tests are `if x then` and `if not x then`; never `x == false`, never `x == true`. A constructor default of `0` for such a field becomes `false` (or is dropped where `nil` already means unset).
- **Call sites.** `f(x) ~= 0` becomes `f(x)`, `f(x) == 0` becomes `not f(x)`, `f(x) == 1` becomes `f(x)`; `(c) and 1 or 0` used as a truth value becomes `c`. Mind precedence: `not` binds tighter than comparison and `and`/`or`, so `f(a) == 0 and g(b) ~= 0` becomes `not f(a) and g(b)`, and a negated compound gets parentheses. Two 0/1 values compared to each other (`has(node, X) == node_has_state_time(...)`) are converted together, in the same chunk, so both sides are booleans.
- **Counting.** `n = n + ((c) and 1 or 0)` becomes `if c then n = n + 1 end`. A predicate result is never used in arithmetic, a bit operation, a `%d` format, a `..` concatenation, a table index or `tostring`; where the C did that, the site converts explicitly with `(c) and 1 or 0` and is listed as a format exception.
- **Format boundaries (kept as `and 1 or 0`, listed as exceptions).** The save encoder's flag bytes (`probe` present, need resource present, route resource flags, the node flag word); the state dump's `visible %d` column (a dump format; the end states filter it, the dump text stays identical); the draw cost model's pattern key bit for `fillp_transparent`; any cache or row key built from a truth value (`row_image`, `planet_row_signature`), which writes `1`/`0` exactly where the old number went.
- **Save record.** The decoded save record mirrors the bytes: its 0/1 fields stay numbers. The game side is boolean: the weighted picker's `recent_set` becomes booleans (`nil` for an unwritten slot), and the conversion sits only in the save builder (`picker.recent_set[i] and 1 or 0`) and the save loader (`saved.recent_set[i] ~= 0`). The save validators, `save_state_decode`, the reader's `failed`, `save_storage`'s status results and the `seen` tables become booleans; `read_u8` and other readers return numbers and stay.
- **`fold_anchor` and `infraspace`.** These are truth values held as the cart's fixed-point `1.0` (`0x10000`), `0` or `nil`, tested with `~= 0`, never computed with. In the game they become `true` or `nil` (archetype data `= 0x10000` becomes `= true`, constructor defaults `or 0` go). The save builder writes `0x10000` for true and `0` otherwise, for planet nodes and the probe; the loader sets `true` for a nonzero value. Before converting, the chunk runs a temporary instrument over one `make e2e` asserting every value written to or read from a record (replays, the attract demo record) is `0` or `0x10000`; a foreign record holding another nonzero value would now re-encode as `0x10000`, which is accepted and recorded in the commit message.
- **Runtime contract.** `p8_palt(mem, col, transparent)` takes a boolean, as PICO-8's `palt` does; its two game callers pass `true`/`false`. `fillp_transparent` is a boolean read by the software rasterizer, the LÖVE backend's pattern code and the draw cost model (which encodes it into its key as a format). `p8_gfx_is_transparent` and `p8_font_has_glyph` return booleans. `p8_sget`, map cell reads, `p8_fixed_shl`, `p8_fixed_sqrt` return numbers and stay. The runtime fixture scripts `palt` with a number and converts it (`v ~= 0`) at the call.
- **Readiness predicates.** Every `<module>_ready()` (entity, camera, mouse, ui, planet, structure, burst, slipgate, synthesizer, ...) returns a boolean, converted with both consumers (`bootstrap` and the cart's load-time asserts) in the engine chunk.
- **Class callbacks.** `hit_test`, `has_refresh`, `keep_open` and `takes_slipway` return booleans; the `stop` field of a `left_press` / `accept_drop` result is a boolean. Every implementation, every default (`entity_registry_hit_test_always`), every result table literal (`{ stop = 0, held = nil }`) and every dispatcher (UI hover dispatch, `ui_hit_test`, the pad panel's candidate filter, the registry's refresh test, message box `keep_open`) change in one chunk, whatever module the implementation lives in.
- **Lower chunk owns the call sites.** A chunk converts the definitions of its group and rewrites every call site of them in any module, including modules of later chunks; a later-chunk predicate whose expression contains a converted call stays 0/1 until its own chunk (`(planet_is_family(e) and ...) and 1 or 0` is a legal intermediate state).
- **Comments.** A comment that states the 1/0 contract ("Returns 1 for a valid record, 0 otherwise") is rewritten to the boolean contract in the chunk that converts the function; nothing else about comments, headers or names changes (step 3.3, 3.4).
- **Docs and skills (last chunk).** `design.md` § Conventions inherited from the C, paragraph "Truth values", is rewritten to the rule as it now stands: predicates and truth-valued fields are Lua booleans, `nil` reads as false, `0`/`1` only at a format boundary, `(c) and a or b` only when `a` is never `nil` or `false`. The rest of that section waits for step 3.5. `warn-review` gains rule 20: "A truth value held as a `0`/`1` number, or a predicate tested against `0` or `1`." `CLAUDE.md` § The game states nothing about 0/1 and is not changed by this step.
- **No guideline question was needed**: every case above is settled by the phase-0 decision ("Predicates return booleans; `x ~= 0` tests and `and 1 or 0` on truth values go") and the step's check.

## Implementation Chunks

Each chunk, before its commit: the chunk's temporary static scan finds
nothing; the chunk's temporary instrument fires on nothing over one
`make e2e`; both are removed; `make test` green; `make e2e` green with every
`.json.sha256` unchanged (time noted); the step's two greps, plus
`grep -rnE 'return [01],'`, restricted to the chunk's files, leave only
exceptions of the kinds above or calls into a later chunk's predicates,
each named in the commit message.

**The proof, the same in every chunk (never committed):**

- *Static scan.* A throwaway Lua script in the scratchpad, fed the chunk's
  converted names (functions and fields), reads each file of `game/`,
  `tests/lua/` and `tests/fixtures/` whole (so calls spanning lines are
  seen) and reports any `name%b()` or `.name` followed by `==`/`~=` and a
  `0`/`1` literal, any `name = 0`/`= 1` assignment or table field, any
  `and 1 or 0` assigned to a converted field, any converted call or field
  used as a table index, in `..`, in `tostring`, in a `%d` format argument
  or in arithmetic other than at a listed format site, and any
  `return 0`/`return 1` left inside a converted predicate. It must report
  nothing.
- *Run-time instrument.* A temporary `assert(type(v) == "boolean", "<name>")`
  on every return of every converted predicate and on every write of every
  converted field, plus a hit counter per converted predicate, over one
  `make e2e`. It must fire on nothing. Every converted predicate the
  replays never reached is read by hand and named in the commit message.
- *Save bytes (chunks 4, 5, 8).* A temporary hook records the bytes of
  every record encoded over one `make e2e` before the chunk and after it;
  the two sets must be identical. The same seeded set of fuzzed records
  (byte flips of the replay records and the demo record) must get the same
  accept/reject result from `save_state_decode` before and after.

**Chunks, in dependency order:**

1. **Runtime.** `p8/` and `love/`: the draw cost model's on-screen tests
   and its key bit, `p8_palt`'s argument with its game callers,
   `fillp_transparent` with the rasterizer and the LÖVE pattern code,
   `p8_gfx_is_transparent`, `p8_font_has_glyph`, map cell range test,
   `p8_tostr_fixed`'s sign local, the image slot `built` table, the draw
   meter's `building` flag. The runtime unit test and the runtime fixture
   updated.
2. **Engine.** Geometry predicates (`box_contains`, `geom_near_segment`,
   `geom_segments_cross` and their local helpers), `entity_geom`
   (`is_on_screen` with the dump's `visible` format site, `any_within`,
   `contains_cursor_inline`), `entity_class` (`entity_has_pos`,
   `entity_has_map_pos`, `mouse_pos_has_world`, `mouse_pos_has_screen`),
   the registry's locals and flags (`remove_first`, message-name tests,
   tag `seen`, `g_initialized`), every `<module>_ready()` with bootstrap and
   the cart's asserts, the entity header's `is_dead` everywhere, and the
   class flags `skip_draw_off_screen`, `is_planet`, `is_structure` in every
   class table and every reader.
3. **Class callbacks.** `hit_test`, `has_refresh`, `keep_open`,
   `takes_slipway`, the `stop` of `left_press` and `accept_drop`: every
   implementation (planets, structures, icons, slipgates, background,
   buttons, panels), `entity_registry_hit_test_always`, every result table,
   every dispatcher.
4. **Data lookups.** `game_data` (`int_seq_has_fixed`, the fixed-sequence
   and other `found, value` lookups), `map_tables` lookups,
   `economy_formulas.resource_map_get` with its unit test, `trade`
   (`resource_equal`, `trade_equal`, its lookups), `gen`'s weighted picker
   (`recent_contains`, `has_drawable_ticket`, `recent_set` on the game side
   with the save builder and loader lines), `bindings`' `mode_actions`
   table.
5. **Planets, slipways, wormholes.** `planet` (`planet_is_family`,
   `planet_is_colonized`, the rest), `planet_family` (`bobs` with its dump
   caller), `planet_family_trade`, `planet_trade` (`contains_trade`, offer
   results), `planet_display` (row signature equality and its key
   format), `slipway` (`served_planets_has`, obstruction predicates,
   `copy_infraspace`), `slipway_display`, `wormhole`, `particle`, `galaxy`,
   and `fold_anchor` / `infraspace` with the save builder and loader
   (value instrument first, then save-bytes proof).
6. **Economy, research, targeting.** `economy` (`economy_spend`),
   `economy_score` (upkeep and colony predicates), `economy_tiers`,
   `research`, `research_display` (available projects, ready test,
   `found, keys` returns), `tech_gate` (`tech_gate_is_unlocked`, available
   technologies), `structure_icon` (`is_revealed`), `probe` and targeting
   (`targeting_confirm`, track predicates, panel box test), `score_panel`,
   `score_panel_display` (`is_shown`, the toggle, `keep_open` counter
   stays a number), `hud`.
7. **UI.** `ui` (`pressed`, `released`, `pad_crosshair`, hover results),
   `ui_button` (`is_hovered`, image width/draw `found, extent` returns),
   `button_image`, `mouse`, `mouse_button`, `message_box` (toast update),
   `menu` (`pause_menu_shown`), `layout`, `text_image` (`cacheable`),
   `row_image` (key equality and key format), `draw_functions`, `help`,
   `background`, `scroll`, `attract`, `demo_overlay`, `loading_screen`.
8. **Input, cart and save.** `bindings` (`bindings_parse`, `bindings_mask`
   as a mask stays a number, parse helpers' status), `cheat`
   (`cheat_step`), `input_step`, `cart` (`cart_loading`,
   `cart_title_shown`, `save_pending`, title and loading flags), `main`
   (`bindings_parse` caller), `save_state` (validators, `is_flag`, `has`,
   `save_state_decode`, reader `failed`, `seen` tables; record fields and
   byte writes stay), `save_load`, `save_storage` (status results; the
   byte count stays a number), the state dump's remaining sites.
9. **Pad layer, docs, full check.** `pad_nav`, `pad_panel`, `pad_select`,
   `pad_jump`, `pad_slipway`, `pad_target`, `pad_build`, `pad_cycle`,
   `pad_back`, `pad_direction`: every predicate and flag left. Then
   `design.md`'s "Truth values" paragraph and `warn-review` rule 20. Then
   the full check over all of `game/`, `make test-all` and
   `make e2e end-state`; the commit message carries the classified
   exception list for the step note.

## Testing Decisions

- The 20 e2e replays are the net: every `.json.sha256` and `.end-state`
  unchanged, `make e2e` green after every chunk. A good test here checks
  what the player or the save sees, not how a predicate is spelled.
- No new permanent unit test: truth values reach no capture, save byte or
  end state, and a test of "returns a boolean" would freeze spelling. The
  proof of completeness is the per-chunk static scan and run-time
  instrument above, both temporary.
- Existing tests that froze 0/1 are updated to expect booleans, in the
  chunk that converts the function: the runtime test
  (`p8_gfx_is_transparent`, `fillp_transparent`), the economy formulas test
  (`resource_map_get`'s found flag). The runtime fixture converts its
  scripted number for `p8_palt`.
- Prior art: step 2.1's per-chunk instruments over one `make e2e`, step
  2.1's save-bytes and fuzzed-decoder comparison, step 1.4's walk check.
- `lua-guard` and `lua-test-modules` run unchanged in `make test`.

## Out of Scope

- 1-based arrays and every `[0]` / 0-based loop (step 3.2): the 0-based
  `seen`, `recent_set` and data tables keep their indexing; only their
  values change.
- The `-- Copy of src/...` headers and comments about the C (step 3.3),
  except a comment whose stated 1/0 contract this step makes false.
- Module-prefix renames (step 3.4) and the rest of `design.md` § Conventions
  inherited from the C (step 3.5).
- The save format: no byte, field width or validation rule changes.
- Bit sets that are not truth values (`SAVE_NODE_*`, `MOUSE_POS_HAS_*`,
  class `tags`, cache-key bits): their `bit.band(...) ~= 0` tests stay.
- Audio, new features.

## Further Notes

- The plan's `return [01]$` grep does not see `return 0, out`; this PRD
  checks `return [01],` too, about 35 lines today.
- Mixed state between chunks is legal and expected; what is never legal is
  a converted name still compared with `0`/`1` anywhere, which the static
  scan forbids repository-wide.
- Silent failure modes the scan targets, because the replays may miss them:
  a surviving `return 0` (truthy), `bool ~= 0` (always true), `bool == 0`
  or `bool == 1` (always false), a boolean as a table index (`nil` lookup),
  `x == false` on a `nil`-able field. Loud ones (arithmetic, bit ops,
  `%d`, `..`, `<` on a boolean) raise an error in the replay that reaches
  them.
- `CLAUDE.md` § The game still says optional parts are "flagged by bits in
  `has`", which step 2.1 made false; not this step's change, reported here
  for plan-pilot.
