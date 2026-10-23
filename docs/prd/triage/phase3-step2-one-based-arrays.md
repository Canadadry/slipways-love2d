---
title: "1-based arrays: every array of game/ indexed from 1"
description: "The data tables, the registry's layer orders, the save record and buffers and the other C arrays of game/ are still indexed from 0; make every array 1-based without changing a hash, an end state or a save byte, keeping as 0-keyed maps only tables whose key is a value."
status: needs-triage
---

## Problem Statement

Plan 5, phase 3, step 2 (1-based arrays), verbatim from `docs/love-port/plan-5.md`:

> 2. 1-based arrays. Seen when
>    `grep -rE '\[0\]|for [a-z_]+ = 0,|\[[a-z_.]+ *\+ *1\]|while [a-z_]+ < |\[[a-z_.]*count\] *=' game/`
>    finds only plain numbers or genuine element shifts, not 0-based indices,
>    each listed in the step note. An array with holes keeps an explicit count
>    or index loop; never `#` or `ipairs` on it.

Step 1.4 made the four list kinds plain 1-based arrays. Everything else the
C indexed from 0 is still indexed from 0: the `{ items, count }` data rows and
integer sequences of `game_data` and `map_tables` (about 200 table literals
starting `[0] =`), the lookup tables (planet types, technologies, resource
codes, project records, difficulty options, menus, score rows), the
registry's message buckets and layer orders, the layout's tuples, keys and
placed items, the message box's buttons and articles, the score panel's
readouts, the research and tech-gate result lists, the bindings masks, the
weighted picker's tickets and recent ring, the runtime's point, segment and
input-frame lists, and the save module's byte buffers and record arrays.
At HEAD `17493c5` (before step 3.1) the step's grep finds 600 lines in 61
files; the four largest are `game_data` (101), `map_tables` (100),
`save_state` (64) and `p8/gfx_software` (62).

For a maintainer this is a trap on every boundary. Half the game is 1-based
(Lua lists, strings, the cart's own `int_seq_at(seq, count, i)` with `i`
from 1, `research_projects_at(k)`, the save's `planet_type` and
`difficulty`) and half is 0-based, so every crossing is a `- 1` or `+ 1`
that nothing names (`seq[i - 1]`, `planet_types_table[node.planet_type - 1]`,
`galaxy_menu_table[i - 1]`, `period_names_table[lo - 1]`), and `#t` or
`ipairs` on a 0-based table silently drops the first element. A wrong
shift is silent in Lua: `t[0]` on a 1-based array reads `nil`, `t[count]`
reads the last element instead of failing, and the replays may not reach the
branch that shows it.

## Solution

Every array of `game/` — a table whose integer key only says "the k-th
element" — is indexed from 1: its literal has no `[0] =`, its loops run
`for i = 1, count`, its appends write position `count + 1`, every constant
index moves up by one, and every crossing `- 1`/`+ 1` between a 1-based
index and a 0-based table disappears. Tables whose key is a *value* that
means something outside the table, where 0 is a real value (a colour, a
pixel coordinate, a byte, a memory address, a bit position, a layer number,
a save-format code, an image slot handle), are maps: they keep their keys
and are listed as exceptions with their reason.

The save format does not move: every rank written on disk keeps the value
the format defines (0-based node, slipway, technology and ticket ranks;
1-based `planet_type`, `description_type`, `project`, `difficulty`). The
record keeps mirroring the bytes; the conversion between a disk rank and a
Lua position sits in the save builder and the save loader only, one visible
`rank + 1` or `position - 1` per site.

The work is split into ten chunks by module group (a table's definition with
all its readers and writers anywhere), bottom-up. Each chunk proves no read
is off by one with a temporary zero-key trap over one `make e2e`, a
temporary static census, and for data files a load-time comparison with the
HEAD version of the table; the save chunk also proves byte-identical saves.
The game plays exactly as before: every `.json.sha256`, every `.end-state`,
every save byte unchanged.

## User Stories

1. As a maintainer, I want every array of `game/` indexed from 1, so that `#`, `ipairs`, `table.insert` and the cart's 1-based indices all agree with the tables they touch.
2. As a maintainer, I want `int_seq_at(seq, count, i)` to read `seq[i]`, so that the cart's 1-based index is the table's index with no hidden `- 1`.
3. As a maintainer, I want the tier tables, layer fades, period names, trail colours, colour ramps, link sprites, ring colours and accepted-resource sequences shifted, so that every sequence the cart reads from 1 is stored from 1.
4. As a maintainer, I want `planet_types_table`, `difficulty_options_table`, `technologies_table`, `resource_codes`, `resource_names`, `production_tables`, `project_records_table`, `icon_tooltips_table`, the menu tables and the score rows 1-based, so that a lookup loop is `for i = 1, COUNT`.
5. As a maintainer, I want every data row's `items` 1-based, so that a row built in a data file and a row built at run time (menu, message box, score panel, research display) read the same way.
6. As a maintainer, I want every `{ items, count }` or `{ x, x_count }` table to keep its count where elements past it may be stale or absent, so that no reader trusts `#` on it.
7. As a maintainer, I want one append idiom for a counted array that names the new position, so that an append never writes position `count` of a 1-based array by habit.
8. As a maintainer, I want every constant index into a shifted array moved up by one (`readouts[5]`, `inner_items[0..2]`, `buttons[0]`, `speed[0..3]`, `words[0..3]`), so that no fixed slot reads its neighbour.
9. As a maintainer, I want the registry's message enum numbered from 1 and its message buckets a plain array, so that the bucket table needs no `[0]`.
10. As a maintainer, I want the registry's two layer orders a pair `{ draw, overlay }` indexed 1 and 2, their items 1-based and the layer view's `first` a 1-based position, so that `draw_layers` walks `items[first .. first + count - 1]`.
11. As a maintainer, I want the layer order's `start` prefix sums kept keyed by layer value 0..15, so that `start[0] = 0` stays the "below layer 1" sentinel and the layer number keeps its meaning.
12. As a maintainer, I want the tag buckets kept keyed by tag number, so that the bucket of `TAG(t)`'s bit position stays `t`.
13. As a maintainer, I want the bindings modes and actions numbered from 1 and the masks, names and mode-action tables plain arrays, so that the default bindings table has no `[0] =` in every row.
14. As a maintainer, I want the pad's direction tables and speeds 1-based, so that the pan arithmetic reads its four directions by position 1 to 4.
15. As a maintainer, I want the runtime's point and segment lists (`p8_psets`, `p8_lines`, the draw meter, the software rasterizer, the LÖVE backend) 1-based, so that a game module hands the runtime a plain Lua list.
16. As a maintainer, I want the runtime's input frames 1-based with a 1-based cursor, so that `main` and `input_step` build `{ frames = { frame }, frame_count = 1, current_frame = 1 }`.
17. As a maintainer, I want the colour-, coordinate-, byte- and address-keyed maps of the runtime (palette, draw palette, screen, sprite sheet, map cells, quarter wave, packed sprite sheet, glyph rows, CLUT colours) kept as they are, so that PICO-8's colour 0 and pixel (0, 0) stay real keys.
18. As a maintainer, I want image slot numbers kept as the runtime's 0-based handles, so that the fatal messages, the runtime test and the runtime fixture keep naming the same slots.
19. As a maintainer, I want the game's own slot caches (row, text and button images) 1-based, each computing its slot as `FIRST + i - 1`, so that a cache index and a slot handle are never confused.
20. As a maintainer, I want the weighted picker's tickets 1-based and its draw to read `tickets[idx]` with the cart's 1-based `idx`, so that the `- 1` of the draw goes.
21. As a maintainer, I want the picker's recent ring written at `3 - (count % 3)` directly, so that the ring's `% 3` arithmetic yields a 1-based slot with no further shift.
22. As a maintainer, I want the save module's byte buffers 1-based like Lua strings, so that `save_storage` converts with `string.char(bytes[i])` and `string.byte(content, i)` at the same `i`.
23. As a maintainer, I want the save cursor to hold the position of the next byte, so that the record length is `position - 1` and the format's offsets (`LENGTH_OFFSET`, `HEADER_BYTES`) stay the format's numbers.
24. As a maintainer, I want the save record's arrays 1-based and its slice starts (`first_route`, `first_slipway`, `first_planet`) 1-based positions, so that a slice is `pool[first .. first + count - 1]`.
25. As a player, I want every save byte unchanged, so that a save written before this step loads after it and one written after is identical to one written before.
26. As a maintainer, I want every rank on disk (node, slipway, technology, ticket, recent) converted to a position only in the save loader and every position to a rank only in the save builder, so that each conversion is in one visible place.
27. As a maintainer, I want the save validators to compare ranks with ranks (a slipway's `start` with the node's rank, not its position), so that no validation changes meaning.
28. As a maintainer, I want `docs/save-format.md` to say that buffers and record arrays are 1-based and that every rank on disk is unchanged, so that the format page stays true.
29. As a maintainer, I want the research tables (project table, project order, projects by name) and the research and tech-gate result lists 1-based, so that `research_projects_at(k)` reads `g_projects[k]`.
30. As a maintainer, I want the tech gate's reverse map and affordable flags indexed by the technology's 1-based position, so that one technology has one number in the game.
31. As a maintainer, I want the resource map's entries 1-based with its unit test updated, so that the economy formulas test the new contract.
32. As a maintainer, I want the message box's buttons argument a 1-based array with its count, converted with every caller (pause menu, galaxy menu, tech tree, help, planet panels, research panels, score panel), so that no caller passes `{ [0] = button }`.
33. As a maintainer, I want the galaxy menu's sparse refresh table moved to position 2 and kept read by index, never by `#` or `ipairs`, so that the hole at position 1 stays harmless.
34. As a maintainer, I want every array with holes or a stale tail named in its chunk's commit with how it is walked, so that a later `#` on it is seen as a bug.
35. As a maintainer, I want every `% n` that feeds an index listed and checked to land in `1 .. n`, so that a ring or wrap never reads position 0.
36. As a maintainer, I want enum constants that are values left as they are (`ENTITY_STATE_*`, saved; `TAG_*`, bit positions; `SAVE_NODE_*`, disk codes; `DRAW_FUNCTION_*`, 0 meaning none; layout kinds; `PAD_DIRECTION_*`, already 1 to 4), so that only enums that exist to index a table move.
37. As a maintainer, I want pure count loops (`for _ = 0, n - 1`) rewritten `for _ = 1, n`, so that the step's exception list holds only lines that need a reason.
38. As a maintainer, I want every comment stating a 0-based contract ("indexed from 0", "0-indexed", "[0] is written whatever...") rewritten in the chunk that makes it false, so that comments never lie about an index.
39. As a reviewer, I want each chunk to show `make e2e` green with every `.json.sha256` unchanged before it is committed, so that a wrong shift is caught in the chunk that made it.
40. As a reviewer, I want each chunk's zero-key trap to raise on any read or write of key 0 or below on a shifted array over one `make e2e`, so that a surviving 0-based access fails loudly instead of reading `nil`.
41. As a reviewer, I want each chunk to count reads past an array's count, before and after the chunk, so that a read that moved from the last element to `nil` (or the reverse) is seen.
42. As a reviewer, I want every shifted data table compared at load with its HEAD version (`new[i + 1]` equal to `old[i]`, same count, no key 0), so that a hand edit of a long literal drops or doubles nothing.
43. As a reviewer, I want a static census of every index expression on each converted table name, so that the readers the replays never reach are checked by reading.
44. As a reviewer, I want the instruments and scripts never committed, so that `make test` and `make e2e` keep their speed.
45. As a reviewer, I want the last chunk to run the step's grep over all of `game/` and produce the classified exception list, so that plan-pilot can write the step note from it.
46. As a reviewer running `warn-review`, I want a rule flagging a 0-based array, so that the C-ism is not reintroduced.
47. As a maintainer, I want `design.md`'s "Arrays and integers" paragraph to state the 1-based rule and the value-keyed exceptions, so that later steps are planned from what is true.
48. As a developer, I want `make e2e` under 9 seconds and `make test` no slower, so that the loop stays fast.

## Implementation Decisions

### What is an array, what is a map

- **The rule.** A table indexed by integers is an *array* when the key only
  says "the k-th element": its elements are a sequence, a list, a cache's
  slots, a ring, a record's pool, or rows of a data file read by position
  (including the cart's 1-based index). It is shifted to start at 1.
  It is a *map keyed by a value* when the key is a number with its own
  meaning, used outside indexing this table, and 0 is a real value of it. It
  stays, and is listed in the step note with its kind.
- **Maps that stay (decided here, each listed in the step note):**
  - Runtime, keyed by colour: `p8.palette`, `mem.draw_palette`, the draw
    meter's CLUT entries' inner `[c]`, the palette loops of `love.screen`
    and `love.sheet` (`bytes[c + 1] = mem.draw_palette[c]` is a colour key
    turned into a LÖVE byte position: an exception of its own).
  - Runtime, keyed by pixel coordinate or offset: `mem.screen[y][x]`,
    `mem.sprite_sheet[y][x]`, `mem.map_cells[y][x]`, the glyph rows
    `FONT_ROWS_*` (row offset within a glyph), every pixel loop of
    `p8/vram`, `p8/sprite_sheet`, `p8/gfx_software`, `love/slot`,
    `love/pattern`, `love/text`, `love/sheet`, and the 32×32 tile loops of
    `background`.
  - Runtime, keyed by address or angle: `p8.sprite_sheet_packed` (byte
    address), `p8.quarter_wave` (angle step 0 to 0x4000).
  - Image slot handles: `SLOT_SIZES` and the `built` tables of `p8/image`,
    `p8/draw_meter`, the per-slot offsets of `p8/gfx_software`, the LÖVE
    backend's slot table, and `image_slot`'s constants (`IMAGE_SLOT_BACKGROUND
    = 0`). A slot number is the runtime API's handle, named in fatals
    (`"image slot 88: no such slot"`), in `tests/lua/test_runtime.lua` and in
    the runtime fixture's scripts; it stays 0-based like a PICO-8 sprite
    number.
  - Game, keyed by value: `hud.palette_blocks[block][colour]` (the cart's
    palette block number and a colour), `save_state.crc32_table` (byte
    value), `save_load.node_constructors` and `save_state`'s `node_kind`
    classes (`SAVE_NODE_*`, a disk code), the class `states` tables
    (`ENTITY_STATE_*`, saved), the registry's tag buckets (`TAG_*`, a bit
    position of `TAG(t)`), the layer order's `start` (layer value 0 to 15,
    `start[0]` the empty prefix), `ui`'s `hovered_by_layer.layers` (an
    entity's floored layer, also a bit position of `written`),
    `draw_functions`' dispatch table (`DRAW_FUNCTION_*`, no 0 entry), the
    save validators' `seen` sets (keyed by a disk rank), and the attract
    demo's `demo_data_pad_buttons` (keyed by the demo's frame clock,
    compared with the captions' `from` and `to` frame numbers).
- **Enums.** An enum that exists only to index tables is renumbered from 1:
  `ENTITY_MESSAGE_*` (1 to 5, `ENTITY_MESSAGE_COUNT` stays 5),
  `BINDINGS_MODE_*` (1 to 9), `BINDINGS_ACTION_*` (1 to 35). Their loops
  keep the same order (every value moves by one), so bucket order, binding
  order and parse order are unchanged; no enum value reaches a capture, a
  dump, an end state or `bindings.cfg` (which names modes and actions). An
  enum that is a value is not renumbered (story 36). The range asserts move
  with the renumbered enums (`mode >= 1 and mode <= BINDINGS_MODE_COUNT`).
- **Plain numbers.** A loop whose variable is a coordinate, colour, offset
  or pure counter is a plain number. Pure counters (`for _ = 0, n - 1`, and
  `for i = 0, n - 1` whose `i` is never used as an index) become
  `for _ = 1, n`; coordinate and colour loops stay and are listed by kind.
  `wormhole_display`'s `for dt = 0, 8, 2`, `p8/fixed`'s `isqrt_u64` bisection
  and `bindings`' string-position `while` loops (1-based already) are plain
  numbers.

### Contracts after the step

- **Counted arrays.** Every table with its own count (`{ items, count }`,
  `tickets_count`, `placed_items_count`, `rows_count`, `articles_count`,
  `needs_count`, the save record's pools) keeps the count field: elements
  past the count may be stale (reused batches, removed tickets, copied rows)
  or absent, so `#` and `ipairs` are never used on it. Reads and walks stop
  at the count: `for i = 1, count`, a scan is `while i <= count`.
- **Append.** A counted append names its new position:
  `local n = list.count + 1; list.items[n] = x; list.count = n`. The two 0-based
  forms (`t[count] = x` then increment, and `t[count + 1]` written inline)
  are what the step's grep looks for; the plain lists of step 1.4 keep
  `t[#t + 1] = x`.
- **Removal.** A removal from a counted array is a genuine element shift
  (`for j = i, count - 1 do t[j] = t[j + 1] end`), then the vacated last
  position is set to `nil` and the count decremented; the old code left a
  stale copy there, which no read below the count ever saw (the past-count
  counter of chunk 10 checks the picker; chunk 2 the layer order). An
  insertion shifts up (`for j = count, index, -1 do t[j + 1] = t[j] end`).
  Both are listed as genuine shifts.
- **Neighbour reads.** An already 1-based read of the next element
  (`main`'s `args[i + 1]`, `economy_tiers`' `planets[index + 1]` of a route,
  the element shifts above) is a genuine element access and listed.
- **Cart indices.** Every `- 1` that turned the cart's 1-based index into a
  0-based table index goes: `int_seq_at` and `game_data_fixed_seq_at` read
  `seq[i]`; `galaxy_menu_table[i]`, `g_galaxy_menu_pos[i]`,
  `galaxy_menu_on_click_table[i]`, `galaxy_menu_refresh_table[i]`,
  `period_names_table[lo]`, `planet_mood_rows()[mi]`,
  `countdown_rows.items[countdown_index]`, `production_table.values[clamped_idx]`,
  `draw_functions_arg`'s `e.items[i]`, `research_projects_at`'s `g_projects[k]`,
  the picker's `tickets[idx]`. The `[x - 1]` form is not in the plan's grep:
  each chunk's census searches it (see the proof).
- **`% n`.** Every modulo whose result feeds an index is listed in its
  chunk's commit and gets a temporary range assert for one `make e2e`. The
  picker's ring slot `3 - cint.mod(tickets_count, 3)` is already in `1..3`
  and indexes `recent[slot]` directly. `pad_select`'s
  `cint.mod(start + step, #candidates) + 1` is already 1-based and stays.
- **Sparse arrays.** `menu.galaxy_menu_refresh_table` (one entry, today at
  `[1]`, becomes `[2]`) and `galaxy_menu_on_click_table` (filled in two
  steps, entry 4 after its function is defined) are read by position only.
  The picker's `recent` and `recent_set` have holes until the ring fills
  (H-3.1-2): walked by `for i = 1, 3`, never `#`.
- **Registry.** `ENTITY_MESSAGE_DRAW = 1` ... `ENTITY_MESSAGE_REFRESH = 5`;
  `g_entities_by_message[1..5]`; `g_layer_orders = { draw_order,
  overlay_order }`, `layer_order_of` returns `[1]` for draw, `[2]` for
  overlay; each order's `items` 1-based, `start[g]` the number of members of
  layers `≤ g` (keyed 0 to 15, unchanged meaning), so layer `g` is
  `items[start[g - 1] + 1 .. start[g]]`. `entity_registry_layer` returns
  `{ items, first = start[layer - 1] + 1, count }` and `draw_layers` walks
  `for index = group.first, group.first + group.count - 1`. Insertion at
  rank `r` of layer `key` is position `start[key - 1] + r + 1`.
- **Runtime lists.** `p8_psets(gfx, mem, points, count)` and
  `p8_lines(gfx, mem, segments, count, col)` read `1..count`; every producer
  (particles' batched points, the draw meter, the LÖVE backend, the software
  rasterizer, any game module the chunk's census finds) writes from 1.
  `p8_input_t.frames` is 1-based and `current_frame` the position of the
  current frame (starts at 1; the advance asserts `current_frame <
  frame_count` before incrementing; the read refuses `current_frame >
  frame_count`). The draw meter's CLUT and pattern caches are positional
  (`cluts[i]`, `pattern_keys[i]` for `i = 1..frame.cluts`), their inner
  colour key stays.
- **Data tables.** Every table literal of `game_data`, `map_tables`,
  `hud` (outer and inner kept, see maps), `bindings_default`, `demo_data`
  (captions and record; pad buttons kept, see maps), `pad_jump`, `pad_nav`,
  `menu`, `planet_display`, `research_display`, `score_panel`,
  `score_panel_display`, `planet_family` (`planet_kinds`) and `cart`
  (`kind_table`) drops its `[0] =`. Sizes (`*_COUNT`) are unchanged. The
  production tables' inner `values` (four entries, the fourth a 0 filler for
  three-tier tables) shift with them.
- **Save.** `save_state`'s byte buffers are 1-based: a writer's and reader's
  `position` is the position of the next byte (starts at 1, or at
  `LENGTH_OFFSET + 1` for the length cursor, `count - TRAILER_BYTES + 1` for
  the trailer, `MAGIC_LENGTH + 1` after the magic); a record's byte count is
  `position - 1`; `write_u8` asserts `position >= 1 and position <=
  SAVE_STATE_MAX_BYTES`; `read_u8` fails when `position > end_`, `end_`
  being the position of the last readable byte. `save_state_crc32(bytes,
  count)` reads `bytes[1..count]`. `magic`, the record arrays (`nodes`,
  `slipways`, `routes`, `route_indices`, `node_slipways`, each node's
  `needs`, `economy.invented`, the picker's `tickets`, `recent`,
  `recent_set`) and the capture's `ids.nodes` / `ids.slipways` are 1-based;
  `first_route`, `first_slipway`, `first_planet` are positions. Every value
  the bytes hold stays the format's value, so the record still mirrors the
  bytes: `route_indices`, `node_slipways`, a slipway's `start` and `end_`,
  `invented`, `tickets`, `recent` hold 0-based ranks; `planet_type`,
  `description_type`, `project`, `difficulty` hold their 1-based values,
  which after the shift equal the Lua table positions. The builder writes
  `position - 1` (`index_of` returns a rank; `technology_index`,
  `planet_type_index` return positions, the ticket and recent fields store
  `position - 1`); the loader reads `rank + 1` (`node_ids[start + 1]`,
  `slipway_ids[rank + 1]`, `technologies_table[rank + 1]`,
  `planet_types_table[rank + 1]`) and `planet_types_table[node.planet_type]`,
  `difficulty_options_table[state.difficulty]`. Validators compare ranks
  with ranks: `node_slipways_are_valid` walks node position `i` and tests
  `start ~= i - 1 and end_ ~= i - 1`; bounds stay `rank >= count` tests on
  ranks. `save_load`'s `node_ids` and `slipway_ids` become 1-based by node
  and slipway position. `save_storage` converts `chars[i] =
  string.char(bytes[i])` and `bytes[i] = string.byte(content, i)`; the cart's
  `g_save_bytes` and `demo_data_record` are 1-based. Each `rank + 1` /
  `position - 1` site is listed in the step note as a save-rank conversion.
- **Docs and skills (last chunk).** `design.md` § Conventions inherited from
  the C, paragraph "Arrays and integers": the sentences "A C array keeps its
  C index, from 0 (`{ [0] = 1, 0, 1, 2 }`)" and "the data tables shaped
  `{ items, count }` stay 0-based until the 1-based step" are replaced by the
  rule as it now stands (every array 1-based; a counted array keeps its
  count and is walked to it; the maps keyed by a value, by kind; disk ranks
  converted only in the save builder and loader), and "or 0-indexed array of
  tables" in "Structs and out-parameters" becomes "array of tables"; the rest
  of the section waits for step 3.5. `docs/save-format.md`: line "A byte
  buffer is a 0-indexed array" becomes 1-based; "Every array of the table is
  0-indexed" becomes 1-based with slice starts as positions; the invented
  tech and ticket rows say "the 0-based rank of a `technologies_table` /
  `planet_types_table` entry (its position − 1)". `warn-review` gains rule
  21 (H-3.1-6): "An array indexed from 0, or a `[0]` key that is a position
  rather than a value." `CLAUDE.md` § The game says nothing about array
  bases and is not changed by this step.
- **Comments.** A comment that states a 0-based contract is rewritten in the
  chunk that converts its table (module shape headers "indexed from 0",
  "0-indexed", `score_panel`'s "[0] is written ... whatever the copied row's
  count", `game_data`'s "seq is the 0-indexed array, i 1-based as in the
  cart", `draw_functions`' "0-indexed by the DRAW_FUNCTION_* constants",
  `hud`'s "palette_blocks is 0-indexed", `p8/gfx`'s point and segment list
  shapes, `p8/input`'s frames). Nothing else about comments, headers or
  names changes (steps 3.3, 3.4).
- **Lower chunk owns the call sites.** A chunk converts the definitions of
  its group and every index expression on them in any module, including
  modules of later chunks; a line touching two tables of two chunks changes
  only its own side (`tuple.scalars[i] = tuple_scalar_of_item(row.items[i])`
  is legal with only one side shifted, its loop rewritten when the second
  chunk lands).
- **Code from step 3.1.** This PRD was written while step 3.1 was being
  implemented, from HEAD `17493c5`; every assumption about 3.1's result is a
  hypothesis below. The implementer reads the committed code after 3.1
  first and, if a hypothesis is false, follows the rule above against what
  is actually there; a hypothesis whose failure makes a decision here
  impossible stops the run (plan § Rules).
- **No guideline question was needed**: the phase-0 decision ("Every array
  is 1-based, data tables included"), the step's check and the save format
  settle every case above.

### Hypotheses about step 3.1 (not done when this PRD was written)

- **H-3.1-1.** Step 3.1 is committed before this step starts, with
  `make test-all` green and every `.json.sha256` and `.end-state`
  unchanged; it changed no table's indexing (its PRD keeps the 0-based
  `seen`, `recent_set` and data tables for this step).
- **H-3.1-2.** The picker's game-side `recent_set` holds `true` or `nil`
  (`nil` for a slot never written), so `recent_set` and `recent` have holes
  until the ring fills; the save record's `recent_set` stays 0/1 numbers and
  the builder and loader convert at their line.
- **H-3.1-3.** The save validators' `seen` tables hold booleans keyed by a
  disk rank; they stay maps keyed by that value.
- **H-3.1-4.** `int_seq_at`, `game_data_fixed_seq_at`, `resource_map_get`
  and the other lookups return a boolean `found` first and their value
  after; this step changes only the index they read.
- **H-3.1-5.** `bindings.mode_actions` rows hold booleans with every action
  present and `bindings_mode_has_action` returns a boolean; this step only
  renumbers their keys. `tech_gate`'s affordable flags and `p8/image`'s
  `built` table hold booleans, at the same keys as today.
- **H-3.1-6.** Step 3.1 added `warn-review` rule 20 (truth values); this
  step's rule is 21. If rule 20 is absent, this step's rule takes the next
  free number.
- **H-3.1-7.** Step 3.1 rewrote only the "Truth values" paragraph of
  `design.md` § Conventions inherited from the C; "Arrays and integers"
  still says the data tables stay 0-based until this step.
- **H-3.1-8.** Lines of the step's grep that 3.1 rewrote (`while i <
  node.need_count and reader.failed == 0`, `is_flag` tests, `== 0` tests on
  predicate results inside 0-based loops) keep their index shape; the line
  counts quoted here (600 lines, 61 files at `17493c5`) are re-measured at
  the start of chunk 1, and each chunk's census works from the code as it
  is then.
- **H-3.1-9.** `save_state_decode` and `save_storage`'s status results are
  booleans; the byte count and every reader's value stay numbers, so the
  save-bytes proof of chunk 10 compares the same encoder output.

## Implementation Chunks

Each chunk, before its commit: the chunk's zero-key trap fires on nothing
over one `make e2e` and its past-count counts match the counts taken on the
code before the chunk; its census reports nothing unclassified; the data
tables it shifts pass the load-time comparison; all three are removed;
`make test` green; `make e2e` green with every `.json.sha256` unchanged
(time noted); the step's grep restricted to the chunk's files leaves only
lines classified as a map kept, a plain number, a genuine element shift or
neighbour read, a save-rank conversion, or an index into a later chunk's
table, each named in the commit message.

**The proof, the same in every chunk (never committed):**

- *Zero-key trap.* A temporary `setmetatable` on every shifted array where
  it is created (a data table at load, a run-time array at its constructor),
  whose `__index` raises `"0-based read <name>[k]"` for any integer key
  `k <= 0` and whose `__newindex` raises for any integer key `k <= 0`.
  `__index` runs only for absent keys, so it costs nothing on correct reads
  and catches the silent case (`t[0]` reading `nil`, `t[count]` written
  where `t[count + 1]` was meant leaves key 0 unwritten and then read). It
  must fire on nothing over one `make e2e`.
- *Past-count counter.* The same `__index` counts, per table, reads of an
  absent key above its count. The counts over one `make e2e` are taken on
  the code before the chunk (on the 0-based table, keys `>= count`) and
  after it (keys `> count`); they must be equal per table, and every
  nonzero one is read by hand and named (a scan that stops on `nil` by
  design, such as `planet_kinds[COUNT] == nil` or `args[i] ~= nil`).
- *Load-time comparison (data tables).* A scratchpad script loads the HEAD
  version of each data module (`git show HEAD:<path>` into the scratchpad,
  required under another name) next to the new one and asserts, for every
  shifted table, `new[0] == nil`, the same count, and `new[i + 1]`
  deep-equal to `old[i]` for every `i` (rows and sequences recursively,
  nested `items`, `values` and `words` included; value-keyed inner maps
  compared key for key).
- *Census.* A scratchpad Lua script fed the chunk's converted table and
  field names reads each file of `game/`, `tests/lua/` and `tests/fixtures/`
  whole and reports, for each name: every index expression on it; every
  `[... - 1]`, `[... + 1]`, `[0]` and constant index; every numeric `for`
  starting at 0 and every `while x < count` walking it; every `#name`,
  `ipairs(name)`, `table.insert`, `table.remove` and `unpack` on a counted
  or holed array; every `cint.mod` or `%` whose result reaches an index.
  Each line is classified by hand; an unclassified line blocks the commit.
- *Range asserts.* Every listed `% n` index site gets a temporary assert
  that its result is in `1 .. n` over the same `make e2e`.

**Chunks, in dependency order:**

1. **Runtime lists.** `p8/` and `love/`: point and segment lists of
   `p8_psets` and `p8_lines` with the software rasterizer, the draw meter,
   the LÖVE backend and every game producer (particles' batch, any other
   found by the census); `p8_input_t` frames and cursor with `input_step`
   and `main`; the draw meter's CLUT and pattern caches. The colour,
   coordinate, address, angle and slot-handle maps of the runtime are kept
   and listed. `tests/lua/test_runtime.lua`'s literal point and segment
   lists become 1-based; the runtime fixture builds its `points` and
   `segments` from 1 (its own decoded call arguments `v[...]` are not
   `game/` and stay).
2. **Registry.** `engine/entity_registry`: `ENTITY_MESSAGE_*` from 1, the
   message buckets, the layer-order pair, each order's items, the layer view
   and `draw_layers`, insertion and removal shifts; `start` and the tag
   buckets kept. The past-count counter covers the orders' items.
3. **Game data sequences and lookups.** `game_data`: every `int_seq_t`'s
   items (tier tables, layer fades, trail colours, colour ramps, link
   sprites, ring colours, made, accepted and synthesizable resources,
   background tile choices) with `int_seq_at`, `game_data_fixed_seq_at`,
   `int_seq_has_fixed` and every caller; `period_names_table`;
   `planet_types_table` with `planet_types_lookup`, `gen`'s fill loop and
   `save_state`'s `planet_type_index_of` and `description_type_index` and
   `save_load`'s planet-type reads (disk values unchanged: positions are now
   the 1-based `planet_type`, tickets and recent store `position - 1`);
   `difficulty_options_table` with `help`, `save_state.difficulty_index`,
   `save_load_difficulty` and `state_format`; the galaxy and pause menu
   tables with `menu`'s positions, on-click and refresh tables;
   `score_rows_table`; `planet_mood_rows_rows`; `planet_family.planet_kinds`.
   Load-time comparison on `game_data`.
4. **Map tables lookups.** `map_tables`: `resource_codes` (with
   `economy_formulas_fill_resource_map`, `save_state.is_resource_code` and
   the economy formulas test's loop), `resource_names`, `production_tables`
   and their `values` (with `economy_tiers`), `project_records_table` (with
   `research_build_projects`), `technologies_table` (with `tech_gate`'s
   reverse map and affordable flags, `save_state.technology_index`,
   `save_load.load_invented` reading `rank + 1`, and every menu or panel
   reader), `icon_tooltips_table` with `cart`'s `kind_table` loop.
   Load-time comparison on `map_tables`.
5. **Data rows.** Every `data_row_t.items` (the `*_items` literals of
   `game_data` and `map_tables`, the rows built at run time by `menu`,
   `message_box`, `score_panel`, `score_panel_display`, `research_display`,
   `planet_display`, `help`) and every reader (`layout_row_cell_row`,
   `layout_row_keys_of_row`, the tooltip constructor, `draw_functions_arg`,
   the score panel's row copies and combined rows, `help`'s rows). Load-time
   comparison on both data files.
6. **Layout values and image caches.** `layout`: tuple `scalars`, row keys
   `items`, `placed_items`, tooltip `rows`; `row_image` keys and slot
   cache, `text_image` slot cache, `button_image` slot cache and key
   `words`, `ui_button`'s key `words`, `planet_display`'s row signature
   (`need_flags`, `need_resources`); every producer and reader. Slot caches
   compute their handle as `FIRST + i - 1`; handles stay 0-based.
7. **Message box, menus and panels.** `message_box_open_box`'s buttons
   argument and the box's articles, with every caller (pause and galaxy
   menu, tech tree screen, help, planet projects and buttons, research
   panels, score panel); `menu`'s rows of items; `research_display`'s inner
   and outer items and outputs; `score_panel`'s readouts and `sub_items`
   (constant indices moved up by one); `draw_functions_draw_rows_default`'s
   rows; `demo_overlay`'s captions with `demo_data_captions`.
8. **Research, tech gate, economy and slipway lists.** `research`'s project
   table, project order and projects by name with
   `research_expire_project_ids` (ids issued in the same order);
   `research_display`'s available projects `{ items, count }`;
   `tech_gate_available_technologies`' `{ items, count }` with the tech
   tree's reader; `resource_map_t.entries` with
   `tests/lua/game/test_economy_formulas.lua`; `slipway`'s served planet
   ids; any `{ items, count }` result list the census still finds.
9. **Input and pad.** `bindings`: `BINDINGS_MODE_*` and `BINDINGS_ACTION_*`
   from 1, `mode_names`, `action_names`, `mode_actions`, the masks and the
   range asserts; `bindings_default`'s rows; `input_step` and `pad_nav`'s
   loops; `pad_jump.directions_by_priority` with `pad_jump_direction`'s
   1-based index; `pad_nav`'s direction tables and `speed` arithmetic;
   `attract` (pad buttons kept keyed by frame). Load-time comparison on
   `bindings_default` (rows and inner masks).
10. **Save, docs, full check.** The byte buffers and cursors of
    `save_state` with `save_storage`, `cart`'s buffer and
    `demo_data_record`; the record arrays and slice positions; the
    picker's tickets and recent ring in `gen` with the save builder and
    loader; `save_load`'s id arrays; validators comparing ranks; `magic`;
    the CRC loop. *Save bytes:* a temporary hook records the bytes of every
    record encoded over one `make e2e` before the chunk and after it; the
    two sets must be identical, and so must the accept/reject results of
    `save_state_decode` on the same seeded set of fuzzed records (byte
    flips of the replay records and the demo record), and the decoded
    record re-encoded must give back its bytes. Then `design.md`,
    `docs/save-format.md`, `warn-review` rule 21; then the step's grep over
    all of `game/`, `make test-all` and `make e2e end-state`; the commit
    message carries the classified exception list for the step note.

## Testing Decisions

- The 20 e2e replays are the net: every `.json.sha256` and `.end-state`
  unchanged, `make e2e` green after every chunk. A good test here checks
  what the player, the screen or the save sees, not how an index is spelled.
- No new permanent unit test: an index base reaches no capture, save byte
  or end state, and a test of "starts at 1" would freeze spelling. The
  proof of completeness is the per-chunk zero-key trap, past-count counter,
  census and load-time comparison, all temporary, plus the save-bytes and
  fuzzed-decoder comparison of chunk 10.
- Existing tests that froze 0-based arrays are updated in the chunk that
  shifts them: `tests/lua/test_runtime.lua` (point and segment list
  literals; slot handles, the screen, the sprite sheet, the map cells and
  the draw palette stay), `tests/lua/game/test_economy_formulas.lua`
  (resource map entries, the `resource_codes` loop), the runtime fixture's
  lists handed to the runtime. `tests/lua/test_capture_read.lua` writes
  capture frames keyed by byte offset, not `game/` arrays, and is not
  touched.
- Prior art: step 1.4's per-walk temporary check over the 20 replays, step
  2.1's per-chunk instruments and save-bytes comparison, step 2.2's shadow
  instrument comparing old and new lookups.
- `lua-guard` and `lua-test-modules` run unchanged in `make test`.

## Out of Scope

- The save format: no byte, field width, rank base or validation rule
  changes; `docs/save-format.md` changes only where it describes Lua tables.
- Truth values (step 3.1), the C headers and comments about the C (step
  3.3) except a comment whose 0-based contract this step makes false,
  module-prefix renames (step 3.4), the rest of `design.md` § Conventions
  inherited from the C (step 3.5).
- Dropping a count field in favour of `#`: a counted array keeps its count.
- Renumbering an enum that is a value (`ENTITY_STATE_*`, `TAG_*`,
  `SAVE_NODE_*`, `DRAW_FUNCTION_*`, layout kinds, `PAD_DIRECTION_*`) or the
  image slot handles.
- The emulated PICO-8 memory (screen, sprite sheet, map, palettes) and the
  generated tables keyed by address or angle.
- Audio, new features.

## Further Notes

- The plan's grep does not see the `[x - 1]` form, the commonest 0-based
  crossing today (`seq[i - 1]`, `planet_types_table[node.planet_type - 1]`,
  `galaxy_menu_table[i - 1]`, `period_names_table[lo - 1]`,
  `production_table.values[clamped_idx - 1]`, `g_projects[k - 1]`,
  `e.items[i - 1]`, `countdown_rows.items[countdown_index - 1]`,
  `planet_mood_rows()[mi - 1]`, `state.recent[slot - 1]`); the census
  searches it in every chunk, and the last chunk reports any left.
- Silent failure modes the trap and the census target, because the replays
  may miss them: `t[0]` on a shifted array (`nil`), an append still writing
  `t[count]` (overwrites the last element, never fails), a loop still
  `for i = 0, count - 1` (reads `nil` first, drops the last), a constant
  index not moved (`readouts[5]` reading the neighbour), `#` or `ipairs` on
  a counted array with a stale tail (too long) or a hole (too short), a
  save rank converted twice or not at all (a valid record pointing one node
  off, which the validators may still accept).
- `CLAUDE.md` § The game still says optional parts are "flagged by bits in
  `has`", which step 2.1 made false; not this step's change, reported for
  plan-pilot (as the step 3.1 PRD did).
