-- Copy of src/game/main_economy.h, which has no .c; its two macros became nil fields. Its functions
-- (main_economy, probe_set_main_economy, main_economy_typed, main_economy_placeholder,
-- main_economy_placeholder_class) are defined in src/game/probe.c and ported with it.
--
-- Shapes, with the C field names:
--   economy_archetype_t: { credits, era, year_ticks, year }.
--   economy_t: the entity header plus { archetype, credits, income, research_points, era,
--     invented, year_ticks, year, production, demand, happiness, end_status, save_pending }, one
--     table; invented an array of technology names, production and demand resource_map_t tables.
--     production, demand and happiness are nil until the first recompute.
--   main_economy_placeholder_t: the entity header plus { research_points, credits }, one table;
--     each nil until the cheat sets it.
local M = {}

return M
