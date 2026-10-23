-- Copy of src/game/game_data.h then src/game/game_data.c.
--
-- Shapes, with the C field names (a pointer is the table, NULL is nil, arrays
-- indexed from 0):
--   int_seq_t: { count, items }.
--   planet_types_entry_t: { name, sprite, weight, description, state }.
--   galaxy_menu_entry_t: { pos_x, pos_y, layout_row }.
--   pause_menu_entry_t: { layout_row }.
--   pause_menu_tail_t: { fixed_pos_x, fixed_pos_y, row_step_x, row_step_y,
--     back_color }.
--   difficulty_options_entry_t: { layout_row, name, trade, type_bias }.
--   score_rows_entry_t: { row (a data_row_t held inline), score (nil when unscored),
--     pos_x, pos_y (both nil when the row has no position) }.
--   kind_record_t: { required_tech, sprite, state, cost, archetype }, sprite and cost nil
--     when the kind has none.
--   The *_archetype_t, targeting_archetype_t, emitter_archetype_t and
--     burst_construct_keys_t: their C fields, box_t { left, top, right,
--     bottom } and vec_t { x, y } held inline as new tables.
-- A char[] table is its Lua string, and so is a pointer to one.
-- int_seq_at and game_data_fixed_seq_at return found, value: value is out
-- unchanged when found is 0.
-- Every data table is a module field, also the C statics: a local per table
-- would pass LuaJIT's 200 locals per function.
local map_tables = require("game.map_tables")
local entity_class = require("engine.entity_class")
local vram = require("p8.vram")
local cint = require("p8.cint")
local fixed = require("p8.fixed")

local M = {}

M.UPKEEP_BY_TIER_COUNT = 4
M.PARTNER_BONUS_BY_TIER_COUNT = 4
M.DEVELOPMENT_SCORE_BY_TIER_COUNT = 4
M.HAPPINESS_BY_TIER_COUNT = 4
M.TIER_THRESHOLDS_COUNT = 4
M.LAYER_FADES_COUNT = 15
M.PERIOD_NAMES_COUNT = 15
M.PLANET_TYPES_COUNT = 11
M.GALAXY_MENU_COUNT = 4
M.PAUSE_MENU_COUNT = 4
M.DIFFICULTY_OPTIONS_COUNT = 4
M.SCORE_ROWS_COUNT = 12
M.PLANET_MOOD_ROWS_COUNT = 4
M.PLANET_RING_COLORS_COUNT = 4

local HALF_WIDTH = cint.div(vram.P8_SCREEN_WIDTH, 2)
local HALF_HEIGHT = cint.div(vram.P8_SCREEN_HEIGHT, 2)

local check_fields = map_tables.map_tables_check_fields
local data_item = map_tables.map_tables_make_data_item
local data_row = map_tables.map_tables_make_data_row

local function score_row(fields)
  return check_fields(fields, {
    row = fields.row or data_row({}),
    score = fields.score,
    pos_x = fields.pos_x,
    pos_y = fields.pos_y,
  }, "score_rows_entry_t")
end

M.upkeep_by_tier_table = { [0] = 1, 0, 1, 2 }

M.partner_bonus_by_tier_table = { [0] = -0x5540, 0x0, 0x4000, 0x8000 }

M.development_score_by_tier_table = { [0] = 0, 100, 200, 400 }

M.happiness_by_tier_table = { [0] = -5, 0, 1, 1 }

M.tier_thresholds_table = { [0] = 2, 4, 7, 12 }

M.layer_fades_table = {
  [0] = 0x8000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
}

M.period_names_table = {
  [0] = "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC", "UND", "DUO", "TER",
}

local NONE = entity_class.ENTITY_STATE_NONE

M.planet_types_table = {
  [0] = {
    name = "vanishing", sprite = 45, weight = 0x0, description = nil, state = entity_class.ENTITY_STATE_VANISHING,
  },
  { name = "e", sprite = 160, weight = 0x48000, description = "earth-like", state = NONE },
  { name = "f", sprite = 128, weight = 0x60000, description = "forgeworld", state = NONE },
  { name = "m", sprite = 134, weight = 0x40000, description = "mineral", state = NONE },
  { name = "o", sprite = 130, weight = 0x30000, description = "ocean", state = NONE },
  { name = "r", sprite = 140, weight = 0x28000, description = "remnant", state = NONE },
  { name = "x", sprite = 162, weight = 0x30000, description = "xeno", state = NONE },
  { name = "j", sprite = 136, weight = 0x20000, description = "jungle", state = NONE },
  { name = "i", sprite = 138, weight = 0x20000, description = "iceball", state = NONE },
  { name = "s", sprite = 142, weight = 0x20000, description = "barren", state = NONE },
  { name = "g", sprite = 96, weight = 0x40000, description = "gas giant", state = NONE },
}

M.galaxy_menu_1_layout_row_1_items = { [0] = data_item({ number = 15 }), data_item({ number = 7 }) }
M.galaxy_menu_1_layout_row_1 = data_row({ count = 2, items = M.galaxy_menu_1_layout_row_1_items })
M.galaxy_menu_1_layout_row_items = { [0] = data_item({ row = M.galaxy_menu_1_layout_row_1 }) }
M.galaxy_menu_1_layout_row = data_row({ count = 1, items = M.galaxy_menu_1_layout_row_items })
M.galaxy_menu_2_layout_row_1_items = { [0] = data_item({ number = 37 }), data_item({ number = 7 }) }
M.galaxy_menu_2_layout_row_1 = data_row({ count = 2, items = M.galaxy_menu_2_layout_row_1_items })
M.galaxy_menu_2_layout_row_items = { [0] = data_item({ row = M.galaxy_menu_2_layout_row_1 }) }
M.galaxy_menu_2_layout_row = data_row({ count = 1, items = M.galaxy_menu_2_layout_row_items })
M.galaxy_menu_3_layout_row_1_items = { [0] = data_item({ number = 25 }), data_item({ number = 7 }) }
M.galaxy_menu_3_layout_row_1 = data_row({ count = 2, items = M.galaxy_menu_3_layout_row_1_items })
M.galaxy_menu_3_layout_row_items = { [0] = data_item({ row = M.galaxy_menu_3_layout_row_1 }) }
M.galaxy_menu_3_layout_row = data_row({ count = 1, items = M.galaxy_menu_3_layout_row_items })
M.galaxy_menu_4_layout_row_1_items = { [0] = data_item({ number = 8 }), data_item({ number = 7 }) }
M.galaxy_menu_4_layout_row_1 = data_row({ count = 2, items = M.galaxy_menu_4_layout_row_1_items })
M.galaxy_menu_4_layout_row_items = { [0] = data_item({ row = M.galaxy_menu_4_layout_row_1 }) }
M.galaxy_menu_4_layout_row = data_row({ count = 1, items = M.galaxy_menu_4_layout_row_items })

M.galaxy_menu_table = {
  [0] = { pos_x = vram.P8_SCREEN_WIDTH - 10, pos_y = 0, layout_row = M.galaxy_menu_1_layout_row },
  { pos_x = 0, pos_y = 0, layout_row = M.galaxy_menu_2_layout_row },
  { pos_x = HALF_WIDTH - 17, pos_y = 0, layout_row = M.galaxy_menu_3_layout_row },
  { pos_x = 0, pos_y = vram.P8_SCREEN_HEIGHT - 11, layout_row = M.galaxy_menu_4_layout_row },
}

M.pause_menu_1_layout_row_items = { [0] = data_item({ number = 31 }), data_item({ text = "restart     " }) }
M.pause_menu_1_layout_row = data_row({ count = 2, items = M.pause_menu_1_layout_row_items })
M.pause_menu_2_layout_row_items = { [0] = data_item({ number = 42 }), data_item({ text = "toggle music" }) }
M.pause_menu_2_layout_row = data_row({ count = 2, items = M.pause_menu_2_layout_row_items })
M.pause_menu_3_layout_row_items = { [0] = data_item({ number = 40 }), data_item({ text = "toggle sfx  " }) }
M.pause_menu_3_layout_row = data_row({ count = 2, items = M.pause_menu_3_layout_row_items })
M.pause_menu_4_layout_row_items = { [0] = data_item({ number = 26 }), data_item({ text = "how to play?" }) }
M.pause_menu_4_layout_row = data_row({ count = 2, items = M.pause_menu_4_layout_row_items })

M.pause_menu_table = {
  [0] = { layout_row = M.pause_menu_1_layout_row },
  { layout_row = M.pause_menu_2_layout_row },
  { layout_row = M.pause_menu_3_layout_row },
  { layout_row = M.pause_menu_4_layout_row },
}

M.resume_layout_row_items = { [0] = data_item({ number = 31 }), data_item({ text = "resume      " }) }
M.resume_layout_row = data_row({ count = 2, items = M.resume_layout_row_items })

M.pause_menu_tail = {
  fixed_pos_x = 0, fixed_pos_y = vram.P8_SCREEN_HEIGHT - 21, row_step_x = 0, row_step_y = -11, back_color = 14,
}

M.difficulty_options_1_layout_row_items = { [0] = data_item({ number = 49 }), data_item({ text = "forgiving   " }) }
M.difficulty_options_1_layout_row = data_row({ count = 2, items = M.difficulty_options_1_layout_row_items })
M.difficulty_options_2_layout_row_items = { [0] = data_item({ number = 50 }), data_item({ text = "reasonable  " }) }
M.difficulty_options_2_layout_row = data_row({ count = 2, items = M.difficulty_options_2_layout_row_items })
M.difficulty_options_3_layout_row_items = { [0] = data_item({ number = 51 }), data_item({ text = "challenging " }) }
M.difficulty_options_3_layout_row = data_row({ count = 2, items = M.difficulty_options_3_layout_row_items })
M.difficulty_options_4_layout_row_items = { [0] = data_item({ number = 52 }), data_item({ text = "tough       " }) }
M.difficulty_options_4_layout_row = data_row({ count = 2, items = M.difficulty_options_4_layout_row_items })

M.difficulty_options_table = {
  [0] = { layout_row = M.difficulty_options_1_layout_row, name = "forgiving", trade = 7, type_bias = -0xfd70 },
  { layout_row = M.difficulty_options_2_layout_row, name = "reasonable", trade = 6, type_bias = -0x999a },
  { layout_row = M.difficulty_options_3_layout_row, name = "challenging", trade = 5, type_bias = -0x6666 },
  { layout_row = M.difficulty_options_4_layout_row, name = "tough", trade = 4, type_bias = -0x3334 },
}

M.score_rows_1_items = {
  [0] = data_item({ number = HALF_HEIGHT - 38 }),
  data_item({ number = HALF_HEIGHT + 22 }),
  data_item({ number = 0 }),
}
M.score_rows_2_items = {
  [0] = data_item({ number = HALF_HEIGHT - 13 }),
  data_item({ number = HALF_HEIGHT - 5 }),
  data_item({ number = 1 }),
}
M.score_rows_3_items = {
  [0] = data_item({ number = HALF_HEIGHT + 12 }),
  data_item({ number = HALF_HEIGHT + 13 }),
  data_item({ number = 1 }),
}
M.score_rows_4_items = {
  [0] = data_item({ number = HALF_HEIGHT - 40 }),
  data_item({ number = HALF_HEIGHT - 39 }),
  data_item({ number = 1 }),
}
M.score_rows_5_items = {
  [0] = data_item({ number = HALF_HEIGHT - 31 }),
  data_item({ number = HALF_HEIGHT - 23 }),
  data_item({ number = 1 }),
}
M.score_rows_6_items = { [0] = data_item({ text = "" }) }
M.score_rows_7_items = { [0] = data_item({ number = 55 }), data_item({ text = "planets:   " }) }
M.score_rows_8_items = { [0] = data_item({ number = 19 }), data_item({ text = "population:" }) }
M.score_rows_9_items = { [0] = data_item({ number = 24 }), data_item({ text = "technology:" }) }
M.score_rows_10_1_items = { [0] = data_item({ number = 27 }), data_item({ number = 5 }) }
M.score_rows_10_1 = data_row({ count = 2, items = M.score_rows_10_1_items })
M.score_rows_10_2_items = { [0] = data_item({ is_false = 1 }), data_item({ number = 0 }) }
M.score_rows_10_2 = data_row({ count = 2, items = M.score_rows_10_2_items })
M.score_rows_10_4_items = { [0] = data_item({ number = 25 }), data_item({ number = 5 }) }
M.score_rows_10_4 = data_row({ count = 2, items = M.score_rows_10_4_items })
M.score_rows_10_items = {
  [0] = data_item({ row = M.score_rows_10_1 }),
  data_item({ row = M.score_rows_10_2 }),
  data_item({ is_false = 1 }),
  data_item({ row = M.score_rows_10_4 }),
  data_item({ text = "happiness" }),
}
M.score_rows_11_items = { [0] = data_item({ text = "total:" }), data_item({ text = "" }) }

local DRAW_BAR = map_tables.DRAW_FUNCTION_DRAW_BAR

M.score_rows_table = {
  [0] = score_row({ row = data_row({ count = 3, items = M.score_rows_1_items, draw_function = DRAW_BAR }) }),
  score_row({ row = data_row({ count = 3, items = M.score_rows_2_items, draw_function = DRAW_BAR }) }),
  score_row({ row = data_row({ count = 3, items = M.score_rows_3_items, draw_function = DRAW_BAR }) }),
  score_row({ row = data_row({ count = 3, items = M.score_rows_4_items, draw_function = DRAW_BAR }) }),
  score_row({ row = data_row({ count = 3, items = M.score_rows_5_items, draw_function = DRAW_BAR }) }),
  score_row({
    row = data_row({ count = 1, items = M.score_rows_6_items, color = 5 }),
    score = 0, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 43,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_7_items }),
    score = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 30,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_8_items }),
    score = 2, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 21,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_9_items }),
    score = 3, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 12,
  }),
  score_row({
    row = data_row({ count = 5, items = M.score_rows_10_items }),
    score = 4, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 3,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_11_items }),
    score = 5, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT + 10,
  }),
  score_row({
    row = data_row({ count = 0, items = nil }),
    score = 6, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT + 18,
  }),
}

M.planet_ring_colors_items = { [0] = 2, 3, 9, 12 }
assert(#M.planet_ring_colors_items + 1 == M.PLANET_RING_COLORS_COUNT, "planet_ring_colors_count_matches")
M.planet_ring_colors_seq = { count = M.PLANET_RING_COLORS_COUNT, items = M.planet_ring_colors_items }
M.planet_mood_rows_1_items = { [0] = data_item({ text = "unhappy" }) }
M.planet_mood_rows_2_items = { [0] = data_item({ text = "content" }) }
M.planet_mood_rows_3_items = { [0] = data_item({ text = "prosperous" }) }
M.planet_mood_rows_4_items = { [0] = data_item({ text = "rich" }) }
M.planet_mood_rows_rows = {
  [0] = data_row({ count = 1, items = M.planet_mood_rows_1_items, color = 8 }),
  data_row({ count = 1, items = M.planet_mood_rows_2_items, color = 3 }),
  data_row({ count = 1, items = M.planet_mood_rows_3_items, color = 9 }),
  data_row({ count = 1, items = M.planet_mood_rows_4_items, color = 12 }),
}
M.planet_made_resources_items = { [0] = 12, 36, 24, 25 }
M.planet_made_resources_seq = { count = 4, items = M.planet_made_resources_items }
M.planet_spacer_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = 3 }),
  data_item({ number = 0 }),
  data_item({ number = -1 }),
}
M.planet_spacer_row = data_row({ count = 4, items = M.planet_spacer_items })
M.structure_name_items = { [0] = data_item({ text = "laboratory" }) }
M.structure_name_row = data_row({ count = 1, items = M.structure_name_items, color = 13 })
M.trading_hub_name_items = { [0] = data_item({ text = "trading hub" }) }
M.trading_hub_name_row = data_row({ count = 1, items = M.trading_hub_name_items, color = 13 })
M.processor_name_items = { [0] = data_item({ text = "processor" }) }
M.processor_name_row = data_row({ count = 1, items = M.processor_name_items, color = 13 })
M.ascension_name_items = { [0] = data_item({ text = "ascension nr" }) }
M.ascension_name_row = data_row({ count = 1, items = M.ascension_name_items, color = 13 })
M.protostar_name_items = { [0] = data_item({ text = "protostar" }) }
M.protostar_name_row = data_row({ count = 1, items = M.protostar_name_items, color = 13 })
M.synthesizer_name_items = { [0] = data_item({ text = "synthesizer" }) }
M.synthesizer_name_row = data_row({ count = 1, items = M.synthesizer_name_items, color = 13 })
M.structure_accepted_resources_items = { [0] = 28, 18, 20, 21, 22, 48 }
M.structure_accepted_resources_seq = { count = 6, items = M.structure_accepted_resources_items }
M.trading_hub_accepted_resources_items = { [0] = 28, 48, 18, 20, 21, 22 }
M.trading_hub_accepted_resources_seq = { count = 6, items = M.trading_hub_accepted_resources_items }
M.ascension_trail_colors_items = { [0] = 1, 13, 12, 12, 12, 13, 13, 5, 1, 1, 1 }
M.ascension_trail_colors_seq = { count = 11, items = M.ascension_trail_colors_items }
M.protostar_trail_colors_items = { [0] = 1, 2, 4, 9, 10, 7 }
M.protostar_trail_colors_seq = { count = 6, items = M.protostar_trail_colors_items }
M.synthesizer_trail_colors_items = { [0] = 1, 1, 5, 5, 3, 3, 3, 5, 5, 1 }
M.synthesizer_trail_colors_seq = { count = 10, items = M.synthesizer_trail_colors_items }
M.burst_trail_colors_items = { [0] = 1, 2, 4, 9, 9, 10, 10, 7, 7, 7, 7, 7 }
M.burst_trail_colors_seq = { count = 12, items = M.burst_trail_colors_items }
M.protostar_burst_trail_colors_items = { [0] = 1, 2, 8, 14, 14, 14, 14, 8, 8, 2, 2, 1, 1 }
M.protostar_burst_trail_colors_seq = { count = 13, items = M.protostar_burst_trail_colors_items }
M.slipgate_link_sprites_items = { [0] = 170, 170, 170, 168 }
M.slipgate_link_sprites_seq = { count = 4, items = M.slipgate_link_sprites_items }
M.infraspace_gate_link_sprites_items = { [0] = 78, 78, 78, 76 }
M.infraspace_gate_link_sprites_seq = { count = 4, items = M.infraspace_gate_link_sprites_items }
M.synthesizer_synthesizable_resources_items = { [0] = 28, 20, 21, 22 }
M.synthesizer_synthesizable_resources_seq = { count = 4, items = M.synthesizer_synthesizable_resources_items }
M.probe_trail_colors_items = { [0] = 1, 1, 5, 13, 6 }
M.probe_trail_colors_seq = { count = 5, items = M.probe_trail_colors_items }
M.help_screen_trail_colors_items = { [0] = 0, 1, 1, 2, 4, 9, 10, 9, 4, 2, 1, 1, 1 }
M.help_screen_trail_colors_seq = { count = 13, items = M.help_screen_trail_colors_items }
M.slipway_trail_colors_items = { [0] = 0, 0, 7, 7, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 4, 4, 4, 2, 2, 1 }
M.slipway_trail_colors_seq = { count = 20, items = M.slipway_trail_colors_items }
M.wormhole_color_ramp_items = { [0] = 7, 7, 6, 13, 5, 1, 1, 1, 0 }
M.wormhole_color_ramp_seq = { count = 9, items = M.wormhole_color_ramp_items }
M.background_tile_choices_items = { [0] = 0, 0, 0, 72, 73, 74, 75, 88, 89, 90, 91, 88, 91, 89, 91 }
M.background_tile_choices_seq = { count = 15, items = M.background_tile_choices_items }
M.help_screen_rows_1_items = {
  [0] = data_item({ number = 192 }),
  data_item({ number = HALF_WIDTH - 64 }),
  data_item({ number = HALF_HEIGHT - 53 }),
  data_item({ number = 16 }),
  data_item({ number = 4 }),
}
M.help_screen_rows_1 = data_row({
  count = 5, items = M.help_screen_rows_1_items, draw_function = map_tables.DRAW_FUNCTION_SPR,
})
M.help_screen_rows_2_items = {
  [0] = data_item({ number = 172 }),
  data_item({ number = HALF_WIDTH - 44 }),
  data_item({ number = HALF_HEIGHT + 43 }),
  data_item({ number = 2 }),
  data_item({ number = 2 }),
}
M.help_screen_rows_2 = data_row({
  count = 5, items = M.help_screen_rows_2_items, draw_function = map_tables.DRAW_FUNCTION_SPR,
})
M.help_screen_rows_3_items = {
  [0] = data_item({ text = "need help?" }),
  data_item({ number = HALF_WIDTH - 24 }),
  data_item({ number = HALF_HEIGHT + 46 }),
  data_item({ number = 13 }),
}
M.help_screen_rows_3 = data_row({
  count = 4, items = M.help_screen_rows_3_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.help_screen_rows_4_items = {
  [0] = data_item({ text = "slipways.net/help" }),
  data_item({ number = HALF_WIDTH - 24 }),
  data_item({ number = HALF_HEIGHT + 52 }),
  data_item({ number = 5 }),
}
M.help_screen_rows_4 = data_row({
  count = 4, items = M.help_screen_rows_4_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.help_screen_rows_items = {
  [0] = data_item({ row = M.help_screen_rows_1 }),
  data_item({ row = M.help_screen_rows_2 }),
  data_item({ row = M.help_screen_rows_3 }),
  data_item({ row = M.help_screen_rows_4 }),
}
M.help_screen_rows_row = data_row({ count = 4, items = M.help_screen_rows_items })
M.research_project_row_tail_items = { [0] = data_item({ is_false = 1 }), data_item({ number = 2 }) }
M.research_project_row_tail_row = data_row({ count = 2, items = M.research_project_row_tail_items })
M.score_panel_countdown_rows_1_items = {
  [0] = data_item({ text = "last year!" }),
  data_item({ number = HALF_WIDTH }),
  data_item({ number = HALF_HEIGHT - 16 }),
  data_item({ number = 9 }),
  data_item({ is_fixed = 1, fixed = 0x8000 }),
}
M.score_panel_countdown_rows_1 = data_row({
  count = 5, items = M.score_panel_countdown_rows_1_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_countdown_rows_2_items = {
  [0] = data_item({ text = "2 years remain" }),
  data_item({ number = HALF_WIDTH }),
  data_item({ number = HALF_HEIGHT - 16 }),
  data_item({ number = 2 }),
  data_item({ is_fixed = 1, fixed = 0x8000 }),
}
M.score_panel_countdown_rows_2 = data_row({
  count = 5, items = M.score_panel_countdown_rows_2_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_countdown_rows_5_items = {
  [0] = data_item({ text = "5 years remain" }),
  data_item({ number = HALF_WIDTH }),
  data_item({ number = HALF_HEIGHT - 16 }),
  data_item({ number = 2 }),
  data_item({ is_fixed = 1, fixed = 0x8000 }),
}
M.score_panel_countdown_rows_5 = data_row({
  count = 5, items = M.score_panel_countdown_rows_5_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_countdown_rows_items = {
  [0] = data_item({ row = M.score_panel_countdown_rows_1 }),
  data_item({ row = M.score_panel_countdown_rows_2 }),
  data_item({ is_false = 1 }),
  data_item({ is_false = 1 }),
  data_item({ row = M.score_panel_countdown_rows_5 }),
}
M.score_panel_countdown_rows_row = data_row({ count = 5, items = M.score_panel_countdown_rows_items })
M.score_panel_frame_rows_1_items = {
  [0] = data_item({ number = vram.P8_SCREEN_HEIGHT - 2 }),
  data_item({ number = vram.P8_SCREEN_HEIGHT - 1 }),
  data_item({ number = 1 }),
}
M.score_panel_frame_rows_1 = data_row({
  count = 3, items = M.score_panel_frame_rows_1_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_BAR,
})
M.score_panel_frame_rows_2_items = {
  [0] = data_item({ number = 0 }),
  data_item({ number = 1 }),
  data_item({ number = 1 }),
}
M.score_panel_frame_rows_2 = data_row({
  count = 3, items = M.score_panel_frame_rows_2_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_BAR,
})
M.score_panel_frame_rows_3_items = {
  [0] = data_item({ number = 8 }),
  data_item({ number = vram.P8_SCREEN_HEIGHT - 10 }),
  data_item({ number = 34 }),
}
M.score_panel_frame_rows_3 = data_row({
  count = 3, items = M.score_panel_frame_rows_3_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_HELP_FRAME,
})
M.score_panel_frame_rows_4_items = {
  [0] = data_item({ number = 0 }),
  data_item({ number = -3 }),
  data_item({ number = 26 }),
}
M.score_panel_frame_rows_4 = data_row({
  count = 3, items = M.score_panel_frame_rows_4_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_HELP_FRAME,
})
M.score_panel_frame_rows_5_items = {
  [0] = data_item({ number = vram.P8_SCREEN_WIDTH - 47 }),
  data_item({ number = -3 }),
  data_item({ number = vram.P8_SCREEN_WIDTH - 11 }),
}
M.score_panel_frame_rows_5 = data_row({
  count = 3, items = M.score_panel_frame_rows_5_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_HELP_FRAME,
})
M.score_panel_frame_rows_items = {
  [0] = data_item({ row = M.score_panel_frame_rows_1 }),
  data_item({ row = M.score_panel_frame_rows_2 }),
  data_item({ row = M.score_panel_frame_rows_3 }),
  data_item({ row = M.score_panel_frame_rows_4 }),
  data_item({ row = M.score_panel_frame_rows_5 }),
}
M.score_panel_frame_rows_row = data_row({ count = 5, items = M.score_panel_frame_rows_items })
M.score_panel_year_banner_1_items = {
  [0] = data_item({ number = HALF_HEIGHT - 22 }),
  data_item({ number = HALF_HEIGHT - 21 }),
  data_item({ number = 1 }),
}
M.score_panel_year_banner_1 = data_row({
  count = 3, items = M.score_panel_year_banner_1_items, draw_function = map_tables.DRAW_FUNCTION_DRAW_BAR,
})
M.score_panel_year_banner_2_items = {
  [0] = data_item({ text = "" }),
  data_item({ number = HALF_WIDTH }),
  data_item({ number = HALF_HEIGHT - 24 }),
  data_item({ number = 6 }),
  data_item({ is_fixed = 1, fixed = 0x8000 }),
}
M.score_panel_year_banner_2 = data_row({
  count = 5, items = M.score_panel_year_banner_2_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_year_banner_items = {
  [0] = data_item({ row = M.score_panel_year_banner_1 }),
  data_item({ row = M.score_panel_year_banner_2 }),
}
M.score_panel_year_banner_row = data_row({ count = 2, items = M.score_panel_year_banner_items })
M.score_panel_readouts_1_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = 13 }),
  data_item({ number = vram.P8_SCREEN_HEIGHT - 7 }),
  data_item({ number = 9 }),
}
M.score_panel_readouts_1 = data_row({
  count = 4, items = M.score_panel_readouts_1_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_2_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = 37 }),
  data_item({ number = vram.P8_SCREEN_HEIGHT - 7 }),
  data_item({ number = 2 }),
}
M.score_panel_readouts_2 = data_row({
  count = 4, items = M.score_panel_readouts_2_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_3_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = 13 }),
  data_item({ number = 2 }),
  data_item({ number = 9 }),
}
M.score_panel_readouts_3 = data_row({
  count = 4, items = M.score_panel_readouts_3_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_4_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = 29 }),
  data_item({ number = 2 }),
  data_item({ number = 2 }),
}
M.score_panel_readouts_4 = data_row({
  count = 4, items = M.score_panel_readouts_4_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_5_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = HALF_WIDTH - 5 }),
  data_item({ number = 2 }),
  data_item({ number = 2 }),
}
M.score_panel_readouts_5 = data_row({
  count = 4, items = M.score_panel_readouts_5_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_6_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = vram.P8_SCREEN_WIDTH - 43 }),
  data_item({ number = 2 }),
  data_item({ number = 13 }),
}
M.score_panel_readouts_6 = data_row({
  count = 4, items = M.score_panel_readouts_6_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_7_items = {
  [0] = data_item({ is_false = 1 }),
  data_item({ number = vram.P8_SCREEN_WIDTH - 29 }),
  data_item({ number = 2 }),
  data_item({ number = 13 }),
}
M.score_panel_readouts_7 = data_row({
  count = 4, items = M.score_panel_readouts_7_items, draw_function = map_tables.DRAW_FUNCTION_PRINT_OUTLINED,
})
M.score_panel_readouts_items = {
  [0] = data_item({ row = M.score_panel_readouts_1 }),
  data_item({ row = M.score_panel_readouts_2 }),
  data_item({ row = M.score_panel_readouts_3 }),
  data_item({ row = M.score_panel_readouts_4 }),
  data_item({ row = M.score_panel_readouts_5 }),
  data_item({ row = M.score_panel_readouts_6 }),
  data_item({ row = M.score_panel_readouts_7 }),
}
M.score_panel_readouts_row = data_row({ count = 7, items = M.score_panel_readouts_items })
M.wormhole_too_far_target_status_text = "-too far-"

local NORMAL = entity_class.ENTITY_STATE_NORMAL
local PLACING = entity_class.ENTITY_STATE_PLACING
local SETTLED = entity_class.ENTITY_STATE_SETTLED
local FIXED_INT = fixed.P8_FIXED_INT

local function vec(x, y)
  return { x = x, y = y }
end

local function box(left, top, right, bottom)
  return { left = left, top = top, right = right, bottom = bottom }
end

local function targeting(fields)
  return check_fields(fields, {
    attach_distance = fields.attach_distance or 0,
    cost = fields.cost,
    confirmed_state = fields.confirmed_state or NONE,
    clock_ticks = fields.clock_ticks or 0,
    clock_skips_recompute = fields.clock_skips_recompute or 0,
  }, "targeting_archetype_t")
end

local function emitter(fields)
  return check_fields(fields, {
    angle_slices = fields.angle_slices or 0,
    base_radius = fields.base_radius or 0,
    speed_factor = fields.speed_factor or 0,
    lifetime = fields.lifetime or 0,
    quarter_turn = fields.quarter_turn or 0,
    spawn_rate = fields.spawn_rate or 0,
    trail_colors = fields.trail_colors,
    matrix_xx = fields.matrix_xx or 0,
    matrix_xy = fields.matrix_xy or 0,
    matrix_yx = fields.matrix_yx or 0,
    matrix_yy = fields.matrix_yy or 0,
  }, "emitter_archetype_t")
end

local function probe_archetype(fields)
  return check_fields(fields, {
    state = fields.state or NONE,
    state_time = fields.state_time or 0,
    fold_anchor = fields.fold_anchor or 0,
    layer = fields.layer or 0,
    pos = fields.pos,
    fixed_pos = fields.fixed_pos or vec(0, 0),
    back_color = fields.back_color or 0,
    row_step = fields.row_step or vec(0, 0),
    clear_ephemerals_after = fields.clear_ephemerals_after or 0,
    hit_box = fields.hit_box or box(0, 0, 0, 0),
    rows = fields.rows,
    emitter = fields.emitter or emitter({}),
    targeting = fields.targeting or targeting({}),
  }, "probe_archetype_t")
end

local function kind_record(required_tech, sprite, state, cost, archetype)
  return {
    required_tech = required_tech,
    sprite = sprite,
    state = state,
    cost = cost,
    archetype = archetype,
  }
end

local function planet_archetype(fields)
  return check_fields(fields, {
    state = fields.state or NONE,
    state_time = fields.state_time or 0,
    is_planet = fields.is_planet or false,
    trade_multiplier = fields.trade_multiplier or 0,
    layer = fields.layer or 0,
    hit_box = fields.hit_box or box(0, 0, 0, 0),
    shake = fields.shake or 0,
    ring_colors = fields.ring_colors,
    mood_rows = fields.mood_rows,
    made_resources = fields.made_resources,
    row_step = fields.row_step or vec(0, 0),
    back_color = fields.back_color or 0,
    spacer = fields.spacer,
    development_tier = fields.development_tier,
    project_index = fields.project_index or 0,
    name = fields.name,
    is_structure = fields.is_structure or false,
    sprite = fields.sprite or 0,
    accepted_resources = fields.accepted_resources,
    required_tech = fields.required_tech,
    is_trading_hub = fields.is_trading_hub or 0,
    palette_block = fields.palette_block or 0,
    spawn_offset = fields.spawn_offset or vec(0, 0),
    class_name = fields.class_name,
    acceleration_factor = fields.acceleration_factor or 0,
    speed = fields.speed or 0,
    link_sprites = fields.link_sprites,
    infraspace = fields.infraspace or 0,
    synthesizable_resources = fields.synthesizable_resources,
    targeting = fields.targeting or targeting({}),
    emitter = fields.emitter or emitter({}),
  }, "planet_archetype_t")
end

M.background_archetype = {
  state = NORMAL, state_time = 0x0, layer = 0x10000, hit_test = 0x10000, tile_choices = M.background_tile_choices_seq,
}

M.ui_archetype = {
  state = NORMAL,
  state_time = 0x0,
  layer = 0xf0000,
  panel_box = box(0xa0000, 0xa0000, FIXED_INT(vram.P8_SCREEN_WIDTH - 10), FIXED_INT(vram.P8_SCREEN_HEIGHT - 10)),
}

M.button_archetype = { state = NORMAL, state_time = 0x0, layer = 0xb0000 }

M.modal_backdrop_archetype = { state = NORMAL, state_time = 0x0, layer = 0xe0000, hit_test = 0x10000 }

M.camera_archetype = {
  state = NORMAL,
  state_time = 0x0,
  shake = 0x0,
  pos_before_shake = vec(0xb00000, 0xc40000),
  pos = vec(0xb00000, 0xc40000),
}

M.mouse_archetype = { state = NORMAL, state_time = 0x0 }

M.message_box_archetype = { state = NORMAL, state_time = 0x0, layer = 0xe0000, duration_scale = 0x50000 }

M.toast_archetype = { state = NORMAL, state_time = 0x0, layer = 0x90000, fade = 0x0, velocity = vec(0x0, -0x570a) }

M.research_project_archetype = { row_tail = M.research_project_row_tail_row }

M.structure_icon_archetype = {
  state = NORMAL,
  state_time = 0x0,
  hit_box = box(0x0, 0x0, 0x90000, 0x80000),
  layer = 0xc0000,
  pos = vec(0x3e70000, 0x3e70000),
  rest_cursor = vec(FIXED_INT(vram.P8_SCREEN_WIDTH - 10), FIXED_INT(vram.P8_SCREEN_HEIGHT - 8)),
}

M.score_panel_archetype = {
  state = NORMAL,
  state_time = 0x0,
  layer = 0xa0000,
  countdown_rows = M.score_panel_countdown_rows_row,
  frame_rows = M.score_panel_frame_rows_row,
}

M.wormhole_archetype = {
  state = PLACING,
  state_time = 0x0,
  spawn_snd = 0x340000,
  radius = 0x0,
  layer = 0x50000,
  color_ramp = M.wormhole_color_ramp_seq,
  targeting = targeting({
    attach_distance = 0x0,
    cost = 0x30000,
    confirmed_state = entity_class.ENTITY_STATE_UNFOLDING,
    clock_ticks = 0x10000,
    clock_skips_recompute = 1,
  }),
}

M.slipway_archetype = {
  state = PLACING,
  state_time = 0x0,
  layer = 0x20000,
  trail_colors = M.slipway_trail_colors_seq,
  fade_in_step = 0x140000,
  targeting = targeting({
    attach_distance = 0x106, confirmed_state = NONE, clock_ticks = 0x0,
  }),
}

M.targeting_archetype_none = targeting({
  attach_distance = 0x0, confirmed_state = NONE, clock_ticks = 0x0,
})

M.probe_archetype = probe_archetype({
  state = NORMAL,
  state_time = 0x0,
  fold_anchor = 0x10000,
  layer = 0x20000,
  emitter = emitter({
    angle_slices = 0x30000,
    base_radius = 0x20000,
    speed_factor = 0xf5c,
    lifetime = 0x90000,
    quarter_turn = 0x10000,
    spawn_rate = 0x10000,
    trail_colors = M.probe_trail_colors_seq,
    matrix_xx = 0x1051e,
    matrix_xy = -0xd70,
    matrix_yx = 0xd70,
    matrix_yy = 0x1051e,
  }),
  targeting = targeting({
    attach_distance = 0x666, cost = 0x0, confirmed_state = SETTLED, clock_ticks = 0x0,
  }),
})

M.help_screen_archetype = probe_archetype({
  state = NORMAL,
  state_time = 0x0,
  fold_anchor = 0x10000,
  layer = 0xa0000,
  pos = vec(FIXED_INT(HALF_WIDTH), FIXED_INT(HALF_HEIGHT - 36)),
  fixed_pos = vec(FIXED_INT(HALF_WIDTH - 30), FIXED_INT(HALF_HEIGHT - 8)),
  back_color = 0xb0000,
  row_step = vec(0x0, 0xb0000),
  clear_ephemerals_after = 0x10000,
  hit_box = box(-0x280000, 0x4e0000, 0x280000, 0x620000),
  rows = M.help_screen_rows_row,
  emitter = emitter({
    angle_slices = 0x50000,
    base_radius = 0xf0000,
    speed_factor = 0x1b6,
    lifetime = 0x110000,
    quarter_turn = 0x10000,
    spawn_rate = 0x50000,
    trail_colors = M.help_screen_trail_colors_seq,
    matrix_xx = 0x1051e,
    matrix_xy = -0x1ca,
    matrix_yx = 0x1ca,
    matrix_yy = 0x1051e,
  }),
  targeting = targeting({
    attach_distance = 0x666, cost = 0x0, confirmed_state = SETTLED, clock_ticks = 0x0,
  }),
})

M.trading_hub_required_tech_text = "trade league"
M.ascension_required_tech_text = "ascension"
M.protostar_class_name_text = "protostar"
M.burst_class_name_text = "burst"
M.slipgate_required_tech_text = "slipgates"
M.infraspace_gate_required_tech_text = "infraspace"
M.synthesizer_required_tech_text = "void synthesis"

M.protostar_burst_construct_keys = {
  matrix_xx = 0xf850,
  matrix_xy = -0x2020,
  matrix_yx = 0x2020,
  matrix_yy = 0xf850,
  acceleration_factor = -0x24e,
  speed_factor = 0x90000,
  speed = 0x18000,
  trail_colors = M.protostar_burst_trail_colors_seq,
}

M.planet_archetype = planet_archetype({
  state = entity_class.ENTITY_STATE_UNKNOWN,
  state_time = 0x0,
  is_planet = true,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
})

M.structure_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x10000,
  name = M.structure_name_row,
  is_structure = true,
  sprite = 0xae0000,
  accepted_resources = M.structure_accepted_resources_seq,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0xf0000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
})

M.trading_hub_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x60000,
  name = M.trading_hub_name_row,
  is_structure = false,
  sprite = 0x440000,
  accepted_resources = M.trading_hub_accepted_resources_seq,
  required_tech = M.trading_hub_required_tech_text,
  is_trading_hub = 0x10000,
  palette_block = 0xf0000,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0x140000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
})

M.processor_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x8000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x40000,
  name = M.processor_name_row,
  is_structure = false,
  sprite = 0x640000,
  accepted_resources = M.structure_accepted_resources_seq,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0xf0000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
})

M.ascension_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x20000,
  name = M.ascension_name_row,
  is_structure = false,
  sprite = 0xa60000,
  accepted_resources = M.structure_accepted_resources_seq,
  required_tech = M.ascension_required_tech_text,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0x3c0000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
  emitter = emitter({
    angle_slices = 0x50000,
    base_radius = 0x78000,
    speed_factor = 0x1eb8,
    lifetime = 0x90000,
    quarter_turn = 0x10000,
    spawn_rate = 0x8000,
    trail_colors = M.ascension_trail_colors_seq,
    matrix_xx = 0xfae0,
    matrix_xy = -0x11ec,
    matrix_yx = 0x11ea,
    matrix_yy = 0xfae0,
  }),
})

M.protostar_archetype = planet_archetype({
  state = SETTLED,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x30000,
  name = M.protostar_name_row,
  is_structure = false,
  sprite = 0x620000,
  accepted_resources = M.structure_accepted_resources_seq,
  required_tech = M.ascension_required_tech_text,
  spawn_offset = vec(0x10000, 0x0),
  class_name = M.protostar_class_name_text,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0x3c0000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
  emitter = emitter({
    angle_slices = 0x80000,
    base_radius = 0x20000,
    speed_factor = 0xf5c,
    lifetime = 0x90000,
    quarter_turn = 0x0,
    spawn_rate = 0x10000,
    trail_colors = M.protostar_trail_colors_seq,
    matrix_xx = 0x1051e,
    matrix_xy = -0xd70,
    matrix_yx = 0xd70,
    matrix_yy = 0x1051e,
  }),
})

M.synthesizer_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x70000, -0x70000, 0x70000, 0x70000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x50000,
  name = M.synthesizer_name_row,
  is_structure = false,
  sprite = 0x420000,
  accepted_resources = M.structure_accepted_resources_seq,
  required_tech = M.synthesizer_required_tech_text,
  synthesizable_resources = M.synthesizer_synthesizable_resources_seq,
  targeting = targeting({
    attach_distance = 0x666,
    cost = 0x280000,
    confirmed_state = SETTLED,
    clock_ticks = 0x20000,
  }),
  emitter = emitter({
    angle_slices = 0x50000,
    base_radius = 0x50000,
    speed_factor = 0x1eb8,
    lifetime = 0x90000,
    quarter_turn = 0x10000,
    spawn_rate = 0x4ccc,
    trail_colors = M.synthesizer_trail_colors_seq,
    matrix_xx = 0xfae0,
    matrix_xy = -0x11ec,
    matrix_yx = 0x11ea,
    matrix_yy = 0xfae0,
  }),
})

M.burst_archetype = planet_archetype({
  state = entity_class.ENTITY_STATE_UNKNOWN,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x30000,
  hit_box = box(0x0, 0x0, -0x10000, -0x10000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  class_name = M.burst_class_name_text,
  acceleration_factor = -0xf5c,
  speed = 0x30000,
  emitter = emitter({
    angle_slices = 0x0,
    base_radius = 0x0,
    speed_factor = 0x60000,
    lifetime = 0x0,
    quarter_turn = 0x0,
    spawn_rate = 0x0,
    trail_colors = M.burst_trail_colors_seq,
    matrix_xx = 0xe666,
    matrix_xy = 0x0,
    matrix_yx = 0x0,
    matrix_yy = 0xe666,
  }),
})

M.slipgate_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x40000, -0x40000, 0x50000, 0x50000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x10000,
  name = M.structure_name_row,
  is_structure = true,
  sprite = 0xaa0000,
  accepted_resources = M.structure_accepted_resources_seq,
  required_tech = M.slipgate_required_tech_text,
  link_sprites = M.slipgate_link_sprites_seq,
  targeting = targeting({
    attach_distance = 0x28e,
    cost = 0xa0000,
    confirmed_state = SETTLED,
    clock_ticks = 0x10000,
  }),
})

M.infraspace_gate_archetype = planet_archetype({
  state = PLACING,
  state_time = 0x0,
  is_planet = false,
  trade_multiplier = 0x10000,
  layer = 0x60000,
  hit_box = box(-0x40000, -0x40000, 0x50000, 0x50000),
  shake = 0x0,
  ring_colors = M.planet_ring_colors_seq,
  mood_rows = M.planet_mood_rows_rows,
  made_resources = M.planet_made_resources_seq,
  row_step = vec(0x0, 0xb0000),
  back_color = 0x90000,
  spacer = M.planet_spacer_row,
  development_tier = 0x10000,
  project_index = 0x10000,
  name = M.structure_name_row,
  is_structure = true,
  sprite = 0x4e0000,
  accepted_resources = M.structure_accepted_resources_seq,
  required_tech = M.infraspace_gate_required_tech_text,
  link_sprites = M.infraspace_gate_link_sprites_seq,
  infraspace = 0x10000,
  targeting = targeting({
    attach_distance = 0x28e,
    cost = 0x190000,
    confirmed_state = SETTLED,
    clock_ticks = 0x10000,
  }),
})

M.structure_kind_record = kind_record(nil, 0xae0000, PLACING, 0xf0000, M.structure_archetype)
M.trading_hub_kind_record = kind_record(
  M.trading_hub_required_tech_text, 0x440000, PLACING, 0x140000, M.trading_hub_archetype
)
M.processor_kind_record = kind_record(nil, 0x640000, PLACING, 0xf0000, M.processor_archetype)
M.ascension_kind_record = kind_record(
  M.ascension_required_tech_text, 0xa60000, PLACING, 0x3c0000, M.ascension_archetype
)
M.protostar_kind_record = kind_record(
  M.ascension_required_tech_text, 0x620000, SETTLED, 0x3c0000, M.protostar_archetype
)
M.burst_kind_record = kind_record(nil, nil, entity_class.ENTITY_STATE_UNKNOWN, nil, M.burst_archetype)
M.slipgate_kind_record = kind_record(
  M.slipgate_required_tech_text, 0xaa0000, PLACING, 0xa0000, M.slipgate_archetype
)
M.infraspace_gate_kind_record = kind_record(
  M.infraspace_gate_required_tech_text, 0x4e0000, PLACING, 0x190000, M.infraspace_gate_archetype
)
M.synthesizer_kind_record = kind_record(
  M.synthesizer_required_tech_text, 0x420000, PLACING, 0x280000, M.synthesizer_archetype
)

-- seq is the 0-indexed array, i 1-based as in the cart.
function M.int_seq_at(seq, count, i, out)
  assert(seq ~= nil and count >= 0, "a sequence is an array and its length")
  if i < 1 or i > count then
    return 0, out
  end
  out = seq[i - 1]
  return 1, out
end

function M.game_data_fixed_seq_at(seq, count, i, out)
  assert(seq ~= nil and count >= 0, "a sequence is an array and its length")
  if i < 1 or i > count then
    return 0, out
  end
  out = seq[i - 1]
  return 1, out
end

function M.int_seq_has_fixed(seq, v)
  for i = 0, seq.count - 1 do
    if fixed.p8_fixed_from_int(seq.items[i]) == v then
      return 1
    end
  end
  return 0
end

function M.planet_types_lookup(px)
  if px == nil then
    return nil
  end
  for i = 0, M.PLANET_TYPES_COUNT - 1 do
    if M.planet_types_table[i].name == px then
      return M.planet_types_table[i]
    end
  end
  return nil
end

function M.game_data_route_cost_factor()
  return fixed.p8_fixed_from_int(6)
end

function M.game_data_route_length_factor()
  return fixed.P8_FIXED_LIT(0x00001902, 0.0977)
end

return M
