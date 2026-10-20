#ifndef ENGINE_ENTITY_CLASS_H
#define ENGINE_ENTITY_CLASS_H

#include "p8/fixed.h"
#include "engine/geom.h"
#include "engine/vec.h"
#include "engine/entity_id_type.h"

typedef enum {
    ENTITY_STATE_NONE,
    ENTITY_STATE_NORMAL,
    ENTITY_STATE_SETTLED,
    ENTITY_STATE_VANISHING,
    ENTITY_STATE_PLACING,
    ENTITY_STATE_UNFOLDING,
    ENTITY_STATE_UNKNOWN,
    ENTITY_STATE_KNOWN,
    STATE_COUNT
} state_t;

typedef enum {
    TAG_STRUCTURE,
    TAG_ASCENSION,
    TAG_SLIPGATE,
    TAG_PROBE,
    TAG_MESSAGE_BOX,
    TAG_SLIPWAY,
    TAG_WORMHOLE,
    TAG_PLANET,
    TAG_NODE,
    TAG_COUNT
} tag_t;

#define TAG(t) (1u << (t))

typedef enum {
    LINEAGE_PLANET,
    LINEAGE_STRUCTURE,
    LINEAGE_TRADING_HUB,
    LINEAGE_PROBE,
} lineage_t;

#define LINEAGE(l) (1u << (l))

typedef struct entity_class entity_class_t;

#define ENTITY_HAS_LAYER (1u << 0)
#define ENTITY_HAS_HIT_BOX (1u << 1)
#define ENTITY_HAS_INFRASPACE (1u << 3)
#define ENTITY_HAS_FOLD_ANCHOR (1u << 4)
#define ENTITY_HAS_SHAKE (1u << 5)
#define ENTITY_HAS_IS_DEAD (1u << 6)
#define ENTITY_HAS_IS_HOVERED (1u << 7)
#define ENTITY_HAS_POS (1u << 8)
#define ENTITY_HAS_MAP_POS (1u << 9)

typedef struct {
    const entity_class_t *class;
    state_t state;
    p8_fixed_t state_time;
    uint16_t has;
    p8_fixed_t layer;
    vec_t pos;
    vec_t map_pos;
    box_t hit_box;
    p8_fixed_t infraspace;
    p8_fixed_t shake;
    p8_fixed_t fold_anchor;
    int is_dead;
    int is_hovered;
    entity_id_t id;
} entity_t;

static inline void entity_init_header(entity_t *self, const entity_class_t *cls) {
    *self = (entity_t){.class = cls};
}

static inline int entity_has(const entity_t *self, unsigned bit) {
    return (self->has & bit) != 0;
}

static inline int entity_has_pos(const entity_t *self) {
    return entity_has(self, ENTITY_HAS_POS);
}

static inline int entity_has_map_pos(const entity_t *self) {
    return entity_has(self, ENTITY_HAS_MAP_POS);
}

/* The zero vector when the position is absent. */
static inline vec_t entity_pos(const entity_t *self) {
    return entity_has_pos(self) ? self->pos : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

/* The zero vector when the map position is absent. */
static inline vec_t entity_map_pos(const entity_t *self) {
    return entity_has_map_pos(self) ? self->map_pos : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

static inline void entity_set_pos(entity_t *self, vec_t pos) {
    self->pos = pos;
    self->has |= ENTITY_HAS_POS;
}

static inline void entity_clear_pos(entity_t *self) {
    self->has &= (uint16_t)~ENTITY_HAS_POS;
}

static inline void entity_set_map_pos(entity_t *self, vec_t map_pos) {
    self->map_pos = map_pos;
    self->has |= ENTITY_HAS_MAP_POS;
}

static inline void entity_clear_map_pos(entity_t *self) {
    self->has &= (uint16_t)~ENTITY_HAS_MAP_POS;
}

typedef struct {
    int stop;
    entity_id_t held;
} down_t;

#define MOUSE_POS_HAS_WORLD (1u << 0)
#define MOUSE_POS_HAS_SCREEN (1u << 1)

typedef struct {
    vec_t world;
    vec_t screen;
    uint16_t has;
} mouse_pos_t;

static inline int mouse_pos_has_world(mouse_pos_t mouse) {
    return (mouse.has & MOUSE_POS_HAS_WORLD) != 0;
}

static inline int mouse_pos_has_screen(mouse_pos_t mouse) {
    return (mouse.has & MOUSE_POS_HAS_SCREEN) != 0;
}

/* The zero vector when the world point is absent. */
static inline vec_t mouse_pos_world(mouse_pos_t mouse) {
    return mouse_pos_has_world(mouse) ? mouse.world : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

/* The zero vector when the screen point is absent. */
static inline vec_t mouse_pos_screen(mouse_pos_t mouse) {
    return mouse_pos_has_screen(mouse) ? mouse.screen : vec_make(p8_fixed_from_int(0), p8_fixed_from_int(0));
}

typedef void (*state_fn_t)(entity_t *self, p8_fixed_t t);

struct particle;
struct trade_offer;
struct str_seq;
struct kind_record;

struct entity_class {
    void (*draw)(entity_t *self, vec_t screen_pos);
    void (*draw_overlay)(entity_t *self, vec_t screen_pos);
    int (*hit_test)(entity_t *self, mouse_pos_t mouse);
    void (*new_year)(entity_t *self, p8_fixed_t year);
    void (*refresh)(entity_t *self);
    int (*has_refresh)(const entity_t *self);
    down_t (*left_press)(entity_t *self, mouse_pos_t mouse);
    down_t (*accept_drop)(entity_t *self, const entity_t *s);
    void (*drag)(entity_t *self, mouse_pos_t mouse);
    void (*drop)(entity_t *self, entity_t *dropped);
    void (*attach_slipway)(entity_t *self, entity_t *slipway);
    void (*offer_trade)(entity_t *self, struct trade_offer *offer);
    void (*draw_at)(entity_t *self, vec_t p, p8_fixed_t a);
    struct particle *(*spawn_particle)(entity_t *self);
    state_fn_t states[STATE_COUNT];
    unsigned tags;
    unsigned lineage;
    void (*apply_camera)(entity_t *self, p8_fixed_t parallax);
    void (*release)(entity_t *self);
    /* 1: draw_layers calls draw only when entity_geom_is_on_screen holds, so draw may trust it. */
    int skip_draw_off_screen;
};

typedef struct planet_keys planet_keys_t;

typedef struct {
    entity_t *(*new_entity)(const planet_keys_t *keys);
    const struct kind_record *record;
} entity_kind_t;

const entity_class_t *slipway_class(void);
const entity_class_t *probe_class(void);

#endif
