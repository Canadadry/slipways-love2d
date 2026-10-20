#include "devtools/state_format.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

#include "engine/entity.h"
#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "engine/entity_registry.h"
#include "engine/exit_queue.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"
#include "engine/zone_sizes.h"
#include "game/economy_score.h"
#include "game/game_data.h"
#include "game/main_economy.h"
#include "game/particle_zone.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/slipway.h"
#include "game/ui_zone.h"
#include "p8/draw_meter.h"
#include "p8/gpu_sizes.h"

static const char *const SLOT_KIND_NAMES[SLOT_KIND_COUNT] = {"button", "modal_backdrop", "message_box", "toast", "row", "ephemerals"};
static const char *const PARTICLE_NAMES[PARTICLE_SLOT_COUNT] = {"particle", "particles", "particles_help_screen", "burst", "transit_dots"};

enum { LINE_CAP = 256 };

typedef struct {
    const state_sink_t *sink;
    char text[LINE_CAP];
    size_t length;
} line_t;

static line_t line_start(const state_sink_t *sink) {
    assert(sink != NULL && sink->write_line != NULL);
    line_t line;
    line.sink = sink;
    line.text[0] = '\0';
    line.length = 0;
    return line;
}

static void line_add(line_t *line, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int written = vsnprintf(line->text + line->length, LINE_CAP - line->length, format, args);
    va_end(args);
    assert(written >= 0 && (size_t)written < LINE_CAP - line->length && "state line longer than LINE_CAP");
    line->length += (size_t)written;
}

static void line_end(line_t *line) {
    line_add(line, "\n");
    line->sink->write_line(line->sink->context, line->text);
    line->text[0] = '\0';
    line->length = 0;
}

static void add_fixed(line_t *line, p8_fixed_t x) {
    int64_t v = x;
    if (v < 0) {
        line_add(line, "-");
        v = -v;
    }
    long whole = (long)(v >> 16);
    long hundredths = (long)(((v & 0xffff) * 100) >> 16);
    if (hundredths == 0) {
        line_add(line, "%ld", whole);
    } else {
        line_add(line, "%ld.%02ld", whole, hundredths);
    }
}

static vec_t ra_of(const entity_t *e) {
    return e != NULL ? entity_pos(e) : vec_make(0, 0);
}

/* World y is printed at rest, without the bob: it moves with frame timing alone. */
static void add_position(line_t *line, const entity_t *e, vec_t camera) {
    vec_t ra = ra_of(e);
    p8_fixed_t rest_y = planet_family_bobs(e) ? p8_fixed_sub(ra.y, planet_bob_total(e->state_time)) : ra.y;
    line_add(line, " world ");
    add_fixed(line, ra.x);
    line_add(line, " ");
    add_fixed(line, rest_y);
    line_add(line, " screen ");
    add_fixed(line, p8_fixed_sub(ra.x, camera.x));
    line_add(line, " ");
    add_fixed(line, p8_fixed_sub(ra.y, camera.y));
    line_add(line, " visible %d", entity_geom_is_on_screen(e));
}

static const char *class_of(const entity_t *e) {
    const entity_class_t *c = e->class;
    if (c == planet_class()) return "planet";
    if (c == structure_class()) return "structure";
    if (c == trading_hub_class()) return "trading_hub";
    if (c == processor_class()) return "processor";
    if (c == ascension_class()) return "ascension";
    if (c == protostar_class()) return "protostar";
    if (c == synthesizer_class()) return "synthesizer";
    if (c == burst_class()) return "burst";
    if (c == slipgate_class()) return "slipgate";
    if (c == infraspace_gate_class()) return "infraspace_gate";
    return NULL;
}

static const char *planet_state(state_t state) {
    if (state == ENTITY_STATE_NONE) return "-";
    if (state == ENTITY_STATE_SETTLED) return "colonized";
    if (state == ENTITY_STATE_VANISHING) return "empty";
    return entity_state_name(state);
}

static const char *id_of(const entity_t *e) {
    if (e == NULL || class_of(e) == NULL) return "-";
    const char *id = planet_id_or_null((const planet_t *)e);
    return id != NULL ? id : "-";
}

static int count_of(const entity_list_t *l) {
    return l != NULL ? l->count : 0;
}

static void write_clock(const state_sink_t *sink) {
    line_t line = line_start(sink);
    const economy_t *economy = main_economy_typed();
    if (economy == NULL) {
        line_add(&line, "clock year - month -");
        line_end(&line);
        return;
    }
    line_add(&line, "clock year ");
    add_fixed(&line, economy->year);
    line_add(&line, " month ");
    add_fixed(&line, economy->year_ticks);
    line_end(&line);
}

static void write_difficulty(const state_sink_t *sink) {
    line_t line = line_start(sink);
    const button_t *difficulty = economy_score_difficulty();
    int running = main_economy_typed() != NULL && difficulty != NULL && difficulty->name != NULL;
    line_add(&line, "difficulty %s", running ? difficulty->name : "-");
    line_end(&line);
}

static void write_resources(const state_sink_t *sink) {
    line_t line = line_start(sink);
    const economy_t *economy = main_economy_typed();
    const main_economy_placeholder_t *placeholder = main_economy_placeholder();
    line_add(&line, "resources credits ");
    if (economy != NULL) {
        add_fixed(&line, economy->credits);
    } else if (placeholder != NULL && placeholder->has_credits) {
        add_fixed(&line, placeholder->credits);
    } else {
        line_add(&line, "-");
    }
    line_end(&line);
    line_add(&line, "resources science ");
    if (economy != NULL) {
        add_fixed(&line, economy->research_points);
    } else if (placeholder != NULL && placeholder->has_research_points) {
        add_fixed(&line, placeholder->research_points);
    } else {
        line_add(&line, "-");
    }
    line_end(&line);
    line_add(&line, "resources income ");
    if (economy != NULL) {
        add_fixed(&line, economy->income);
    } else {
        line_add(&line, "-");
    }
    line_end(&line);
}

static int write_techs(const state_sink_t *sink) {
    const economy_t *economy = main_economy_typed();
    if (economy == NULL) return 0;
    line_t line = line_start(sink);
    for (int i = 0; i < economy->invented.count; i++) {
        line_add(&line, "techs %s", (const char *)economy->invented.items[i]);
        line_end(&line);
    }
    return economy->invented.count;
}

static vec_t write_camera(const state_sink_t *sink) {
    line_t line = line_start(sink);
    vec_t camera = ra_of(main_camera());
    line_add(&line, "camera world ");
    add_fixed(&line, camera.x);
    line_add(&line, " ");
    add_fixed(&line, camera.y);
    line_end(&line);
    return camera;
}

static void write_planets(const state_sink_t *sink, vec_t camera) {
    line_t line = line_start(sink);
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    for (int i = 0; i < count_of(planets); i++) {
        const planet_t *p = (const planet_t *)planets->items[i];
        const char *cls = class_of(&p->header);
        line_add(&line, "planet %s %s %s %s", id_of(&p->header), planet_state(p->header.state),
                 cls != NULL ? cls : "-", p->planet_type != NULL && p->planet_type->name != NULL ? p->planet_type->name : "-");
        add_position(&line, &p->header, camera);
        line_end(&line);
    }
}

static int slipway_placed(const entity_t *e) {
    return e->state != ENTITY_STATE_PLACING;
}

static int write_slipways(const state_sink_t *sink) {
    line_t line = line_start(sink);
    const entity_list_t *jc = entities_by_tag(TAG_SLIPWAY);
    int n = 0;
    for (int i = 0; i < count_of(jc); i++) {
        const slipway_t *s = (const slipway_t *)jc->items[i];
        if (!slipway_placed(&s->header)) continue;
        line_add(&line, "slipway %s %s %s", id_of(slipway_start(s)), id_of(slipway_end(s)),
                 entity_has(&s->header, ENTITY_HAS_INFRASPACE) && s->header.infraspace != 0 ? "infragate" : "normal");
        line_end(&line);
        n++;
    }
    return n;
}

static void write_structures(const state_sink_t *sink, vec_t camera) {
    line_t line = line_start(sink);
    const entity_list_t *nr = entities_by_tag(TAG_SLIPGATE);
    for (int i = 0; i < count_of(nr); i++) {
        const planet_t *g = (const planet_t *)nr->items[i];
        line_add(&line, "structure %s %s", id_of(&g->header),
                 g->header.class == infraspace_gate_class() ? "infragate" : "slipgate");
        add_position(&line, &g->header, camera);
        line_add(&line, " links %d", g->slipways.count);
        line_end(&line);
    }
}

/* Not planet_is_colonized(): it skips labs, whose archetype has no is_planet. */
static int count_colonies(void) {
    const entity_list_t *planets = entities_by_tag(TAG_PLANET);
    int n = 0;
    for (int i = 0; i < count_of(planets); i++) {
        n += planets->items[i]->state == ENTITY_STATE_SETTLED;
    }
    return n;
}

static void write_count(const state_sink_t *sink, const char *name, int count) {
    line_t line = line_start(sink);
    line_add(&line, "count %s %d", name, count);
    line_end(&line);
}

static void write_zone(const state_sink_t *sink, const char *name, size_t current, size_t peak, size_t limit) {
    line_t line = line_start(sink);
    line_add(&line, "zone %s %lu %lu %lu", name, (unsigned long)current, (unsigned long)peak, (unsigned long)limit);
    line_end(&line);
}

static void write_zones(const state_sink_t *sink) {
    write_zone(sink, "game", game_zone_used(), game_zone_max(), GENERATION_ZONE_BYTES);
    write_zone(sink, "load", load_zone_used(), game_zone_load_zone_max(), LOAD_ZONE_BYTES);
    write_zone(sink, "frame", frame_zone_used(), frame_zone_max(), FRAME_ZONE_BYTES);
    write_zone(sink, "ui", ui_zone_used(), ui_zone_max_used(), ui_zone_reserved());
    write_zone(sink, "particles", particle_zone_used(), particle_zone_max_used(), particle_zone_reserved());
    line_t line = line_start(sink);
    line_add(&line, "budget zones %lu %lu", (unsigned long)ZONE_BUDGET_USED_BYTES, (unsigned long)ZONE_BUDGET_BYTES);
    line_end(&line);
    write_zone(sink, "gpu_frame", p8_draw_meter_last_frame().bytes, p8_draw_meter_peak().bytes, GPU_FRAME_BUFFER_BYTES);
    write_zone(sink, "gpu_build", p8_draw_meter_last_frame().build_bytes, p8_draw_meter_peak().build_bytes,
               GPU_BUILD_BUFFER_BYTES);
}

static void write_cap(const state_sink_t *sink, const char *name, int live, int peak, int limit) {
    line_t line = line_start(sink);
    line_add(&line, "cap %s %d %d %d", name, live, peak, limit);
    line_end(&line);
}

static void write_peak(const state_sink_t *sink, const char *name, size_t peak, size_t limit) {
    line_t line = line_start(sink);
    line_add(&line, "cap %s - %lu %lu", name, (unsigned long)peak, (unsigned long)limit);
    line_end(&line);
}

static void write_caps(const state_sink_t *sink) {
    for (int i = 0; i < SLOT_KIND_COUNT; i++) {
        slot_kind_t k = (slot_kind_t)i;
        write_cap(sink, SLOT_KIND_NAMES[i], ui_zone_live(k), ui_zone_max_live(k), ui_zone_count(k));
    }
    for (int i = 0; i < PARTICLE_SLOT_COUNT; i++) {
        particle_slot_kind_t k = (particle_slot_kind_t)i;
        write_cap(sink, PARTICLE_NAMES[i], particle_zone_live(k), particle_zone_max_live(k), particle_zone_count(k));
    }
    write_cap(sink, "exit_queue", exit_queue_count(), exit_queue_max(), EXIT_QUEUE_CAP);
    write_cap(sink, "entity_table", entity_id_live(), entity_id_max(), ENTITY_TABLE_CAP);
    write_peak(sink, "elements_particles", (size_t)particle_zone_max_elements(PARTICLE_SLOT_LIST), PARTICLES_CAPACITY);
    write_peak(sink, "elements_particles_help_screen", (size_t)particle_zone_max_elements(PARTICLE_SLOT_HELP_LIST),
               PARTICLES_HELP_SCREEN_CAPACITY);
    write_peak(sink, "elements_transit_dots", (size_t)particle_zone_max_elements(PARTICLE_SLOT_TRANSIT_DOT),
               SLIPWAY_TRANSIT_DOTS_RESERVE);
    write_peak(sink, "elements_ephemerals", (size_t)ui_zone_max_ephemerals(), UI_EPHEMERALS_RESERVE);
    char name[64];
    for (int i = PARTICLE_SLOT_LIST; i <= PARTICLE_SLOT_BURST; i++) {
        particle_slot_kind_t k = (particle_slot_kind_t)i;
        snprintf(name, sizeof name, "bytes_%s", PARTICLE_NAMES[i]);
        write_peak(sink, name, particle_zone_max_bytes(k), particle_zone_slot_size(k));
    }
    for (int i = 0; i < SLOT_KIND_COUNT; i++) {
        slot_kind_t k = (slot_kind_t)i;
        snprintf(name, sizeof name, "bytes_%s", SLOT_KIND_NAMES[i]);
        write_peak(sink, name, ui_zone_max_bytes(k), ui_zone_slot_size(k));
    }
    write_peak(sink, "gpu_cluts", (size_t)p8_draw_meter_peak().cluts, GPU_CLUT_SLOTS);
    write_peak(sink, "gpu_patterns", (size_t)p8_draw_meter_peak().patterns, GPU_PATTERN_SLOTS);
}

void state_format(const state_sink_t *sink) {
    write_clock(sink);
    write_difficulty(sink);
    write_resources(sink);
    int techs = write_techs(sink);
    vec_t camera = write_camera(sink);
    write_planets(sink, camera);
    int slipways = write_slipways(sink);
    write_structures(sink, camera);
    write_count(sink, "planets", count_of(entities_by_tag(TAG_PLANET)));
    write_count(sink, "colonies", count_colonies());
    write_count(sink, "slipways", slipways);
    write_count(sink, "structures", count_of(entities_by_tag(TAG_SLIPGATE)));
    write_count(sink, "techs", techs);
    write_count(sink, "nodes", count_of(entities_by_tag(TAG_NODE)));
    write_zones(sink);
    write_caps(sink);
}

void state_format_save_summary(const state_sink_t *sink, const save_state_t *state, const char *prefix) {
    assert(state != NULL && prefix != NULL);
    assert(state->difficulty >= 1 && state->difficulty <= DIFFICULTY_OPTIONS_COUNT);
    line_t line = line_start(sink);
    line_add(&line, "%sdifficulty %s", prefix, difficulty_options_table[state->difficulty - 1].name);
    line_end(&line);
    line_add(&line, "%sclock year ", prefix);
    add_fixed(&line, state->economy.year);
    line_add(&line, " month ");
    add_fixed(&line, state->economy.year_ticks);
    line_end(&line);
    line_add(&line, "%sresources credits ", prefix);
    add_fixed(&line, state->economy.credits);
    line_end(&line);
    line_add(&line, "%scount nodes %d", prefix, state->node_count);
    line_end(&line);
}

void state_format_stopped(const state_sink_t *sink, int frame) {
    line_t line = line_start(sink);
    line_add(&line, "stopped p8_fatal frame %d", frame);
    line_end(&line);
}

void state_format_cheat_activations(const state_sink_t *sink, int count) {
    line_t line = line_start(sink);
    line_add(&line, "cheat activations %d", count);
    line_end(&line);
}
