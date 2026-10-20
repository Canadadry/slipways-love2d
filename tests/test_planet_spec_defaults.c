/* The defaults of the ten classes of the planet lineage, on a fresh struct
 * of each, against its kind's archetype; and each module's `_ready` over
 * init and shutdown. */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "engine/entity.h"
#include "p8/vram.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

/* The very string of the generated data, not a copy. */
static void assert_same_state(state_t a, state_t b) {
    assert(a != ENTITY_STATE_NONE && a == b);
}

/* The planet members every class of the lineage inherits, and its kind's
 * archetype; the production aside, which planet_install_project marks as
 * the planet's own when it has a research project. */
static void assert_planet_tail(const planet_t *jo, const planet_archetype_t *archetype) {
    assert(jo->archetype == archetype);
    assert(jo->header.shake == archetype->shake);
    assert(jo->development_tier == archetype->development_tier && jo->has_development_tier == archetype->has_development_tier);
}

static void assert_box(const box_t *kq, int xl, int yt, int xr, int yb) {
    assert(kq->left == p8_fixed_from_int(xl) && kq->top == p8_fixed_from_int(yt) &&
           kq->right == p8_fixed_from_int(xr) && kq->bottom == p8_fixed_from_int(yb));
}

/* The planet's hit box, which the burst and the slipgate replace. */
static void assert_planet_box(const box_t *kq) {
    assert_box(kq, -7, -7, 7, 7);
}

/* The structure members every class below it inherits, its targeting's
 * archetype, and the targeting's init literals. */
static void assert_structure_tail(const planet_t *qh, const planet_archetype_t *archetype) {
    assert(qh->targeting.archetype == &archetype->targeting);
    assert(qh->development_tier == archetype->development_tier && qh->has_development_tier == 1);
    assert(qh->targeting.has_cost == 1);
}

/* The emitter of an ascension, protostar or synthesizer, and its archetype. */
static void assert_emitter(const emitter_t *em, const planet_archetype_t *archetype) {
    assert(em->archetype == &archetype->emitter);
    assert(em->speed_factor == archetype->emitter.speed_factor && em->angle_slices == archetype->emitter.angle_slices && em->trail_colors == archetype->emitter.trail_colors);
    assert(em->base_radius == archetype->emitter.base_radius && em->matrix_xx == archetype->emitter.matrix_xx && em->matrix_xy == archetype->emitter.matrix_xy);
    assert(em->matrix_yx == archetype->emitter.matrix_yx && em->matrix_yy == archetype->emitter.matrix_yy);
}

static void test_planet_defaults(void) {
    planet_t *jo = planet_new(NULL);

    assert(planet_ready());
    assert_same_state(jo->header.state, jo->archetype->state);
    assert_planet_box(&jo->archetype->hit_box);
    assert_planet_tail(jo, &planet_archetype);
    assert(jo->archetype->ring_colors == &planet_ring_colors_seq && jo->archetype->mood_rows == planet_mood_rows_rows && jo->archetype->made_resources == &planet_made_resources_seq);
    assert(jo->development_tier == 0 && !jo->has_development_tier);
    assert(!(jo->has & PLANET_HAS_PRODUCTION) && planet_production_read(jo) == planet_shared_production());
    assert(planet_production_read(jo)->amount == 0 && !planet_production_read(jo)->has_resource);
    assert(jo->production.amount == 0 && !jo->production.has_resource && jo->production.resource == 0);
}

static void test_structure_defaults(void) {
    planet_t *qh = structure_new(NULL);

    assert(structure_ready());
    assert_same_state(qh->header.state, structure_archetype.state);
    assert(qh->targeting.cost == structure_archetype.targeting.cost);
    assert(qh->sprite == structure_archetype.sprite);
    assert_structure_tail(qh, &structure_archetype);
    assert_planet_box(&qh->archetype->hit_box);
    assert_planet_tail(qh, &structure_archetype);
}

static void test_trading_hub_defaults(void) {
    planet_t *nl = trading_hub_new(NULL);

    assert(trading_hub_ready());
    assert(nl->sprite == trading_hub_archetype.sprite);
    assert(nl->targeting.cost == trading_hub_archetype.targeting.cost);
    assert(nl->palette_block == trading_hub_archetype.palette_block);
    assert_same_state(nl->header.state, trading_hub_archetype.state);
    assert_structure_tail(nl, &trading_hub_archetype);
    assert_planet_box(&nl->archetype->hit_box);
    assert_planet_tail(nl, &trading_hub_archetype);
}

static void test_processor_defaults(void) {
    planet_t *os = processor_new(NULL);

    assert(processor_ready());
    assert(os->sprite == processor_archetype.sprite);
    assert(os->targeting.cost == processor_archetype.targeting.cost);
    assert_same_state(os->header.state, processor_archetype.state);
    assert_structure_tail(os, &processor_archetype);
    assert_planet_box(&os->archetype->hit_box);
    assert_planet_tail(os, &processor_archetype);
}

static void test_ascension_defaults(void) {
    planet_t *hj = ascension_new(NULL);

    assert(ascension_ready());
    assert(hj->targeting.cost == ascension_archetype.targeting.cost);
    assert(hj->sprite == ascension_archetype.sprite);
    assert_emitter(&hj->emitter, &ascension_archetype);
    assert_same_state(hj->header.state, ascension_archetype.state);
    assert_structure_tail(hj, &ascension_archetype);
    assert_planet_box(&hj->archetype->hit_box);
    assert_planet_tail(hj, &ascension_archetype);
}

static void test_protostar_defaults(void) {
    planet_t *pm = protostar_new(NULL);

    assert(protostar_ready());
    assert_same_state(pm->header.state, protostar_archetype.state);
    assert(pm->sprite == protostar_archetype.sprite);
    assert_emitter(&pm->emitter, &protostar_archetype);
    assert(pm->targeting.cost == protostar_archetype.targeting.cost);
    assert_structure_tail(pm, &protostar_archetype);
    assert_planet_box(&pm->archetype->hit_box);
    assert_planet_tail(pm, &protostar_archetype);
}

static void test_burst_defaults(void) {
    planet_t *ga = burst_new(NULL);
    planet_t *other = burst_new(NULL);

    assert(burst_ready());
    assert((ga->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(&ga->outgoing_routes) == 0 &&
           (other->has & PLANET_HAS_OUTGOING_ROUTES) && trade_route_list_count(&other->outgoing_routes) == 0);
    assert(ga->emitter.archetype == &burst_archetype.emitter);
    assert(ga->emitter.matrix_xx == burst_archetype.emitter.matrix_xx);
    assert(ga->emitter.matrix_xy == burst_archetype.emitter.matrix_xy);
    assert(ga->emitter.matrix_yx == burst_archetype.emitter.matrix_yx);
    assert(ga->emitter.matrix_yy == burst_archetype.emitter.matrix_yy);
    assert(ga->emitter.trail_colors == burst_archetype.emitter.trail_colors);
    assert(ga->acceleration_factor == burst_archetype.acceleration_factor);
    assert(ga->emitter.speed_factor == burst_archetype.emitter.speed_factor);
    assert(ga->speed == burst_archetype.speed);
    assert_box(&ga->archetype->hit_box, 0, 0, -1, -1);
    assert_same_state(ga->header.state, ga->archetype->state);
    assert_planet_tail(ga, &burst_archetype);
    assert(ga->archetype->trade_multiplier == p8_fixed_from_int(1));
    assert(ga->development_tier == 0 && !ga->has_development_tier);
    assert(!(ga->has & PLANET_HAS_PRODUCTION) && planet_production_read(ga) == planet_shared_production());
    assert(planet_production_read(ga)->amount == 0 && !planet_production_read(ga)->has_resource);
    assert(ga->production.amount == 0 && !ga->production.has_resource && ga->production.resource == 0);
}

/* What the slipgate and the infraspace gate inherit from the structure and
 * the planet, and the slipgate's hit box, which the infraspace gate keeps. */
static void assert_slipgate_inherited(const planet_t *nr, const planet_archetype_t *archetype) {
    assert(nr->targeting.cost == archetype->targeting.cost);
    assert(nr->sprite == archetype->sprite);
    assert_box(&nr->archetype->hit_box, -4, -4, 5, 5);
    assert_same_state(nr->header.state, archetype->state);
    assert_structure_tail(nr, archetype);
    assert_planet_tail(nr, archetype);
}

static void test_slipgate_defaults(void) {
    planet_t *nr = slipgate_new(NULL);

    assert(slipgate_ready());
    assert_slipgate_inherited(nr, &slipgate_archetype);
}

static void test_infraspace_gate_defaults(void) {
    planet_t *fr = infraspace_gate_new(NULL);

    assert(infraspace_gate_ready());
    assert_slipgate_inherited(fr, &infraspace_gate_archetype);
}

static void test_synthesizer_defaults(void) {
    planet_t *mx = synthesizer_new(NULL);

    assert(synthesizer_ready());
    assert(mx->sprite == synthesizer_archetype.sprite);
    assert(mx->targeting.cost == synthesizer_archetype.targeting.cost);
    assert(mx->synthesizable_resources == synthesizer_archetype.synthesizable_resources);
    assert_emitter(&mx->emitter, &synthesizer_archetype);
    assert_same_state(mx->header.state, synthesizer_archetype.state);
    assert_structure_tail(mx, &synthesizer_archetype);
    assert_planet_box(&mx->archetype->hit_box);
    assert_planet_tail(mx, &synthesizer_archetype);
}

/* The second reset_world() (the one cart_start_game runs) finds the entity
 * module ready and leaves it alone: a fresh struct of each module still
 * holds a state time of 0. */
static void test_second_reset_world_keeps_entity(void) {
    assert(entity_ready());
    reset_world(NULL);
    assert(entity_ready());
    assert(planet_new(NULL)->header.state_time == 0);
    assert(structure_new(NULL)->header.state_time == 0);
    assert(trading_hub_new(NULL)->header.state_time == 0);
    assert(processor_new(NULL)->header.state_time == 0);
    assert(ascension_new(NULL)->header.state_time == 0);
    assert(protostar_new(NULL)->header.state_time == 0);
    assert(burst_new(NULL)->header.state_time == 0);
    assert(slipgate_new(NULL)->header.state_time == 0);
    assert(infraspace_gate_new(NULL)->header.state_time == 0);
    assert(synthesizer_new(NULL)->header.state_time == 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    assert(!planet_ready());
    assert(!structure_ready());
    assert(!trading_hub_ready());
    assert(!processor_ready());
    assert(!ascension_ready());
    assert(!protostar_ready());
    assert(!burst_ready());
    assert(!slipgate_ready());
    assert(!infraspace_gate_ready());
    assert(!synthesizer_ready());
    cart_load();
    reset_world(NULL);

    test_second_reset_world_keeps_entity();
    test_planet_defaults();
    test_structure_defaults();
    test_trading_hub_defaults();
    test_processor_defaults();
    test_ascension_defaults();
    test_protostar_defaults();
    /* The bursts the protostars spawned go, as at a new game: the burst
     * slots hold what lives at once in a game, not one of each test. */
    reset_world(NULL);
    cart_flush_exit_queue();
    test_burst_defaults();
    test_slipgate_defaults();
    test_infraspace_gate_defaults();
    test_synthesizer_defaults();

    planet_family_trade_shutdown();
    assert(!slipgate_ready());
    assert(!infraspace_gate_ready());
    assert(!synthesizer_ready());
    assert(structure_ready());
    planet_family_shutdown();
    assert(!structure_ready());
    assert(!trading_hub_ready());
    assert(!processor_ready());
    assert(!ascension_ready());
    assert(!protostar_ready());
    assert(!burst_ready());
    assert(planet_ready());
    planet_shutdown();
    assert(!planet_ready());
    assert(entity_ready());
    entity_shutdown();
    assert(!entity_ready());

    printf("test_planet_spec_defaults: all tests passed\n");
    return 0;
}
