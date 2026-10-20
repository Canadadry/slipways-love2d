#include "engine/entity_registry.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "engine/entity_geom.h"
#include "engine/entity_id.h"
#include "engine/exit_queue.h"
#include "game/game_data.h"
#include "engine/entity_class.h"
#include "engine/game_zone.h"
#include "engine/zone_sizes.h"
#include "p8/fatal.h"

static entity_list_t g_entities_by_message[ENTITY_MESSAGE_COUNT];

typedef struct {
    int seen;
    entity_list_t list;
} entities_by_tag_entry_t;

static entities_by_tag_entry_t g_entities_by_tag[TAG_COUNT];

/* Group g (1..15) is items[start[g - 1] .. start[g]); start[ENTITY_REGISTRY_LAYERS] is the count. */
typedef struct {
    entity_t *items[ENTITY_TABLE_CAP];
    int start[ENTITY_REGISTRY_LAYERS + 1];
} layer_order_t;

static layer_order_t g_layer_orders[2];
static unsigned g_layer_order_mutations;

static entity_list_t g_live_entities;
static entity_t *g_main_camera = NULL;
static int g_initialized = 0;
static unsigned g_generation;

static const entity_class_t main_camera_placeholder_class_def = {0};

typedef struct {
    entity_t header;
} main_camera_placeholder_t;

static main_camera_placeholder_t g_main_camera_placeholder = {{.class = &main_camera_placeholder_class_def}};

static void layer_orders_reset(void) {
    memset(g_layer_orders[0].start, 0, sizeof g_layer_orders[0].start);
    memset(g_layer_orders[1].start, 0, sizeof g_layer_orders[1].start);
}

void entity_registry_shutdown(void) {
    int i;

    layer_orders_reset();
    g_generation++;
    if (!g_initialized) {
        return;
    }

    entity_list_free(&g_live_entities);

    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        entity_list_free(&g_entities_by_message[i]);
    }

    for (i = 0; i < TAG_COUNT; i++) {
        entity_list_free(&g_entities_by_tag[i].list);
        g_entities_by_tag[i].seen = 0;
    }

    entity_id_table_forget();

    g_main_camera = NULL;

    g_initialized = 0;
}

void entity_registry_init(const entity_registry_reserve_t *reserve) {
    static const entity_registry_reserve_t no_reserve = {0};
    int i;

    if (reserve == NULL) reserve = &no_reserve;

    entity_registry_shutdown();

    entity_id_table_init(game_zone());

    entity_list_init_reserve(&g_live_entities, game_zone(), reserve->live);

    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        entity_list_init_reserve(&g_entities_by_message[i], game_zone(), reserve->by_message[i]);
    }

    for (i = 0; i < TAG_COUNT; i++) {
        entity_list_init_reserve(&g_entities_by_tag[i].list, game_zone(), reserve->by_tag[i]);
    }

    g_main_camera = (entity_t *)&g_main_camera_placeholder;

    g_initialized = 1;
}

void entity_registry_abandon_live_entities(void) {
    int i;

    for (i = 0; i < g_live_entities.count; i++) {
        if (g_live_entities.items[i]->class->release != NULL) {
            exit_queue_put(g_live_entities.items[i]);
        }
    }
}

static void forget_list(entity_list_t *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}

void entity_registry_forget_registries(void) {
    int i;

    entity_id_table_forget();
    layer_orders_reset();
    g_generation++;
    forget_list(&g_live_entities);
    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        forget_list(&g_entities_by_message[i]);
    }
    for (i = 0; i < TAG_COUNT; i++) {
        forget_list(&g_entities_by_tag[i].list);
        g_entities_by_tag[i].seen = 0;
    }
}

unsigned entity_registry_generation(void) {
    return g_generation;
}

const entity_list_t *live_entities(void) {
    return &g_live_entities;
}

const entity_list_t *entities_by_message(entity_message_t message) {
    assert(g_initialized && "entity_registry_init() must run before entities_by_message is read");
    return &g_entities_by_message[message];
}

static int is_message_name(const entity_t *e, entity_message_t message) {
    const entity_class_t *cls;

    cls = e->class;

    switch (message) {
    case ENTITY_MESSAGE_DRAW:
        return cls->draw != NULL;
    case ENTITY_MESSAGE_DRAW_OVERLAY:
        return cls->draw_overlay != NULL;
    case ENTITY_MESSAGE_HIT_TEST:
        return cls->hit_test != NULL;
    case ENTITY_MESSAGE_NEW_YEAR:
        return cls->new_year != NULL;
    default:
        return cls->refresh != NULL && (cls->has_refresh == NULL || cls->has_refresh(e));
    }
}

int entity_registry_hit_test_always(entity_t *self, mouse_pos_t mouse) {
    (void)self;
    (void)mouse;
    return 1;
}

const entity_list_t *entities_by_tag(tag_t tag) {
    return g_entities_by_tag[tag].seen ? &g_entities_by_tag[tag].list : NULL;
}

int entity_registry_count_tagged(tag_t tag) {
    const entity_list_t *tagged = entities_by_tag(tag);
    return tagged == NULL ? 0 : tagged->count;
}

static int is_drawn_message(int message) {
    return message == ENTITY_MESSAGE_DRAW || message == ENTITY_MESSAGE_DRAW_OVERLAY;
}

static layer_order_t *layer_order_of(entity_message_t message) {
    return &g_layer_orders[message == ENTITY_MESSAGE_DRAW ? 0 : 1];
}

/* 0 for no layer or one outside 1..15: such an entity is never drawn. */
static int layer_key(const entity_t *e) {
    int layer;

    if (!entity_has(e, ENTITY_HAS_LAYER)) {
        return 0;
    }
    layer = p8_fixed_flr_int(e->layer);
    return layer < 1 || layer > ENTITY_REGISTRY_LAYERS ? 0 : layer;
}

static void layer_order_insert(layer_order_t *order, entity_t *e, int key, int index) {
    int count = order->start[ENTITY_REGISTRY_LAYERS];
    int g;

    if (count >= ENTITY_TABLE_CAP) {
        char message[64];
        snprintf(message, sizeof message, "layer order full, limit %d", ENTITY_TABLE_CAP);
        p8_fatal(message);
    }
    memmove(&order->items[index + 1], &order->items[index], (size_t)(count - index) * sizeof order->items[0]);
    order->items[index] = e;
    for (g = key; g <= ENTITY_REGISTRY_LAYERS; g++) {
        order->start[g]++;
    }
    g_layer_order_mutations++;
}

static void layer_order_remove(layer_order_t *order, const entity_t *e) {
    int count = order->start[ENTITY_REGISTRY_LAYERS];
    int index, g;

    for (index = 0; index < count && order->items[index] != e; index++) {
    }
    if (index == count) {
        return;
    }
    memmove(&order->items[index], &order->items[index + 1], (size_t)(count - index - 1) * sizeof order->items[0]);
    for (g = ENTITY_REGISTRY_LAYERS; g >= 1 && order->start[g] > index; g--) {
        order->start[g]--;
    }
    g_layer_order_mutations++;
}

entity_layer_view_t entity_registry_layer(entity_message_t message, int layer) {
    const layer_order_t *order;
    entity_layer_view_t view;

    assert(is_drawn_message(message) && "entity_registry_layer: only draw and draw_overlay keep a layer order");
    assert(layer >= 1 && layer <= ENTITY_REGISTRY_LAYERS && "entity_registry_layer: layers run 1..15");
    order = layer_order_of(message);
    view.items = &order->items[order->start[layer - 1]];
    view.count = order->start[layer] - order->start[layer - 1];
    return view;
}

/* Missing or malformed data is skipped where Lua would crash: message members are tested by presence, unregistering an unknown entity does nothing, draw_layers drops an entity with no layer or one outside 1..15. Making one site fatal alone breaks consistency. */
entity_t *entity_register(entity_t *e) {
    int i, key;

    g_generation++;

    entity_list_add(&g_live_entities, e);

    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        if (is_message_name(e, (entity_message_t)i)) {
            entity_list_add(&g_entities_by_message[i], e);
        }
    }

    key = layer_key(e);
    for (i = 0; i < ENTITY_MESSAGE_COUNT && key != 0; i++) {
        if (is_drawn_message(i) && is_message_name(e, (entity_message_t)i)) {
            layer_order_t *order = layer_order_of((entity_message_t)i);
            layer_order_insert(order, e, key, order->start[key]);
        }
    }

    for (i = 0; i < TAG_COUNT; i++) {
        if (e->class->tags & TAG(i)) {
            g_entities_by_tag[i].seen = 1;
            entity_list_add(&g_entities_by_tag[i].list, e);
        }
    }

    return e;
}

/* A message member changed since entity_register() leaves the entity in a list this never removes it from. */
void entity_unregister(entity_t *e) {
    int i;

    g_generation++;

    if (entity_list_del(&g_live_entities, e)) {
        exit_queue_put(e);
    }

    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        if (is_message_name(e, (entity_message_t)i)) {
            entity_list_del(&g_entities_by_message[i], e);
            if (is_drawn_message(i)) {
                layer_order_remove(layer_order_of((entity_message_t)i), e);
            }
        }
    }

    for (i = 0; i < TAG_COUNT; i++) {
        if (e->class->tags & TAG(i)) {
            entity_list_del(&g_entities_by_tag[i].list, e);
        }
    }
}

/* Registration rank: the bucket members before e that share its key. */
static void layer_order_reinsert(entity_message_t message, entity_t *e, int key) {
    const entity_list_t *bucket = &g_entities_by_message[message];
    layer_order_t *order = layer_order_of(message);
    int rank = 0;
    int index;

    layer_order_remove(order, e);
    if (key == 0) {
        return;
    }
    for (index = 0; index < bucket->count && bucket->items[index] != e; index++) {
        if (layer_key(bucket->items[index]) == key) {
            rank++;
        }
    }
    if (index == bucket->count) {
        return;
    }
    layer_order_insert(order, e, key, order->start[key - 1] + rank);
}

void entity_registry_set_layer(entity_t *e, p8_fixed_t layer) {
    int old_key = layer_key(e);
    int new_key;
    int i;

    e->has |= ENTITY_HAS_LAYER;
    e->layer = layer;
    new_key = layer_key(e);
    if (new_key == old_key || entity_id_is_none(entity_id_of(e))) {
        return;
    }
    for (i = 0; i < ENTITY_MESSAGE_COUNT; i++) {
        if (is_drawn_message(i) && is_message_name(e, (entity_message_t)i)) {
            layer_order_reinsert((entity_message_t)i, e, new_key);
        }
    }
}

void draw_layers(entity_message_t message) {
    int is_draw = message == ENTITY_MESSAGE_DRAW;
    int layer, index;

    assert((is_draw || message == ENTITY_MESSAGE_DRAW_OVERLAY) && "draw_layers: only draw and draw_overlay go through draw_layers");

    for (layer = 1; layer <= ENTITY_REGISTRY_LAYERS; layer++) {
        entity_layer_view_t group = entity_registry_layer(message, layer);

        if (is_draw && group.count > 0) {
            p8_fixed_t ci;
            if (game_data_fixed_seq_at(layer_fades_table, LAYER_FADES_COUNT, layer, &ci) && g_main_camera != NULL &&
                g_main_camera->class->apply_camera != NULL) {
                g_main_camera->class->apply_camera(g_main_camera, ci);
            }
        }

        for (index = 0; index < group.count; index++) {
            entity_t *qc = group.items[index];
            void (*draw)(entity_t *self, vec_t screen_pos) = is_draw ? qc->class->draw : qc->class->draw_overlay;
            unsigned mutations = g_layer_order_mutations;

            assert(layer_key(qc) == layer && "draw_layers: a layer written directly, not through entity_registry_set_layer");
            if (draw != NULL && !(is_draw && qc->class->skip_draw_off_screen && !entity_geom_is_on_screen(qc))) {
                draw(qc, entity_pos(qc));
            }
            assert(g_layer_order_mutations == mutations &&
                   "draw_layers: a draw method must not register, unregister or re-layer an entity -- it walks the kept order in place");
            (void)mutations;
        }
    }
}

const entity_class_t *main_camera_placeholder_class(void) {
    return &main_camera_placeholder_class_def;
}

entity_t *main_camera(void) {
    return g_main_camera;
}

void entity_registry_set_main_camera(entity_t *instance) {
    if (instance == g_main_camera) {
        return;
    }
    g_main_camera = instance;
}
