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
--   score_rows_entry_t: { row (a data_row_t held inline), has_score, score,
--     has_pos, pos_x, pos_y }.
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
    has_score = fields.has_score or 0,
    score = fields.score or 0,
    has_pos = fields.has_pos or 0,
    pos_x = fields.pos_x or 0,
    pos_y = fields.pos_y or 0,
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
    row = data_row({ count = 1, items = M.score_rows_6_items, has_color = 1, color = 5 }),
    has_score = 1, score = 0, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 43,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_7_items }),
    has_score = 1, score = 1, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 30,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_8_items }),
    has_score = 1, score = 2, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 21,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_9_items }),
    has_score = 1, score = 3, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 12,
  }),
  score_row({
    row = data_row({ count = 5, items = M.score_rows_10_items }),
    has_score = 1, score = 4, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT - 3,
  }),
  score_row({
    row = data_row({ count = 2, items = M.score_rows_11_items }),
    has_score = 1, score = 5, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT + 10,
  }),
  score_row({
    row = data_row({ count = 0, items = nil }),
    has_score = 1, score = 6, has_pos = 1, pos_x = HALF_WIDTH, pos_y = HALF_HEIGHT + 18,
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
  [0] = data_row({ count = 1, items = M.planet_mood_rows_1_items, has_color = 1, color = 8 }),
  data_row({ count = 1, items = M.planet_mood_rows_2_items, has_color = 1, color = 3 }),
  data_row({ count = 1, items = M.planet_mood_rows_3_items, has_color = 1, color = 9 }),
  data_row({ count = 1, items = M.planet_mood_rows_4_items, has_color = 1, color = 12 }),
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
M.structure_name_row = data_row({ count = 1, items = M.structure_name_items, has_color = 1, color = 13 })
M.trading_hub_name_items = { [0] = data_item({ text = "trading hub" }) }
M.trading_hub_name_row = data_row({ count = 1, items = M.trading_hub_name_items, has_color = 1, color = 13 })
M.processor_name_items = { [0] = data_item({ text = "processor" }) }
M.processor_name_row = data_row({ count = 1, items = M.processor_name_items, has_color = 1, color = 13 })
M.ascension_name_items = { [0] = data_item({ text = "ascension nr" }) }
M.ascension_name_row = data_row({ count = 1, items = M.ascension_name_items, has_color = 1, color = 13 })
M.protostar_name_items = { [0] = data_item({ text = "protostar" }) }
M.protostar_name_row = data_row({ count = 1, items = M.protostar_name_items, has_color = 1, color = 13 })
M.synthesizer_name_items = { [0] = data_item({ text = "synthesizer" }) }
M.synthesizer_name_row = data_row({ count = 1, items = M.synthesizer_name_items, has_color = 1, color = 13 })
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
