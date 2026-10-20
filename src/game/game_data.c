#include "game/game_data.h"

#include "p8/vram.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

const int upkeep_by_tier_table[UPKEEP_BY_TIER_COUNT] = {1, 0, 1, 2};

const p8_fixed_t partner_bonus_by_tier_table[PARTNER_BONUS_BY_TIER_COUNT] = {-0x5540, 0x0, 0x4000, 0x8000};

const int development_score_by_tier_table[DEVELOPMENT_SCORE_BY_TIER_COUNT] = {0, 100, 200, 400};

const int happiness_by_tier_table[HAPPINESS_BY_TIER_COUNT] = {-5, 0, 1, 1};

const int tier_thresholds_table[TIER_THRESHOLDS_COUNT] = {2, 4, 7, 12};

const p8_fixed_t layer_fades_table[LAYER_FADES_COUNT] = {0x8000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000, 0x10000,
    0x10000, 0x10000, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};

const char *const period_names_table[PERIOD_NAMES_COUNT] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP",
    "OCT", "NOV", "DEC", "UND", "DUO", "TER"};

const planet_types_entry_t planet_types_table[PLANET_TYPES_COUNT] = {
    {"vanishing", 45, 0x0, NULL, ENTITY_STATE_VANISHING},
    {"e", 160, 0x48000, "earth-like", ENTITY_STATE_NONE},
    {"f", 128, 0x60000, "forgeworld", ENTITY_STATE_NONE},
    {"m", 134, 0x40000, "mineral", ENTITY_STATE_NONE},
    {"o", 130, 0x30000, "ocean", ENTITY_STATE_NONE},
    {"r", 140, 0x28000, "remnant", ENTITY_STATE_NONE},
    {"x", 162, 0x30000, "xeno", ENTITY_STATE_NONE},
    {"j", 136, 0x20000, "jungle", ENTITY_STATE_NONE},
    {"i", 138, 0x20000, "iceball", ENTITY_STATE_NONE},
    {"s", 142, 0x20000, "barren", ENTITY_STATE_NONE},
    {"g", 96, 0x40000, "gas giant", ENTITY_STATE_NONE},
};

static const data_item_t galaxy_menu_1_layout_row_1_items[] = {{.number = 15}, {.number = 7}};
static const data_row_t galaxy_menu_1_layout_row_1 = {.count = 2, .items = galaxy_menu_1_layout_row_1_items};
static const data_item_t galaxy_menu_1_layout_row_items[] = {{.row = &galaxy_menu_1_layout_row_1}};
static const data_row_t galaxy_menu_1_layout_row = {.count = 1, .items = galaxy_menu_1_layout_row_items};
static const data_item_t galaxy_menu_2_layout_row_1_items[] = {{.number = 37}, {.number = 7}};
static const data_row_t galaxy_menu_2_layout_row_1 = {.count = 2, .items = galaxy_menu_2_layout_row_1_items};
static const data_item_t galaxy_menu_2_layout_row_items[] = {{.row = &galaxy_menu_2_layout_row_1}};
static const data_row_t galaxy_menu_2_layout_row = {.count = 1, .items = galaxy_menu_2_layout_row_items};
static const data_item_t galaxy_menu_3_layout_row_1_items[] = {{.number = 25}, {.number = 7}};
static const data_row_t galaxy_menu_3_layout_row_1 = {.count = 2, .items = galaxy_menu_3_layout_row_1_items};
static const data_item_t galaxy_menu_3_layout_row_items[] = {{.row = &galaxy_menu_3_layout_row_1}};
static const data_row_t galaxy_menu_3_layout_row = {.count = 1, .items = galaxy_menu_3_layout_row_items};
static const data_item_t galaxy_menu_4_layout_row_1_items[] = {{.number = 8}, {.number = 7}};
static const data_row_t galaxy_menu_4_layout_row_1 = {.count = 2, .items = galaxy_menu_4_layout_row_1_items};
static const data_item_t galaxy_menu_4_layout_row_items[] = {{.row = &galaxy_menu_4_layout_row_1}};
static const data_row_t galaxy_menu_4_layout_row = {.count = 1, .items = galaxy_menu_4_layout_row_items};

const galaxy_menu_entry_t galaxy_menu_table[GALAXY_MENU_COUNT] = {
    {P8_SCREEN_WIDTH - 10, 0, &galaxy_menu_1_layout_row},
    {0, 0, &galaxy_menu_2_layout_row},
    {P8_SCREEN_WIDTH / 2 - 17, 0, &galaxy_menu_3_layout_row},
    {0, P8_SCREEN_HEIGHT - 11, &galaxy_menu_4_layout_row},
};

static const data_item_t pause_menu_1_layout_row_items[] = {{.number = 31}, {.text = "restart     "}};
static const data_row_t pause_menu_1_layout_row = {.count = 2, .items = pause_menu_1_layout_row_items};
static const data_item_t pause_menu_2_layout_row_items[] = {{.number = 42}, {.text = "toggle music"}};
static const data_row_t pause_menu_2_layout_row = {.count = 2, .items = pause_menu_2_layout_row_items};
static const data_item_t pause_menu_3_layout_row_items[] = {{.number = 40}, {.text = "toggle sfx  "}};
static const data_row_t pause_menu_3_layout_row = {.count = 2, .items = pause_menu_3_layout_row_items};
static const data_item_t pause_menu_4_layout_row_items[] = {{.number = 26}, {.text = "how to play\?"}};
static const data_row_t pause_menu_4_layout_row = {.count = 2, .items = pause_menu_4_layout_row_items};

const pause_menu_entry_t pause_menu_table[PAUSE_MENU_COUNT] = {
    {&pause_menu_1_layout_row},
    {&pause_menu_2_layout_row},
    {&pause_menu_3_layout_row},
    {&pause_menu_4_layout_row},
};

static const data_item_t resume_layout_row_items[] = {{.number = 31}, {.text = "resume      "}};
const data_row_t resume_layout_row = {.count = 2, .items = resume_layout_row_items};

const pause_menu_tail_t pause_menu_tail = {0, P8_SCREEN_HEIGHT - 21, 0, -11, 14};

static const data_item_t difficulty_options_1_layout_row_items[] = {{.number = 49}, {.text = "forgiving   "}};
static const data_row_t difficulty_options_1_layout_row = {.count = 2, .items = difficulty_options_1_layout_row_items};
static const data_item_t difficulty_options_2_layout_row_items[] = {{.number = 50}, {.text = "reasonable  "}};
static const data_row_t difficulty_options_2_layout_row = {.count = 2, .items = difficulty_options_2_layout_row_items};
static const data_item_t difficulty_options_3_layout_row_items[] = {{.number = 51}, {.text = "challenging "}};
static const data_row_t difficulty_options_3_layout_row = {.count = 2, .items = difficulty_options_3_layout_row_items};
static const data_item_t difficulty_options_4_layout_row_items[] = {{.number = 52}, {.text = "tough       "}};
static const data_row_t difficulty_options_4_layout_row = {.count = 2, .items = difficulty_options_4_layout_row_items};

const difficulty_options_entry_t difficulty_options_table[DIFFICULTY_OPTIONS_COUNT] = {
    {&difficulty_options_1_layout_row, "forgiving", 7, -0xfd70},
    {&difficulty_options_2_layout_row, "reasonable", 6, -0x999a},
    {&difficulty_options_3_layout_row, "challenging", 5, -0x6666},
    {&difficulty_options_4_layout_row, "tough", 4, -0x3334},
};

static const data_item_t score_rows_1_items[] = {{.number = P8_SCREEN_HEIGHT / 2 - 38}, {.number = P8_SCREEN_HEIGHT / 2 + 22}, {.number = 0}};
static const data_item_t score_rows_2_items[] = {{.number = P8_SCREEN_HEIGHT / 2 - 13}, {.number = P8_SCREEN_HEIGHT / 2 - 5}, {.number = 1}};
static const data_item_t score_rows_3_items[] = {{.number = P8_SCREEN_HEIGHT / 2 + 12}, {.number = P8_SCREEN_HEIGHT / 2 + 13}, {.number = 1}};
static const data_item_t score_rows_4_items[] = {{.number = P8_SCREEN_HEIGHT / 2 - 40}, {.number = P8_SCREEN_HEIGHT / 2 - 39}, {.number = 1}};
static const data_item_t score_rows_5_items[] = {{.number = P8_SCREEN_HEIGHT / 2 - 31}, {.number = P8_SCREEN_HEIGHT / 2 - 23}, {.number = 1}};
static const data_item_t score_rows_6_items[] = {{.text = ""}};
static const data_item_t score_rows_7_items[] = {{.number = 55}, {.text = "planets:   "}};
static const data_item_t score_rows_8_items[] = {{.number = 19}, {.text = "population:"}};
static const data_item_t score_rows_9_items[] = {{.number = 24}, {.text = "technology:"}};
static const data_item_t score_rows_10_1_items[] = {{.number = 27}, {.number = 5}};
static const data_row_t score_rows_10_1 = {.count = 2, .items = score_rows_10_1_items};
static const data_item_t score_rows_10_2_items[] = {{.is_false = 1}, {.number = 0}};
static const data_row_t score_rows_10_2 = {.count = 2, .items = score_rows_10_2_items};
static const data_item_t score_rows_10_4_items[] = {{.number = 25}, {.number = 5}};
static const data_row_t score_rows_10_4 = {.count = 2, .items = score_rows_10_4_items};
static const data_item_t score_rows_10_items[] = {{.row = &score_rows_10_1}, {.row = &score_rows_10_2}, {.is_false = 1}, {.row = &score_rows_10_4},
    {.text = "happiness"}};
static const data_item_t score_rows_11_items[] = {{.text = "total:"}, {.text = ""}};

const score_rows_entry_t score_rows_table[SCORE_ROWS_COUNT] = {
    {.row = {.count = 3, .items = score_rows_1_items, .draw_function = DRAW_FUNCTION_DRAW_BAR}},
    {.row = {.count = 3, .items = score_rows_2_items, .draw_function = DRAW_FUNCTION_DRAW_BAR}},
    {.row = {.count = 3, .items = score_rows_3_items, .draw_function = DRAW_FUNCTION_DRAW_BAR}},
    {.row = {.count = 3, .items = score_rows_4_items, .draw_function = DRAW_FUNCTION_DRAW_BAR}},
    {.row = {.count = 3, .items = score_rows_5_items, .draw_function = DRAW_FUNCTION_DRAW_BAR}},
    {.row = {.count = 1, .items = score_rows_6_items, .has_color = 1, .color = 5}, .has_score = 1, .score = 0, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2,
        .pos_y = P8_SCREEN_HEIGHT / 2 - 43},
    {.row = {.count = 2, .items = score_rows_7_items}, .has_score = 1, .score = 1, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 - 30},
    {.row = {.count = 2, .items = score_rows_8_items}, .has_score = 1, .score = 2, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 - 21},
    {.row = {.count = 2, .items = score_rows_9_items}, .has_score = 1, .score = 3, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 - 12},
    {.row = {.count = 5, .items = score_rows_10_items}, .has_score = 1, .score = 4, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 - 3},
    {.row = {.count = 2, .items = score_rows_11_items}, .has_score = 1, .score = 5, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 + 10},
    {.row = {.count = 0, .items = NULL}, .has_score = 1, .score = 6, .has_pos = 1, .pos_x = P8_SCREEN_WIDTH / 2, .pos_y = P8_SCREEN_HEIGHT / 2 + 18},
};

static const int planet_ring_colors_items[] = {2, 3, 9, 12};
typedef char planet_ring_colors_count_matches[sizeof planet_ring_colors_items / sizeof planet_ring_colors_items[0] == PLANET_RING_COLORS_COUNT ? 1 : -1];
const int_seq_t planet_ring_colors_seq = {PLANET_RING_COLORS_COUNT, planet_ring_colors_items};

static const data_item_t planet_mood_rows_1_items[] = {{.text = "unhappy"}};
static const data_item_t planet_mood_rows_2_items[] = {{.text = "content"}};
static const data_item_t planet_mood_rows_3_items[] = {{.text = "prosperous"}};
static const data_item_t planet_mood_rows_4_items[] = {{.text = "rich"}};
const data_row_t planet_mood_rows_rows[PLANET_MOOD_ROWS_COUNT] = {
    {.count = 1, .items = planet_mood_rows_1_items, .has_color = 1, .color = 8},
    {.count = 1, .items = planet_mood_rows_2_items, .has_color = 1, .color = 3},
    {.count = 1, .items = planet_mood_rows_3_items, .has_color = 1, .color = 9},
    {.count = 1, .items = planet_mood_rows_4_items, .has_color = 1, .color = 12},
};

static const int planet_made_resources_items[] = {12, 36, 24, 25};
const int_seq_t planet_made_resources_seq = {4, planet_made_resources_items};

static const data_item_t planet_spacer_items[] = {{.is_false = 1}, {.number = 3}, {.number = 0}, {.number = -1}};
const data_row_t planet_spacer_row = {.count = 4, .items = planet_spacer_items};

static const data_item_t structure_name_items[] = {{.text = "laboratory"}};
const data_row_t structure_name_row = {.count = 1, .items = structure_name_items, .has_color = 1, .color = 13};

static const data_item_t trading_hub_name_items[] = {{.text = "trading hub"}};
const data_row_t trading_hub_name_row = {.count = 1, .items = trading_hub_name_items, .has_color = 1, .color = 13};

static const data_item_t processor_name_items[] = {{.text = "processor"}};
const data_row_t processor_name_row = {.count = 1, .items = processor_name_items, .has_color = 1, .color = 13};

static const data_item_t ascension_name_items[] = {{.text = "ascension nr"}};
const data_row_t ascension_name_row = {.count = 1, .items = ascension_name_items, .has_color = 1, .color = 13};

static const data_item_t protostar_name_items[] = {{.text = "protostar"}};
const data_row_t protostar_name_row = {.count = 1, .items = protostar_name_items, .has_color = 1, .color = 13};

static const data_item_t synthesizer_name_items[] = {{.text = "synthesizer"}};
const data_row_t synthesizer_name_row = {.count = 1, .items = synthesizer_name_items, .has_color = 1, .color = 13};

static const int structure_accepted_resources_items[] = {28, 18, 20, 21, 22, 48};
const int_seq_t structure_accepted_resources_seq = {6, structure_accepted_resources_items};

static const int trading_hub_accepted_resources_items[] = {28, 48, 18, 20, 21, 22};
const int_seq_t trading_hub_accepted_resources_seq = {6, trading_hub_accepted_resources_items};

static const int ascension_trail_colors_items[] = {1, 13, 12, 12, 12, 13, 13, 5, 1, 1, 1};
const int_seq_t ascension_trail_colors_seq = {11, ascension_trail_colors_items};

static const int protostar_trail_colors_items[] = {1, 2, 4, 9, 10, 7};
const int_seq_t protostar_trail_colors_seq = {6, protostar_trail_colors_items};

static const int synthesizer_trail_colors_items[] = {1, 1, 5, 5, 3, 3, 3, 5, 5, 1};
const int_seq_t synthesizer_trail_colors_seq = {10, synthesizer_trail_colors_items};

static const int burst_trail_colors_items[] = {1, 2, 4, 9, 9, 10, 10, 7, 7, 7, 7, 7};
const int_seq_t burst_trail_colors_seq = {12, burst_trail_colors_items};

static const int protostar_burst_trail_colors_items[] = {1, 2, 8, 14, 14, 14, 14, 8, 8, 2, 2, 1, 1};
const int_seq_t protostar_burst_trail_colors_seq = {13, protostar_burst_trail_colors_items};

static const int slipgate_link_sprites_items[] = {170, 170, 170, 168};
const int_seq_t slipgate_link_sprites_seq = {4, slipgate_link_sprites_items};

static const int infraspace_gate_link_sprites_items[] = {78, 78, 78, 76};
const int_seq_t infraspace_gate_link_sprites_seq = {4, infraspace_gate_link_sprites_items};

static const int synthesizer_synthesizable_resources_items[] = {28, 20, 21, 22};
const int_seq_t synthesizer_synthesizable_resources_seq = {4, synthesizer_synthesizable_resources_items};

static const int probe_trail_colors_items[] = {1, 1, 5, 13, 6};
const int_seq_t probe_trail_colors_seq = {5, probe_trail_colors_items};

static const int help_screen_trail_colors_items[] = {0, 1, 1, 2, 4, 9, 10, 9, 4, 2, 1, 1, 1};
const int_seq_t help_screen_trail_colors_seq = {13, help_screen_trail_colors_items};

static const int slipway_trail_colors_items[] = {0, 0, 7, 7, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 4, 4, 4, 2, 2, 1};
const int_seq_t slipway_trail_colors_seq = {20, slipway_trail_colors_items};

static const int wormhole_color_ramp_items[] = {7, 7, 6, 13, 5, 1, 1, 1, 0};
const int_seq_t wormhole_color_ramp_seq = {9, wormhole_color_ramp_items};

static const int background_tile_choices_items[] = {0, 0, 0, 72, 73, 74, 75, 88, 89, 90, 91, 88, 91, 89, 91};
const int_seq_t background_tile_choices_seq = {15, background_tile_choices_items};

static const data_item_t help_screen_rows_1_items[] = {{.number = 192}, {.number = P8_SCREEN_WIDTH / 2 - 64}, {.number = P8_SCREEN_HEIGHT / 2 - 53}, {.number = 16},
    {.number = 4}};
static const data_row_t help_screen_rows_1 = {.count = 5, .items = help_screen_rows_1_items, .draw_function = DRAW_FUNCTION_SPR};
static const data_item_t help_screen_rows_2_items[] = {{.number = 172}, {.number = P8_SCREEN_WIDTH / 2 - 44}, {.number = P8_SCREEN_HEIGHT / 2 + 43}, {.number = 2},
    {.number = 2}};
static const data_row_t help_screen_rows_2 = {.count = 5, .items = help_screen_rows_2_items, .draw_function = DRAW_FUNCTION_SPR};
static const data_item_t help_screen_rows_3_items[] = {{.text = "need help\?"}, {.number = P8_SCREEN_WIDTH / 2 - 24}, {.number = P8_SCREEN_HEIGHT / 2 + 46},
    {.number = 13}};
static const data_row_t help_screen_rows_3 = {.count = 4, .items = help_screen_rows_3_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t help_screen_rows_4_items[] = {{.text = "slipways.net/help"}, {.number = P8_SCREEN_WIDTH / 2 - 24}, {.number = P8_SCREEN_HEIGHT / 2 + 52},
    {.number = 5}};
static const data_row_t help_screen_rows_4 = {.count = 4, .items = help_screen_rows_4_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t help_screen_rows_items[] = {{.row = &help_screen_rows_1}, {.row = &help_screen_rows_2}, {.row = &help_screen_rows_3},
    {.row = &help_screen_rows_4}};
const data_row_t help_screen_rows_row = {.count = 4, .items = help_screen_rows_items};

static const data_item_t research_project_row_tail_items[] = {{.is_false = 1}, {.number = 2}};
const data_row_t research_project_row_tail_row = {.count = 2, .items = research_project_row_tail_items};

static const data_item_t score_panel_countdown_rows_1_items[] = {{.text = "last year!"}, {.number = P8_SCREEN_WIDTH / 2}, {.number = P8_SCREEN_HEIGHT / 2 - 16}, {.number = 9},
    {.is_fixed = 1, .fixed = 0x8000}};
static const data_row_t score_panel_countdown_rows_1 = {.count = 5, .items = score_panel_countdown_rows_1_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_countdown_rows_2_items[] = {{.text = "2 years remain"}, {.number = P8_SCREEN_WIDTH / 2}, {.number = P8_SCREEN_HEIGHT / 2 - 16}, {.number = 2},
    {.is_fixed = 1, .fixed = 0x8000}};
static const data_row_t score_panel_countdown_rows_2 = {.count = 5, .items = score_panel_countdown_rows_2_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_countdown_rows_5_items[] = {{.text = "5 years remain"}, {.number = P8_SCREEN_WIDTH / 2}, {.number = P8_SCREEN_HEIGHT / 2 - 16}, {.number = 2},
    {.is_fixed = 1, .fixed = 0x8000}};
static const data_row_t score_panel_countdown_rows_5 = {.count = 5, .items = score_panel_countdown_rows_5_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_countdown_rows_items[] = {{.row = &score_panel_countdown_rows_1}, {.row = &score_panel_countdown_rows_2}, {.is_false = 1}, {.is_false = 1},
    {.row = &score_panel_countdown_rows_5}};
const data_row_t score_panel_countdown_rows_row = {.count = 5, .items = score_panel_countdown_rows_items};

static const data_item_t score_panel_frame_rows_1_items[] = {{.number = P8_SCREEN_HEIGHT - 2}, {.number = P8_SCREEN_HEIGHT - 1}, {.number = 1}};
static const data_row_t score_panel_frame_rows_1 = {.count = 3, .items = score_panel_frame_rows_1_items, .draw_function = DRAW_FUNCTION_DRAW_BAR};
static const data_item_t score_panel_frame_rows_2_items[] = {{.number = 0}, {.number = 1}, {.number = 1}};
static const data_row_t score_panel_frame_rows_2 = {.count = 3, .items = score_panel_frame_rows_2_items, .draw_function = DRAW_FUNCTION_DRAW_BAR};
static const data_item_t score_panel_frame_rows_3_items[] = {{.number = 8}, {.number = P8_SCREEN_HEIGHT - 10}, {.number = 34}};
static const data_row_t score_panel_frame_rows_3 = {.count = 3, .items = score_panel_frame_rows_3_items, .draw_function = DRAW_FUNCTION_DRAW_HELP_FRAME};
static const data_item_t score_panel_frame_rows_4_items[] = {{.number = 0}, {.number = -3}, {.number = 26}};
static const data_row_t score_panel_frame_rows_4 = {.count = 3, .items = score_panel_frame_rows_4_items, .draw_function = DRAW_FUNCTION_DRAW_HELP_FRAME};
static const data_item_t score_panel_frame_rows_5_items[] = {{.number = P8_SCREEN_WIDTH - 47}, {.number = -3}, {.number = P8_SCREEN_WIDTH - 11}};
static const data_row_t score_panel_frame_rows_5 = {.count = 3, .items = score_panel_frame_rows_5_items, .draw_function = DRAW_FUNCTION_DRAW_HELP_FRAME};
static const data_item_t score_panel_frame_rows_items[] = {{.row = &score_panel_frame_rows_1}, {.row = &score_panel_frame_rows_2}, {.row = &score_panel_frame_rows_3},
    {.row = &score_panel_frame_rows_4}, {.row = &score_panel_frame_rows_5}};
const data_row_t score_panel_frame_rows_row = {.count = 5, .items = score_panel_frame_rows_items};

static const data_item_t score_panel_year_banner_1_items[] = {{.number = P8_SCREEN_HEIGHT / 2 - 22}, {.number = P8_SCREEN_HEIGHT / 2 - 21}, {.number = 1}};
static const data_row_t score_panel_year_banner_1 = {.count = 3, .items = score_panel_year_banner_1_items, .draw_function = DRAW_FUNCTION_DRAW_BAR};
static const data_item_t score_panel_year_banner_2_items[] = {{.text = ""}, {.number = P8_SCREEN_WIDTH / 2}, {.number = P8_SCREEN_HEIGHT / 2 - 24}, {.number = 6},
    {.is_fixed = 1, .fixed = 0x8000}};
static const data_row_t score_panel_year_banner_2 = {.count = 5, .items = score_panel_year_banner_2_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_year_banner_items[] = {{.row = &score_panel_year_banner_1}, {.row = &score_panel_year_banner_2}};
const data_row_t score_panel_year_banner_row = {.count = 2, .items = score_panel_year_banner_items};

static const data_item_t score_panel_readouts_1_items[] = {{.is_false = 1}, {.number = 13}, {.number = P8_SCREEN_HEIGHT - 7}, {.number = 9}};
static const data_row_t score_panel_readouts_1 = {.count = 4, .items = score_panel_readouts_1_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_2_items[] = {{.is_false = 1}, {.number = 37}, {.number = P8_SCREEN_HEIGHT - 7}, {.number = 2}};
static const data_row_t score_panel_readouts_2 = {.count = 4, .items = score_panel_readouts_2_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_3_items[] = {{.is_false = 1}, {.number = 13}, {.number = 2}, {.number = 9}};
static const data_row_t score_panel_readouts_3 = {.count = 4, .items = score_panel_readouts_3_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_4_items[] = {{.is_false = 1}, {.number = 29}, {.number = 2}, {.number = 2}};
static const data_row_t score_panel_readouts_4 = {.count = 4, .items = score_panel_readouts_4_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_5_items[] = {{.is_false = 1}, {.number = P8_SCREEN_WIDTH / 2 - 5}, {.number = 2}, {.number = 2}};
static const data_row_t score_panel_readouts_5 = {.count = 4, .items = score_panel_readouts_5_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_6_items[] = {{.is_false = 1}, {.number = P8_SCREEN_WIDTH - 43}, {.number = 2}, {.number = 13}};
static const data_row_t score_panel_readouts_6 = {.count = 4, .items = score_panel_readouts_6_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_7_items[] = {{.is_false = 1}, {.number = P8_SCREEN_WIDTH - 29}, {.number = 2}, {.number = 13}};
static const data_row_t score_panel_readouts_7 = {.count = 4, .items = score_panel_readouts_7_items, .draw_function = DRAW_FUNCTION_PRINT_OUTLINED};
static const data_item_t score_panel_readouts_items[] = {{.row = &score_panel_readouts_1}, {.row = &score_panel_readouts_2}, {.row = &score_panel_readouts_3},
    {.row = &score_panel_readouts_4}, {.row = &score_panel_readouts_5}, {.row = &score_panel_readouts_6}, {.row = &score_panel_readouts_7}};
const data_row_t score_panel_readouts_row = {.count = 7, .items = score_panel_readouts_items};

const char wormhole_too_far_target_status_text[] = "-too far-";

const background_archetype_t background_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0x10000,
    .hit_test = 0x10000,
    .tile_choices = &background_tile_choices_seq,
};

const ui_archetype_t ui_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0xf0000,
    .panel_box = {0xa0000, 0xa0000, P8_FIXED_INT(P8_SCREEN_WIDTH - 10), P8_FIXED_INT(P8_SCREEN_HEIGHT - 10)},
};

const button_archetype_t button_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0xb0000,
};

const modal_backdrop_archetype_t modal_backdrop_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0xe0000,
    .hit_test = 0x10000,
};

const camera_archetype_t camera_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .shake = 0x0,
    .pos_before_shake = {0xb00000, 0xc40000},
    .pos = {0xb00000, 0xc40000},
};

const mouse_archetype_t mouse_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
};

const message_box_archetype_t message_box_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0xe0000,
    .duration_scale = 0x50000,
};

const toast_archetype_t toast_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0x90000,
    .fade = 0x0,
    .velocity = {0x0, -0x570a},
};

const research_project_archetype_t research_project_archetype = {
    .row_tail = &research_project_row_tail_row,
};

const structure_icon_archetype_t structure_icon_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .hit_box = {0x0, 0x0, 0x90000, 0x80000},
    .layer = 0xc0000,
    .pos = {0x3e70000, 0x3e70000},
    .rest_cursor = {P8_FIXED_INT(P8_SCREEN_WIDTH - 10), P8_FIXED_INT(P8_SCREEN_HEIGHT - 8)},
};

const score_panel_archetype_t score_panel_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .layer = 0xa0000,
    .countdown_rows = &score_panel_countdown_rows_row,
    .frame_rows = &score_panel_frame_rows_row,
};

const wormhole_archetype_t wormhole_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .spawn_snd = 0x340000,
    .radius = 0x0,
    .layer = 0x50000,
    .color_ramp = &wormhole_color_ramp_seq,
    .targeting = {.attach_distance = 0x0, .cost = 0x30000, .has_cost = 1, .confirmed_state = ENTITY_STATE_UNFOLDING, .clock_ticks = 0x10000,
                  .clock_skips_recompute = 1},
};

const slipway_archetype_t slipway_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .layer = 0x20000,
    .trail_colors = &slipway_trail_colors_seq,
    .fade_in_step = 0x140000,
    .targeting = {.attach_distance = 0x106, .cost = 0x0, .has_cost = 0, .confirmed_state = ENTITY_STATE_NONE, .clock_ticks = 0x0},
};

const targeting_archetype_t targeting_archetype_none = {
    .attach_distance = 0x0, .cost = 0x0, .has_cost = 0, .confirmed_state = ENTITY_STATE_NONE, .clock_ticks = 0x0};

const probe_archetype_t probe_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .fold_anchor = 0x10000,
    .layer = 0x20000,
    .has_pos = 0,
    .emitter = {.angle_slices = 0x30000, .base_radius = 0x20000, .speed_factor = 0xf5c, .lifetime = 0x90000, .quarter_turn = 0x10000, .spawn_rate = 0x10000,
                .trail_colors = &probe_trail_colors_seq, .matrix_xx = 0x1051e, .matrix_xy = -0xd70, .matrix_yx = 0xd70, .matrix_yy = 0x1051e},
    .targeting = {.attach_distance = 0x666, .cost = 0x0, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x0},
};

const probe_archetype_t help_screen_archetype = {
    .state = ENTITY_STATE_NORMAL,
    .state_time = 0x0,
    .fold_anchor = 0x10000,
    .layer = 0xa0000,
    .pos = {P8_FIXED_INT(P8_SCREEN_WIDTH / 2), P8_FIXED_INT(P8_SCREEN_HEIGHT / 2 - 36)},
    .has_pos = 1,
    .fixed_pos = {P8_FIXED_INT(P8_SCREEN_WIDTH / 2 - 30), P8_FIXED_INT(P8_SCREEN_HEIGHT / 2 - 8)},
    .back_color = 0xb0000,
    .row_step = {0x0, 0xb0000},
    .clear_ephemerals_after = 0x10000,
    .hit_box = {-0x280000, 0x4e0000, 0x280000, 0x620000},
    .rows = &help_screen_rows_row,
    .emitter = {.angle_slices = 0x50000, .base_radius = 0xf0000, .speed_factor = 0x1b6, .lifetime = 0x110000, .quarter_turn = 0x10000, .spawn_rate = 0x50000,
                .trail_colors = &help_screen_trail_colors_seq, .matrix_xx = 0x1051e, .matrix_xy = -0x1ca, .matrix_yx = 0x1ca, .matrix_yy = 0x1051e},
    .targeting = {.attach_distance = 0x666, .cost = 0x0, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x0},
};

static const char trading_hub_required_tech_text[] = "trade league";
static const char ascension_required_tech_text[] = "ascension";
static const char protostar_class_name_text[] = "protostar";
static const char burst_class_name_text[] = "burst";
static const char slipgate_required_tech_text[] = "slipgates";
static const char infraspace_gate_required_tech_text[] = "infraspace";
static const char synthesizer_required_tech_text[] = "void synthesis";

const burst_construct_keys_t protostar_burst_construct_keys = {
    .matrix_xx = 0xf850,
    .matrix_xy = -0x2020,
    .matrix_yx = 0x2020,
    .matrix_yy = 0xf850,
    .acceleration_factor = -0x24e,
    .speed_factor = 0x90000,
    .speed = 0x18000,
    .trail_colors = &protostar_burst_trail_colors_seq,
};

const planet_archetype_t planet_archetype = {
    .state = ENTITY_STATE_UNKNOWN,
    .state_time = 0x0,
    .is_planet = 1,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x0,
    .has_development_tier = 0,
};

const planet_archetype_t structure_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x10000,
    .name = &structure_name_row,
    .is_structure = 1,
    .sprite = 0xae0000,
    .accepted_resources = &structure_accepted_resources_seq,
    .targeting = {.attach_distance = 0x666, .cost = 0xf0000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
};

const planet_archetype_t trading_hub_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x60000,
    .name = &trading_hub_name_row,
    .is_structure = 0,
    .sprite = 0x440000,
    .accepted_resources = &trading_hub_accepted_resources_seq,
    .required_tech = trading_hub_required_tech_text,
    .is_trading_hub = 0x10000,
    .palette_block = 0xf0000,
    .targeting = {.attach_distance = 0x666, .cost = 0x140000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
};

const planet_archetype_t processor_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x8000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x40000,
    .name = &processor_name_row,
    .is_structure = 0,
    .sprite = 0x640000,
    .accepted_resources = &structure_accepted_resources_seq,
    .targeting = {.attach_distance = 0x666, .cost = 0xf0000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
};

const planet_archetype_t ascension_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x20000,
    .name = &ascension_name_row,
    .is_structure = 0,
    .sprite = 0xa60000,
    .accepted_resources = &structure_accepted_resources_seq,
    .required_tech = ascension_required_tech_text,
    .targeting = {.attach_distance = 0x666, .cost = 0x3c0000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
    .emitter = {.angle_slices = 0x50000, .base_radius = 0x78000, .speed_factor = 0x1eb8, .lifetime = 0x90000, .quarter_turn = 0x10000, .spawn_rate = 0x8000,
                .trail_colors = &ascension_trail_colors_seq, .matrix_xx = 0xfae0, .matrix_xy = -0x11ec, .matrix_yx = 0x11ea, .matrix_yy = 0xfae0},
};

const planet_archetype_t protostar_archetype = {
    .state = ENTITY_STATE_SETTLED,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x30000,
    .name = &protostar_name_row,
    .is_structure = 0,
    .sprite = 0x620000,
    .accepted_resources = &structure_accepted_resources_seq,
    .required_tech = ascension_required_tech_text,
    .spawn_offset = {0x10000, 0x0},
    .class_name = protostar_class_name_text,
    .targeting = {.attach_distance = 0x666, .cost = 0x3c0000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
    .emitter = {.angle_slices = 0x80000, .base_radius = 0x20000, .speed_factor = 0xf5c, .lifetime = 0x90000, .quarter_turn = 0x0, .spawn_rate = 0x10000,
                .trail_colors = &protostar_trail_colors_seq, .matrix_xx = 0x1051e, .matrix_xy = -0xd70, .matrix_yx = 0xd70, .matrix_yy = 0x1051e},
};

const planet_archetype_t synthesizer_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x70000, -0x70000, 0x70000, 0x70000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x50000,
    .name = &synthesizer_name_row,
    .is_structure = 0,
    .sprite = 0x420000,
    .accepted_resources = &structure_accepted_resources_seq,
    .required_tech = synthesizer_required_tech_text,
    .synthesizable_resources = &synthesizer_synthesizable_resources_seq,
    .targeting = {.attach_distance = 0x666, .cost = 0x280000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x20000},
    .emitter = {.angle_slices = 0x50000, .base_radius = 0x50000, .speed_factor = 0x1eb8, .lifetime = 0x90000, .quarter_turn = 0x10000, .spawn_rate = 0x4ccc,
                .trail_colors = &synthesizer_trail_colors_seq, .matrix_xx = 0xfae0, .matrix_xy = -0x11ec, .matrix_yx = 0x11ea, .matrix_yy = 0xfae0},
};

const planet_archetype_t burst_archetype = {
    .state = ENTITY_STATE_UNKNOWN,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x30000,
    .hit_box = {0x0, 0x0, -0x10000, -0x10000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x0,
    .has_development_tier = 0,
    .class_name = burst_class_name_text,
    .acceleration_factor = -0xf5c,
    .speed = 0x30000,
    .emitter = {.angle_slices = 0x0, .base_radius = 0x0, .speed_factor = 0x60000, .lifetime = 0x0, .quarter_turn = 0x0, .spawn_rate = 0x0,
                .trail_colors = &burst_trail_colors_seq, .matrix_xx = 0xe666, .matrix_xy = 0x0, .matrix_yx = 0x0, .matrix_yy = 0xe666},
};

const planet_archetype_t slipgate_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x40000, -0x40000, 0x50000, 0x50000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x10000,
    .name = &structure_name_row,
    .is_structure = 1,
    .sprite = 0xaa0000,
    .accepted_resources = &structure_accepted_resources_seq,
    .required_tech = slipgate_required_tech_text,
    .link_sprites = &slipgate_link_sprites_seq,
    .targeting = {.attach_distance = 0x28e, .cost = 0xa0000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x10000},
};

const planet_archetype_t infraspace_gate_archetype = {
    .state = ENTITY_STATE_PLACING,
    .state_time = 0x0,
    .is_planet = 0,
    .trade_multiplier = 0x10000,
    .layer = 0x60000,
    .hit_box = {-0x40000, -0x40000, 0x50000, 0x50000},
    .shake = 0x0,
    .ring_colors = &planet_ring_colors_seq,
    .mood_rows = planet_mood_rows_rows,
    .made_resources = &planet_made_resources_seq,
    .row_step = {0x0, 0xb0000},
    .back_color = 0x90000,
    .spacer = &planet_spacer_row,
    .development_tier = 0x10000,
    .has_development_tier = 1,
    .project_index = 0x10000,
    .name = &structure_name_row,
    .is_structure = 1,
    .sprite = 0x4e0000,
    .accepted_resources = &structure_accepted_resources_seq,
    .required_tech = infraspace_gate_required_tech_text,
    .link_sprites = &infraspace_gate_link_sprites_seq,
    .infraspace = 0x10000,
    .targeting = {.attach_distance = 0x28e, .cost = 0x190000, .has_cost = 1, .confirmed_state = ENTITY_STATE_SETTLED, .clock_ticks = 0x10000},
};

const kind_record_t structure_kind_record = {NULL, 1, 0xae0000, ENTITY_STATE_PLACING, 1, 0xf0000, &structure_archetype};
const kind_record_t trading_hub_kind_record = {trading_hub_required_tech_text, 1, 0x440000, ENTITY_STATE_PLACING, 1, 0x140000, &trading_hub_archetype};
const kind_record_t processor_kind_record = {NULL, 1, 0x640000, ENTITY_STATE_PLACING, 1, 0xf0000, &processor_archetype};
const kind_record_t ascension_kind_record = {ascension_required_tech_text, 1, 0xa60000, ENTITY_STATE_PLACING, 1, 0x3c0000, &ascension_archetype};
const kind_record_t protostar_kind_record = {ascension_required_tech_text, 1, 0x620000, ENTITY_STATE_SETTLED, 1, 0x3c0000, &protostar_archetype};
const kind_record_t burst_kind_record = {NULL, 0, 0x0, ENTITY_STATE_UNKNOWN, 0, 0x0, &burst_archetype};
const kind_record_t slipgate_kind_record = {slipgate_required_tech_text, 1, 0xaa0000, ENTITY_STATE_PLACING, 1, 0xa0000, &slipgate_archetype};
const kind_record_t infraspace_gate_kind_record = {infraspace_gate_required_tech_text, 1, 0x4e0000, ENTITY_STATE_PLACING, 1, 0x190000, &infraspace_gate_archetype};
const kind_record_t synthesizer_kind_record = {synthesizer_required_tech_text, 1, 0x420000, ENTITY_STATE_PLACING, 1, 0x280000, &synthesizer_archetype};

int int_seq_at(const int *seq, int count, int i, int *out) {
    assert(seq != NULL && count >= 0 && "a sequence is an array and its length");
    if (i < 1 || i > count) {
        return 0;
    }
    *out = seq[i - 1];
    return 1;
}

int game_data_fixed_seq_at(const p8_fixed_t *seq, int count, int i, p8_fixed_t *out) {
    assert(seq != NULL && count >= 0 && "a sequence is an array and its length");
    if (i < 1 || i > count) {
        return 0;
    }
    *out = seq[i - 1];
    return 1;
}

int int_seq_has_fixed(const int_seq_t *seq, p8_fixed_t v) {
    int i;

    for (i = 0; i < seq->count; i++) {
        if (p8_fixed_from_int(seq->items[i]) == v) {
            return 1;
        }
    }
    return 0;
}

const planet_types_entry_t *planet_types_lookup(const char *px) {
    int i;

    if (px == NULL) {
        return NULL;
    }
    for (i = 0; i < PLANET_TYPES_COUNT; i++) {
        if (strcmp(planet_types_table[i].name, px) == 0) {
            return &planet_types_table[i];
        }
    }
    return NULL;
}

p8_fixed_t game_data_route_cost_factor(void) {
    return p8_fixed_from_int(6);
}

p8_fixed_t game_data_route_length_factor(void) {
    return P8_FIXED_LIT(0x00001902, 0.0977);
}
