#ifndef DEVTOOLS_PAD_METRICS_H
#define DEVTOOLS_PAD_METRICS_H

#include "engine/entity_class.h"
#include "engine/entity_id_type.h"
#include "engine/zone.h"
#include "game/input_step.h"
#include "game/pad_jump.h"
#include "p8/fixed.h"
#include "p8/input.h"

#include <stdio.h>

#define PAD_METRICS_RELEASE_FRAMES 2
#define PAD_METRICS_PENDING_MAX 64
#define PAD_METRICS_LISTED_MAX 8192

typedef enum {
    PAD_METRICS_MODE_TITLE,
    PAD_METRICS_MODE_PAUSE,
    PAD_METRICS_MODE_PLACEMENT,
    PAD_METRICS_MODE_BAR,
    PAD_METRICS_MODE_TECH,
    PAD_METRICS_MODE_SCORE,
    PAD_METRICS_MODE_MENU,
    PAD_METRICS_MODE_JUMP,
    PAD_METRICS_MODE_AIM,
    PAD_METRICS_MODE_SLIPWAY,
    PAD_METRICS_MODE_COUNT
} pad_metrics_mode_t;

typedef struct {
    int has_economy;
    int pause_shown;
    int placing;
    int build_bar_open;
    int tech_open;
    int score_open;
    int panel_open;
    int aim_mode;
    int slipway_started;
} pad_metrics_flags_t;

typedef struct {
    pad_metrics_mode_t mode;
    entity_id_t hovered;
    entity_id_t slipway_start;
    entity_id_t slipway_target;
    entity_id_t build_icon;
    entity_id_t placing;
    entity_id_t focus;
    int camera_x;
    int camera_y;
    int panel_open;
    int box_open;
    int message_box;
    p8_fixed_t credits;
    p8_fixed_t year;
    p8_fixed_t year_ticks;
    int nodes;
    int probes;
    int techs;
    entity_id_t last_acted;
    int cone_empty;
    int no_candidate;
} pad_metrics_view_t;

typedef enum { PAD_METRICS_PRESS, PAD_METRICS_REPEAT, PAD_METRICS_KIND_COUNT } pad_metrics_kind_t;

typedef enum {
    PAD_METRICS_LIVE,
    PAD_METRICS_DEAD_REQUIRED,
    PAD_METRICS_DEAD_OTHER,
    PAD_METRICS_VERDICT_COUNT
} pad_metrics_verdict_t;

typedef struct {
    int frame;
    int end;
    pad_metrics_kind_t kind;
    int button;
    pad_metrics_mode_t mode;
    int required_if_dead;
    int moved;
    pad_metrics_view_t before;
} pad_metrics_pending_t;

typedef struct {
    int frame;
    pad_metrics_kind_t kind;
    int button;
    pad_metrics_mode_t mode;
    pad_metrics_verdict_t verdict;
} pad_metrics_listed_t;

typedef struct {
    int cheat_activations;
    int jump_dpad_dead_strict;
    int jump_dpad_dead_lenient;
    int button_6_dead_other;
    int repeats_dead_required;
    int repeats_dead_other;
    int aim_ok;
    int ok;
} pad_metrics_section4_t;

typedef struct {
    int held[PAD_JUMP_DIRECTIONS];
} pad_metrics_hold_t;

typedef struct {
    bump_zone_t *scratch;
    pad_metrics_view_t view;
    int frames;
    int previous_buttons;
    pad_metrics_hold_t hold;
    int mode_frames[PAD_METRICS_MODE_COUNT];
    int presses[PAD_METRICS_MODE_COUNT][P8_PAD_BUTTON_COUNT];
    int hold_repeats;
    int required_noops;
    int cheat_activations;
    int judging;
    int repeats[PAD_METRICS_MODE_COUNT][P8_PAD_BUTTON_COUNT];
    int chords[PAD_METRICS_MODE_COUNT][P8_PAD_BUTTON_COUNT];
    int chord_frames;
    int verdicts[PAD_METRICS_KIND_COUNT][PAD_METRICS_MODE_COUNT][P8_PAD_BUTTON_COUNT][PAD_METRICS_VERDICT_COUNT];
    pad_metrics_pending_t pending[PAD_METRICS_PENDING_MAX];
    int pending_count;
    pad_metrics_listed_t listed[PAD_METRICS_LISTED_MAX];
    int listed_count;
    int listed_dropped;
} pad_metrics_t;

void pad_metrics_init(pad_metrics_t *metrics, bump_zone_t *scratch);
void pad_metrics_init_judging(pad_metrics_t *metrics, bump_zone_t *scratch);

pad_metrics_flags_t pad_metrics_flags_read(const input_step_t *step);
pad_metrics_mode_t pad_metrics_classify(const pad_metrics_flags_t *flags);
pad_metrics_mode_t pad_metrics_mode(const input_step_t *step);
const char *pad_metrics_mode_name(pad_metrics_mode_t mode);

pad_metrics_view_t pad_metrics_view_read(const input_step_t *step);
int pad_metrics_navigation_changed(const pad_metrics_view_t *a, const pad_metrics_view_t *b);
int pad_metrics_view_changed(const pad_metrics_view_t *a, const pad_metrics_view_t *b);

int pad_metrics_direction_button(pad_direction_t direction);
int pad_metrics_first_direction_button(int buttons);

int pad_metrics_hold_step(pad_metrics_hold_t *hold, pad_metrics_mode_t mode, int buttons);

int pad_metrics_cone_empty(const pad_metrics_t *metrics, const entity_t *from, pad_direction_t direction);

void pad_metrics_observe(pad_metrics_t *metrics, const input_step_t *step);
const pad_metrics_view_t *pad_metrics_view(const pad_metrics_t *metrics);
int pad_metrics_pressed(const pad_metrics_t *metrics, int buttons);
void pad_metrics_record(pad_metrics_t *metrics, const input_step_t *step, int buttons, int cheat);
void pad_metrics_count_required_noop(pad_metrics_t *metrics);

void pad_metrics_begin(pad_metrics_t *metrics, const pad_metrics_view_t *view);
void pad_metrics_step(pad_metrics_t *metrics, int buttons, int cheat, const pad_metrics_view_t *after);
void pad_metrics_finish(pad_metrics_t *metrics);

int pad_metrics_verdict_count(const pad_metrics_t *metrics, pad_metrics_kind_t kind, pad_metrics_mode_t mode,
                              int button, pad_metrics_verdict_t verdict);
int pad_metrics_chord_count(const pad_metrics_t *metrics, pad_metrics_mode_t mode, int button);
int pad_metrics_in_game_frames(const pad_metrics_t *metrics);
int pad_metrics_aim_ok(const pad_metrics_t *metrics);
pad_metrics_section4_t pad_metrics_section4(const pad_metrics_t *metrics);

int pad_metrics_mode_frames(const pad_metrics_t *metrics, pad_metrics_mode_t mode);
int pad_metrics_cheat_activations(const pad_metrics_t *metrics);

void pad_metrics_write_totals(const pad_metrics_t *metrics, FILE *out, const char *prefix);
void pad_metrics_write_report(const pad_metrics_t *metrics, FILE *out);

#endif
