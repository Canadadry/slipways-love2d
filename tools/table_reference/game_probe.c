#include "game/probe.c"
#include "reference_class.h"
#include "table_reference.h"

void table_reference_game_probe(void) {
    const char *module = "game.probe";
    reference_class(module, "main_economy_placeholder_class_def", &main_economy_placeholder_class_def);
    reference_header(module, "g_main_economy_placeholder", &g_main_economy_placeholder.header,
                     "main_economy_placeholder_class_def", &main_economy_placeholder_class_def);
    table_reference_int(module, g_main_economy_placeholder.research_points, "g_main_economy_placeholder.research_points");
    table_reference_int(module, g_main_economy_placeholder.has_research_points,
                        "g_main_economy_placeholder.has_research_points");
    table_reference_int(module, g_main_economy_placeholder.credits, "g_main_economy_placeholder.credits");
    table_reference_int(module, g_main_economy_placeholder.has_credits, "g_main_economy_placeholder.has_credits");
    reference_class(module, "targeting_track_target_class_def", &targeting_track_target_class_def);
    reference_header(module, "g_targeting_track_target", &g_targeting_track_target.header,
                     "targeting_track_target_class_def", &targeting_track_target_class_def);
    table_reference_int(module, g_targeting_track_target.targeted_by.index, "g_targeting_track_target.targeted_by.index");
    table_reference_int(module, g_targeting_track_target.targeted_by.generation,
                        "g_targeting_track_target.targeted_by.generation");
    reference_class(module, "probe_class_def", &probe_class_def);
    table_reference_int(module, (long long)sizeof(probe_t), "SIZEOF.probe_t");
}
