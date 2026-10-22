#include "game/game_data.c"
#include "reference_data.h"

#define MODULE "game.game_data"

/* Every table of game_data.c in C order, with the function rendering it. */
#define GAME_DATA(ARRAY, STRUCT, TEXT) \
    ARRAY(upkeep_by_tier_table, reference_data_ints) \
    ARRAY(partner_bonus_by_tier_table, reference_data_ints) \
    ARRAY(development_score_by_tier_table, reference_data_ints) \
    ARRAY(happiness_by_tier_table, reference_data_ints) \
    ARRAY(tier_thresholds_table, reference_data_ints) \
    ARRAY(layer_fades_table, reference_data_ints) \
    ARRAY(period_names_table, render_strings) \
    ARRAY(planet_types_table, render_planet_types) \
    ARRAY(galaxy_menu_1_layout_row_1_items, reference_data_items) \
    STRUCT(galaxy_menu_1_layout_row_1, reference_data_row) \
    ARRAY(galaxy_menu_1_layout_row_items, reference_data_items) \
    STRUCT(galaxy_menu_1_layout_row, reference_data_row) \
    ARRAY(galaxy_menu_2_layout_row_1_items, reference_data_items) \
    STRUCT(galaxy_menu_2_layout_row_1, reference_data_row) \
    ARRAY(galaxy_menu_2_layout_row_items, reference_data_items) \
    STRUCT(galaxy_menu_2_layout_row, reference_data_row) \
    ARRAY(galaxy_menu_3_layout_row_1_items, reference_data_items) \
    STRUCT(galaxy_menu_3_layout_row_1, reference_data_row) \
    ARRAY(galaxy_menu_3_layout_row_items, reference_data_items) \
    STRUCT(galaxy_menu_3_layout_row, reference_data_row) \
    ARRAY(galaxy_menu_4_layout_row_1_items, reference_data_items) \
    STRUCT(galaxy_menu_4_layout_row_1, reference_data_row) \
    ARRAY(galaxy_menu_4_layout_row_items, reference_data_items) \
    STRUCT(galaxy_menu_4_layout_row, reference_data_row) \
    ARRAY(galaxy_menu_table, render_galaxy_menu) \
    ARRAY(pause_menu_1_layout_row_items, reference_data_items) \
    STRUCT(pause_menu_1_layout_row, reference_data_row) \
    ARRAY(pause_menu_2_layout_row_items, reference_data_items) \
    STRUCT(pause_menu_2_layout_row, reference_data_row) \
    ARRAY(pause_menu_3_layout_row_items, reference_data_items) \
    STRUCT(pause_menu_3_layout_row, reference_data_row) \
    ARRAY(pause_menu_4_layout_row_items, reference_data_items) \
    STRUCT(pause_menu_4_layout_row, reference_data_row) \
    ARRAY(pause_menu_table, render_pause_menu) \
    ARRAY(resume_layout_row_items, reference_data_items) \
    STRUCT(resume_layout_row, reference_data_row) \
    STRUCT(pause_menu_tail, render_pause_menu_tail) \
    ARRAY(difficulty_options_1_layout_row_items, reference_data_items) \
    STRUCT(difficulty_options_1_layout_row, reference_data_row) \
    ARRAY(difficulty_options_2_layout_row_items, reference_data_items) \
    STRUCT(difficulty_options_2_layout_row, reference_data_row) \
    ARRAY(difficulty_options_3_layout_row_items, reference_data_items) \
    STRUCT(difficulty_options_3_layout_row, reference_data_row) \
    ARRAY(difficulty_options_4_layout_row_items, reference_data_items) \
    STRUCT(difficulty_options_4_layout_row, reference_data_row) \
    ARRAY(difficulty_options_table, render_difficulty_options) \
    ARRAY(score_rows_1_items, reference_data_items) \
    ARRAY(score_rows_2_items, reference_data_items) \
    ARRAY(score_rows_3_items, reference_data_items) \
    ARRAY(score_rows_4_items, reference_data_items) \
    ARRAY(score_rows_5_items, reference_data_items) \
    ARRAY(score_rows_6_items, reference_data_items) \
    ARRAY(score_rows_7_items, reference_data_items) \
    ARRAY(score_rows_8_items, reference_data_items) \
    ARRAY(score_rows_9_items, reference_data_items) \
    ARRAY(score_rows_10_1_items, reference_data_items) \
    STRUCT(score_rows_10_1, reference_data_row) \
    ARRAY(score_rows_10_2_items, reference_data_items) \
    STRUCT(score_rows_10_2, reference_data_row) \
    ARRAY(score_rows_10_4_items, reference_data_items) \
    STRUCT(score_rows_10_4, reference_data_row) \
    ARRAY(score_rows_10_items, reference_data_items) \
    ARRAY(score_rows_11_items, reference_data_items) \
    ARRAY(score_rows_table, render_score_rows) \
    ARRAY(planet_ring_colors_items, reference_data_ints) \
    STRUCT(planet_ring_colors_seq, reference_data_int_seq) \
    ARRAY(planet_mood_rows_1_items, reference_data_items) \
    ARRAY(planet_mood_rows_2_items, reference_data_items) \
    ARRAY(planet_mood_rows_3_items, reference_data_items) \
    ARRAY(planet_mood_rows_4_items, reference_data_items) \
    ARRAY(planet_mood_rows_rows, reference_data_rows) \
    ARRAY(planet_made_resources_items, reference_data_ints) \
    STRUCT(planet_made_resources_seq, reference_data_int_seq) \
    ARRAY(planet_spacer_items, reference_data_items) \
    STRUCT(planet_spacer_row, reference_data_row) \
    ARRAY(structure_name_items, reference_data_items) \
    STRUCT(structure_name_row, reference_data_row) \
    ARRAY(trading_hub_name_items, reference_data_items) \
    STRUCT(trading_hub_name_row, reference_data_row) \
    ARRAY(processor_name_items, reference_data_items) \
    STRUCT(processor_name_row, reference_data_row) \
    ARRAY(ascension_name_items, reference_data_items) \
    STRUCT(ascension_name_row, reference_data_row) \
    ARRAY(protostar_name_items, reference_data_items) \
    STRUCT(protostar_name_row, reference_data_row) \
    ARRAY(synthesizer_name_items, reference_data_items) \
    STRUCT(synthesizer_name_row, reference_data_row) \
    ARRAY(structure_accepted_resources_items, reference_data_ints) \
    STRUCT(structure_accepted_resources_seq, reference_data_int_seq) \
    ARRAY(trading_hub_accepted_resources_items, reference_data_ints) \
    STRUCT(trading_hub_accepted_resources_seq, reference_data_int_seq) \
    ARRAY(ascension_trail_colors_items, reference_data_ints) \
    STRUCT(ascension_trail_colors_seq, reference_data_int_seq) \
    ARRAY(protostar_trail_colors_items, reference_data_ints) \
    STRUCT(protostar_trail_colors_seq, reference_data_int_seq) \
    ARRAY(synthesizer_trail_colors_items, reference_data_ints) \
    STRUCT(synthesizer_trail_colors_seq, reference_data_int_seq) \
    ARRAY(burst_trail_colors_items, reference_data_ints) \
    STRUCT(burst_trail_colors_seq, reference_data_int_seq) \
    ARRAY(protostar_burst_trail_colors_items, reference_data_ints) \
    STRUCT(protostar_burst_trail_colors_seq, reference_data_int_seq) \
    ARRAY(slipgate_link_sprites_items, reference_data_ints) \
    STRUCT(slipgate_link_sprites_seq, reference_data_int_seq) \
    ARRAY(infraspace_gate_link_sprites_items, reference_data_ints) \
    STRUCT(infraspace_gate_link_sprites_seq, reference_data_int_seq) \
    ARRAY(synthesizer_synthesizable_resources_items, reference_data_ints) \
    STRUCT(synthesizer_synthesizable_resources_seq, reference_data_int_seq) \
    ARRAY(probe_trail_colors_items, reference_data_ints) \
    STRUCT(probe_trail_colors_seq, reference_data_int_seq) \
    ARRAY(help_screen_trail_colors_items, reference_data_ints) \
    STRUCT(help_screen_trail_colors_seq, reference_data_int_seq) \
    ARRAY(slipway_trail_colors_items, reference_data_ints) \
    STRUCT(slipway_trail_colors_seq, reference_data_int_seq) \
    ARRAY(wormhole_color_ramp_items, reference_data_ints) \
    STRUCT(wormhole_color_ramp_seq, reference_data_int_seq) \
    ARRAY(background_tile_choices_items, reference_data_ints) \
    STRUCT(background_tile_choices_seq, reference_data_int_seq) \
    ARRAY(help_screen_rows_1_items, reference_data_items) \
    STRUCT(help_screen_rows_1, reference_data_row) \
    ARRAY(help_screen_rows_2_items, reference_data_items) \
    STRUCT(help_screen_rows_2, reference_data_row) \
    ARRAY(help_screen_rows_3_items, reference_data_items) \
    STRUCT(help_screen_rows_3, reference_data_row) \
    ARRAY(help_screen_rows_4_items, reference_data_items) \
    STRUCT(help_screen_rows_4, reference_data_row) \
    ARRAY(help_screen_rows_items, reference_data_items) \
    STRUCT(help_screen_rows_row, reference_data_row) \
    ARRAY(research_project_row_tail_items, reference_data_items) \
    STRUCT(research_project_row_tail_row, reference_data_row) \
    ARRAY(score_panel_countdown_rows_1_items, reference_data_items) \
    STRUCT(score_panel_countdown_rows_1, reference_data_row) \
    ARRAY(score_panel_countdown_rows_2_items, reference_data_items) \
    STRUCT(score_panel_countdown_rows_2, reference_data_row) \
    ARRAY(score_panel_countdown_rows_5_items, reference_data_items) \
    STRUCT(score_panel_countdown_rows_5, reference_data_row) \
    ARRAY(score_panel_countdown_rows_items, reference_data_items) \
    STRUCT(score_panel_countdown_rows_row, reference_data_row) \
    ARRAY(score_panel_frame_rows_1_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_1, reference_data_row) \
    ARRAY(score_panel_frame_rows_2_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_2, reference_data_row) \
    ARRAY(score_panel_frame_rows_3_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_3, reference_data_row) \
    ARRAY(score_panel_frame_rows_4_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_4, reference_data_row) \
    ARRAY(score_panel_frame_rows_5_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_5, reference_data_row) \
    ARRAY(score_panel_frame_rows_items, reference_data_items) \
    STRUCT(score_panel_frame_rows_row, reference_data_row) \
    ARRAY(score_panel_year_banner_1_items, reference_data_items) \
    STRUCT(score_panel_year_banner_1, reference_data_row) \
    ARRAY(score_panel_year_banner_2_items, reference_data_items) \
    STRUCT(score_panel_year_banner_2, reference_data_row) \
    ARRAY(score_panel_year_banner_items, reference_data_items) \
    STRUCT(score_panel_year_banner_row, reference_data_row) \
    ARRAY(score_panel_readouts_1_items, reference_data_items) \
    STRUCT(score_panel_readouts_1, reference_data_row) \
    ARRAY(score_panel_readouts_2_items, reference_data_items) \
    STRUCT(score_panel_readouts_2, reference_data_row) \
    ARRAY(score_panel_readouts_3_items, reference_data_items) \
    STRUCT(score_panel_readouts_3, reference_data_row) \
    ARRAY(score_panel_readouts_4_items, reference_data_items) \
    STRUCT(score_panel_readouts_4, reference_data_row) \
    ARRAY(score_panel_readouts_5_items, reference_data_items) \
    STRUCT(score_panel_readouts_5, reference_data_row) \
    ARRAY(score_panel_readouts_6_items, reference_data_items) \
    STRUCT(score_panel_readouts_6, reference_data_row) \
    ARRAY(score_panel_readouts_7_items, reference_data_items) \
    STRUCT(score_panel_readouts_7, reference_data_row) \
    ARRAY(score_panel_readouts_items, reference_data_items) \
    STRUCT(score_panel_readouts_row, reference_data_row) \
    TEXT(wormhole_too_far_target_status_text, render_text) \
    STRUCT(background_archetype, render_background_archetype) \
    STRUCT(ui_archetype, render_ui_archetype) \
    STRUCT(button_archetype, render_button_archetype) \
    STRUCT(modal_backdrop_archetype, render_modal_backdrop_archetype) \
    STRUCT(camera_archetype, render_camera_archetype) \
    STRUCT(mouse_archetype, render_mouse_archetype) \
    STRUCT(message_box_archetype, render_message_box_archetype) \
    STRUCT(toast_archetype, render_toast_archetype) \
    STRUCT(research_project_archetype, render_research_project_archetype) \
    STRUCT(structure_icon_archetype, render_structure_icon_archetype) \
    STRUCT(score_panel_archetype, render_score_panel_archetype) \
    STRUCT(wormhole_archetype, render_wormhole_archetype) \
    STRUCT(slipway_archetype, render_slipway_archetype) \
    STRUCT(targeting_archetype_none, render_targeting) \
    STRUCT(probe_archetype, render_probe_archetype) \
    STRUCT(help_screen_archetype, render_probe_archetype) \
    TEXT(trading_hub_required_tech_text, render_text) \
    TEXT(ascension_required_tech_text, render_text) \
    TEXT(protostar_class_name_text, render_text) \
    TEXT(burst_class_name_text, render_text) \
    TEXT(slipgate_required_tech_text, render_text) \
    TEXT(infraspace_gate_required_tech_text, render_text) \
    TEXT(synthesizer_required_tech_text, render_text) \
    STRUCT(protostar_burst_construct_keys, render_burst_construct_keys) \
    STRUCT(planet_archetype, render_planet_archetype) \
    STRUCT(structure_archetype, render_planet_archetype) \
    STRUCT(trading_hub_archetype, render_planet_archetype) \
    STRUCT(processor_archetype, render_planet_archetype) \
    STRUCT(ascension_archetype, render_planet_archetype) \
    STRUCT(protostar_archetype, render_planet_archetype) \
    STRUCT(synthesizer_archetype, render_planet_archetype) \
    STRUCT(burst_archetype, render_planet_archetype) \
    STRUCT(slipgate_archetype, render_planet_archetype) \
    STRUCT(infraspace_gate_archetype, render_planet_archetype) \
    STRUCT(structure_kind_record, render_kind_record) \
    STRUCT(trading_hub_kind_record, render_kind_record) \
    STRUCT(processor_kind_record, render_kind_record) \
    STRUCT(ascension_kind_record, render_kind_record) \
    STRUCT(protostar_kind_record, render_kind_record) \
    STRUCT(burst_kind_record, render_kind_record) \
    STRUCT(slipgate_kind_record, render_kind_record) \
    STRUCT(infraspace_gate_kind_record, render_kind_record) \
    STRUCT(synthesizer_kind_record, render_kind_record)

#define NAME_ARRAY(name, render) {name, #name},
#define NAME_STRUCT(name, render) {&name, #name},
#define NAME_TEXT(name, render) {name, #name},
static const reference_data_name_t NAMES[] = {GAME_DATA(NAME_ARRAY, NAME_STRUCT, NAME_TEXT)};
#undef NAME_ARRAY
#undef NAME_STRUCT
#undef NAME_TEXT

static const reference_data_t DATA = {MODULE, NAMES, (int)(sizeof NAMES / sizeof NAMES[0])};

static void render_text(const reference_data_t *data, const char *text, const char *name) {
    table_reference_string(data->module, text, "%s", name);
}

static void render_strings(const reference_data_t *data, const char *name, const char *const *table, int count) {
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i], "%s[%d]", name, i);
    }
}

static void render_planet_types(const reference_data_t *data, const char *name, const planet_types_entry_t *table,
                                int count) {
    for (int i = 0; i < count; i++) {
        table_reference_string(data->module, table[i].name, "%s[%d].name", name, i);
        table_reference_int(data->module, table[i].sprite, "%s[%d].sprite", name, i);
        table_reference_int(data->module, table[i].weight, "%s[%d].weight", name, i);
        if (table[i].description != NULL) {
            table_reference_string(data->module, table[i].description, "%s[%d].description", name, i);
        }
        table_reference_int(data->module, table[i].state, "%s[%d].state", name, i);
    }
}

static void render_galaxy_menu(const reference_data_t *data, const char *name, const galaxy_menu_entry_t *table,
                               int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        table_reference_int(data->module, table[i].pos_x, "%s[%d].pos_x", name, i);
        table_reference_int(data->module, table[i].pos_y, "%s[%d].pos_y", name, i);
        snprintf(path, sizeof path, "%s[%d].layout_row", name, i);
        reference_data_pointer(data, table[i].layout_row, path);
    }
}

static void render_pause_menu(const reference_data_t *data, const char *name, const pause_menu_entry_t *table,
                              int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s[%d].layout_row", name, i);
        reference_data_pointer(data, table[i].layout_row, path);
    }
}

static void render_pause_menu_tail(const reference_data_t *data, const pause_menu_tail_t *tail, const char *name) {
    table_reference_int(data->module, tail->fixed_pos_x, "%s.fixed_pos_x", name);
    table_reference_int(data->module, tail->fixed_pos_y, "%s.fixed_pos_y", name);
    table_reference_int(data->module, tail->row_step_x, "%s.row_step_x", name);
    table_reference_int(data->module, tail->row_step_y, "%s.row_step_y", name);
    table_reference_int(data->module, tail->back_color, "%s.back_color", name);
}

static void render_difficulty_options(const reference_data_t *data, const char *name,
                                      const difficulty_options_entry_t *table, int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s[%d].layout_row", name, i);
        reference_data_pointer(data, table[i].layout_row, path);
        table_reference_string(data->module, table[i].name, "%s[%d].name", name, i);
        table_reference_int(data->module, table[i].trade, "%s[%d].trade", name, i);
        table_reference_int(data->module, table[i].type_bias, "%s[%d].type_bias", name, i);
    }
}

static void render_score_rows(const reference_data_t *data, const char *name, const score_rows_entry_t *table,
                              int count) {
    char path[256];
    for (int i = 0; i < count; i++) {
        snprintf(path, sizeof path, "%s[%d].row", name, i);
        reference_data_row(data, &table[i].row, path);
        table_reference_int(data->module, table[i].has_score, "%s[%d].has_score", name, i);
        table_reference_int(data->module, table[i].score, "%s[%d].score", name, i);
        table_reference_int(data->module, table[i].has_pos, "%s[%d].has_pos", name, i);
        table_reference_int(data->module, table[i].pos_x, "%s[%d].pos_x", name, i);
        table_reference_int(data->module, table[i].pos_y, "%s[%d].pos_y", name, i);
    }
}

#define INT_FIELD(owner, field) table_reference_int(data->module, (owner)->field, "%s." #field, name)
#define POINTER_FIELD(owner, field) \
    (snprintf(path, sizeof path, "%s." #field, name), reference_data_pointer(data, (owner)->field, path))
#define VEC_FIELD(owner, field) (snprintf(path, sizeof path, "%s." #field, name), render_vec(data, &(owner)->field, path))
#define BOX_FIELD(owner, field) (snprintf(path, sizeof path, "%s." #field, name), render_box(data, &(owner)->field, path))

static void render_vec(const reference_data_t *data, const vec_t *vec, const char *name) {
    INT_FIELD(vec, x);
    INT_FIELD(vec, y);
}

static void render_box(const reference_data_t *data, const box_t *box, const char *name) {
    INT_FIELD(box, left);
    INT_FIELD(box, top);
    INT_FIELD(box, right);
    INT_FIELD(box, bottom);
}

static void render_targeting(const reference_data_t *data, const targeting_archetype_t *targeting, const char *name) {
    INT_FIELD(targeting, attach_distance);
    INT_FIELD(targeting, cost);
    INT_FIELD(targeting, has_cost);
    INT_FIELD(targeting, confirmed_state);
    INT_FIELD(targeting, clock_ticks);
    INT_FIELD(targeting, clock_skips_recompute);
}

#define TARGETING_FIELD(owner, field) \
    (snprintf(path, sizeof path, "%s." #field, name), render_targeting(data, &(owner)->field, path))

static void render_emitter(const reference_data_t *data, const emitter_archetype_t *emitter, const char *name) {
    char path[256];
    INT_FIELD(emitter, angle_slices);
    INT_FIELD(emitter, base_radius);
    INT_FIELD(emitter, speed_factor);
    INT_FIELD(emitter, lifetime);
    INT_FIELD(emitter, quarter_turn);
    INT_FIELD(emitter, spawn_rate);
    POINTER_FIELD(emitter, trail_colors);
    INT_FIELD(emitter, matrix_xx);
    INT_FIELD(emitter, matrix_xy);
    INT_FIELD(emitter, matrix_yx);
    INT_FIELD(emitter, matrix_yy);
}

#define EMITTER_FIELD(owner, field) \
    (snprintf(path, sizeof path, "%s." #field, name), render_emitter(data, &(owner)->field, path))

static void render_background_archetype(const reference_data_t *data, const background_archetype_t *archetype,
                                        const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    INT_FIELD(archetype, hit_test);
    POINTER_FIELD(archetype, tile_choices);
}

static void render_ui_archetype(const reference_data_t *data, const ui_archetype_t *archetype, const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    BOX_FIELD(archetype, panel_box);
}

static void render_button_archetype(const reference_data_t *data, const button_archetype_t *archetype,
                                    const char *name) {
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
}

static void render_modal_backdrop_archetype(const reference_data_t *data, const modal_backdrop_archetype_t *archetype,
                                            const char *name) {
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    INT_FIELD(archetype, hit_test);
}

static void render_camera_archetype(const reference_data_t *data, const camera_archetype_t *archetype,
                                    const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, shake);
    VEC_FIELD(archetype, pos_before_shake);
    VEC_FIELD(archetype, pos);
}

static void render_mouse_archetype(const reference_data_t *data, const mouse_archetype_t *archetype, const char *name) {
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
}

static void render_message_box_archetype(const reference_data_t *data, const message_box_archetype_t *archetype,
                                         const char *name) {
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    INT_FIELD(archetype, duration_scale);
}

static void render_toast_archetype(const reference_data_t *data, const toast_archetype_t *archetype, const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    INT_FIELD(archetype, fade);
    VEC_FIELD(archetype, velocity);
}

static void render_research_project_archetype(const reference_data_t *data,
                                              const research_project_archetype_t *archetype, const char *name) {
    char path[256];
    POINTER_FIELD(archetype, row_tail);
}

static void render_structure_icon_archetype(const reference_data_t *data, const structure_icon_archetype_t *archetype,
                                            const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    BOX_FIELD(archetype, hit_box);
    INT_FIELD(archetype, layer);
    VEC_FIELD(archetype, pos);
    VEC_FIELD(archetype, rest_cursor);
}

static void render_score_panel_archetype(const reference_data_t *data, const score_panel_archetype_t *archetype,
                                         const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    POINTER_FIELD(archetype, countdown_rows);
    POINTER_FIELD(archetype, frame_rows);
}

static void render_wormhole_archetype(const reference_data_t *data, const wormhole_archetype_t *archetype,
                                      const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, spawn_snd);
    INT_FIELD(archetype, radius);
    INT_FIELD(archetype, layer);
    POINTER_FIELD(archetype, color_ramp);
    TARGETING_FIELD(archetype, targeting);
}

static void render_slipway_archetype(const reference_data_t *data, const slipway_archetype_t *archetype,
                                     const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, layer);
    POINTER_FIELD(archetype, trail_colors);
    INT_FIELD(archetype, fade_in_step);
    TARGETING_FIELD(archetype, targeting);
}

static void render_probe_archetype(const reference_data_t *data, const probe_archetype_t *archetype, const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, fold_anchor);
    INT_FIELD(archetype, layer);
    VEC_FIELD(archetype, pos);
    INT_FIELD(archetype, has_pos);
    VEC_FIELD(archetype, fixed_pos);
    INT_FIELD(archetype, back_color);
    VEC_FIELD(archetype, row_step);
    INT_FIELD(archetype, clear_ephemerals_after);
    BOX_FIELD(archetype, hit_box);
    POINTER_FIELD(archetype, rows);
    EMITTER_FIELD(archetype, emitter);
    TARGETING_FIELD(archetype, targeting);
}

static void render_burst_construct_keys(const reference_data_t *data, const burst_construct_keys_t *keys,
                                        const char *name) {
    char path[256];
    INT_FIELD(keys, matrix_xx);
    INT_FIELD(keys, matrix_xy);
    INT_FIELD(keys, matrix_yx);
    INT_FIELD(keys, matrix_yy);
    INT_FIELD(keys, acceleration_factor);
    INT_FIELD(keys, speed_factor);
    INT_FIELD(keys, speed);
    POINTER_FIELD(keys, trail_colors);
}

#define TEXT_FIELD(owner, field) \
    if ((owner)->field != NULL) table_reference_string(data->module, (owner)->field, "%s." #field, name)

static void render_planet_archetype(const reference_data_t *data, const planet_archetype_t *archetype,
                                    const char *name) {
    char path[256];
    INT_FIELD(archetype, state);
    INT_FIELD(archetype, state_time);
    INT_FIELD(archetype, is_planet);
    INT_FIELD(archetype, trade_multiplier);
    INT_FIELD(archetype, layer);
    BOX_FIELD(archetype, hit_box);
    INT_FIELD(archetype, shake);
    POINTER_FIELD(archetype, ring_colors);
    POINTER_FIELD(archetype, mood_rows);
    POINTER_FIELD(archetype, made_resources);
    VEC_FIELD(archetype, row_step);
    INT_FIELD(archetype, back_color);
    POINTER_FIELD(archetype, spacer);
    INT_FIELD(archetype, development_tier);
    INT_FIELD(archetype, has_development_tier);
    INT_FIELD(archetype, project_index);
    POINTER_FIELD(archetype, name);
    INT_FIELD(archetype, is_structure);
    INT_FIELD(archetype, sprite);
    POINTER_FIELD(archetype, accepted_resources);
    TEXT_FIELD(archetype, required_tech);
    INT_FIELD(archetype, is_trading_hub);
    INT_FIELD(archetype, palette_block);
    VEC_FIELD(archetype, spawn_offset);
    TEXT_FIELD(archetype, class_name);
    INT_FIELD(archetype, acceleration_factor);
    INT_FIELD(archetype, speed);
    POINTER_FIELD(archetype, link_sprites);
    INT_FIELD(archetype, infraspace);
    POINTER_FIELD(archetype, synthesizable_resources);
    TARGETING_FIELD(archetype, targeting);
    EMITTER_FIELD(archetype, emitter);
}

static void render_kind_record(const reference_data_t *data, const kind_record_t *record, const char *name) {
    char path[256];
    TEXT_FIELD(record, required_tech);
    INT_FIELD(record, has_sprite);
    INT_FIELD(record, sprite);
    INT_FIELD(record, state);
    INT_FIELD(record, has_cost);
    INT_FIELD(record, cost);
    POINTER_FIELD(record, archetype);
}

void table_reference_game_game_data(void) {
    table_reference_int(MODULE, UPKEEP_BY_TIER_COUNT, "UPKEEP_BY_TIER_COUNT");
    table_reference_int(MODULE, PARTNER_BONUS_BY_TIER_COUNT, "PARTNER_BONUS_BY_TIER_COUNT");
    table_reference_int(MODULE, DEVELOPMENT_SCORE_BY_TIER_COUNT, "DEVELOPMENT_SCORE_BY_TIER_COUNT");
    table_reference_int(MODULE, HAPPINESS_BY_TIER_COUNT, "HAPPINESS_BY_TIER_COUNT");
    table_reference_int(MODULE, TIER_THRESHOLDS_COUNT, "TIER_THRESHOLDS_COUNT");
    table_reference_int(MODULE, LAYER_FADES_COUNT, "LAYER_FADES_COUNT");
    table_reference_int(MODULE, PERIOD_NAMES_COUNT, "PERIOD_NAMES_COUNT");
    table_reference_int(MODULE, PLANET_TYPES_COUNT, "PLANET_TYPES_COUNT");
    table_reference_int(MODULE, GALAXY_MENU_COUNT, "GALAXY_MENU_COUNT");
    table_reference_int(MODULE, PAUSE_MENU_COUNT, "PAUSE_MENU_COUNT");
    table_reference_int(MODULE, DIFFICULTY_OPTIONS_COUNT, "DIFFICULTY_OPTIONS_COUNT");
    table_reference_int(MODULE, SCORE_ROWS_COUNT, "SCORE_ROWS_COUNT");
    table_reference_int(MODULE, PLANET_MOOD_ROWS_COUNT, "PLANET_MOOD_ROWS_COUNT");
    table_reference_int(MODULE, PLANET_RING_COLORS_COUNT, "PLANET_RING_COLORS_COUNT");
#define RENDER_ARRAY(name, render) render(&DATA, #name, name, (int)(sizeof name / sizeof name[0]));
#define RENDER_STRUCT(name, render) render(&DATA, &name, #name);
#define RENDER_TEXT(name, render) render(&DATA, name, #name);
    GAME_DATA(RENDER_ARRAY, RENDER_STRUCT, RENDER_TEXT)
#undef RENDER_ARRAY
#undef RENDER_STRUCT
#undef RENDER_TEXT
}
