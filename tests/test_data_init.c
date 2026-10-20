#include "game/data_init.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "p8/sprite_sheet.h"
#include "game/map_tables.h"
#include "p8/vram.h"
#include "game/research.h"

static void test_production_tables_project_records_icon_tooltips_technologies_decode_real_game_data(void) {
    p8_vram_t mem;

    p8_vram_init(&mem);

    data_init();

    assert(PRODUCTION_TABLES_COUNT == 6);
    assert(production_tables_lookup("f-") != NULL);
    assert(production_tables_lookup("structure+") != NULL);
    assert(production_tables_lookup("does-not-exist") == NULL);

    assert(PROJECT_RECORDS_COUNT == 42);
    assert(research_projects_count() == 34 && research_projects_at(34) != NULL);
    assert(research_projects_at(35) == NULL);

    assert(ICON_TOOLTIPS_COUNT == 7);
    assert(icon_tooltips_table[0].count > 0 && icon_tooltips_table[0].items[0].row != NULL);
    {
        const data_row_t *title = icon_tooltips_table[0].items[0].row;
        assert(title->count > 0 && title->items[0].text != NULL);
        assert(strcmp(title->items[0].text, "-lab-") == 0);
    }

    assert(TECHNOLOGIES_COUNT == 19);
    {
        const technologies_entry_t *tech = &technologies_table[0];

        assert(strcmp(tech->name, "space folding") == 0);
        assert(tech->era == 1);
        assert(tech->icon == 22);
        assert(tech->cost == 4);
        assert(tech->tooltip_rows != NULL);
    }

    data_init_shutdown();
}

int main(void) {
    test_production_tables_project_records_icon_tooltips_technologies_decode_real_game_data();

    printf("test_data_init: all tests passed\n");
    return 0;
}
