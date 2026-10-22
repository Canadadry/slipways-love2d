#include "game/demo_data.h"
#include "table_reference.h"

void table_reference_game_demo_data(void) {
    const char *module = "game.demo_data";
    table_reference_int(module, DEMO_DATA_FRAME_LIMIT, "DEMO_DATA_FRAME_LIMIT");
    table_reference_int(module, DEMO_DATA_CAPTION_TEXT_SIZE, "DEMO_DATA_CAPTION_TEXT_SIZE");
    table_reference_int(module, demo_data_record_count, "demo_data_record_count");
    table_reference_int(module, demo_data_frame_count, "demo_data_frame_count");
    table_reference_int(module, demo_data_caption_count, "demo_data_caption_count");
    for (int i = 0; i < demo_data_record_count; i++) {
        table_reference_int(module, demo_data_record[i], "demo_data_record[%d]", i);
    }
    for (int i = 0; i < demo_data_frame_count; i++) {
        table_reference_int(module, demo_data_pad_buttons[i], "demo_data_pad_buttons[%d]", i);
    }
    for (int i = 0; i < demo_data_caption_count; i++) {
        table_reference_int(module, demo_data_captions[i].from, "demo_data_captions[%d].from", i);
        table_reference_int(module, demo_data_captions[i].to, "demo_data_captions[%d].to", i);
        table_reference_string(module, demo_data_captions[i].text, "demo_data_captions[%d].text", i);
    }
}
