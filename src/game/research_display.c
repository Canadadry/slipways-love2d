#include "game/research_display.h"

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "game/layout.h"
#include "game/map_tables.h"
#include "game/planet.h"
#include "game/research.h"
#include "p8/str.h"
#include "game/tech_gate.h"
#include "engine/frame_zone.h"

static char *research_project_tooltip_join(const research_project_t *self) {
    const pointer_list_t *i_list;
    size_t capacity = 16;
    size_t length = 0;
    char *names = (char *)allocator_alloc(frame_zone(), capacity);
    int first = 1;
    pointer_list_iter_t iter;
    void *entry;

    names[0] = '\0';
    i_list = &self->inputs;
    iter = pointer_list_all(i_list);
    while (pointer_list_iter_next(&iter, &entry)) {
        resource_t qe = {.kind = RESOURCE_NIL};
        char *num_str;
        const char *resource_names_name;
        const char *piece = "";
        size_t piece_len;
        size_t add_len;

        if (entry != NULL) {
            qe = need_resource(entry);
        }
        num_str = qe.kind == RESOURCE_FIXED ? p8_tostr_fixed(frame_zone(), qe.number) : NULL;
        resource_names_name = num_str != NULL ? resource_names_lookup(num_str) : NULL;
        if (resource_names_name != NULL) {
            piece = resource_names_name;
        }

        piece_len = strlen(piece);
        add_len = (first ? 0 : 1) + piece_len;
        if (length + add_len + 1 > capacity) {
            size_t new_cap = (length + add_len + 1) * 2;

            names = (char *)allocator_grow(frame_zone(), (unsigned char *)names, capacity, new_cap);
            capacity = new_cap;
        }
        if (!first) {
            names[length++] = ',';
        }
        memcpy(names + length, piece, piece_len + 1);
        length += piece_len;
        first = 0;
    }
    return names;
}

typedef struct {
    data_row_t outer;
    data_item_t outer_item;
    data_row_t inner;
    data_item_t inner_items[3];
} research_project_tooltip_t;

const data_row_t *research_project_tooltip(const research_project_t *self) {
    research_project_tooltip_t *jb;

    research_project_assert(self);
    if (self == NULL) {
        return NULL;
    }
    if (self->has_icon_sprite) {
        return NULL;
    }

    jb = (research_project_tooltip_t *)allocator_alloc(frame_zone(), sizeof *jb);
    memset(jb, 0, sizeof *jb);
    jb->inner_items[0].text = research_project_tooltip_join(self);
    jb->inner_items[1].number = 13;
    jb->inner.count = 2;
    jb->inner.items = jb->inner_items;

    if (self->has_output) {
        char *o_str = p8_tostr_fixed(frame_zone(), self->output);
        const char *resource_names_o = resource_names_lookup(o_str);

        if (resource_names_o != NULL) {
            jb->inner_items[2].text = resource_names_o;
            jb->inner.count = 3;
        }
    }

    jb->outer_item.row = &jb->inner;
    jb->outer.count = 1;
    jb->outer.items = &jb->outer_item;
    return &jb->outer;
}

static layout_row_cell_t planet_resource_row_pair(layout_row_scalar_t first, p8_fixed_t width) {
    layout_row_tuple_t tuple = {.count = 2, .scalars = {first, layout_row_scalar_fixed(width)}};

    return layout_row_cell_tuple(tuple);
}

static int planet_resource_row_keys(const pointer_list_t *inputs, int has_output, p8_fixed_t output, p8_fixed_t amount,
                                    p8_fixed_t used, p8_fixed_t spacing, layout_row_keys_t *out_keys) {
    int l_cap;
    layout_row_keys_t keys = {0};
    int crowded;
    int is_count = (inputs != NULL) ? inputs->count : 0;
    p8_fixed_t outputs[2];
    int outputs_count;
    int wide;

    if (!has_output) {
        return 0;
    }

    crowded = (is_count >= 3) ? 1 : 0;

    {
        int r_w;
        int o_is_r_w = resource_code_lookup("w", &r_w) && output == p8_fixed_from_int(r_w);

        if (o_is_r_w) {
            outputs[0] = p8_fixed_from_int(25);
            outputs[1] = p8_fixed_from_int(23);
            outputs_count = 2;
        } else {
            outputs[0] = output;
            outputs_count = 1;
        }
    }

    wide = p8_fixed_mul(amount, p8_fixed_from_int(outputs_count)) >= p8_fixed_from_int(3);

    {
        p8_fixed_t amount_drawn = amount > p8_fixed_from_int(5) ? p8_fixed_from_int(1) : amount;
        p8_fixed_t loop_bound = (amount_drawn > p8_fixed_from_int(1)) ? amount_drawn : p8_fixed_from_int(1);

        l_cap = is_count + (is_count > 0 ? 2 : 0) + (amount > p8_fixed_from_int(5) ? 2 : 0) +
                outputs_count * (p8_fixed_flr_int(loop_bound) + 1);
    }

    if (inputs != NULL) {
        pointer_list_iter_t it = pointer_list_all(inputs);
        void *entry;

        while (pointer_list_iter_next(&it, &entry)) {
            layout_row_tuple_t tuple = {.count = 3};
            resource_t qe = {.kind = RESOURCE_NIL};
            int gc_zero = 0;

            if (entry != NULL) {
                const need_t *i = entry;

                qe = need_resource(i);
                gc_zero = i->has_satisfied && i->satisfied == 0;
            }
            tuple.scalars[0] = qe.kind == RESOURCE_FIXED ? layout_row_scalar_fixed(qe.number) : layout_row_scalar_nothing();
            tuple.scalars[1] = layout_row_scalar_fixed(p8_fixed_sub(p8_fixed_from_int(3), p8_fixed_from_int(crowded)));
            tuple.scalars[2] = layout_row_scalar_fixed(p8_fixed_from_int(gc_zero ? 7 : 0));
            layout_row_keys_append(&keys, layout_row_cell_tuple(tuple));
        }
    }

    if (is_count > 0) {
        int use_35 = (spacing == p8_fixed_from_int(1)) || wide;

        layout_row_keys_append(&keys, planet_resource_row_pair(layout_row_scalar_nothing(), p8_fixed_add(spacing, p8_fixed_from_int(crowded))));
        if (use_35) {
            layout_row_keys_append(&keys, planet_resource_row_pair(layout_row_scalar_fixed(p8_fixed_from_int(35)), p8_fixed_add(p8_fixed_from_int(2), spacing)));
        } else {
            layout_row_keys_append(&keys, planet_resource_row_pair(layout_row_scalar_fixed(p8_fixed_from_int(32)), p8_fixed_from_int(5)));
        }
    }

    if (amount > p8_fixed_from_int(5)) {
        char *amount_text = p8_tostr_fixed(frame_zone(), amount);
        static const data_item_t tail_row_items[] = {{.is_false = 1}, {.number = -4}};
        static const data_row_t tail_row = {.count = 2, .items = tail_row_items};

        layout_row_keys_append(&keys, layout_row_cell_string(amount_text));
        layout_row_keys_append(&keys, layout_row_cell_row(&tail_row));
        keys.has_color = 1;
        keys.color = p8_fixed_from_int(13);
        amount = p8_fixed_from_int(1);
    }

    {
        int output_index;

        for (output_index = 0; output_index < outputs_count; output_index++) {
            p8_fixed_t qq = outputs[output_index];
            p8_fixed_t loop_bound = (amount > p8_fixed_from_int(1)) ? amount : p8_fixed_from_int(1);
            int n = p8_fixed_flr_int(loop_bound);
            int i;

            for (i = 1; i <= n; i++) {
                p8_fixed_t i_fixed = p8_fixed_from_int(i);
                int within = (i_fixed <= amount) && (i_fixed > used);
                layout_row_tuple_t tuple = {.count = 4,
                                            .scalars = {layout_row_scalar_fixed(qq), layout_row_scalar_fixed(p8_fixed_from_int(6)),
                                                        layout_row_scalar_fixed(p8_fixed_from_int(within ? 0 : 7)),
                                                        layout_row_scalar_fixed(p8_fixed_from_int(wide ? -4 : -3))}};

                layout_row_keys_append(&keys, layout_row_cell_tuple(tuple));
            }

            layout_row_keys_append(&keys, layout_row_cell_row(planet_spacer()));
        }
    }

    assert(keys.count == l_cap && "planet_resource_row_keys: the row is sized at entry from the inputs, the amount and the outputs; a positional more or less "
                               "is a miscount of that size");
    *out_keys = keys;
    return 1;
}

int research_display_planet_resource_row_keys(const pointer_list_t *inputs, int has_output, p8_fixed_t output, p8_fixed_t amount,
                                              p8_fixed_t used, p8_fixed_t spacing, layout_row_keys_t *out_keys) {
    return planet_resource_row_keys(inputs, has_output, output, amount, used, spacing, out_keys);
}

int research_project_layout_row_keys(const research_project_t *self, layout_row_keys_t *out_keys) {
    p8_fixed_t jq;
    const production_tables_entry_t *po;

    research_project_assert(self);
    if (self == NULL || !research_project_ready()) {
        return 0;
    }

    po = research_project_production(self);
    jq = p8_fixed_from_int(1);
    if (po != NULL && po->count >= 1) {
        p8_fixed_t first_production = p8_fixed_from_int(po->values[0]);
        jq = (first_production > p8_fixed_from_int(1)) ? first_production : p8_fixed_from_int(1);
    }

    if (!planet_resource_row_keys(&self->inputs, self->has_output, self->output, jq, p8_fixed_from_int(0), p8_fixed_from_int(1), out_keys)) {
        return 0;
    }

    layout_row_keys_append(out_keys, layout_row_cell_row(research_project_row_tail()));
    if (self->name != NULL) {
        layout_row_keys_append(out_keys, layout_row_cell_string(self->name));
    }
    return 1;
}

layout_row_keys_t research_project_icon_row_keys(const research_project_t *self) {
    layout_row_keys_t keys = {0};
    layout_row_tuple_t icon = {.count = 2};

    assert(self->has_icon_sprite && "research_project_icon_row_keys: the row {name, 32, {icon_sprite, 8}} is a project with an icon sprite's");
    icon.scalars[0] = layout_row_scalar_fixed(self->icon_sprite);
    icon.scalars[1] = layout_row_scalar_fixed(p8_fixed_from_int(8));
    layout_row_keys_append(&keys, layout_row_cell_string(self->name));
    layout_row_keys_append(&keys, layout_row_cell_fixed(p8_fixed_from_int(32)));
    layout_row_keys_append(&keys, layout_row_cell_tuple(icon));
    return keys;
}

static int available_projects_predicate(const research_project_t *b, const entity_t *planet_instance) {
    const char *name_str = NULL;
    int mg = 0;

    research_project_assert(b);

    if (!tech_gate_is_unlocked(b->required_tech)) {
        return 0;
    }

    if (planet_instance != NULL) {
        const planet_t *jo;

        assert(planet_is_family(planet_instance) && "available_projects_predicate: the planet display passes its own planet");
        jo = (const planet_t *)planet_instance;
        if (jo->planet_type != NULL) {
            name_str = jo->planet_type->name;
        }
    }

    if (name_str != NULL && name_str[0] != '\0' && name_str[1] == '\0' && b->planet_codes != NULL) {
        size_t n = strlen(b->planet_codes);
        size_t i;

        for (i = 0; i < n; i++) {
            if (b->planet_codes[i] == name_str[0]) {
                mg = 1;
                break;
            }
        }
    }
    return mg;
}

available_projects_kept_t research_display_available_projects(const entity_t *planet_instance) {
    available_projects_kept_t result;
    int k;

    assert(research_projects_count() <= PROJECT_RECORDS_COUNT && "research_display_available_projects: one project per record at most, so the record count bounds the result");
    result.count = 0;
    for (k = 1; k <= research_projects_count(); k++) {
        research_project_t *b = research_projects_at(k);

        if (available_projects_predicate(b, planet_instance)) {
            result.items[result.count++] = research_project_id_of(b);
        }
    }
    return result;
}
