#include "game/galaxy.h"

#include "game/economy_score.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "game/game_data.h"
#include "game/gen.h"
#include "game/planet.h"
#include "p8/str.h"
#include "engine/vec.h"
#include "engine/frame_zone.h"
#include "engine/game_zone.h"

void galaxy_populate(p8_fixed_t planet_count) {
    p8_fixed_t fixed_one = p8_fixed_from_int(1);
    p8_fixed_t threshold = P8_FIXED_LIT(0x00000ccc, 0.05);
    p8_fixed_t planet_id;

    for (planet_id = fixed_one; planet_id <= planet_count; planet_id = p8_fixed_add(planet_id, fixed_one)) {
        vec_t p_vec;
        char *id_num_str;
        planet_keys_t keys = {0};
        planet_t *made;

        /* Not an inert loop: every accepted planet registers under the planet tag, so later candidates are rejected against it. */
        do {
            p8_fixed_t rx = p8_fixed_add(p8_fixed_rnd(GALAXY_MAP_SPAN), GALAXY_MAP_MARGIN);
            p8_fixed_t ry = p8_fixed_add(p8_fixed_rnd(GALAXY_MAP_SPAN), GALAXY_MAP_MARGIN);
            p_vec = vec_make(rx, ry);
        } while (entity_geom_any_within(p_vec, entities_by_tag(TAG_PLANET), threshold));

        id_num_str = p8_tostr_fixed(frame_zone(), planet_id);
        planet_keys_set_id(&keys, p8_concat2(frame_zone(), "p", id_num_str));
        keys.map_pos = p_vec;
        keys.has_map_pos = 1;
        keys.pos = vec_mul(p_vec, p8_fixed_from_int(GALAXY_MAP_SCALE));
        keys.has_pos = 1;

        made = planet_new(&keys);
        (void)made;
    }

    galaxy_install_rolls();
}

void galaxy_install_rolls(void) {
    const button_t *difficulty = economy_score_difficulty();
    p8_fixed_t difficulty_type_bias = p8_fixed_from_int(0);
    random_walk_t *type_picker_random;
    weighted_picker_t *type_picker_state;
    random_walk_t *vanish_roll_state;

    if (difficulty != NULL && difficulty->has_type_bias) {
        difficulty_type_bias = difficulty->type_bias;
    }

    type_picker_random = random_walk_new(game_zone(), P8_FIXED_LIT(0x00008000, 0.5), difficulty_type_bias);
    type_picker_state = weighted_picker_new(game_zone(), p8_fixed_from_int(2), random_walk_draw, type_picker_random);
    planet_set_type_picker(type_picker_state);

    vanish_roll_state = random_walk_new(game_zone(), P8_FIXED_LIT(0x0000cccc, 0.8), P8_FIXED_LIT(0x00008000, 0.5));
    planet_set_vanish_roll(vanish_roll_state);
}
