---
title: "Optional parts as nil-able fields, has_<field> numbers included"
description: "Optional values in the Lua game are still marked present by C-style bits (`has`, `has_`, `*_HAS_*`) and by `has_<field>` 0/1 numbers next to fields that always hold something, when a field that is `nil` when absent would say the same thing in plain Lua."
status: needs-triage
---

Plan: `docs/love-port/plan-5.md`, Phase 2 — The entity model, step 1:

> 1. Optional parts as `nil`-able fields, `has_<field>` numbers included. Seen
>    when
>    `grep -rnE '_HAS_|entity_has\(|\.has\b|\bhas_[a-z_]+ *([~=]?= *[01]\b|= *[a-z_.]*has_[a-z_]+)|has_[a-z_]+ or [01]\b' game/`
>    finds only bit sets that are not an entity's optional parts, and `has_`
>    names that are not a 0/1 flag paired with a value (a method such as
>    `has_refresh`, a local boolean), each listed in the step note.

## Problem Statement

The game still says "this value is optional" the way the C did: a flag next
to a field that always holds something. There are two kinds of flag.

**Bit sets.** The entity header's `has` holds nine `ENTITY_HAS_*` bits (layer,
hit box, infraspace, fold anchor, shake, is_dead, is_hovered, pos, map_pos).
The fields they guard are always there, set to a zero by
`entity_init_header`: a `{0, 0}` position, a `{0,0,0,0}` hit box, a layer of
0. Because `has` was taken, several kinds add a second set named `has_`: the
planet (`PLANET_HAS_TARGETING`, `_EMITTER`, `_PRODUCTION`, `_NEEDS`,
`_INCOMING_ROUTES`, `_OUTGOING_ROUTES`, `_ID`), the economy
(`ECONOMY_HAS_PRODUCTION`, `_DEMAND`), the UI (`UI_HAS_MOUSE_WORLD`,
`_HOVERED_BY_LAYER`, `_OWN_EPHEMERALS`), the button
(`BUTTON_HAS_SLIDE_TARGET`), the wormhole (`WORMHOLE_HAS_TOO_FAR_TARGET`,
written and never read), the score panel layout block and the pause menu box
(`*_HAS_FIXED_POS`, `*_HAS_ROW_STEP`). A particle emitter, which is part of a
planet, a probe or the help screen, has its own `has` holding
`EMITTER_HAS_PARTICLES`.

**`has_<field>` numbers.** About 55 different `has_<x>` 0/1 numbers sit next
to a value `<x>`, and they are on every kind of table. They are on entities
(`has_production_bonus`, `has_development_tier`, `has_net_income`, the
button's `has_on_click` / `has_trade` / `has_type_bias` / `has_project_cost`,
`has_is_shown`, `has_curtain`, `has_is_revealed`, `has_happiness`,
`has_end_status`). They are on parts and records (`production.has_resource`,
a need's `has_satisfied`, a particle's `has_acceleration`, the targeting's
`has_cost`, the research project's `has_base_cost` / `has_output` /
`has_upgrade_tech` / `has_byproduct` / `has_icon_sprite`, a trade route's
`has_resource`, the economy placeholder's `has_credits` /
`has_research_points`, the mouse button's `has_held`, the save record's
`has_probe` and need and route `has_resource`). They are on keys and frames
(`has_pos`, `has_map_pos`, `has_planet_id`, `has_layer`, `has_slide_target`,
`has_tooltip_row`, `has_side_tooltip_row`, `has_text`, `has_duration_scale`,
`has_description`, `has_fixed_pos`, `has_row_step`, `has_back_color`, the
probe keys' `has_angle_slices` / `has_base_radius` / `has_speed_factor`).
They are on data tables (a data row's `has_color` / `has_resource`, a score
row's `has_score` / `has_pos`, a kind record's `has_sprite` / `has_cost`, an
archetype's `has_pos` / `has_development_tier` / `has_cost`, a project
record's `has_icon_sprite`). And they are on locals, parameters and module
state (`has_value[]`, `has_list`, `has_dispatcher`, `has_target`,
`has_planet_type`, `has_bonus_resource`, `has_any_resource`,
`g_has_difficulty`, `g_has_requested_option`, `g_has_cursor`,
`g_has_id_counter`).

For each optional value, a developer has to keep three things in step: the
field, its flag, and the placeholder the field holds while the flag is off.
Setting a value takes two lines. Clearing it leaves a stale value behind the
cleared flag. Testing it is `entity_class.entity_has(e, ENTITY_HAS_X) ~= 0`,
`bit.band(self.has_, M.PLANET_HAS_X) ~= 0` or `self.has_x ~= 0`. Nothing
stops a reader from reading the field without the flag and quietly getting
the placeholder or a stale value, and some readers do (listed below). Phase 0
decided, and the user confirmed after the phase-1 review: an entity's
optional parts are fields that are `nil` when absent; the `has` bits go, and
so do the `has_<field>` 0/1 numbers paired with a value.

## Solution

Every optional value becomes a field (or local, or parameter) that is `nil`
when the value is absent and holds the value when present. Setting it is one
assignment, clearing it assigns `nil`, and testing it is `x ~= nil`. This
applies to an entity's optional parts, to the parts an entity holds, and to
every table and local that carries a `has_<x>` number next to `<x>`. The
following are deleted:

- every `*_HAS_*` constant that marks an optional value;
- the header's `has`, every `has_` field and the emitter's `has`;
- `entity_class.entity_has`;
- every `has_<x>` flag paired with a value.

`entity_init_header` and the zero constructors no longer write placeholders
for values the object does not have.

Two header fields lose only their bit and stay on every entity that has them
today. `is_dead` is a field of every entity. The UI's `ephemerals` stays an
array that is empty when the UI owns none.

These stay, and are the step note's exceptions:

- the save format's node flag bits (`SAVE_NODE_*`), which are on-disk bytes;
- the planet row signature's cache-key bits (`ROW_SIGNATURE_HAS_OUTPUT`,
  `ROW_NEED_HAS_RESOURCE`);
- the by-value `mouse_pos_t` with `MOUSE_POS_HAS_*` and its `has` field,
  which is not an entity;
- the class method `has_refresh` (`button_has_refresh`).

The game plays exactly as before: every e2e capture hash and every end state
is unchanged, `make e2e` stays under 9 s, and `make test` is no slower.

## User Stories

1. As a developer, I want an optional value to be a field that is `nil` when absent, so that one field says both whether the value exists and what it is.
2. As a developer, I want setting a value to be one assignment, so that I cannot set the value and forget its flag.
3. As a developer, I want clearing a value to assign `nil`, so that no stale value hides behind a cleared flag.
4. As a developer, I want testing a value to be `x ~= nil`, so that I never call `entity_has`, `bit.band` or compare a `has_x` with 0 to ask whether a field exists.
5. As a developer, I want a value of 0 (an infraspace of 0, a layer of 0, a cost of 0, a development tier of 0) to still count as present, so that the difference the flags made between "absent" and "present and zero" survives.
6. As a developer, I want reading an absent value without testing it to fail loudly (an error on `nil`), not to return a made-up zero, so that a forgotten presence test shows at once in the replays.
7. As a developer, I want the header's `has`, every `has_` field and every `has_<x>` number gone, so that no field name dodges another with a trailing underscore and no table carries a flag next to its value.
8. As a developer, I want `entity_init_header` to write only what every entity has (class, state, state_time, id, is_dead), so that an entity table holds only its real parts.
9. As a developer, I want the zero constructors (`button_zero`, `button_keys_zero`, `emitter_zero`, `targeting_zero`, `planet_keys_zero`, `layout_row_keys_zero`, `open_box_frame_zero`, `toast_keys_zero`, the save record zeros, ...) to leave optional values out, so that "zero" means "nothing given".
10. As a developer, I want the data constructors (`map_tables_make_data_row`, `score_row`, `targeting`, `probe_archetype`, `planet_archetype`, `project_record`, `kind_record`) and their tables to give a value only where it exists, so that a data entry with `has_x = 0, x = 0` becomes an entry without `x`.
11. As a developer, I want `entity_pos` and `entity_map_pos` to keep returning the zero vector when the position is absent, so that their callers keep their contract.
12. As a developer, I want `entity_has_pos`, `entity_has_map_pos`, `structure_icon_has_cursor` and the other predicates to keep returning 0 or 1, so that their callers and step 3.1 (booleans) keep working as planned.
13. As a developer, I want `entity_set_pos` and `entity_set_map_pos` to keep copying the vector in and `entity_pos` to keep copying it out, so that changing a position in place never changes another table.
14. As a developer, I want `entity_clear_pos` and `entity_clear_map_pos` to assign `nil`, so that a cleared position cannot be read stale.
15. As a developer, I want `entity_registry_set_layer` to assert that its layer is not `nil`, so that setting a layer can never quietly clear it.
16. As a developer, I want `is_dead` to stay a field every entity has (0 unless dying), so that "is this entity alive" stays a one-field test on any entity.
17. As a developer, I want `is_hovered` to exist only on entities that take hover, so that the UI's "the hovered entity has an is_hovered member" asserts keep protecting the hover contract, now as `is_hovered ~= nil`.
18. As a developer, I want a planet's id, targeting, emitter, production, needs, incoming routes, outgoing routes, production bonus, net income and development tier to be `nil` when the planet has none, so that a planet table holds only its parts.
19. As a developer, I want `planet_production` and `planet_production_read` to keep returning the shared production for a planet without its own, so that writes through it keep showing on every such planet.
20. As a developer, I want `planet_id_or_null` and `emitter_of` to keep returning `nil` when the part is absent, so that their callers do not change.
21. As a developer, I want an emitter's particles to be `nil` when the emitter holds none, so that the emitter has no `has` of its own.
22. As a developer, I want the economy's production and demand maps, happiness and end status to be `nil` until they are computed, so that the economy has no `has_` field and no `has_<x>` number.
23. As a developer, I want the UI's world mouse point and hover layers to be `nil` until the first update writes them, so that the UI has no `has_` field.
24. As a developer, I want the UI's ephemerals to stay an array that is empty when the UI owns none, so that its many readers keep reading `#ui.ephemerals` without a test.
25. As a developer, I want a button's slide target, click handler, trade, type bias and project cost to be `nil` when not given, so that the button and its keys carry no `has_<x>`.
26. As a developer, I want the open-box frame's description, fixed position, row step, back color and position, and the message box keys' text and duration scale, to be `nil` when absent, so that a frame says only what it holds.
27. As a developer, I want a resource that may be absent (a data row's resource, a production's resource, a route's resource, a need's satisfied amount, a project's output) to be `nil` when absent, so that "no resource" is one value.
28. As a developer, I want a flag that nothing reads (`WORMHOLE_HAS_TOO_FAR_TARGET`, the button's `has_side_tooltip`, the keys' `has_tooltip`, the project's `has_upgrade_tech` once its one reader tests the value) removed outright, so that no field is written and never read.
29. As a developer, I want a local 0/1 copy of a presence (`has_target`, `has_dispatcher`, `has_list`, `has_value[]`, `has_planet_type`, `has_bonus_resource`, `has_any_resource`) replaced by the value being `nil` or not, so that a local flag never says what the value already says.
30. As a developer, I want module state such as `g_has_difficulty`, `g_has_requested_option`, `g_has_cursor` and `g_has_id_counter` folded into its value, so that the module holds `nil` when it holds nothing.
31. As a developer, I want the save to keep writing exactly the same bytes, so that every save stays readable and every replay that saves keeps its hash.
32. As a developer, I want the save record to hold `nil` for an absent probe, need resource or route resource, while the encoder writes the flag byte from presence and the decoder rejects any flag byte other than 0 or 1, so that the record has no `has_<x>` number and `save_state_decode` accepts exactly the same files.
33. As a developer, I want the state dump to keep writing the same lines, so that every `.end-state` stays the same.
34. As a developer, I want the save format's node flags, the row signature bits, `mouse_pos_t` and `has_refresh` kept and named in the step note, so that the step's check lists only what is not an optional value.
35. As a developer, I want a temporary instrument to prove, before each flag goes, that every reader of the value ran only while the value was present, so that dropping the placeholder is shown not to change what any reader sees.
36. As a player, I want the game to look and play exactly as before, so that every `.json.sha256` stays the same.
37. As a developer running `make e2e end-state`, I want every `.end-state` to stay the same, so that the game state is shown not to move.
38. As a developer, I want `make e2e` under 9 s and `make test` no slower, so that the fast loop holds.
39. As a plan pilot, I want the step cut into one chunk per group of fields with all their readers and writers, so that a failure points at one group.
40. As a plan pilot, I want each chunk verified by `make e2e` green with every `.json.sha256` unchanged and by its own restricted check, so that it can be committed on its own.
41. As a plan pilot, I want the last chunk to run the step's full check and list its exceptions, so that the step note can record them.
42. As a developer reading `docs/love-port/design.md`, `CLAUDE.md`, `docs/save-format.md` and the `warn-review` and `tdd` skills, I want every sentence that says presence is a bit in `has` replaced by what is true now, so that later steps and the phase-end review are planned from true docs.

## Implementation Decisions

### What an optional value is now

- An optional value is a field, local or parameter that is `nil` when absent.
  Presence is tested with `x ~= nil` (or `== nil`), never by truthiness. That
  way a value of 0 reads as present, and step 3.1 can later turn values into
  booleans without changing any presence test. Setting assigns the value,
  clearing assigns `nil`.
- **In scope**: every bit set that marks an optional value of an entity or of
  a part an entity holds, and every `has_<x>` 0/1 number paired with a value.
  That covers fields of entities, parts, records, keys, frames, archetypes
  and data tables, locals and parameters, and module-level `g_has_<x>`
  state. The step's grep does not see `g_has_<x>` or `self_has_infraspace`,
  but they are the same idiom and are converted. A `has_<x>` with no paired
  value that nothing reads (`keys.has_tooltip`) is deleted.
- An object never holds a placeholder for an absent value: the zero written
  today for a value the object lacks is not written. Absent values are not
  listed in a zero constructor or a data constructor (`x = fields.x`, not
  `x = fields.x or 0`, when `x` is optional). In a data table, an entry with
  `has_x = 0` loses both `has_x` and `x`; an entry with `has_x = 1` keeps
  `x`. `map_tables_check_fields` keeps rejecting unknown fields; a given
  field is never `nil`, so the check is not affected.
- A value that callers fill in place before raising its flag (`menu` appends
  into `b.side_tooltip_row` and then sets `has_side_tooltip_row = 1`; needs
  and route lists are emptied and refilled) is created by the caller with
  the constructor the zero used (`layout_row_keys_zero()`, `{}`), at its
  first write.
- Predicates keep returning 0/1 (step 3.1). Where an unchanged consumer
  needs a stored 0/1 presence (`keys.has_map_pos = entity_has_map_pos(self)`
  goes, but a 0/1 passed to a function that still takes one stays), the
  value is `(x ~= nil) and 1 or 0`. `entity_has_pos`, `entity_has_map_pos`
  and `structure_icon_has_cursor` stay, computed from presence.
- A flag paired with a value that the cart could hold as `false` (the
  project's `upgrade_tech` under `upgrade_tech_false`; a button's
  `side_tooltip` under `side_tooltip_is_false`) does not need Lua `false`. No
  reader tells "present and false" from "absent":
  `research_project_upgrade_tech` returns `nil` for both, and the
  `side_tooltip_is_false` branch of `button_construct` assigns `nil`, which
  is already the default, and sets only the button's `has_side_tooltip`,
  which nothing reads. That branch and `side_tooltip_is_false` go with
  `has_side_tooltip`.
- Two forms simplify once `nil` is a value. A presence-then-value comparison
  `(has_a ~= has_b) or (has_a ~= 0 and a ~= b)` becomes `a ~= b`. That is
  the slipway obstruction context: `self_has_infraspace` goes and the test
  is `sw.infraspace ~= ctx.self_infraspace`. A read written
  `(has ~= 0) and x or nil` becomes `x`.
- The state that `resource_is_bool` (route) and `background_is_bool` (layout
  row) encode is not a `has_` flag: they stay as they are, and are read only
  when the value is present.
- Names stay (module prefixes until step 3.4). `-- Copy of` headers stay
  (step 3.3). A shape comment that lists a deleted field (`has`, `has_`,
  `has_x`, "since the header's `has` holds the ENTITY_HAS_* bits") is
  reworded, following the comments rule.

### Traps every chunk checks

- **Equality with 0 on a value that can now be `nil`.** `nil ~= 0` is true,
  so a test `x ~= 0` or `x == 0` on a newly nil-able value changes meaning
  silently. Each chunk finds every such test of its fields
  (`grep -rnE '\.<field> *[~=]= *0'` and the same for locals) and writes
  `x ~= nil and x ~= 0` where the value may be absent. Known case: the state
  dump's slipway line becomes `s.infraspace ~= nil and s.infraspace ~= 0`.
- **A read without the flag.** Such a read used to get the placeholder or a
  stale value. Without the placeholder it errors on `nil` (loud) or, through
  `== 0` or `or`, changes silently. Each chunk handles it with the
  instrument (below).
- **Identity.** A value refilled in place (`needs_refill`, the incoming
  routes reset, `emitter_release_particles`, the economy maps,
  `g_difficulty`) keeps being refilled in place while present. It gets a new
  table only when it was absent, which nothing else can hold. Emptying a
  value before setting it to `nil` keeps a holder of the old table seeing it
  emptied, as today.
- **pairs copies.** `button_keys_copy` (message box),
  `economy_score_set_difficulty` and `save_load_difficulty` copy fields with
  `pairs`. An absent value is simply not copied, which means the same thing.
  Nothing iterates an entity with `pairs` or prints its fields in an order
  that depends on them (the state dump names each field).

### The temporary instrument

About 670 lines in 45 files match the step's grep at HEAD `a190fe6`. A
reader that relied on the placeholder can't be seen by reading the code, but
an assert over the 20 replays shows it. Each chunk:

1. Lists every read and write of each of its fields in `game/` (search on
   `.<field>`; leave out tables that only share the name).
2. Adds, uncommitted, an `assert` that the flag is set at each read not
   already behind a test of that flag, and at each write of the value that
   does not also set the flag. Runs `make e2e` once and records in the
   chunk's commit message where an assert fired. Removes the instrument.
3. Writes a read where nothing fired as a direct read of the field.
4. Writes a read where an assert fired so that it keeps the old meaning with
   the placeholder it read (`x or 0`, `entity_pos(e)` for a position, `x or
   {}` for a list), with a contract comment saying what an absent value
   reads as. If the old read got a stale value (read after its flag was
   cleared) that differs from the placeholder, the chunk stops and reports:
   the phase-0 decision cannot be carried out as written for that read (plan
   rule).

### Known sites, checked at HEAD `a190fe6`

- The UI's screen mouse point reads the mouse's raw `pos`, bypassing
  `entity_pos`. The mouse sets its position at init and every frame, so the
  read stays direct (`mouse.pos`) and the comment explaining the raw read
  goes.
- The save writes every node's `infraspace` and `fold_anchor` from the
  header. These become `header.infraspace or 0` and `header.fold_anchor or
  0`, with a contract comment ("an absent part is saved as 0"), so every
  save's bytes stay the same. The loader writes `infraspace` back only when
  the saved value is not 0; only an infraspace gate saves one, and the
  gate's constructor already sets the part. The instrument proves it; the
  loader is kept as it is.
- The pause menu copies its box's `fixed_pos`, `row_step` and `back_color`
  into the frame whatever its bits say. It copies each only when present
  (`frame.fixed_pos = box.fixed_pos and vec_copy(box.fixed_pos)`), as the
  score panel layout block's reader already does.
- The pad panel's `pad_panel_entity_under` reads `e.layer` on every live
  entity of the hit-test bucket without the layer bit. If the instrument
  finds an entity without a layer there, the read becomes `e.layer or 0`
  with its contract comment; otherwise it is a direct read. The candidate
  walks (`top_layer`, `is_item_on`) read only candidates, which test the
  layer.
- `slipway_inherit_infraspace` sets the infraspace to 0 and clears its bit
  before copying from an end ("an infraspace of 0 still counts as one"). It
  becomes `self.infraspace = nil`, then a copy from the start, else from the
  end, that sets the field only when that end has one. The layer choice
  that follows tests `self.infraspace ~= nil`. `slipway_display`'s
  `infraspace_truthy` becomes `(self.infraspace ~= nil) and 1 or 0`, or the
  presence test at each use.
- The incoming routes reset (`rebuild_incoming_routes_reset_visit`) empties
  the list before it tests the bit, so the list is emptied even on a planet
  that never had one. With `nil`: empty in place when present, else assign
  `{}`. The route visit appends only when present.
- `ui_forget_world` empties `ephemerals` only under `UI_HAS_OWN_EPHEMERALS`.
  The bit is unset only while `ephemerals` is the fresh `{}` of
  `ui_create` / `ui_ephemerals_release`, so emptying it unconditionally is a
  no-op there. The instrument asserts `#self.ephemerals == 0` wherever the
  bit tested unset.
- `economy_score_set_difficulty` clears and refills `g_difficulty` in place.
  All six callers of `economy_score_difficulty` (cart, economy_score,
  galaxy, score_panel_display, save_state, state_format) read it into a
  local within one call. `g_difficulty` becomes `nil` when absent, is
  refilled in place when present, and is a new table only when it was
  `nil`.
- `pad_select`'s need test `need == nil or need.has_satisfied == 0 or
  need.satisfied == 0` becomes `need == nil or need.satisfied == nil or
  need.satisfied == 0`. Every `(has_satisfied ~= 0) and satisfied or 0`
  becomes `satisfied or 0`.

### Facts about step 1.4's result that this PRD relies on (checked at `a190fe6`)

- `entity_list`, `pointer_list`, `entity_id_list`, `value_list` and `collect`
  are gone; `grep -r last_count game/` finds nothing. `planet.needs`,
  `planet.incoming_routes`, `planet.outgoing_routes`, `emitter.particles`,
  `ui.ephemerals` and the hover layers are plain 1-based arrays.
- `needs_refill` (planet) empties and refills `self.needs` in place.
  `emitter_release_particles` empties the array in place under its bit and
  does not replace it. `emitter_init` releases, then empties in place, then
  sets the bit. The save loader's `load_needs` assigns a new array.
  `load_outgoing_routes` and `planet_trade`'s first offer empty in place.
- `engine.entity_class` still has `entity_has`, the nine `ENTITY_HAS_*`,
  `entity_has_pos` / `entity_has_map_pos` returning 0/1, and `mouse_pos_t`
  with its bits. `ui_set_ephemerals` stores the list it is given; the
  ephemerals readers (pad panel, menu, UI) read it with no test.
- `design.md` § Conventions inherited from the C still has the Naming
  sentence on `has_`. `CLAUDE.md` still says "optional parts flagged by bits
  in `has` rather than by a nil field". `docs/save-format.md` still says
  "mirrors `PLANET_HAS_NEEDS` / `_PRODUCTION` / `_OUTGOING_ROUTES`".
- The phase-1 review's decisions put the `has_<field>` numbers in this step,
  research ids in step 2.2 and non-rule capacity limits in step 2.3.

### Field groups and their decisions

**Display rows** (`map_tables`, `game_data`, `layout`, `score_panel`,
`score_panel_display`, `message_box`, `menu`, `planet`, `planet_display`,
`research_display`, `ui_button`, `help`).

- `has_color`, `has_background` and `has_score` go from data rows, layout
  row keys and layout rows. `color`, `background` and `score` are `nil` when
  absent.
- `background_is_bool` stays and is read only when `background ~= nil`.
- The score rows' `has_score` goes: an unscored entry has no `score`.
- `score_panel_update_add_row`'s `has_value[]` goes: `values[i]` is `nil`
  when the slot has no value. `data_item_assign` into a slot becomes an
  assignment of a new item.

**Resources** (`map_tables`, `research`, `planet`, `trade`, `planet_trade`,
`planet_family_trade`, `economy`, `economy_score`, `economy_tiers`,
`planet_display`, `research_display`, `layout`, `ui_button`, `menu`,
`score_panel`, `pad_select`, `save_state`, `save_load`).

- A data row's `resource`, a production's `resource`, a trade route's or
  trade keys' `resource` and a need's `satisfied` are `nil` when absent.
  `has_resource` and `has_satisfied` go.
- `research.need_resource` returns the nil kind when `row.resource == nil`.
- The research project's `output` is `nil` when absent and `has_output`
  goes. So do `planet_display`'s and `research_display`'s `has_output`
  parameters, fed from the production's `resource`. The row signature
  stores `output or 0` and keeps its `ROW_SIGNATURE_HAS_OUTPUT` bit, which
  is a cache key.
- `trade_extend`'s `resource_is_bool = (has) and is_bool or 1` keeps its
  meaning, with presence read from `resource`.
- `planet_offer_trade`'s locals `has_bonus_resource` / `has_any_resource`
  go: `bonus_resource` / `any_resource` are `nil` unless the lookup finds
  the code. `resource_code_lookup`'s `found, out` signature is unchanged; it
  is an out-parameter, not a field.
- The save record's need is `{ resource }` (`nil` when absent) and its route
  is `{ ..., resource, resource_is_bool }`. The encoder writes the need's
  flag byte as `(resource ~= nil) and 1 or 0` and the route's resource flags
  from presence and `resource_is_bool`. The decoder reads the flag byte into
  a local, sets `reader.failed` when a need's byte is not 0 or 1 (that check
  moves from `resources_are_valid`, so `save_state_decode` still returns 0
  for the same files), and sets `resource` only when the byte is 1. The
  route's `bit.bnot(3)` check stays.

**Research projects** (`map_tables` project records, `research`,
`research_display`, `planet_display`, `economy_tiers`).

- `base_cost`, `byproduct`, `upgrade_tech` and `icon_sprite` are `nil` when
  absent, and their `has_` go.
- `research_project_upgrade_tech` returns `self.upgrade_tech`, which is
  `nil` for "absent" and for the cart's `false`, as today.
- `project_zero` sets each optional field to `nil`; `project_create` stops
  writing their zeros. Its header lines go with the header group, and the
  `project` id fields with step 2.2.

**The planet's parts** (`planet`, `planet_family`, `planet_family_trade`,
`planet_trade`, `planet_display`, `economy`, `economy_score`,
`economy_tiers`, `particle`, `probe`, `pad_select`, `save_state`,
`save_load`, `state_format`, `docs/save-format.md`).

- `planet_init_instance` no longer creates `targeting`, `emitter`,
  `production`, `needs`, `incoming_routes` or `outgoing_routes`, and leaves
  `planet_id`, `production_bonus`, `net_income` and `development_tier` as
  `nil` when the archetype has none.
- `targeting` is created by `structure_init_instance`; `emitter` by the
  ascension and burst inits (`emitter_zero()` then filled); `production` by
  `planet_install_project` and the loader's `load_production` (a new table,
  then filled); `needs` by `planet_install_project` (a new array holding the
  refilled elements when absent, refilled in place when present) and
  `load_needs`; `outgoing_routes` where the bit was set (the burst's
  create, the first offer, the route load).
- Where the bit was cleared, the field is set to `nil`.
- `planet_production` / `planet_production_read` return `self.production or
  g_planet_production_shared`, with their contract comment kept.
  `planet_id_or_null` returns `self.planet_id`. `emitter_of` returns
  `e.emitter` for a planet.
- `has_production_bonus`, `has_net_income` and `has_development_tier`
  (planet and planet archetype) go. `economy_tiers`' local
  `has_development_tier` goes with them.
- The planet keys' `has_planet_id` goes: `planet_id` is `nil` when not
  given. The keys' positions belong to the positions group.
- The seven `PLANET_HAS_*` and the planet's `has_` are deleted.
- `docs/save-format.md`: the flag rows that "mirror `PLANET_HAS_*`" say
  instead that the flag is set when the planet holds needs, its own
  production, or outgoing routes. The format does not change.

**Particles, probes and targeting** (`particle`, `planet_family`, `probe`,
`help`, `targeting`, `game_data`, `structure_icon`, `slipway`, `wormhole`,
`tech_gate`, `cheat`, `score_panel`, `save_state`, `save_load`,
`state_format`).

- `particles` is `nil` when the emitter holds none. `emitter_init` releases,
  then assigns `{}`. `emitter_release_particles` returns at once when `nil`,
  else empties in place and then assigns `nil`. Spawn, update, draw and
  burst test `emitter.particles ~= nil`. `emitter_zero` has no `has` and no
  `particles`; the inits' `self.emitter.has = 0` lines go.
  `EMITTER_HAS_PARTICLES` is deleted.
- A particle's `acceleration` is `nil` when it has none.
- The probe keys' `angle_slices`, `base_radius` and `speed_factor` are
  `nil` when not given.
- The targeting's `cost` (instance and archetype) and the structure icon's
  kind record and view `sprite` and `cost` are `nil` when absent.
  `has_cost` and `has_sprite` go, and so does the `kind_record` positional
  constructor's `has_sprite` / `has_cost` parameters.
- `probe`'s local `has_target` goes in favour of the target itself.
- The economy placeholder's `credits` and `research_points` are `nil` until
  the cheat sets them.
- The save record's `probe` is `nil` when no probe is saved. The encoder
  writes `(probe ~= nil) and 1 or 0`. The decoder reads the byte into a
  local, sets `reader.failed` on a byte other than 0 or 1 (moved from
  `state_is_valid`'s `is_flag(state.has_probe)`, with the same decode
  result), and builds `probe` only on 1. The loader tests `state.probe ~=
  nil`.

**The economy** (`economy`, `main_economy`, `economy_score`, `tech_gate`,
`score_panel`, `score_panel_display`, `cart`).

- `production` and `demand` are `nil` until the first recompute. That
  recompute assigns each a new `{ count = 0, entries = {} }` when `nil`,
  then fills it in place as today; from then on both keep their identity. If
  the instrument shows a read of either before the first recompute, they
  stay created at init and only the bits go (the maps are then always
  present and the readers drop their test).
- `happiness` is `nil` until the first recompute sets it. `end_status` is
  already `nil` until set; `has_end_status` goes, and the readers' `has ~= 0
  and end_status ~= nil` becomes `end_status ~= nil`.
- `tech_gate`'s `research_cost_with_discount(od, has_planet_type, nk)`
  becomes `(od, nk)` with `nk` `nil` when there is no planet type.
- `ECONOMY_HAS_*` and the economy's `has_` go.

**UI, buttons and icons** (`ui`, `ui_button`, `mouse`, `mouse_button`,
`pad_panel`, `menu`, `planet_display`, `message_box`, `score_panel_display`,
`help`, `structure_icon`, `economy_score`, `cart`, `save_load`, `wormhole`).

- `mouse_world` is `nil` until the first update writes it, and
  `ui_mouse_pos` tests it.
- `hovered_by_layer` is `nil` until the first hover pass builds it (the same
  layer tables `ui_create` builds today). `ui_send_to_hovered` returns "not
  found" when it is `nil`; `ui_forget_world` and `ui_hovered_layer` treat
  `nil` as "nothing written".
- `UI_HAS_OWN_EPHEMERALS` goes and no field takes its place (known sites
  above).
- The button's `slide_target`, `on_click`, `trade`, `type_bias` and
  `project_cost`, and the same keys, are `nil` when not given.
  `BUTTON_HAS_SLIDE_TARGET`, the button's `has_` and its `has_<x>` go. The
  keys' `layer`, `tooltip_row` and `side_tooltip_row` are `nil` when not
  given, created by the caller that fills them.
- The mouse button's `held` is `nil` until the first update.
  `mouse_button_update` reads `button.held or 0` as `was_held`;
  `ui_main_mouse_button` tests `b.held ~= nil`.
- The structure icon's `is_revealed` is `nil` until revealed; its readers
  test `is_revealed ~= nil and is_revealed ~= 0`.
- `g_cursor` and `g_id_counter` are `nil` when absent;
  `structure_icon_has_cursor` and `structure_icon_id_counter` keep their 0/1
  results computed from presence.
- `cart`'s `g_requested_option` is `nil` without a chosen option.
  `g_difficulty` follows the known site above.
- `WORMHOLE_HAS_TOO_FAR_TARGET` and the wormhole's `has_` go (never read).

**Boxes and panels** (`message_box`, `menu`, `score_panel`,
`score_panel_display`, `help`, `planet_display`).

- The open-box frame's `description`, `fixed_pos`, `row_step` and
  `back_color` are `nil` when absent; `open_box_frame_zero` lists none of
  them.
- `message_box_open_box`'s `has_dispatcher` / `open_box_add`'s `has_list`
  go: `open_box_add(list, button)` appends when the list is not `nil`, and
  the box passes `nil` without a dispatcher.
- The message box keys' `duration_scale` and the toast keys' `text` are
  `nil` when not given.
- The score panel layout block's and the pause menu box's `fixed_pos` and
  `row_step` are `nil` when absent. The menu's init sets them and its
  shutdown sets them to `nil`. Both copy each only when present. The box's
  `back_color` follows the same rule.
- The score panel's `is_shown` and `curtain` are `nil` until set; the
  toggle reads `is_shown ~= nil and is_shown ~= 0`.
- `SCORE_PANEL_LAYOUT_BLOCK_HAS_*`, `PAUSE_MENU_OPEN_BOX_HAS_*` and both
  `has_` go.

**Positions** (`engine.entity_class`, `entity_geom`, `planet`,
`planet_family`, `planet_family_trade`, `planet_display`, `galaxy`,
`save_load`, `save_state`, `layout`, `message_box`, `menu`, `help`,
`probe`, `ui_button`, `score_panel_display`, `game_data`, `structure_icon`).

- `entity_set_pos` / `entity_set_map_pos` assign a copy.
  `entity_clear_pos` / `entity_clear_map_pos` assign `nil`. `entity_pos` /
  `entity_map_pos` return a copy, or the zero vector when `nil`.
  `entity_has_pos` / `entity_has_map_pos` return 0/1 from presence.
- `ENTITY_HAS_POS` and `ENTITY_HAS_MAP_POS` go.
- The planet's in-place bob of `self.pos.y` stays behind its presence test.
- Readers that copy `self.pos` raw next to a `has_pos` value (help, the
  planet display's frame and keys) read `entity_pos(self)`. Those entities
  never clear their position, so the raw value and the zero vector agree
  when it is absent; the instrument proves it.
- Every `has_pos` / `has_map_pos` of keys, frames, layout row keys, score
  rows and archetypes goes; `pos` / `map_pos` (or `pos_x`/`pos_y` in a
  score row) is `nil` when not given. The `keys.has_map_pos == 0 and
  keys.has_pos == 0` asserts become `keys.map_pos == nil and keys.pos ==
  nil`, with their messages.

**The header's other parts** (`engine.entity_class`, `entity_registry`,
`entity_geom`, `bootstrap`, `research`, and every kind's init: background,
help, message box, particle, planet, planet family trade, probe, scroll,
slipway, slipway display, structure icon, ui, ui button, wormhole,
`pad_panel`, `state_format`).

- `entity_init_header` writes `class`, `state`, `state_time`, `is_dead = 0`
  and `id = nil`, and nothing else.
- `layer` is set only by `entity_registry_set_layer`, which asserts it is
  not `nil`. Every `entity_has(e, ENTITY_HAS_LAYER)` becomes `e.layer ~=
  nil`.
- `hit_box`, `shake`, `fold_anchor` and `infraspace` are present only on the
  kinds that set them today. Their inits set the value only. The
  `entity_geom` shake assert becomes `e.shake ~= nil`, with its message.
- `wormhole`'s `fold_anchor` test becomes `p.fold_anchor ~= nil and
  p.fold_anchor ~= 0`.
- `is_dead` is on every entity. The bit goes; the bootstrap update walk
  tests `qc.is_dead ~= 0` alone. The instrument proves that no entity
  without the bit is ever marked dead; at HEAD every writer of 1 is a kind
  that sets the bit. `probe`'s "carries the member" assert goes.
- `is_hovered` is present (0 or 1) only on the kinds that set the bit
  (background, help, planet, structure icon, button). The UI's two asserts
  become `~= nil`, with their messages.
- `project_zero` stops writing header fields other than those
  `entity_init_header` writes.
- `entity_has` and the remaining `ENTITY_HAS_*` are deleted. `mouse_pos_t`,
  `MOUSE_POS_HAS_*` and its four functions stay unchanged.

### What stays, and why (the step note's exceptions)

- `SAVE_NODE_*` (`game.save_state`, `game.save_load`): the flag word of the
  on-disk node record, decoded into the save record. The save format's
  layout and its 255-node limit are game rules.
- `ROW_SIGNATURE_HAS_OUTPUT` and `ROW_NEED_HAS_RESOURCE`
  (`game.planet_display`): bits of a cache key that is compared to decide
  whether to rebuild a row; they mark nothing optional.
- `mouse_pos_t` with `MOUSE_POS_HAS_*` and its `has` field
  (`engine.entity_class`, `game.ui`, `game.pad_panel`): a by-value struct
  built per call, not an entity or one of its parts. It uses the same idiom,
  but it is outside the phase-0 decision; it is noted for the phase-end
  `warn-review`.
- `has_refresh = button_has_refresh` (`game.ui_button`,
  `engine.entity_registry`): a class method, not a flag.
- Whatever else the final grep finds is a 0/1 that is not paired with a
  value (a predicate's result stored in a local, `has_color_ramp`'s
  boolean). The implementer lists each one with its reason in the step
  note.

### Docs and skills

- `CLAUDE.md` § The game: "optional parts flagged by bits in `has` rather
  than by a nil field" becomes "optional parts as fields that are `nil` when
  absent".
- `docs/love-port/design.md` § Conventions inherited from the C:
  - **Naming**: the `has_` sentence goes; the trailing-underscore rule keeps
    only the Lua-keyword case.
  - **Structs and out-parameters**: "a static struct ... with every field at
    its C zero value" gains "except an optional value, which is `nil` when
    absent and tested with `~= nil`; no `has` bit or `has_<x>` number marks
    it", and "`is_dead` is on every entity".
  - Any other sentence that names `has` bits is fixed in the same chunk.
- `docs/save-format.md`: the `PLANET_HAS_*` rows as above, plus any
  sentence that names an in-memory `has_` field that no longer exists. The
  on-disk byte names (`has_probe`, need `has_resource`, resource flags) stay:
  they name bytes.
- `.claude/skills/warn-review/SKILL.md`, rule 13 ("Presence or absence
  encoded by a `nil` field instead of a bit in the table's `has`"), becomes
  its inverse: "Presence or absence encoded by a bit or a `has_<x>` 0/1
  number instead of a field that is `nil` when absent". The number 13 is
  kept, so warnings stay comparable. In `.claude/skills/tdd/interfaces-design.md`,
  "presence is a bit in `has`, never a `nil` field" becomes "presence is a
  field that is `nil` when absent, never a bit or a `has_` flag". Left as
  they are, both would have the phase-end `warn-review` flag this step's
  result as a violation.
- No e2e note, `.json`, `.sha256` or `.end-state` is touched.

### Decided without `guideline`

- **`is_dead` stays on every entity.** Readers on arbitrary entities test
  `is_dead == 0`, and a `nil` would silently make that false.
- **`is_hovered` becomes a part.** Its only writer outside the inits is the
  hover pass, and that pass is guarded by an assert stating exactly that
  contract.
- **`UI_HAS_OWN_EPHEMERALS` goes without a nil-able field.** Every reader
  reads `ephemerals` as an array.
- **The save record converts too.** Its `has_probe` and `has_resource` are
  0/1 numbers paired with values. The bytes on disk are produced from
  presence. The byte check moves from validation into the decoder, and
  `save_state_decode`, which returns only 1 or 0, accepts the same files.
- **Module-level `g_has_<x>` state is in scope.** The grep cannot see it,
  but it is the same idiom.
- **`mouse_pos_t`, the save node flags and the row signature bits stay.**
  They are not optional values of an entity, and the plan's check allows
  them when listed.
- **The skills are updated in the step that makes them false.** The plan's
  rule names `design.md` and `CLAUDE.md`. The skills are read by the
  phase-end review, so leaving them would turn this step's result into
  warnings.
- **Chunks follow field groups, each with all its readers and writers, the
  header last.** The header chunk touches every kind's init, so it lands on
  code whose other values are already settled. Positions come just before
  the header because `entity_class` is shared by both.

## Implementation Chunks

Each chunk is verified before its commit by:

- `make e2e` green with every `.json.sha256` unchanged;
- `make test` green;
- its restricted check holding (false before the chunk, true after);
- the instrument's result recorded in its commit message.

A chunk converts every field of its group and every reader and writer of
it; no field is left half converted. A chunk may leave a bridge such as
`(x ~= nil) and 1 or 0` where a later chunk's flag still consumes a 0/1.

1. **Display rows.** `has_color`, `has_background` and `has_score` (data rows, layout row keys and rows, score rows) and `score_panel_update_add_row`'s `has_value`, with every caller. Check: `grep -rnwE 'has_(color|background|score|value)' game/` finds nothing.
2. **Resources.** `has_resource` (data rows, production, trade routes and keys, save need and route records), `has_satisfied`, `has_output` (project, row parameters), `has_bonus_resource`, `has_any_resource`, with every caller and the decoder's flag-byte check. Check: `grep -rnwE 'has_(resource|satisfied|output|bonus_resource|any_resource)' game/` finds nothing.
3. **Research projects.** `has_base_cost`, `has_upgrade_tech`, `has_byproduct` and `has_icon_sprite` (project records, projects, `project_zero`, `project_create`), with every caller. Check: `grep -rnwE 'has_(base_cost|upgrade_tech|byproduct|icon_sprite)' game/` finds nothing.
4. **The planet's parts.** The seven `PLANET_HAS_*`, the planet's `has_`, `has_production_bonus`, `has_net_income`, `has_development_tier` (planet, archetype, `economy_tiers` local) and `has_planet_id`, with every caller and `docs/save-format.md`. Check: `grep -rnE 'PLANET_HAS_|\bhas_(production_bonus|net_income|development_tier|planet_id)\b' game/ docs/` finds nothing.
5. **Particles, probes and targeting.** `EMITTER_HAS_PARTICLES` and the emitter's `has`, `has_acceleration`, the probe keys' `has_angle_slices` / `has_base_radius` / `has_speed_factor`, `has_cost` and `has_sprite` (targeting, archetypes, kind records, structure icon view), `has_target`, the placeholder's `has_credits` / `has_research_points` and the save record's `has_probe`, with every caller. Check: `grep -rnE 'EMITTER_HAS_|emitter\.has\b|\bhas_(acceleration|angle_slices|base_radius|speed_factor|cost|sprite|target|credits|research_points|probe)\b' game/` finds nothing.
6. **The economy.** `ECONOMY_HAS_*`, the economy's `has_`, `has_happiness`, `has_end_status` and `tech_gate`'s `has_planet_type`, with every caller. Check: `grep -rnE 'ECONOMY_HAS_|\bhas_(happiness|end_status|planet_type)\b' game/` finds nothing.
7. **UI, buttons and icons.** `UI_HAS_*`, `BUTTON_HAS_SLIDE_TARGET`, `WORMHOLE_HAS_TOO_FAR_TARGET` and the `has_` of the UI, button and wormhole; the button and key flags `has_on_click`, `has_trade`, `has_type_bias`, `has_project_cost`, `has_side_tooltip`, `has_side_tooltip_row`, `has_tooltip`, `has_tooltip_row`, `has_layer`, `has_slide_target` and `side_tooltip_is_false`; `has_held`, `has_is_revealed`, `g_has_cursor`, `g_has_id_counter`, `g_has_requested_option` and `g_has_difficulty`; with every caller. Check: `grep -rnE '(UI|BUTTON|WORMHOLE)_HAS_|\bhas_(on_click|trade|type_bias|project_cost|side_tooltip|side_tooltip_row|tooltip|tooltip_row|layer|slide_target|held|is_revealed)\b|g_has_|side_tooltip_is_false' game/` finds nothing.
8. **Boxes and panels.** `SCORE_PANEL_LAYOUT_BLOCK_HAS_*`, `PAUSE_MENU_OPEN_BOX_HAS_*` and both `has_`; the frame and key flags `has_description`, `has_fixed_pos`, `has_row_step`, `has_back_color`, `has_text` and `has_duration_scale`; `has_list`, `has_dispatcher`, `has_is_shown` and `has_curtain`; with every caller. Check: `grep -rnE '(SCORE_PANEL_LAYOUT_BLOCK|PAUSE_MENU_OPEN_BOX)_HAS_|\bhas_(description|fixed_pos|row_step|back_color|text|duration_scale|list|dispatcher|is_shown|curtain)\b|\bhas_\b' game/` finds nothing (no own `has_` field is left on any kind).
9. **Positions.** `ENTITY_HAS_POS`, `ENTITY_HAS_MAP_POS`, the six position functions and every `has_pos` / `has_map_pos` of keys, frames, layout rows, score rows and archetypes, with every caller. Check: `grep -rnE 'ENTITY_HAS_(MAP_)?POS|\bhas_(map_)?pos\b' game/` finds nothing.
10. **The header's other parts, the docs and the skills.** The other seven `ENTITY_HAS_*`, `entity_has`, the header's `has`, `entity_init_header`, `entity_registry_set_layer`, `project_zero`'s header lines, every kind's init and every reader; `self_has_infraspace` and `infraspace_truthy`; `CLAUDE.md`, `design.md`, `docs/save-format.md` and the two skills. Full check:
    - the step's grep lists only the exceptions under "What stays"; the implementer reads the list and writes it, with reasons, in the step note;
    - `make test-all` green;
    - `make e2e end-state` passing with every `.json.sha256` and `.end-state` unchanged;
    - the `make e2e` time recorded.

## Testing Decisions

- **The replays are the test.** This step changes how optional values are
  represented, not what the game does. Optional values reach the captures
  (hover, layers, hit tests, positions, particles, menus, tooltips, rows),
  the end states (infragate slipways, planet ids, needs, routes, credits)
  and the saves (infraspace, fold anchor, needs, production, routes, probe).
  A value wrongly absent, wrongly present, or read as a different zero
  shows up there.
- **The instrument.** Each chunk's temporary instrument turns "this reader
  only runs while the value is present" from a reading of the code into
  evidence over the 20 replays. It is removed before the commit, so `make
  e2e` gets no slower.
- **No new unit test.** The only module under `tests/lua/game/` (economy
  formulas) has no optional value. A new test file would make `make test`
  slower, which is forbidden. The contracts that matter (presence, what an
  absent value reads as, the save bytes) show only in play and in saves.
  The save decoder's flag-byte rejection moves but keeps its result; no
  replay feeds a corrupt save, and none is added.
- **The static net.** `lua-guard` catches a leftover use of a deleted
  constant only when it is a global. A leftover `M.PLANET_HAS_X` read is a
  `nil` passed to `bit.band`, which errors at once in the replays. Each
  chunk's restricted grep is the static net for the rest.
- **No other tests change.** No test under `tests/` or `tools/` names a
  `has` bit or a `has_` field of `game/` (checked at HEAD).
- **Prior art.** Steps 1.2, 1.3 and 1.4, chunked under the same e2e net; the
  per-walk temporary check of step 1.4's chunks, which this instrument
  copies.

## Out of Scope

- `mouse_pos_t`, the save format's node flags and the row signature bits
  (kept, listed above).
- Booleans (step 3.1): predicates and stored presence values keep 0/1, and
  `is_dead`, `is_hovered`, `is_shown`, `is_revealed` and `held` keep 0/1.
  Out-parameters such as `resource_code_lookup`'s `found, out` stay.
- Research project ids `{ index, epoch }` (step 2.2): the planet's and
  button's `project` fields and `research_project_id_none()` are untouched.
- Capacity limits (step 2.3): `OPEN_BOX_DESCRIPTION_CAP`,
  `PLANET_ID_SIZE`, `LAYOUT_ROW_KEYS_CAP` and the rest stay.
- 0-based arrays (step 3.2), `-- Copy of` headers (step 3.3), module
  prefixes (step 3.4), and replacing `design.md`'s section (step 3.5),
  beyond the sentences this step makes false.
- Fixing any quirk the flags encoded on purpose: the shared production
  written through by planets without their own, an infraspace of 0
  counting as one, the incoming routes emptied on a planet without them.

## Further Notes

- **The step note records:**
  - the step's check with its exceptions (`SAVE_NODE_*` in
    `game.save_state` and `game.save_load`; `ROW_SIGNATURE_HAS_OUTPUT` and
    `ROW_NEED_HAS_RESOURCE` in `game.planet_display`; `MOUSE_POS_HAS_*` and
    `mouse_pos_t`'s `has` in `engine.entity_class`, `game.ui` and
    `game.pad_panel`; `has_refresh` in `game.ui_button`; anything else with
    its reason);
  - the instrument's results per chunk (which sites fired and what form
    they took);
  - how long `make e2e` took.
- **Scale at HEAD `a190fe6`:** 671 lines match the step's grep, in 45 files.
  `game_data.lua` (50), `save_state.lua` (64), `ui_button.lua` (45) and
  `planet.lua` (40) are the largest. There are 42 `*_HAS_*`
  constants, of which 10 are `SAVE_NODE_*`, 2 are row-signature bits and 2
  are `MOUSE_POS_HAS_*`, and about 55 distinct `has_<x>` names.
- **Expected effect on speed:** fewer `bit` calls and smaller tables; `make
  e2e` (4.9 s after step 1.4) should not grow.
- **For the phase-end `warn-review`:** `mouse_pos_t` is the one place left
  using the bit-set idiom for an optional value.
