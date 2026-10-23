-- Copy of src/game/main_economy.h, which has no .c: its two macros as fields. Its functions
-- (main_economy, probe_set_main_economy, main_economy_typed, main_economy_placeholder,
-- main_economy_placeholder_class) are defined in src/game/probe.c and ported with it.
--
-- Shapes, with the C field names:
--   economy_archetype_t: { credits, era, year_ticks, year }.
--   economy_t: the entity header plus { archetype, credits, income, research_points, era,
--     invented, year_ticks, year, production, demand, has_, happiness, has_happiness,
--     end_status, has_end_status, save_pending }, one table; invented an array of
--     technology names, production and demand inline resource_map_t tables. The economy's own
--     `has` (ECONOMY_HAS_* bits) is `has_`, since the header's `has` holds the ENTITY_HAS_* bits.
--   main_economy_placeholder_t: the entity header plus { research_points,
--     has_research_points, credits, has_credits }, one table.
local bit = require("bit")

local M = {}

M.ECONOMY_HAS_PRODUCTION = bit.lshift(1, 0)
M.ECONOMY_HAS_DEMAND = bit.lshift(1, 1)

return M
