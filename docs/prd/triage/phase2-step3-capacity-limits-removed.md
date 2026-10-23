---
title: "Capacity limits that are not a game rule removed"
description: "The Lua game still bounds its collections and texts with the fixed capacities of the deleted C arrays and buffers, by asserts, a fatal, cuts and refusals that no game rule asks for."
status: needs-triage
---

## Problem Statement

A developer reading the Lua game finds collections and texts still bounded
by the sizes of C arrays and buffers that no longer exist. A message box
holds at most 12 articles of at most 5 arguments, and its texts at most 16
bytes. A layout row holds at most 16 cells and a tuple at most 4 scalars.
The score panel copies its rows into storage of 7, 4, 3 and 5 items. The help
screen draws at most 4 rows. A slipway serves at most 90 planets. A planet id
is under 8 bytes. A resource map has at most 16 entries. The research project
pool dies with "research project table full" past 42 projects. The bindings
parser refuses a line over 127 characters and cuts its error text to 255
bytes. The text-image cache refuses a text over 31 characters. The state dump
asserts a line under 256 bytes. Each of these limits is enforced by an assert,
a fatal, a cut or a refusal. Many of the asserts end in "stop, do not widen":
in the C, widening meant changing an array's size. A Lua table has no size,
so these limits now only stop a change that would be harmless. The phase-0
decision says the capacity limits that are not a game rule go.

## Solution

Every limit in `game/` that bounds a growing collection or a text is removed,
unless it is a game rule. A removed limit takes its constant, its assert or
fatal, its cut or refusal, and any preallocation sized by it. The collection
or text it bounded then grows as far as the code writes it. The limits that
stay are the save format's, one cart rule, and the rules of a fixed machine.
Each is listed below with its reason, and the step note lists them again.
Data table sizes, value ranges and caches that lose nothing when they evict
are not limits. They are listed too, so the step note can name every hit of
the check's candidate grep.

The game plays and draws exactly as before. Every `.json.sha256` and every
`.end-state` stays unchanged. A removed cut or refusal could in principle
change what is drawn. For each one the inventory below names the replays
that could notice. If a hash changes, that is a stop and a report under the
plan's rules. It is never a re-baseline.

The step's check: every remaining constant or local in `game/` that bounds a
growing collection or text (by a fatal, an assert, a cut or a refusal),
whatever its name, is a game rule. The step note lists each one with its
reason. A constant that is only the size of a data table is not a limit.

## Inventory

Read at `f44d815` (the step 2.1 note) and `f69967f` (step 2.2's commit, which
appeared while this PRD was being written; see the H-2.2 hypotheses). Line
numbers are omitted on purpose. Each entry names the module, the limit and
how it is enforced.

### Remove (not a game rule)

| # | Module | Limit | Enforced by | Could a replay notice? |
|---|---|---|---|---|
| R1 | `devtools.state_format` | `LINE_CAP` 256 | assert per dump line | No. The dump only reaches `.end-state`. The assert never fires, so no line changes. |
| R2 | `game.bindings` | `ERROR_TEXT_CAPACITY` 256, the `capacity` local of `fail` | cut of the error text to 255 bytes | No. The text goes only to `p8_fatal` at startup, never to the screen. No replay's bindings fail to parse. |
| R3 | `game.bindings` | `BINDINGS_MAX_LINE_LENGTH` 127 | refusal of the whole file ("line longer than %d characters"), then a fatal in `main.read_bindings` | No. The longest line of any replay's `.cfg` is 78 characters (`swap_aim_menu.cfg`, `unbound_enter_aim.cfg`), and `bindings.default.cfg` is compiled in. |
| R4 | `game.demo_data` | `DEMO_DATA_FRAME_LIMIT` 3600, `DEMO_DATA_CAPTION_TEXT_SIZE` 66 | nothing reads them (limits of the deleted C generator and its buffer) | No. Nothing reads them. |
| R5 | `game.demo_overlay` | `DEMO_OVERLAY_LINE_LENGTH` 32 | assert in `copy_line` (the C line buffer) | `attract.json` draws the captions. The assert never fires, so it draws the same. |
| R6 | `game.demo_overlay` | at most 2 caption lines | assert in `draw_caption` | `attract.json`. Every caption has 1 or 2 lines, so the drawing is unchanged. With the assert gone, any further lines go to the second `p8_print`, as one text. |
| R7 | `game.message_box` | `MESSAGE_BOX_ITEMS_CAP` (= `SCORE_ROWS_COUNT`, 12) | assert in `message_box_next_article`; 12 articles preallocated in `message_box_create` | No. Articles are only ever read below `articles_count`. |
| R8 | `game.message_box` | `MESSAGE_BOX_ARGUMENTS_CAP` 5 | assert in `message_box_add_row`; 5 arguments preallocated in `article_zero` | No. Arguments are read below `scalars.count` only. A draw function reads its first five and ignores the rest. |
| R9 | `game.message_box` | `MESSAGE_BOX_TEXT_CAP` 16 | assert on an article's text | No (assert only). |
| R10 | `game.message_box` | `TOAST_TEXT_CAP` 16 | assert in `toast_keys_set_text` | No (assert only). |
| R11 | `game.message_box` | `OPEN_BOX_DESCRIPTION_CAP` 17 | assert in `open_box_frame_set_description` | No (assert only). |
| R12 | `game.layout` | `LAYOUT_ROW_KEYS_CAP` 16 | assert in `layout_row_keys_append`; 16 nothing cells preallocated in `keys_zero` | No. Cells are read below `keys.count` only. |
| R13 | `game.layout` (+ `planet_display`, `research_display`) | `LAYOUT_ROW_TUPLE_CAP` 4 | asserts in `layout_row_cell_tuple` and `layout_row_cell_row`; 4 scalars preallocated by the three `tuple_zero` | No. `row_measure_item` reads positions 0–3 by meaning (sprite, width, palette block, spacing) and treats a position at or past `count` as nothing. |
| R14 | `game.row_image` | `ROW_IMAGE_KEY_CAP` (= `LAYOUT_ROW_KEYS_CAP`) | assert in `key_of`; 16 entries preallocated by `key_zero` and copied by `key_copy` | No. Entries are hashed and compared below `count` only. |
| R15 | `game.research_display` | `l_cap` in `planet_resource_row_keys` | assert `keys.count == l_cap`: the C sized the keys at entry | No (assert only). Its computation goes with it. |
| R16 | `game.text_image` | `TEXT_CAP` 31 | refusal to cache (`text_length` returns -1, so the text is drawn directly) | No. The slot width test (48 px, so 11 characters at most) already refuses every text the cap refused. No text changes path. |
| R17 | `game.score_panel` | the `cap` argument of `score_panel_row_copy` (7, 4, 3, 5) and the storage `data_items_zero(7/4/3/5)` | assert `src.count <= cap` | No. See the trap under Implementation Decisions. |
| R18 | `game.score_panel` | `SCORE_PANEL_TEXT_CAP` 16 | assert in `score_panel_refresh`; assert in `score_panel_display`'s new-year banner | No (asserts only). |
| R19 | `game.score_panel` | `SCORE_PANEL_DRAW_ARTICLES_CAP` 5 + 7 | assert in `score_panel_draw` | No (assert only). |
| R20 | `game.score_panel_display` | 5 items in a scored row | assert in `score_panel_update_add_row` | No (assert only). |
| R21 | `game.help` | `HELP_SCREEN_ROWS_CAP` 4 | assert in `help_screen_draw` | No (assert only). |
| R22 | `game.slipway` | `SLIPWAY_SERVED_PLANETS_CAP` 90 | assert in `slipway_served_planets_add` | No (assert only). The galaxy's 90 planets stay where the galaxy is generated. |
| R23 | `game.planet` (+ `slipway`) | `PLANET_ID_SIZE` 8 (C `char[8]`) | asserts in `planet_keys_set_id`, `planet_copy_keys`, `slipway_served_planets_add` | No (asserts only). The save format's 7-byte rule stays in `save_state` (K3). |
| R24 | `game.planet` / `planet_display` | `PLANET_ROW_SIGNATURE_NEEDS` 4 | refusal: a planet with more than 4 needs gets no valid row signature, so `planet_refresh` rebuilds its row at every refresh; 4 need slots preallocated by `planet_row_signature_zero` | Possibly. A planet with 5 or more needs now keeps its row between refreshes, as every other planet does. The rebuilt row should give the same pixels. Replays that could notice are those with a colonised planet holding 5 or more needs: `pad_colonise`, `pad_planet_projects`, `pad_research`, `pad_tech_costs`, `pad_score`, `pad_game_end`, `pad_bankrupt`, `pad_end_turn`, `pad_restart_resume`, `pad_resume_at_start`. The instrument records which of them actually reach the refusal. A changed hash is a stop and a report. |
| R25 | `game.planet_display` | route count ≤ 0xFFFF in `planet_row_signature` | assert ("would alias in the signature": a C `uint16_t` field) | No (assert only). Lua compares the number itself. |
| R26 | `game.planet_display` | `PLANET_INSPECTION_PANEL_ROWS` 4 | 4 button keys preallocated: a fifth row would index `nil` | No. The panel writes its rows 0..3 in code. Each row's keys are created when the row is opened. |
| R27 | `game.economy_formulas` | `RESOURCE_MAP_CAP` (= 13 + 3) | assert in `resource_map_set` | No (assert only). |
| R28 | `game.research` | `PROJECT_RECORDS_COUNT` as the pool's capacity | fatal "research project table full" in `project_create`; the assert in `projects_register`; the pool, `project_index_of`, `g_projects` and `g_projects_by_name` preallocated to 42 | No. `research_build_projects` creates at most one project per record and forgets the pool first. Neither the fatal nor the assert can fire. |
| R29 | `game.research_display` | `research_projects_count() <= PROJECT_RECORDS_COUNT` | assert in `research_display_available_projects` | No (assert only). |
| R30 | `game.tech_gate` | `result.count < TECHNOLOGIES_COUNT` | assert in `tech_gate_available_technologies` | No (assert only). |
| R31 | `game.save_storage` (+ `cart`) | the `cap` argument of `save_storage_read`, its assert `cap >= SAVE_STATE_MAX_BYTES`, `cart`'s `SAVE_BUFFER_BYTES` 8192 | assert on a C buffer's size | No. The 8192-byte refusal of a record that is too long stays (K1). |

### Keep: game rules

| # | Module | Limit | Reason |
|---|---|---|---|
| K1 | `game.save_state`, `game.save_storage` | `SAVE_STATE_MAX_BYTES` 8192: the encoder's assert, the decoder's and the storage's refusals | Save format: "at most 8192 bytes; the limit never moves" (`docs/save-format.md`). |
| K2 | `game.save_state` | `SAVE_STATE_NODES_CAP`, `SAVE_STATE_SLIPWAYS_CAP` 255 and their capture and encode asserts | Save format: node and slipway indices are one byte. The phase-0 decision names them. |
| K3 | `game.save_state` | `SAVE_STATE_PLANET_ID_MAX` 7, `NODE_PLANET_ID_LENGTH` 8, `SAVE_STATE_NEEDS_CAP` 8, `MAGIC_LENGTH` 4, `SAVE_STATE_RECENT_COUNT` 3 | Save format field ranges (planet id length 1–7, need count 0–8, magic `SLWY`, three recent types). |
| K4 | `game.save_state` | `SAVE_STATE_ROUTES_CAP`, `SAVE_STATE_ROUTE_INDICES_CAP`, `SAVE_STATE_NODE_SLIPWAYS_CAP` | Save format: "Pool caps, exceeded means invalid" (`docs/save-format.md`). Removing them would change what the decoder accepts, and the decoder's results were checked unchanged under fuzzing in step 2.1. |
| K5 | `game.save_state` | the `<= 0xFF` / `<= 0xFFFF` asserts on counts the encoder writes | Save format field widths (u8, u16). |
| K6 | `game.gen` | `WEIGHTED_PICKER_TICKETS_CAP` 66 and the assert in `weighted_picker_fill` | Save format: "type picker tickets count u8 0–66 (`WEIGHTED_PICKER_TICKETS_CAP`)". The decoder refuses more. The fill assert keeps the generator from building a picker the save cannot hold. Its message is reworded for that rule, since the "pool is sized" reason is gone. |
| K7 | `game.planet_family_trade` | `slipgate_takes_slipway`: fewer than 3 slipways | Cart rule: a slipgate accepts a slipway drop only below 3. It is gameplay that the replays show. |
| K8 | `p8.draw_meter` | `GPU_CLUT_SLOTS` 6, `GPU_PATTERN_SLOTS` 4, the counts saturating at twice those | A fixed machine's rule: the meter measures a frame against the PSX GPU's tables. The saturation is the measurement (it reports up to "twice the table"). It holds no game data. |

### Not limits (listed so the step note can name every candidate hit)

- **Data table sizes**: every `*_COUNT` of `game_data`, `map_tables`,
  `bindings` (modes, actions), `entity_class` (states, tags),
  `entity_registry` (messages); `PROJECT_RECORDS_COUNT` and
  `TECHNOLOGIES_COUNT` as table lengths; `DRAW_PALETTE_LENGTH` 16 (the
  palette); `SPRITE_COUNT` 256 and `BLOCK_COUNT` 16 in `row_image` (the sheet
  and the palette blocks); `FIRST_CODE`/`LAST_CODE` in `love.text` (the
  font's glyphs); `LOADING_TEXT_LENGTH` (the C `sizeof "loading"` of a
  constant text); `PAD_JUMP_DIRECTIONS`; `ENTITY_REGISTRY_LAYERS` 15 (the
  draw layers an archetype may name).
- **The image slot table and the caches over it**: `P8_IMAGE_SLOTS`,
  `SLOT_SIZES`, every `IMAGE_SLOT_*_COUNT`, `BUTTON_IMAGE_SLOT_COUNT`. These
  are sizes of a data table, the renderer's fixed list of off-screen images.
  The row, text and button image caches evict their least recently used slot
  and rebuild it later. A row or text that is too wide for a slot, or not
  cacheable, is drawn directly. Nothing is lost or refused, and the pixels
  are the same.
- **Numbers, not collections**: `ATTRACT_AFTER_MAX` (the range of
  `--attract-after`, the fake LÖVE's `int`), `INT_MAX` in `pad_build`
  (a sentinel distance) and `pad_select` (a saturating hold counter),
  `HELD_FRAMES_CAP` 41 in `pad_nav` (a saturating hold counter: the pan speed
  is the same past 40).
- **Batches that lose nothing**: the 4-segment bundle in `slipway_display`
  (flushed when full).
- **Offsets**: `LENGTH_OFFSET`, `HEADER_BYTES`, `TRAILER_BYTES` in
  `save_state`.

## User Stories

1. As a developer, I want no limit in `game/` that only restates the size of a deleted C array, so that a Lua table grows where the code writes it.
2. As a developer, I want the message box to create an article when it adds one, so that a box holds as many articles as the code adds.
3. As a developer, I want an article's arguments created as they are written, so that a scalars article holds exactly its row's items.
4. As a developer, I want no length assert on a message box text, a toast text or an open box description, so that a longer text is drawn rather than refused.
5. As a developer, I want a layout row's keys to hold as many cells as are appended, so that a wider row needs no constant raised.
6. As a developer, I want a tuple to hold only the scalars written into it, so that `row_measure_item`'s reading of positions 0–3 is the only place that gives them meaning.
7. As a developer, I want the three `tuple_zero` copies to build an empty tuple with no preallocated scalars, so that no module sizes a tuple.
8. As a developer, I want the row-image key to hold only the entries it hashes, so that it no longer depends on a layout constant.
9. As a developer, I want `planet_resource_row_keys` without its `l_cap` precount, so that the row keys are built without the C's sizing at entry.
10. As a developer, I want the text-image cache to decide cacheability by the slot width alone, so that the redundant 31-character refusal is gone.
11. As a developer, I want the score panel's row copies sized by their source rows, so that no literal storage size remains.
12. As a developer, I want no text-length or article-count assert in the score panel, so that its readouts and banner are bounded only by what the game builds.
13. As a developer, I want the help screen to draw every row of its archetype, so that it has no four-row assert.
14. As a developer, I want a slipway's served planets bounded only by the galaxy, so that the 90 is defined once, where the galaxy is generated.
15. As a developer, I want planet ids free of the C `char[8]`, so that the only planet-id rule left is the save format's 7 bytes.
16. As a developer, I want every planet's row signature to be valid whatever its number of needs, so that the cache key holds every need.
17. As a developer, I want no `uint16_t` width assert on the route count in the signature, so that the key compares the number itself.
18. As a developer, I want the inspection panel to create each row's button keys when it opens the row, so that it preallocates no fixed row count.
19. As a developer, I want a resource map to accept any key it is given, so that it has no "13 resources and 3 spares" assert.
20. As a developer, I want the research project pool to grow when a project is created, so that "research project table full" and its preallocation go.
21. As a developer, I want the pool to keep reusing its existing tables across games, so that a project table's identity is the same as before.
22. As a developer, I want no assert bounding the available projects or the available technologies by their table size, so that a filter result is just a list.
23. As a developer, I want `save_storage_read` without a buffer capacity argument, so that a Lua array is not described as a C buffer.
24. As a developer, I want the bindings parser to accept a line of any length and to report its full error text, so that a configuration file is limited by nothing but its syntax.
25. As a developer, I want the demo overlay without its line-buffer and two-line asserts, so that the captions are bounded only by the data.
26. As a developer, I want the demo data's two unread generator constants removed, so that no module exports a limit nothing reads.
27. As a developer, I want the state dump to write a line of any length, so that `LINE_CAP` goes.
28. As a developer, I want the save format's limits kept exactly, so that every save byte and every decode result stays the same.
29. As a developer, I want the weighted picker's 66-ticket rule kept, with its assert saying it is the save format's range, so that the reason it stays is the true one.
30. As a developer, I want the slipgate's three-slipway rule kept, so that gameplay is unchanged.
31. As a developer, I want the draw meter's PSX tables kept, so that its readings mean the same.
32. As a developer, I want each shape comment that names a removed cap rewritten, so that no comment describes a size that is gone.
33. As a developer, I want `design.md`'s sentence on static structs and inline arrays updated, so that later steps are planned from a true description.
34. As a plan pilot, I want the inventory's keep and not-a-limit lists in the step note, each item with its reason, so that the step's check is recorded.
35. As a plan pilot, I want the step's check false before the step (for example `MESSAGE_BOX_ITEMS_CAP` bounds the articles and is no game rule) and true after, so that the note records it.
36. As a plan pilot, I want each chunk verified with `make e2e` green and every `.json.sha256` unchanged, so that a failure points at one module group.
37. As a plan pilot, I want a temporary instrument at each removed cut and refusal to count how often the old condition held over one `make e2e`, so that "no replay can notice" is evidence and not a reading of the code.
38. As a plan pilot, I want the instrument's counts per replay in the chunk's commit message and the instrument removed before the commit, so that `make e2e` gets no slower.
39. As a plan pilot, I want a changed hash treated as a stop and a report, never a re-baseline, so that the plan's rule holds.
40. As a plan pilot, I want `make e2e end-state` passing with every `.end-state` unchanged and the `make e2e` time recorded under 9 s, so that the step meets the plan's rules.
41. As a plan pilot, I want every assumption about step 2.2's outcome checked against the tree before the research chunk, so that a broken assumption stops the step instead of breaking it silently.
42. As a maintainer, I want no unit test added, so that `make test` gets no slower.

## Implementation Decisions

### Hypotheses about step 2.2's outcome

Step 2.2 (research project ids as plain numbers) is not confirmed done when
this PRD is written. Its commit `f69967f` appeared on the branch while this
PRD was being written, with a step note. It has not been checked here by
`plan-pilot`'s end-state run. Before chunk 5, the implementer checks each
hypothesis below against the tree. If one is false, the step stops and
reports that hypothesis. It does not adapt the plan on its own. Chunks 1–4
touch no module that step 2.2 changes, except a comment line in
`research_display` (H-2.2-5).

- **H-2.2-1.** `research` still holds the pool as before step 2.2:
  `PROJECT_RECORDS_COUNT` tables `{ inputs = {} }` created at load in
  `g_project_table`, reused by `project_create` after `project_zero`, the
  `project_index_of` reverse map built at load over them, `g_projects` and
  `g_projects_by_name` preallocated to `PROJECT_RECORDS_COUNT`, the "research
  project table full" fatal and `projects_register`'s
  `g_projects_by_name_count < PROJECT_RECORDS_COUNT` assert. Step 2.2's PRD
  leaves all of these to this step. At `f69967f` they are as described.
- **H-2.2-2.** A project's id is issued in `project_create` once the project
  claims its pool slot. It is held in module-local `id → project` and
  `project → id` maps, never in a field of the project. Expiry reissues an
  id to every table below `g_project_table_count`. Growing the pool on demand
  therefore touches neither the id maps nor the expiry, except that a table
  created on demand must enter `project_index_of` before anything reads it.
- **H-2.2-3.** `research_build_projects` records each project's pool index
  through `project_index_of[...]`, and `research_projects_at` resolves
  through `g_project_table[g_projects[k - 1]]`.
- **H-2.2-4.** `research_display_available_projects` still asserts
  `research_projects_count() <= PROJECT_RECORDS_COUNT` and still returns a
  0-based `{ items, count }` of project ids.
- **H-2.2-5.** Step 2.2 changed in `planet`, `ui_button` and
  `research_display` only the project id lines and one header comment line.
  It did not touch `PLANET_ID_SIZE`, `PLANET_ROW_SIGNATURE_NEEDS`, the
  `tuple_zero` copies, `l_cap` or any other limit of this inventory.
- **H-2.2-6.** `grep -rn epoch game/` finds nothing, and every
  `.json.sha256` and `.end-state` matches at the start of this step. The step
  starts from a green tree.
- **H-2.2-7.** `design.md`'s sentence "A research project id follows the same
  convention from `research`'s own counter and table ..." exists. This step
  does not change it.

### Order and scope

- **Chunks, one or more whole module groups each**, never part of a group.
  A module group is a module's definitions together with all their callers.
  Small groups with no caller in common share a chunk. A commit per
  three-line group would cost more than it gains.
- **Each chunk is verified** with `make e2e` green, every `.json.sha256`
  unchanged, and the step's check restricted to its modules (no removed
  limit left in them). The last chunk runs the full check, `make test-all`,
  and `make e2e end-state`.

### How a limit goes

- **Preallocation becomes creation on write.** An array the C sized (message
  box articles and arguments, layout keys and tuples, row-image key entries,
  score panel storage, planet row signature needs, inspection panel button
  keys, the research pool and its two index arrays) starts empty. An element
  is created where the code first writes it. Every read already stops below
  the array's count. The implementer checks this per array before removing
  its preallocation. A read at or past the count found in a group is a stop
  and a report.
- **Arrays keep their base.** The 0-based arrays stay 0-based. Step 3.2 turns
  them 1-based. This step changes their size, not their indexing.
- **An assert whose only reason was the capacity goes whole.** An assert
  that also checks a contract the name does not state stays, without its
  capacity half. Examples: `layout_row_cell_tuple`'s `count >= 0`, and
  `message_box_add_row`'s "a draw function reads no nested row" and "only
  print_outlined's first argument is read as text".
- **The constant goes with its last reader.** An exported constant goes from
  `M` too. None is read under `tests/` or `tools/` (checked at `f69967f`).
- **Comments.** Each shape comment that names a removed cap (`message_box`,
  `layout`, `bindings`, `planet`, `demo_overlay`, `research`) is rewritten or
  cut under `CLAUDE.md` § Comments. A comment that only explained a removed
  limit goes. `rework-comments` and the `-- Copy of` headers stay out of
  scope (step 3.3).

### Per group

- **Bindings (+ `main`).** `fail` keeps its prefix and full message with no
  cut. `bindings_parse` drops the line-length refusal. The parse otherwise
  reads the same lines. The header's "at most ERROR_TEXT_CAPACITY - 1 bytes"
  goes.
- **State dump.** `line_add` appends with no assert.
- **Demo.** `copy_line` becomes a plain `string.sub`. `draw_caption` splits
  at the first newline only, as it does now. `demo_data`'s two constants
  go. `demo_data` names a deleted generator, but it is plain data and is
  edited by hand.
- **Save storage (+ `cart`).** `save_storage_read(bytes)` loses `cap` and its
  assert. `cart` drops `SAVE_BUFFER_BYTES`. The record-length refusal stays
  (K1).
- **Layout (+ `row_image`, `research_display`, `planet_display`).**
  `keys_zero` returns an empty `items`. `tuple_zero` (all three copies)
  returns an empty `scalars`. `row_measure_item` reads `scalars[i]` for
  `i < count` and nothing beyond, without building the padded `f` array to a
  constant. `key_zero` starts with empty entries, and `key_copy` copies the
  entries below `count`. `planet_resource_row_keys` loses `l_cap` and its
  computation.
- **Text image.** `text_length` returns the NUL-cut length with no cap.
  `text_image_cacheable` keeps its width test.
- **Message box, score panel, help (+ their callers).** Covers R7–R11 and
  R17–R21. **Trap:** the score panel writes into storage past the copied
  count after the copy (the year banner's `items[2]` when `row.count`
  becomes 3, `second_items[0]`, `sub_items[i][0]`). Each such index must
  still hold a table when written: create it there, or size the storage by
  the largest index written. Never write into a missing slot.
- **Planet, slipway, economy (+ callers).** Covers R22–R27. The signature
  holds every need. `planet_release_row` still clears its `flags`. The
  inspection panel creates `buttons[row_count]` when it opens a row. Every
  later reader (`for i = 0, row_count - 1`, or the panel's handover) reads
  below `row_count`. The implementer checks this.
- **Research, tech gate (+ `research_display`, `gen`).** Covers R28–R30 and
  K6. `project_create` takes `g_project_table[index]`, or creates it and
  enters it in `project_index_of` when absent. The table at a given index is
  therefore the same table across games, as now. `g_projects` and
  `g_projects_by_name` start empty and grow. `forget_projects` keeps
  resetting the counts and does not empty the pool. `gen`'s fill assert
  stays, with its message reworded: a picker past 66 tickets cannot be
  saved (the save format's range).

### Docs

- `design.md` § Conventions inherited from the C, **Structs and
  out-parameters**: the static-struct sentence says that an inline array is
  created as its elements are written, with no fixed length. The only fixed
  lengths left are those of data tables and the save format. No other
  sentence is made false. `CLAUDE.md` § The game names no capacity and stays
  as it is. `docs/save-format.md` stays as it is: every limit it names is
  kept.

### Questions settled without `guideline`

- **What "game rule" covers.** The phase-0 decision gives the save format's
  255 nodes and slipways as its example. This PRD reads it as: the save
  format (every range `docs/save-format.md` documents), the cart's gameplay
  (the slipgate), and a fixed machine that is the subject of a measurement
  (the PSX meter). The image slot table is a data table, and its caches lose
  nothing by evicting, so it is not a limit under the check's own carve-out.
- **The weighted picker's 66 stays** although its assert's wording came from
  C storage. The save format documents the 0–66 range and the decoder
  refuses more, so the bound is a format rule.
- **The research pool keeps reusing tables.** Replacing the pool by a fresh
  table per project would be the allocator decision of step 1.2 applied to
  a new module, outside this step's "capacity" scope. It could also change
  which table a stale holder sees. Only its capacity goes.
- **`PLANET_ROW_SIGNATURE_NEEDS` goes** even though it changes how often a
  row is rebuilt. It is a C array bound with no game meaning. The instrument
  and the hashes decide whether that is visible. If it is, the step stops.

## Implementation Chunks

1. **Startup, devtools and storage** — groups `bindings` (+ `main`),
   `state_format`, `demo_overlay` + `demo_data` (+ `attract`'s reads),
   `save_storage` (+ `cart`). R1–R6, R31. Instrument: R2's cut, R3's refusal
   and R5/R6's conditions (expected 0).
2. **Layout, row image and text image** — groups `layout` (+ `row_image`,
   `research_display`'s and `planet_display`'s `tuple_zero` and `l_cap`,
   every caller of `layout_row_keys_append`), `text_image`. R12–R16.
   Instrument: R16's refusal by the cap alone (expected 0).
3. **Boxes and panels** — groups `message_box` (+ every caller of its setters
   and adders), `score_panel` + `score_panel_display`, `help`. R7–R11,
   R17–R21. Instrument: none needed (asserts only). Check the trap above.
4. **Planets, slipways and the economy** — groups `planet` (+ `galaxy`,
   `structure_icon`, `save_load`, `planet_display`), `slipway` (+
   `economy_tiers`), `economy_formulas`. R22–R27. Instrument: R24's refusal,
   counted per replay (planets refused, refreshes affected). Its counts go in
   the commit message, whatever they are.
5. **Research and the tech gate** — check H-2.2-1…7 first. Groups
   `research` (+ `research_display`), `tech_gate`, `gen` (message only).
   R28–R30, K6. Then `design.md`. Then the full check: the candidate grep
   below, every hit classified as in the inventory, `make test-all`,
   `make e2e end-state`, and the `make e2e` time.

## Testing Decisions

- **The replays are the test.** What a removed limit could change reaches the
  captures (a cut text, a refused cache, a rebuilt row) or the end states
  (the dump). A removed assert that never fired changes nothing. The 20
  hashes and end states show it.
- **Temporary instrument, run once per chunk, then removed.** At each removed
  cut or refusal (R2, R3, R5, R6, R16, R24), the chunk keeps the old
  condition in a counter, reported per replay at exit, and runs `make e2e`
  once. The counts go in the commit message, and every trace is removed
  before the chunk's check and commit. Expected: 0 everywhere except perhaps
  R24. This copies step 1.4's per-walk check and step 2.1's instrument.
- **The candidate grep for the full check** (the note records its hits and
  each one's class):
  `grep -rnE '\b[A-Za-z_]*(CAP|CAPACITY|MAX|LENGTH|SIZE|LIMIT|COUNT)\b|p8_fatal|assert\(.*(#|count|_n\b)' game/`
  plus a read of every `string.sub`/`:sub(` call (a cut) and every early
  `return` on a count in `game/`. A hit that bounds a growing collection or
  text and is not in K1–K8 fails the check.
- **No new unit test.** `tests/lua/game/` holds one module test (economy
  formulas), which reads no removed constant. A new test file would slow
  `make test`, which is forbidden. No file under `tests/` or `tools/` names a
  removed constant (checked at `f69967f`).
- **Static nets.** `lua-guard` catches a read of a removed module field (for
  example `layout.LAYOUT_ROW_TUPLE_CAP` left in a caller). The replays error
  at once on a write into a missing preallocated slot.
- **Prior art.** Step 1.2 (zones removed, sizes kept as named constants:
  those named constants are what this step removes) and step 2.1 (chunks by
  module group, a temporary instrument).

## Out of Scope

- The fixed-point format and the random generator (plan rule).
- The save format and its every range (K1–K6). `docs/save-format.md` is
  unchanged.
- The research pool's table reuse, `project_zero`, and `project_index_of` as
  a concept (only its sizing goes).
- `c_strlen` and the other walks to a NUL byte (`layout`, `ui_button`,
  `text_image`). They cut at a terminator, not at a capacity.
- The exit queue dropping an entity queued during its own flush. That is a
  behavior kept for the hashes, not a capacity.
- Asserts on a type or a contract that end in "stop, do not widen the
  member" without bounding a size (`research_project_string`,
  `resource_code_of`, `slipway_served_planets_add`'s nil id, the icon-sprite
  row hole).
- 0/1 predicates (step 3.1), 0-based arrays and `for i = 0, ...` loops
  (step 3.2), `-- Copy of` headers (step 3.3), module-prefixed names
  including `save_storage_read` and `planet_keys_set_id` (step 3.4).
- The C tools and the fake LÖVE (`tools/`, `tests/*.c`): the check covers
  `game/` only.

## Further Notes

- The step note records: the inventory's keep list (K1–K8) and not-a-limit
  list with their reasons, the candidate grep's result, each chunk's
  instrument counts, `make e2e` time, `make e2e end-state` 20/20, `make test`
  time, and which H-2.2-k hypotheses held as written.
- If R24's instrument shows refused planets and a hash changes, the cause is
  a difference between a rebuilt row and a kept one. The step stops and
  reports. It does not keep the limit to get green on its own initiative,
  because keeping it is a phase-0 decision to reopen with the user.
