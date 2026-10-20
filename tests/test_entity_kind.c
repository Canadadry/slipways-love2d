/* The nine planet kinds handled as values -- the seven of the kind table,
 * then protostar and burst, looked up by a research project's `class_name`.
 * test_kind_fields_and_build() reads them through each kind's `record` and
 * kind_build() only; its frozen value was taken on the code where these
 * were still classes and must not be re-taken. */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "p8/sprite_sheet.h"
#include "engine/bootstrap.h"
#include "game/cart.h"
#include "engine/dispatch_context.h"
#include "engine/entity.h"
#include "p8/fixed.h"
#include "p8/gfx.h"
#include "p8/input.h"
#include "p8/vram.h"
#include "game/grab_arena.h"
#include "game/planet.h"
#include "game/planet_family.h"
#include "game/planet_family_trade.h"
#include "game/research.h"
#include "engine/entity_class.h"

#define KIND_HASH 0x4B139A82u

#define KIND_COUNT 9

static p8_vram_t memory;
static p8_graphics_t graphics;
static p8_input_t input;

static const entity_kind_t *kind_of(int i) {
    switch (i) {
    case 0:
        return structure_kind();
    case 1:
        return processor_kind();
    case 2:
        return slipgate_kind();
    case 3:
        return infraspace_gate_kind();
    case 4:
        return trading_hub_kind();
    case 5:
        return synthesizer_kind();
    case 6:
        return ascension_kind();
    case 7:
        return protostar_kind();
    default:
        return burst_kind();
    }
}

static const kind_record_t *record_of(int i) {
    const kind_record_t *record = kind_of(i)->record;

    assert(record != NULL);
    return record;
}

static void *kind_build(int i) {
    return kind_of(i)->new_entity(NULL);
}

static const entity_class_t *class_of(int i) {
    switch (i) {
    case 0:
        return structure_class();
    case 1:
        return processor_class();
    case 2:
        return slipgate_class();
    case 3:
        return infraspace_gate_class();
    case 4:
        return trading_hub_class();
    case 5:
        return synthesizer_class();
    case 6:
        return ascension_class();
    case 7:
        return protostar_class();
    default:
        return burst_class();
    }
}

static uint32_t fnv(uint32_t h, uint32_t word) {
    int i;
    for (i = 0; i < 4; i++) {
        h ^= (word >> (8 * i)) & 0xFFu;
        h *= 16777619u;
    }
    return h;
}

/* The kind words the frozen hashes fold before a value: any other value
 * moves them. */
enum { FIXED_KIND = 0, STRING_KIND = 1 };

static uint32_t fold_string(uint32_t h, const char *s) {
    for (; *s != '\0'; s++) {
        h = fnv(h, (uint32_t)(unsigned char)*s);
    }
    return fnv(h, 0xFFFFFFFFu);
}

/* The kind word is constant by the struct's shape (`required_tech` and
 * `state` strings, `sprite` and `cost` fixed), kept so that KIND_HASH still
 * proves the typed reads. */
static uint32_t fold_string_member(uint32_t h, const char *s) {
    h = fnv(h, (uint32_t)(s != NULL));
    return s == NULL ? h : fold_string(fnv(h, (uint32_t)STRING_KIND), s);
}

static uint32_t fold_fixed_member(uint32_t h, int has, p8_fixed_t v) {
    h = fnv(h, (uint32_t)(has != 0));
    return has ? fnv(fnv(h, (uint32_t)FIXED_KIND), (uint32_t)v) : h;
}

static void check(const char *label, uint32_t got, uint32_t want) {
    printf("[%s] 0x%08Xu\n", label, (unsigned)got);
    fflush(stdout);
    assert(got == want);
}

/* `required_tech`, `sprite`, `state` and `cost` of each kind as a
 * structure icon's `refresh` and `draw` read them, absent included
 * (structure, processor and burst have no `required_tech`), and the struct
 * each kind builds. */
static void test_kind_fields_and_build(void) {
    uint32_t h = 2166136261u;
    int i;

    for (i = 0; i < KIND_COUNT; i++) {
        const kind_record_t *record = record_of(i);
        void *made;

        h = fold_string_member(h, record->required_tech);
        h = fold_fixed_member(h, record->has_sprite, record->sprite);
        h = fold_string_member(h, entity_state_name(record->state));
        h = fold_fixed_member(h, record->has_cost, record->cost);
        made = kind_build(i);
        assert(made != NULL &&
               ((const entity_t *)made)->class == class_of(i));
        h = fnv(h, (uint32_t)i);
    }
    check("kind", h, KIND_HASH);
}

/* Built in a grab arena opened for planet_kind_construction_bytes, each
 * kind but burst (built in its slot) fills it exactly. */
static void test_kind_fills_its_grab_arena(void) {
    int i;

    for (i = 0; i < KIND_COUNT - 1; i++) {
        const entity_t *made;
        const allocator_t *arena;

        grab_arena_open(planet_kind_construction_bytes(kind_of(i)));
        arena = grab_arena_construction_zone();
        made = kind_build(i);
        grab_arena_close();
        assert(made != NULL && ((const planet_t *)made)->slipways.zone == arena);
        if (arena_used(arena) != arena_size(arena))
            fprintf(stderr, "kind %d: arena %lu, used %lu\n", i, (unsigned long)arena_size(arena),
                    (unsigned long)arena_used(arena));
        assert(arena_used(arena) == arena_size(arena) && "the kind's construction bytes are exact");
    }
}

/* Each kind's `record` holds the generated default of the class that sets
 * each field. */
static void test_kind_reader_serves_the_generated_defaults(void) {
    const kind_record_t *record;

    record = record_of(0);
    assert(record->required_tech == NULL);
    assert(record->has_sprite && record->sprite == structure_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == structure_archetype.targeting.cost);

    record = record_of(1);
    assert(record->required_tech == NULL);
    assert(record->has_sprite && record->sprite == processor_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == processor_archetype.targeting.cost);

    record = record_of(2);
    assert(record->required_tech == slipgate_archetype.required_tech);
    assert(record->has_sprite && record->sprite == slipgate_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == slipgate_archetype.targeting.cost);

    record = record_of(3);
    assert(record->required_tech == infraspace_gate_archetype.required_tech);
    assert(record->has_sprite && record->sprite == infraspace_gate_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == infraspace_gate_archetype.targeting.cost);

    record = record_of(4);
    assert(record->required_tech == trading_hub_archetype.required_tech);
    assert(record->has_sprite && record->sprite == trading_hub_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == trading_hub_archetype.targeting.cost);

    record = record_of(5);
    assert(record->required_tech == synthesizer_archetype.required_tech);
    assert(record->has_sprite && record->sprite == synthesizer_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == synthesizer_archetype.targeting.cost);

    record = record_of(6);
    assert(record->required_tech == ascension_archetype.required_tech);
    assert(record->has_sprite && record->sprite == ascension_archetype.sprite);
    assert(record->state == structure_archetype.state);
    assert(record->has_cost && record->cost == ascension_archetype.targeting.cost);

    record = record_of(7);
    assert(record->required_tech == protostar_archetype.required_tech);
    assert(record->has_sprite && record->sprite == protostar_archetype.sprite);
    assert(record->state == protostar_archetype.state);
    assert(record->has_cost && record->cost == protostar_archetype.targeting.cost);

    record = record_of(8);
    assert(record->required_tech == NULL);
    assert(!record->has_sprite && record->sprite == 0);
    assert(record->state == planet_archetype.state);
    assert(!record->has_cost && record->cost == 0);
}

/* The kind lookup by class name only ever sees "protostar" or "burst":
 * every research project carries one of them as `class_name`, or none. */
static void test_every_class_name_is_protostar_or_burst(void) {
    int with_class_name = 0;
    int k;

    assert(research_projects_count() > 0);
    for (k = 1; k <= research_projects_count(); k++) {
        const research_project_t *hp;
        const char *cw;

        hp = research_projects_at(k);
        cw = hp->class_name;
        if (cw == NULL) {
            continue;
        }
        assert(strcmp(cw, "protostar") == 0 || strcmp(cw, "burst") == 0);
        with_class_name++;
    }
    printf("[class_name] %d of %d researches\n", with_class_name, research_projects_count());
    assert(with_class_name > 0);
}

int main(void) {
    p8_vram_init(&memory);
    p8_gfx_init(&graphics);
    p8_sprite_sheet_load(&memory);
    p8_input_init(&input);
    dispatch_context_set_gfx(&graphics);
    dispatch_context_set_mem(&memory);
    dispatch_context_set_input(&input);

    cart_load();
    test_every_class_name_is_protostar_or_burst();
    reset_world(NULL);
    test_kind_fields_and_build();
    test_kind_reader_serves_the_generated_defaults();
    test_kind_fills_its_grab_arena();

    printf("test_entity_kind: all tests passed\n");
    return 0;
}
