#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "engine/entity.h"
#include "engine/entity_registry.h"
#include "engine/entity_geom.h"
#include "engine/exit_queue.h"
#include "engine/game_zone.h"
#include "engine/entity_id.h"
#include "engine/zone_sizes.h"
#include "p8/fixed.h"
#include "game/game_data.h"

static const entity_class_t tagged_cls = {.tags = TAG(TAG_PROBE)};
static const entity_class_t untagged_cls = {0};

/* Each draw, overlay or camera call, in order: an entity's index in g_entities, or -layer for the camera. */
static int g_calls[64];
static int g_call_count;
static p8_fixed_t g_fades[64];

static entity_t g_entities[ENTITY_TABLE_CAP + 1];

static void record(int call) {
    assert(g_call_count < 64);
    g_calls[g_call_count++] = call;
}

static void stub_draw(entity_t *self, vec_t screen_pos) {
    (void)screen_pos;
    record((int)(self - g_entities));
}

static void stub_apply_camera(entity_t *self, p8_fixed_t fade) {
    (void)self;
    g_fades[g_call_count] = fade;
    record(-1);
}

static const entity_class_t drawn_cls = {.draw = stub_draw};
static const entity_class_t overlaid_cls = {.draw_overlay = stub_draw};
static const entity_class_t camera_cls = {.apply_camera = stub_apply_camera};
static entity_t g_camera = {.class = &camera_cls};

static void registry_world(void) {
    entity_registry_shutdown();
    game_zone_reset();
    entity_registry_init(NULL);
}

static entity_t *fresh_entity(int i, const entity_class_t *cls) {
    entity_t none = {0};
    g_entities[i] = none;
    g_entities[i].class = cls;
    return &g_entities[i];
}

static void a_tag_never_registered_counts_zero(void) {
    registry_world();
    entity_register(fresh_entity(0, &untagged_cls));
    assert(entities_by_tag(TAG_PROBE) == NULL);
    assert(entity_registry_count_tagged(TAG_PROBE) == 0);
}

static void the_count_follows_registered_entities_of_the_tag(void) {
    registry_world();
    entity_t *first = fresh_entity(0, &tagged_cls);
    entity_register(first);
    entity_register(fresh_entity(1, &tagged_cls));
    entity_register(fresh_entity(2, &untagged_cls));
    assert(entity_registry_count_tagged(TAG_PROBE) == 2);
    entity_unregister(first);
    exit_queue_flush();
    assert(entity_registry_count_tagged(TAG_PROBE) == 1);
}

static void setting_a_layer_writes_the_field_and_its_bit(void) {
    registry_world();
    entity_t *e = fresh_entity(0, &untagged_cls);
    entity_registry_set_layer(e, p8_fixed_from_int(7));
    assert(entity_has(e, ENTITY_HAS_LAYER));
    assert(e->layer == p8_fixed_from_int(7));
}

static entity_t *layered(int i, const entity_class_t *cls, int layer) {
    entity_t *e = fresh_entity(i, cls);
    entity_registry_set_layer(e, p8_fixed_from_int(layer));
    return e;
}

static void a_registered_entity_joins_the_end_of_its_layer(void) {
    registry_world();
    entity_t *first = entity_register(layered(0, &drawn_cls, 4));
    entity_t *second = entity_register(layered(1, &drawn_cls, 4));
    entity_layer_view_t four = entity_registry_layer(ENTITY_MESSAGE_DRAW, 4);
    assert(four.count == 2 && four.items[0] == first && four.items[1] == second);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW, 3).count == 0);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW_OVERLAY, 4).count == 0);
}

static void unregistering_keeps_the_order_of_the_rest(void) {
    registry_world();
    entity_t *first = entity_register(layered(0, &drawn_cls, 4));
    entity_t *middle = entity_register(layered(1, &drawn_cls, 4));
    entity_t *last = entity_register(layered(2, &drawn_cls, 4));
    entity_t *above = entity_register(layered(3, &drawn_cls, 9));
    entity_unregister(middle);
    exit_queue_flush();
    entity_layer_view_t four = entity_registry_layer(ENTITY_MESSAGE_DRAW, 4);
    assert(four.count == 2 && four.items[0] == first && four.items[1] == last);
    entity_layer_view_t nine = entity_registry_layer(ENTITY_MESSAGE_DRAW, 9);
    assert(nine.count == 1 && nine.items[0] == above);
}

static entity_t *constructed(int i, const entity_class_t *cls, int layer) {
    entity_t *e = layered(i, cls, layer);
    entity_construct(e);
    return e;
}

static void a_layer_change_lands_at_registration_rank(void) {
    registry_world();
    entity_t *older = constructed(0, &drawn_cls, 4);
    entity_t *mover = constructed(1, &drawn_cls, 2);
    entity_t *newer = constructed(2, &drawn_cls, 4);
    entity_registry_set_layer(mover, p8_fixed_from_int(4));
    entity_layer_view_t four = entity_registry_layer(ENTITY_MESSAGE_DRAW, 4);
    assert(four.count == 3 && four.items[0] == older && four.items[1] == mover && four.items[2] == newer);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW, 2).count == 0);
}

static void draw_layers_applies_the_camera_once_per_drawn_layer_then_its_entities(void) {
    registry_world();
    entity_registry_set_main_camera(&g_camera);
    entity_register(layered(0, &drawn_cls, 7));
    entity_register(layered(1, &drawn_cls, 3));
    entity_register(layered(2, &drawn_cls, 7));
    entity_register(layered(3, &overlaid_cls, 3));
    g_call_count = 0;
    draw_layers(ENTITY_MESSAGE_DRAW);
    assert(g_call_count == 5);
    assert(g_calls[0] == -1 && g_fades[0] == layer_fades_table[2] && g_calls[1] == 1);
    assert(g_calls[2] == -1 && g_fades[2] == layer_fades_table[6] && g_calls[3] == 0 && g_calls[4] == 2);
    g_call_count = 0;
    draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY);
    assert(g_call_count == 1 && g_calls[0] == 3);
}

static void constructing_draw(entity_t *self, vec_t screen_pos) {
    (void)self;
    (void)screen_pos;
    constructed(1, &drawn_cls, 2);
}

static const entity_class_t constructing_cls = {.draw = constructing_draw};

static void a_draw_method_that_constructs_an_entity_stops_on_an_assert(void) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        registry_world();
        constructed(0, &constructing_cls, 5);
        draw_layers(ENTITY_MESSAGE_DRAW);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && "a draw method that registers an entity must assert");
}

static void a_layer_written_behind_the_registry_stops_on_an_assert(void) {
    fflush(stdout);
    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        assert(freopen("/dev/null", "w", stderr) != NULL);
        registry_world();
        constructed(0, &drawn_cls, 5)->layer = p8_fixed_from_int(6);
        draw_layers(ENTITY_MESSAGE_DRAW);
        _exit(0);
    }
    int status;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFSIGNALED(status) && WTERMSIG(status) == SIGABRT && "a layer written without entity_registry_set_layer must assert");
}

static void stub_overlay(entity_t *self, vec_t screen_pos) {
    (void)self;
    (void)screen_pos;
}

static const entity_class_t drawn_and_overlaid_cls = {.draw = stub_draw, .draw_overlay = stub_overlay};

static int reference_key(const entity_t *e) {
    int layer = p8_fixed_flr_int(e->layer);
    return entity_has(e, ENTITY_HAS_LAYER) && layer >= 1 && layer <= 15 ? layer : 0;
}

/* The grouping draw_layers built from the message bucket before the order was kept. */
static int matches_the_bucket_pass(entity_message_t message) {
    const entity_list_t *bucket = entities_by_message(message);
    int layer, index;

    for (layer = 1; layer <= 15; layer++) {
        entity_layer_view_t group = entity_registry_layer(message, layer);
        int at = 0;
        for (index = 0; index < bucket->count; index++) {
            if (reference_key(bucket->items[index]) != layer) continue;
            if (at >= group.count || group.items[at] != bucket->items[index]) return 0;
            at++;
        }
        if (at != group.count) return 0;
    }
    return 1;
}

static unsigned g_seed = 12345u;

static int random_below(int n) {
    g_seed = g_seed * 1103515245u + 12345u;
    return (int)((g_seed >> 16) % (unsigned)n);
}

static p8_fixed_t random_layer(void) {
    return p8_fixed_from_int(random_below(19) - 1) + (p8_fixed_t)random_below(0x10000);
}

#define POOL 40

static void the_kept_order_follows_any_mix_of_operations(void) {
    static const entity_class_t *const classes[] = {&drawn_cls, &overlaid_cls, &drawn_and_overlaid_cls};
    int live[POOL] = {0};
    int step;

    registry_world();
    for (step = 0; step < 4000; step++) {
        int i = random_below(POOL);
        int op = random_below(3);
        if (!live[i]) {
            entity_t *e = fresh_entity(i, classes[random_below(3)]);
            if (random_below(5) != 0) entity_registry_set_layer(e, random_layer());
            entity_construct(e);
            live[i] = 1;
        } else if (op == 0) {
            entity_unregister(&g_entities[i]);
            exit_queue_flush();
            live[i] = 0;
        } else if (op == 1) {
            entity_registry_set_layer(&g_entities[i], g_entities[i].layer);
        } else {
            entity_registry_set_layer(&g_entities[i], random_layer());
        }
        assert(matches_the_bucket_pass(ENTITY_MESSAGE_DRAW) && matches_the_bucket_pass(ENTITY_MESSAGE_DRAW_OVERLAY));
    }
}

static void a_same_value_rewrite_does_not_move_the_entity(void) {
    registry_world();
    entity_t *first = constructed(0, &drawn_cls, 4);
    entity_t *second = constructed(1, &drawn_cls, 4);
    entity_registry_set_layer(first, p8_fixed_from_int(4) + 0x8000);
    entity_layer_view_t four = entity_registry_layer(ENTITY_MESSAGE_DRAW, 4);
    assert(four.count == 2 && four.items[0] == first && four.items[1] == second);
}

static void a_layerless_entity_is_never_drawn_and_can_be_unregistered(void) {
    registry_world();
    entity_t *bare = fresh_entity(0, &drawn_cls);
    entity_construct(bare);
    entity_t *out_of_range = constructed(1, &drawn_cls, 16);
    g_call_count = 0;
    draw_layers(ENTITY_MESSAGE_DRAW);
    assert(g_call_count == 0);
    entity_unregister(bare);
    entity_unregister(out_of_range);
    exit_queue_flush();
    assert(entities_by_message(ENTITY_MESSAGE_DRAW)->count == 0);
}

static void a_layer_set_before_construction_lands_at_the_end_of_its_group(void) {
    registry_world();
    entity_t *older = constructed(0, &drawn_cls, 4);
    entity_t *newer = fresh_entity(1, &drawn_cls);
    entity_registry_set_layer(newer, p8_fixed_from_int(2));
    entity_registry_set_layer(newer, p8_fixed_from_int(4));
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW, 2).count == 0);
    entity_construct(newer);
    entity_layer_view_t four = entity_registry_layer(ENTITY_MESSAGE_DRAW, 4);
    assert(four.count == 2 && four.items[0] == older && four.items[1] == newer);
}

static void forgetting_or_initializing_the_registries_empties_both_orders(void) {
    registry_world();
    constructed(0, &drawn_and_overlaid_cls, 4);
    entity_registry_forget_registries();
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW, 4).count == 0);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW_OVERLAY, 4).count == 0);
    registry_world();
    constructed(0, &drawn_and_overlaid_cls, 4);
    entity_registry_init(NULL);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW, 4).count == 0);
    assert(entity_registry_layer(ENTITY_MESSAGE_DRAW_OVERLAY, 4).count == 0);
}

#define STDERR_LOG "tmp/test_entity_registry_stderr.txt"

/* Runs in a child process: p8_fatal exits. */
static int draw_one_entity_too_many(void) {
    registry_world();
    for (int i = 0; i <= ENTITY_TABLE_CAP; i++) entity_register(layered(i, &drawn_cls, 1));
    return 0;
}

static void a_full_layer_order_is_fatal(const char *self_path) {
    char cmd[512];
    snprintf(cmd, sizeof cmd, "%s full > /dev/null 2> " STDERR_LOG, self_path);
    int status = system(cmd);
    assert(status != -1 && WIFEXITED(status));
    FILE *log = fopen(STDERR_LOG, "rb");
    char err[512] = {0};
    assert(log != NULL);
    (void)fread(err, 1, sizeof err - 1, log);
    fclose(log);
    if (WEXITSTATUS(status) != 3 || strstr(err, "layer order full, limit 512") == NULL) {
        fprintf(stderr, "full: expected exit 3 and the limit, exit %d\n%s", WEXITSTATUS(status), err);
        assert(0 && "registering past the layer order's capacity must call p8_fatal");
    }
    remove(STDERR_LOG);
}

static vec_t g_drawn_at[64];

static void stub_draw_at(entity_t *self, vec_t screen_pos) {
    g_drawn_at[g_call_count] = screen_pos;
    record((int)(self - g_entities));
}

static const entity_class_t culled_cls = {.draw = stub_draw_at, .draw_overlay = stub_draw_at, .skip_draw_off_screen = 1};
static const entity_class_t unculled_cls = {.draw = stub_draw_at};

static entity_t *placed(int i, const entity_class_t *cls, int layer, int x, int y) {
    entity_t *e = layered(i, cls, layer);
    entity_set_pos(e, vec_make(p8_fixed_from_int(x), p8_fixed_from_int(y)));
    return entity_register(e);
}

static void draw_layers_skips_an_off_screen_entity_of_a_culled_class_on_draw_only(void) {
    static const int camera_xs[] = {0, 200, -77};
    registry_world();
    entity_registry_set_main_camera(&g_camera);
    placed(0, &culled_cls, 6, 64, 64);
    placed(1, &culled_cls, 6, 300, 64);
    placed(2, &unculled_cls, 6, 300, 64);
    placed(3, &culled_cls, 6, 64, -60);
    placed(4, &culled_cls, 6, 139, 139);
    for (size_t c = 0; c < sizeof camera_xs / sizeof camera_xs[0]; c++) {
        entity_set_pos(&g_camera, vec_make(p8_fixed_from_int(camera_xs[c]), 0));
        int want[5];
        int wanted = 0;
        for (int i = 0; i < 5; i++) {
            if (g_entities[i].class == &unculled_cls || entity_geom_is_on_screen(&g_entities[i])) {
                want[wanted++] = i;
            }
        }
        g_call_count = 0;
        draw_layers(ENTITY_MESSAGE_DRAW);
        assert(g_call_count == 1 + wanted && g_calls[0] == -1);
        for (int k = 0; k < wanted; k++) {
            assert(g_calls[1 + k] == want[k]);
            assert(g_drawn_at[1 + k].x == g_entities[want[k]].pos.x && g_drawn_at[1 + k].y == g_entities[want[k]].pos.y);
        }
    }
    entity_set_pos(&g_camera, vec_make(0, 0));
    assert(!entity_geom_is_on_screen(&g_entities[1]) && entity_geom_is_on_screen(&g_entities[0]));
    g_call_count = 0;
    draw_layers(ENTITY_MESSAGE_DRAW_OVERLAY);
    assert(g_call_count == 4 && "draw_overlay is never culled");
    entity_clear_pos(&g_camera);
}

static void the_generation_moves_on_every_registration_change_only(void) {
    registry_world();
    unsigned before = entity_registry_generation();
    entity_t *e = constructed(0, &drawn_cls, 2);
    unsigned registered = entity_registry_generation();
    assert(registered != before);
    entity_registry_set_layer(e, p8_fixed_from_int(3));
    (void)entities_by_message(ENTITY_MESSAGE_DRAW);
    (void)entity_registry_count_tagged(TAG_PROBE);
    assert(entity_registry_generation() == registered && "a layer change or a read is not a registration");
    entity_unregister(e);
    unsigned unregistered = entity_registry_generation();
    assert(unregistered != registered);
    entity_registry_forget_registries();
    assert(entity_registry_generation() != unregistered);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "full") == 0) return draw_one_entity_too_many();
    the_kept_order_follows_any_mix_of_operations();
    a_same_value_rewrite_does_not_move_the_entity();
    a_layerless_entity_is_never_drawn_and_can_be_unregistered();
    a_layer_set_before_construction_lands_at_the_end_of_its_group();
    forgetting_or_initializing_the_registries_empties_both_orders();
    a_full_layer_order_is_fatal(argv[0]);
    a_draw_method_that_constructs_an_entity_stops_on_an_assert();
    a_layer_written_behind_the_registry_stops_on_an_assert();
    draw_layers_applies_the_camera_once_per_drawn_layer_then_its_entities();
    draw_layers_skips_an_off_screen_entity_of_a_culled_class_on_draw_only();
    a_layer_change_lands_at_registration_rank();
    unregistering_keeps_the_order_of_the_rest();
    a_registered_entity_joins_the_end_of_its_layer();
    setting_a_layer_writes_the_field_and_its_bit();
    a_tag_never_registered_counts_zero();
    the_count_follows_registered_entities_of_the_tag();
    the_generation_moves_on_every_registration_change_only();
    printf("test_entity_registry: all tests passed\n");
    return 0;
}
