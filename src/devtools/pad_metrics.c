#include "devtools/pad_metrics.h"

#include "engine/allocator.h"
#include "engine/entity_id.h"
#include "engine/entity_list.h"
#include "engine/entity_registry.h"
#include "game/main_economy.h"
#include "game/menu.h"
#include "game/message_box.h"
#include "game/pad_nav.h"
#include "game/pad_panel.h"
#include "game/pad_slipway.h"
#include "game/pad_target.h"
#include "game/scroll.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static const char *const MODE_NAMES[PAD_METRICS_MODE_COUNT] = {
    "title", "pause", "placement", "bar", "tech", "score", "menu", "jump", "aim", "slipway",
};

void pad_metrics_init(pad_metrics_t *metrics, bump_zone_t *scratch) {
    assert(metrics != NULL && scratch != NULL);
    memset(metrics, 0, sizeof *metrics);
    metrics->scratch = scratch;
}

void pad_metrics_init_judging(pad_metrics_t *metrics, bump_zone_t *scratch) {
    pad_metrics_init(metrics, scratch);
    metrics->judging = 1;
}

pad_metrics_flags_t pad_metrics_flags_read(const input_step_t *step) {
    const pad_nav_t *nav = &step->pad_nav;
    pad_metrics_flags_t flags;
    flags.has_economy = main_economy_typed() != NULL;
    flags.pause_shown = pause_menu_shown();
    flags.placing = pad_nav_placing(nav) != NULL;
    flags.build_bar_open = pad_nav_build_bar_open(nav);
    flags.tech_open = pad_nav_tech_open(nav);
    flags.score_open = pad_nav_score_open(nav);
    flags.panel_open = pad_nav_panel_open();
    flags.aim_mode = pad_nav_aim_mode(nav);
    flags.slipway_started = pad_nav_slipway_start(nav) != NULL;
    return flags;
}

pad_metrics_mode_t pad_metrics_classify(const pad_metrics_flags_t *flags) {
    if (!flags->has_economy) return PAD_METRICS_MODE_TITLE;
    if (flags->pause_shown) return PAD_METRICS_MODE_PAUSE;
    if (flags->placing) return PAD_METRICS_MODE_PLACEMENT;
    if (flags->build_bar_open) return PAD_METRICS_MODE_BAR;
    if (flags->tech_open) return PAD_METRICS_MODE_TECH;
    if (flags->score_open) return PAD_METRICS_MODE_SCORE;
    if (flags->panel_open) return PAD_METRICS_MODE_MENU;
    if (flags->aim_mode) return PAD_METRICS_MODE_AIM;
    if (flags->slipway_started) return PAD_METRICS_MODE_SLIPWAY;
    return PAD_METRICS_MODE_JUMP;
}

pad_metrics_mode_t pad_metrics_mode(const input_step_t *step) {
    pad_metrics_flags_t flags = pad_metrics_flags_read(step);
    return pad_metrics_classify(&flags);
}

const char *pad_metrics_mode_name(pad_metrics_mode_t mode) {
    assert(mode >= 0 && mode < PAD_METRICS_MODE_COUNT);
    return MODE_NAMES[mode];
}

static void camera_point(int *x, int *y) {
    const camera_t *camera = main_camera_typed();
    if (camera == NULL) {
        *x = 0;
        *y = 0;
        return;
    }
    vec_t at = camera_pos_before_shake(camera);
    *x = p8_fixed_flr_int(at.x);
    *y = p8_fixed_flr_int(at.y);
}

static int count_of(const entity_list_t *list) {
    return list != NULL ? list->count : 0;
}

static entity_id_t id_or_none(const entity_t *e) {
    return e != NULL ? entity_id_of(e) : ENTITY_ID_NONE;
}

pad_metrics_view_t pad_metrics_view_read(const input_step_t *step) {
    const pad_nav_t *nav = &step->pad_nav;
    const economy_t *economy = main_economy_typed();
    pad_metrics_view_t view;
    memset(&view, 0, sizeof view);
    view.mode = pad_metrics_mode(step);
    view.hovered = id_or_none(pad_nav_hovered(nav));
    view.slipway_start = id_or_none(pad_nav_slipway_start(nav));
    view.slipway_target = id_or_none(pad_nav_slipway_target(nav));
    view.build_icon = id_or_none(pad_nav_build_icon(nav));
    view.placing = id_or_none(pad_nav_placing(nav));
    view.focus = id_or_none(pad_nav_focus(nav));
    camera_point(&view.camera_x, &view.camera_y);
    view.panel_open = pad_nav_panel_open();
    view.box_open = economy != NULL && pad_panel_box_open();
    view.message_box = shown_message_box() != NULL;
    if (economy != NULL) {
        view.credits = economy->credits;
        view.year = economy->year;
        view.year_ticks = economy->year_ticks;
        view.techs = economy->invented.count;
    }
    view.nodes = count_of(entities_by_tag(TAG_NODE));
    view.probes = count_of(entities_by_tag(TAG_PROBE));
    return view;
}

int pad_metrics_navigation_changed(const pad_metrics_view_t *a, const pad_metrics_view_t *b) {
    return a->mode != b->mode || !entity_id_equal(a->hovered, b->hovered) ||
           !entity_id_equal(a->slipway_start, b->slipway_start) ||
           !entity_id_equal(a->slipway_target, b->slipway_target) || !entity_id_equal(a->build_icon, b->build_icon) ||
           !entity_id_equal(a->placing, b->placing) || !entity_id_equal(a->focus, b->focus) ||
           a->camera_x != b->camera_x || a->camera_y != b->camera_y || a->panel_open != b->panel_open ||
           a->box_open != b->box_open || a->message_box != b->message_box;
}

int pad_metrics_view_changed(const pad_metrics_view_t *a, const pad_metrics_view_t *b) {
    return pad_metrics_navigation_changed(a, b) || a->credits != b->credits || a->year != b->year ||
           a->year_ticks != b->year_ticks || a->nodes != b->nodes || a->probes != b->probes || a->techs != b->techs;
}

int pad_metrics_direction_button(pad_direction_t direction) {
    switch (direction) {
    case PAD_DIRECTION_UP:
        return P8_PAD_BUTTON_1;
    case PAD_DIRECTION_DOWN:
        return P8_PAD_BUTTON_2;
    case PAD_DIRECTION_LEFT:
        return P8_PAD_BUTTON_3;
    case PAD_DIRECTION_RIGHT:
        return P8_PAD_BUTTON_4;
    case PAD_DIRECTION_NONE:
        break;
    }
    return 0;
}

int pad_metrics_first_direction_button(int buttons) {
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        int button = pad_metrics_direction_button(pad_jump_direction(i));
        if (buttons & button) return button;
    }
    return 0;
}

int pad_metrics_hold_step(pad_metrics_hold_t *hold, pad_metrics_mode_t mode, int buttons) {
    int repeats = 0;
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        int button = pad_metrics_direction_button(pad_jump_direction(i));
        hold->held[i] = (buttons & button) ? hold->held[i] + 1 : 0;
        int past = hold->held[i] - PAD_JUMP_REPEAT_DELAY;
        if ((mode == PAD_METRICS_MODE_JUMP || mode == PAD_METRICS_MODE_SLIPWAY) && past > 0 &&
            (past - 1) % PAD_JUMP_REPEAT_PERIOD == 0) {
            repeats |= button;
        }
    }
    return repeats;
}

int pad_metrics_cone_empty(const pad_metrics_t *metrics, const entity_t *from, pad_direction_t direction) {
    /* Resets the caller's scratch zone: nothing the caller holds there survives this call. */
    bump_zone_reset(metrics->scratch);
    allocator_t zone = allocator_from_bump_zone(metrics->scratch);
    entity_list_t candidates = pad_jump_candidates(&zone, entities_by_tag(TAG_NODE));
    return from == NULL || pad_target_pick(from, direction, &candidates) == NULL;
}

static int is_moving_mode(pad_metrics_mode_t mode) {
    return mode == PAD_METRICS_MODE_JUMP || mode == PAD_METRICS_MODE_SLIPWAY;
}

static int empty_cones(const entity_t *from, const entity_list_t *candidates) {
    int empty = 0;
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) {
        pad_direction_t direction = pad_jump_direction(i);
        if (from == NULL || pad_target_pick(from, direction, candidates) == NULL) {
            empty |= pad_metrics_direction_button(direction);
        }
    }
    return empty;
}

static void read_group_facts(const pad_metrics_t *metrics, const input_step_t *step, pad_metrics_view_t *view) {
    const pad_nav_t *nav = &step->pad_nav;
    view->last_acted = id_or_none(pad_nav_last_acted(nav));
    view->cone_empty = 0;
    view->no_candidate = view->mode == PAD_METRICS_MODE_AIM && !pad_jump_any_candidate(entities_by_tag(TAG_NODE));
    if (!is_moving_mode(view->mode)) return;
    bump_zone_reset(metrics->scratch);
    allocator_t zone = allocator_from_bump_zone(metrics->scratch);
    entity_list_t candidates = pad_jump_candidates(&zone, entities_by_tag(TAG_NODE));
    if (view->mode == PAD_METRICS_MODE_JUMP) {
        view->cone_empty = empty_cones(pad_nav_hovered(nav), &candidates);
        return;
    }
    entity_t *start = pad_nav_slipway_start(nav);
    entity_t *aimed = pad_nav_slipway_target(nav) != NULL ? pad_nav_slipway_target(nav) : start;
    entity_list_t targets = pad_slipway_candidates(&zone, &candidates, start);
    view->cone_empty = empty_cones(aimed, &targets);
}

static pad_metrics_view_t snapshot(const pad_metrics_t *metrics, const input_step_t *step) {
    pad_metrics_view_t view = pad_metrics_view_read(step);
    if (metrics->judging) read_group_facts(metrics, step, &view);
    return view;
}

void pad_metrics_observe(pad_metrics_t *metrics, const input_step_t *step) {
    pad_metrics_view_t view = snapshot(metrics, step);
    pad_metrics_begin(metrics, &view);
}

const pad_metrics_view_t *pad_metrics_view(const pad_metrics_t *metrics) {
    return &metrics->view;
}

int pad_metrics_pressed(const pad_metrics_t *metrics, int buttons) {
    return buttons & ~metrics->previous_buttons;
}

static int bit_count(int bits) {
    int n = 0;
    for (; bits != 0; bits &= bits - 1) n++;
    return n;
}

static int is_required(const pad_metrics_view_t *before, int button_bit, int first_direction_button) {
    int aim_with_nothing_to_jump_to = before->mode == PAD_METRICS_MODE_AIM && before->no_candidate;
    if (button_bit == P8_PAD_BUTTON_7) {
        return aim_with_nothing_to_jump_to;
    }
    if (button_bit == P8_PAD_BUTTON_6) {
        int at_root = before->mode == PAD_METRICS_MODE_JUMP &&
                      (entity_id_is_none(before->last_acted) || entity_id_equal(before->last_acted, before->hovered));
        return (at_root || aim_with_nothing_to_jump_to) && !before->message_box && !before->box_open;
    }
    return is_moving_mode(before->mode) && button_bit == first_direction_button && (before->cone_empty & button_bit);
}

static void open_event(pad_metrics_t *metrics, pad_metrics_kind_t kind, int button, int first_direction_button) {
    assert(metrics->pending_count < PAD_METRICS_PENDING_MAX && "pad_metrics: too many presses open at once");
    pad_metrics_pending_t *event = &metrics->pending[metrics->pending_count++];
    event->frame = metrics->frames;
    event->end = -1;
    event->kind = kind;
    event->button = button;
    event->mode = metrics->view.mode;
    event->required_if_dead = is_required(&metrics->view, 1 << button, first_direction_button);
    event->moved = 0;
    event->before = metrics->view;
}

static void list_event(pad_metrics_t *metrics, const pad_metrics_pending_t *event, pad_metrics_verdict_t verdict) {
    if (metrics->listed_count == PAD_METRICS_LISTED_MAX) {
        metrics->listed_dropped++;
        return;
    }
    pad_metrics_listed_t *row = &metrics->listed[metrics->listed_count++];
    row->frame = event->frame;
    row->kind = event->kind;
    row->button = event->button;
    row->mode = event->mode;
    row->verdict = verdict;
}

/* Moves the last pending event into index: callers that settle inside a loop must walk it backward. */
static void settle(pad_metrics_t *metrics, int index) {
    pad_metrics_pending_t event = metrics->pending[index];
    metrics->pending[index] = metrics->pending[--metrics->pending_count];
    pad_metrics_verdict_t verdict = event.moved              ? PAD_METRICS_LIVE
                                    : event.required_if_dead ? PAD_METRICS_DEAD_REQUIRED
                                                             : PAD_METRICS_DEAD_OTHER;
    metrics->verdicts[event.kind][event.mode][event.button][verdict]++;
    if (verdict != PAD_METRICS_LIVE || event.kind == PAD_METRICS_REPEAT) list_event(metrics, &event, verdict);
    if (verdict == PAD_METRICS_DEAD_REQUIRED && event.kind == PAD_METRICS_PRESS && (1 << event.button) == P8_PAD_BUTTON_6) {
        metrics->required_noops++;
    }
}

static void judge_frame(pad_metrics_t *metrics, int buttons, int pressed, int repeats, const pad_metrics_view_t *after) {
    int t = metrics->frames;
    for (int i = 0; i < metrics->pending_count; i++) {
        pad_metrics_pending_t *event = &metrics->pending[i];
        if (event->end < 0 && !(buttons & (1 << event->button))) event->end = t + PAD_METRICS_RELEASE_FRAMES;
    }
    for (int i = metrics->pending_count - 1; i >= 0; i--) {
        if (metrics->pending[i].end < 0 && (repeats & (1 << metrics->pending[i].button))) settle(metrics, i);
    }
    for (int i = metrics->pending_count - 1; i >= 0; i--) {
        const pad_metrics_pending_t *event = &metrics->pending[i];
        int released_before = event->end >= 0 && event->end - PAD_METRICS_RELEASE_FRAMES < t;
        if (event->kind == PAD_METRICS_PRESS && released_before && (pressed & ~(1 << event->button))) settle(metrics, i);
    }
    const int face = P8_PAD_BUTTON_5 | P8_PAD_BUTTON_6;
    int chord = (pressed & face) == face;
    if (chord) metrics->chord_frames++;
    for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
        int bit = 1 << b;
        if ((pressed & bit) && chord && (bit & face)) {
            metrics->chords[metrics->view.mode][b]++;
        } else if (pressed & bit) {
            open_event(metrics, PAD_METRICS_PRESS, b, pad_metrics_first_direction_button(pressed));
        }
        if (repeats & bit) {
            open_event(metrics, PAD_METRICS_REPEAT, b, pad_metrics_first_direction_button(repeats));
        }
    }
    for (int i = 0; i < metrics->pending_count; i++) {
        if (pad_metrics_view_changed(&metrics->pending[i].before, after)) metrics->pending[i].moved = 1;
    }
    for (int i = metrics->pending_count - 1; i >= 0; i--) {
        if (metrics->pending[i].end >= 0 && metrics->pending[i].end <= t) settle(metrics, i);
    }
}

void pad_metrics_begin(pad_metrics_t *metrics, const pad_metrics_view_t *view) {
    metrics->view = *view;
}

void pad_metrics_step(pad_metrics_t *metrics, int buttons, int cheat, const pad_metrics_view_t *after) {
    pad_metrics_mode_t mode = metrics->view.mode;
    int pressed = pad_metrics_pressed(metrics, buttons);
    int repeats = pad_metrics_hold_step(&metrics->hold, mode, buttons);
    metrics->cheat_activations += cheat;
    metrics->mode_frames[mode]++;
    for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
        if (pressed & (1 << b)) metrics->presses[mode][b]++;
        if (repeats & (1 << b)) metrics->repeats[mode][b]++;
    }
    metrics->hold_repeats += bit_count(repeats);
    if (metrics->judging) judge_frame(metrics, buttons, pressed, repeats, after);
    metrics->previous_buttons = buttons;
    metrics->frames++;
    metrics->view = *after;
}

void pad_metrics_record(pad_metrics_t *metrics, const input_step_t *step, int buttons, int cheat) {
    pad_metrics_view_t after = snapshot(metrics, step);
    pad_metrics_step(metrics, buttons, cheat, &after);
}

static int listed_order(const void *a, const void *b) {
    const pad_metrics_listed_t *x = a, *y = b;
    if (x->frame != y->frame) return x->frame < y->frame ? -1 : 1;
    if (x->kind != y->kind) return x->kind < y->kind ? -1 : 1;
    return (x->button > y->button) - (x->button < y->button);
}

void pad_metrics_finish(pad_metrics_t *metrics) {
    while (metrics->pending_count > 0) settle(metrics, metrics->pending_count - 1);
    qsort(metrics->listed, (size_t)metrics->listed_count, sizeof metrics->listed[0], listed_order);
}

int pad_metrics_verdict_count(const pad_metrics_t *metrics, pad_metrics_kind_t kind, pad_metrics_mode_t mode,
                              int button, pad_metrics_verdict_t verdict) {
    assert(kind >= 0 && kind < PAD_METRICS_KIND_COUNT && mode >= 0 && mode < PAD_METRICS_MODE_COUNT);
    assert(button >= 0 && button < P8_PAD_BUTTON_COUNT && verdict >= 0 && verdict < PAD_METRICS_VERDICT_COUNT);
    return metrics->verdicts[kind][mode][button][verdict];
}

int pad_metrics_chord_count(const pad_metrics_t *metrics, pad_metrics_mode_t mode, int button) {
    assert(mode >= 0 && mode < PAD_METRICS_MODE_COUNT && button >= 0 && button < P8_PAD_BUTTON_COUNT);
    return metrics->chords[mode][button];
}

void pad_metrics_count_required_noop(pad_metrics_t *metrics) {
    metrics->required_noops++;
}

int pad_metrics_mode_frames(const pad_metrics_t *metrics, pad_metrics_mode_t mode) {
    assert(mode >= 0 && mode < PAD_METRICS_MODE_COUNT);
    return metrics->mode_frames[mode];
}

int pad_metrics_cheat_activations(const pad_metrics_t *metrics) {
    return metrics->cheat_activations;
}

void pad_metrics_write_totals(const pad_metrics_t *metrics, FILE *out, const char *prefix) {
    int frames = metrics->frames;
    int aim = metrics->mode_frames[PAD_METRICS_MODE_AIM];
    int per_mille = frames > 0 ? (1000 * aim + frames / 2) / frames : 0;
    fprintf(out, "%sframes %d\n", prefix, frames);
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        if (metrics->mode_frames[m] > 0) {
            fprintf(out, "%smode %s frames %d\n", prefix, MODE_NAMES[m], metrics->mode_frames[m]);
        }
    }
    fprintf(out, "%saim share %d.%d%% (%d of %d frames)\n", prefix, per_mille / 10, per_mille % 10, aim, frames);
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
            if (metrics->presses[m][b] > 0) {
                fprintf(out, "%spresses %s %d %d\n", prefix, MODE_NAMES[m], b + 1, metrics->presses[m][b]);
            }
        }
    }
    fprintf(out, "%shold repeats %d\n", prefix, metrics->hold_repeats);
    fprintf(out, "%srequired no-ops %d\n", prefix, metrics->required_noops);
    fprintf(out, "%scheat activations %d\n", prefix, metrics->cheat_activations);
}

int pad_metrics_in_game_frames(const pad_metrics_t *metrics) {
    return metrics->frames - metrics->mode_frames[PAD_METRICS_MODE_TITLE] - metrics->mode_frames[PAD_METRICS_MODE_PAUSE];
}

int pad_metrics_aim_ok(const pad_metrics_t *metrics) {
    int aim = metrics->mode_frames[PAD_METRICS_MODE_AIM];
    return aim == 0 || 4 * aim < pad_metrics_in_game_frames(metrics);
}

static int verdict_sum(const pad_metrics_t *metrics, pad_metrics_kind_t kind, int mode_first, int mode_last,
                       int buttons, pad_metrics_verdict_t verdict) {
    int sum = 0;
    for (int m = mode_first; m <= mode_last; m++) {
        for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
            if (buttons & (1 << b)) sum += metrics->verdicts[kind][m][b][verdict];
        }
    }
    return sum;
}

pad_metrics_section4_t pad_metrics_section4(const pad_metrics_t *metrics) {
    int dpad = 0;
    for (int i = 0; i < PAD_JUMP_DIRECTIONS; i++) dpad |= pad_metrics_direction_button(pad_jump_direction(i));
    const int all = (1 << P8_PAD_BUTTON_COUNT) - 1;
    const int jump = PAD_METRICS_MODE_JUMP, last = PAD_METRICS_MODE_COUNT - 1;
    pad_metrics_section4_t s;
    s.cheat_activations = metrics->cheat_activations;
    s.jump_dpad_dead_lenient = verdict_sum(metrics, PAD_METRICS_PRESS, jump, jump, dpad, PAD_METRICS_DEAD_OTHER);
    s.jump_dpad_dead_strict =
        s.jump_dpad_dead_lenient + verdict_sum(metrics, PAD_METRICS_PRESS, jump, jump, dpad, PAD_METRICS_DEAD_REQUIRED);
    s.button_6_dead_other = verdict_sum(metrics, PAD_METRICS_PRESS, 0, last, P8_PAD_BUTTON_6, PAD_METRICS_DEAD_OTHER);
    s.repeats_dead_required = verdict_sum(metrics, PAD_METRICS_REPEAT, 0, last, all, PAD_METRICS_DEAD_REQUIRED);
    s.repeats_dead_other = verdict_sum(metrics, PAD_METRICS_REPEAT, 0, last, all, PAD_METRICS_DEAD_OTHER);
    s.aim_ok = pad_metrics_aim_ok(metrics);
    s.ok = s.cheat_activations == 0 && s.jump_dpad_dead_strict == 0 && s.button_6_dead_other == 0 && s.aim_ok;
    return s;
}

static void write_share(FILE *out, int part, int whole) {
    long basis_points = whole > 0 ? (10000L * part + whole / 2) / whole : 0;
    fprintf(out, "%ld.%02ld%% (%d of %d frames)", basis_points / 100, basis_points % 100, part, whole);
}

static const char *const VERDICT_NAMES[PAD_METRICS_VERDICT_COUNT] = {"live", "dead-required", "dead-other"};

static const char *verdict_of(int ok) {
    return ok ? "ok" : "fail";
}

static void write_event_counts(const pad_metrics_t *metrics, FILE *out) {
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
            const int *press = metrics->verdicts[PAD_METRICS_PRESS][m][b];
            if (metrics->presses[m][b] > 0) {
                fprintf(out, "presses %s %d %d chords %d live %d dead-required %d dead-other %d\n", MODE_NAMES[m],
                        b + 1, metrics->presses[m][b], metrics->chords[m][b], press[PAD_METRICS_LIVE],
                        press[PAD_METRICS_DEAD_REQUIRED], press[PAD_METRICS_DEAD_OTHER]);
            }
        }
    }
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        for (int b = 0; b < P8_PAD_BUTTON_COUNT; b++) {
            const int *repeat = metrics->verdicts[PAD_METRICS_REPEAT][m][b];
            if (metrics->repeats[m][b] > 0) {
                fprintf(out, "repeats %s %d %d live %d dead-required %d dead-other %d\n", MODE_NAMES[m],
                        b + 1, metrics->repeats[m][b], repeat[PAD_METRICS_LIVE],
                        repeat[PAD_METRICS_DEAD_REQUIRED], repeat[PAD_METRICS_DEAD_OTHER]);
            }
        }
    }
}

static void write_listed(const pad_metrics_t *metrics, FILE *out) {
    for (int i = 0; i < metrics->listed_count; i++) {
        const pad_metrics_listed_t *row = &metrics->listed[i];
        if (row->kind == PAD_METRICS_PRESS) {
            fprintf(out, "dead f%d press %d %s %s\n", row->frame, row->button + 1, MODE_NAMES[row->mode],
                    row->verdict == PAD_METRICS_DEAD_REQUIRED ? "required" : "other");
        } else {
            fprintf(out, "repeat f%d %d %s %s\n", row->frame, row->button + 1, MODE_NAMES[row->mode],
                    VERDICT_NAMES[row->verdict]);
        }
    }
    if (metrics->listed_dropped > 0) fprintf(out, "listed dropped %d\n", metrics->listed_dropped);
}

static void write_section4(const pad_metrics_t *metrics, FILE *out) {
    pad_metrics_section4_t s = pad_metrics_section4(metrics);
    int aim = metrics->mode_frames[PAD_METRICS_MODE_AIM];
    fprintf(out, "section4 cheat activations %d %s\n", s.cheat_activations, verdict_of(s.cheat_activations == 0));
    fprintf(out, "section4 dead d-pad presses in jump mode %d %s (lenient, other group only: %d)\n",
            s.jump_dpad_dead_strict, verdict_of(s.jump_dpad_dead_strict == 0), s.jump_dpad_dead_lenient);
    fprintf(out, "section4 dead circle presses in the other group %d %s\n", s.button_6_dead_other,
            verdict_of(s.button_6_dead_other == 0));
    fprintf(out, "section4 dead hold repeats required %d other %d (not deciding)\n", s.repeats_dead_required,
            s.repeats_dead_other);
    fprintf(out, "section4 aim share ");
    write_share(out, aim, pad_metrics_in_game_frames(metrics));
    fprintf(out, " under 25%% %s\n", verdict_of(s.aim_ok));
    fprintf(out, "section4 verdict %s\n", verdict_of(s.ok));
}

void pad_metrics_write_report(const pad_metrics_t *metrics, FILE *out) {
    assert(metrics->judging && metrics->pending_count == 0 && "pad_metrics: report before pad_metrics_finish");
    int in_game = pad_metrics_in_game_frames(metrics);
    int aim = metrics->mode_frames[PAD_METRICS_MODE_AIM];
    fprintf(out, "frames %d\n", metrics->frames);
    fprintf(out, "in-game frames %d (without title and pause)\n", in_game);
    fprintf(out, "in-game frames with pause %d\n", in_game + metrics->mode_frames[PAD_METRICS_MODE_PAUSE]);
    for (int m = 0; m < PAD_METRICS_MODE_COUNT; m++) {
        fprintf(out, "mode %s ", MODE_NAMES[m]);
        write_share(out, metrics->mode_frames[m], metrics->frames);
        fputc('\n', out);
    }
    fprintf(out, "aim share in-game ");
    write_share(out, aim, in_game);
    fprintf(out, "\naim share in-game with pause ");
    write_share(out, aim, in_game + metrics->mode_frames[PAD_METRICS_MODE_PAUSE]);
    fprintf(out, "\naim share all frames ");
    write_share(out, aim, metrics->frames);
    fprintf(out, "\nchord frames %d\n", metrics->chord_frames);
    write_event_counts(metrics, out);
    write_listed(metrics, out);
    pad_metrics_write_totals(metrics, out, "total ");
    write_section4(metrics, out);
}
