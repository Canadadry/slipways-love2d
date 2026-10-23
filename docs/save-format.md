# Save format

The binary record that `game/game/save_state.lua` writes and reads: one save of a running game, pointer-free, at most `SAVE_STATE_MAX_BYTES` = 8192 bytes. This page describes version 1 as the code has it. Any change to the encoder or the decoder updates this page in the same commit.

The module (`require("game.save_state")`) has four entry points. A byte buffer is a 0-indexed array of integers 0–255, filled in place.

- `save_state_capture(out)` reads the live game into the save state table `out`, changing nothing else;
- `save_state_encode(state, buffer)` writes a save state table to `buffer` and returns the record's byte count, asserting that the record fits 8 KiB; it writes what it is given and validates nothing;
- `save_state_decode(bytes, count, out)` reads `count` bytes back into the table `out` and returns true for a valid record, false otherwise, never asserting on the bytes;
- `save_state_crc32(bytes, count)` is the trailer checksum.

`save_state_zero(out)` resets a table to the empty save state. The module exports the constants this page names as `M.SAVE_STATE_*` fields (`M.SAVE_STATE_MAX_BYTES`) and `M.SAVE_NODE_*` fields (`M.SAVE_NODE_HAS_POS` for the flag `HAS_POS`).

The largest record of the `worst-case/` games, measured with the C game before plan 4 deleted it, is 4519 bytes (`worst-case/full_game.json`, frame 5964), 3673 bytes under the limit. If the size assert ever fires, the encoding gets more compact; the limit never moves.

When the record format changes (a version bump, a new field), `tests/e2e/pad_resume_at_start.json`'s `initial_save` stops decoding and its hash changes: the fixture must be re-cut from `pad_research`'s last record, as `tests/e2e/pad_resume_at_start.txt` records, and that one hash rebaselined. No target writes a save to a file today, so re-cutting it first needs one (a leftover for plan 5).

Visual-only effects (particles, the probe and title emitters, harvest bursts, transit dots, the flicker of a vanishing planet) draw from a second random stream, `game/game/visual_random.lua`, that the record does not hold. The stream is seeded once at boot and never reseeded: a resumed game draws from wherever the stream stands at the resume, and particles restart from nothing. `visual_random_state` and `visual_random_set_state` read and set the stream; nothing in `game/` calls them.

## Encoding rules

- Every multi-byte value is little-endian and written byte by byte.
- `u8`, `u16`, `u32` are unsigned; `i32` is a two's-complement 32-bit value. Every fixed-point game value is its raw 16.16 `i32`.
- Every reference is a table index. Node and slipway indices are one byte, so a save holds at most 255 nodes and 255 slipways.
- A field marked "if `FLAG`" is present only when that bit of the node's flags is set; otherwise it takes no byte, and the decoder leaves it at 0.

## Header (8 bytes)

| Offset | Width | Field | Value |
|---|---|---|---|
| 0 | 4 bytes | magic | ASCII `SLWY` |
| 4 | u16 | version | 1 |
| 6 | u16 | payload length | byte count of the payload, header and trailer excluded |

## Payload

In this order.

### Globals

| Field | Width | Valid range |
|---|---|---|
| difficulty | u8 | 1–4, the 1-based `difficulty_options_table` entry |
| credits | i32 | any |
| research_points | i32 | any |
| era | i32 | any |
| year | i32 | any |
| year_ticks | i32 | any |
| invented count | u8 | 0–19 |
| invented tech, × invented count | u8 each | 0–18, a `technologies_table` index; no duplicate; in invention order |
| `rnd_state` | u32 | any; the game stream, which only game decisions draw from |
| `background_seed` | u32 | any; the `rnd_state` the galaxy background drew its tile map from |
| type picker tickets count | u8 | 0–66 (`WEIGHTED_PICKER_TICKETS_CAP`) |
| ticket, × tickets count | u8 each | 0–10, the 0-based rank of a `planet_types_table` entry (its position − 1) |
| recent, × 3 | u8 each | 0–10 when its recent_set is 1; anything when 0 (capture writes 0) |
| recent_set, × 3 | u8 each | 0 or 1 |
| type picker multiplier | i32 | any |
| type picker walk step | i32 | any |
| type picker walk bias | i32 | any |
| vanish walk step | i32 | any |
| vanish walk bias | i32 | any |
| structure id counter | i32 | any; 0 when no structure was ever built |
| has_probe | u8 | 0 or 1 |
| probe pos x, y | i32 × 2 | any; only when has_probe is 1 |
| probe map_pos x, y | i32 × 2 | any; only when has_probe is 1 |
| probe fold_anchor | i32 | any, nonzero loads as anchored (capture writes 0x10000 or 0); only when has_probe is 1 |
| camera map_pos x, y | i32 × 2 | any |
| node count | u8 | 0–255 |
| node record, × node count | see below | |
| slipway count | u8 | 0–255 |
| slipway record, × slipway count | start u8, end u8, state_time i32 | start and end below node count, start ≠ end; state_time any |

### Node record

| Field | Width | Presence | Valid range |
|---|---|---|---|
| kind | u8 | always | 0–8 (below) |
| state | u8 | always | `NORMAL` 1, `SETTLED` 2, `VANISHING` 3, `UNKNOWN` 6, `KNOWN` 7; never `NONE` 0, `PLACING` 4, `UNFOLDING` 5 or ≥ `STATE_COUNT` 8 |
| flags | u16 | always | bits 0–10 only (below) |
| pos x, y | i32 × 2 | if `HAS_POS` | any |
| map_pos x, y | i32 × 2 | if `HAS_MAP_POS` | any |
| planet id length | u8 | if `HAS_PLANET_ID` | 1–7 |
| planet id bytes, × length | u8 each | if `HAS_PLANET_ID` | printable ASCII 0x21–0x7E, no NUL |
| fold_anchor | i32 | always | any; nonzero loads as anchored (capture writes 0x10000 or 0) |
| infraspace | i32 | always | any; 0 for a node without one, nonzero loads as one (capture writes 0x10000) |
| planet_type | u8 | always | 0 = none, 1–11 the 1-based `planet_types_table` entry |
| description_type | u8 | always | 0 = no description, 1–11 the 1-based `planet_types_table` entry whose `description` the node holds (terraform leaves the old type's) |
| project | u8 | always | 0 = none, 1–42 the 1-based `research_projects_at` entry |
| need count | u8 | if `HAS_NEEDS` | 0–8 |
| need has_resource, × need count | u8 | if `HAS_NEEDS` | 0 or 1 |
| need resource | i32 | after each need whose has_resource is 1 | one of the 13 `resource_codes` values, as an integer in 16.16 |
| development_tier | i32 | if `HAS_DEVELOPMENT_TIER` | any |
| production_bonus | i32 | if `HAS_PRODUCTION_BONUS` | any |
| production resource | i32 | if `PRODUCTION_HAS_RESOURCE` | one of the 13 `resource_codes` values, as an integer in 16.16 |
| state_time | i32 | if `HAS_STATE_TIME` | any |
| route count | u16 | if `HAS_OUTGOING_ROUTES` | the route pool cap (below) |
| route record, × route count | see below | if `HAS_OUTGOING_ROUTES` | |
| node slipway count | u8 | always | the node slipway pool cap (below) |
| node slipway, × count | u8 each | always | below slipway count; a slipway with this node as an end; no duplicate in the list; in attach order |

Node kinds: 0 planet, 1 protostar, 2 structure, 3 processor, 4 slipgate, 5 infraspace gate, 6 trading hub, 7 synthesizer, 8 ascension.

### Node flags (u16, bit 0 first)

| Bit | Flag | Meaning |
|---|---|---|
| 0 | `HAS_POS` | pos follows |
| 1 | `HAS_MAP_POS` | map_pos follows |
| 2 | `HAS_PLANET_ID` | the planet id follows |
| 3 | `HAS_NEEDS` | set when the planet holds needs; the needs follow |
| 4 | `HAS_DEVELOPMENT_TIER` | development_tier follows |
| 5 | `HAS_PRODUCTION_BONUS` | production_bonus follows |
| 6 | `HAS_PRODUCTION` | set when the planet holds its own production; no field of its own |
| 7 | `PRODUCTION_HAS_RESOURCE` | the production resource follows; requires `HAS_PRODUCTION` |
| 8 | `SYNTHESIZER_SPENT` | the node's `synthesizable_resources` differs from its archetype's; synthesizer kind only; no field |
| 9 | `HAS_OUTGOING_ROUTES` | set when the planet holds outgoing routes; the routes follow |
| 10 | `HAS_STATE_TIME` | state_time follows; set if and only if the state is `VANISHING` or the kind is not planet |

Bits 11–15 are always 0.

### Route record

| Field | Width | Valid range |
|---|---|---|
| planet count | u8 | 0–255 |
| planet, × planet count | u8 each | below node count |
| slipway count | u8 | 0–255 |
| slipway, × slipway count | u8 each | below slipway count |
| resource flags | u8 | bit 0 has_resource, bit 1 resource_is_bool, other bits 0 |
| resource | i32 | only when has_resource; any |

A node's slipway list and its routes name slipways that come later in the bytes. The decoder checks those references once the whole payload is read, before it returns.

## Trailer (4 bytes)

| Width | Field |
|---|---|
| u32 | CRC-32 of every byte before it, header and payload |

The CRC is CRC-32 IEEE 802.3, reflected: polynomial `0xEDB88320`, initial value `0xFFFFFFFF`, final XOR `0xFFFFFFFF`. Check value: `"123456789"` gives `0xCBF43926`.

## In memory: the save state table

Decode fills one flat table, with the field names listed at the head of `game/game/save_state.lua`; `background_seed`, like `rnd_state`, is an unsigned 32-bit value (0 to 2^32 − 1), every `i32` its signed raw value. Each node's routes and slipway list live in shared pools, filled in node order (`routes`, `route_indices` for the route planet and slipway indices, `node_slipways`); a node or route holds a first index and a count into them. Those offsets never appear in the bytes: the encoder writes each slice inline in its node record. Pool caps, exceeded means invalid: `SAVE_STATE_ROUTES_CAP` = 8192 / 3 routes, `SAVE_STATE_ROUTE_INDICES_CAP` = 8192 indices, `SAVE_STATE_NODE_SLIPWAYS_CAP` = 510 node slipway entries. A decoded planet id is a Lua string. Every array of the table is 0-indexed.

## Validation

`save_state_decode` returns false on the first failure, in this order:

1. the byte count is below 12 (header and trailer) or above 8192;
2. the magic is not `SLWY`;
3. the version is not 1;
4. the payload length is not the byte count minus 12;
5. the CRC does not match;
6. while parsing: a read past the payload's end; an invented count above 19; a tickets count above 66; has_probe outside 0/1; a planet id length of 0 or above 7, or a byte outside 0x21–0x7E; a need count above 8; a need has_resource outside 0/1; unknown route resource flag bits; a pool cap exceeded;
7. a payload byte left unread;
8. the field rules: difficulty outside 1–4; a tech ≥ 19 or a duplicate tech; a ticket ≥ 11; a recent_set outside 0/1, or a recent ≥ 11 whose recent_set is 1; a node kind ≥ 9; a forbidden node state; an unknown flag bit; `PRODUCTION_HAS_RESOURCE` without `HAS_PRODUCTION`; `SYNTHESIZER_SPENT` on another kind; `HAS_STATE_TIME` not matching (`VANISHING` or kind ≠ planet); planet_type or description_type above 11; project above 42; a need or production resource that is not a `resource_codes` value; a slipway end ≥ node count, or start equal to end; a route planet ≥ node count or route slipway ≥ slipway count; a node slipway entry ≥ slipway count, not having the node as an end, or twice in the node's list.

Values that are not references (credits, positions, tiers, bonuses, route resources, timers) are not range-checked: they are game values, and the game's readers bound their own table lookups. On false, the contents of the output table are unspecified.

## What capture reads

- Nodes are the `TAG_NODE` entities, slipways the `TAG_SLIPWAY` entities, each in tag order, skipping dead entities and entities in `PLACING`. A slipway whose end is the mouse point is skipped too.
- The probe is the `TAG_PROBE` entity of the probe class that is not dead and is `SETTLED`: a held probe is not saved, and the loader rebuilds a saved one as `SETTLED`. When there is none, has_probe is 0.
- Every entity id in a route, a slipway or a node slipway list becomes the index of the captured node or slipway; capture asserts if the id names anything it did not capture.
- A node's state_time is saved when the node is `VANISHING` or its kind is not planet: structures bob and a trading hub cycles its palette with it. A planet-kind node reads it only while vanishing, so it is not saved otherwise. Every slipway's state_time is saved: its line ends turn with it.
- `background_seed` is the seed of the live background: the RNG state it recorded just before drawing its tile map. The loader sets the RNG to it before building the background, so the 1 KiB map itself is never saved.

## What the save never holds

Wormholes, entities in `PLACING`, dead entities, particles, emitters, bursts, fades, shakes, transit dots, `served_planets`, tooltips, `targeted_by`, `layout_row`, message boxes, panels and menus, and every derived field (income, incoming routes, satisfied needs, production amount, demand, happiness, end status).
