#ifndef GAME_DEMO_DATA_H
#define GAME_DEMO_DATA_H

#include <stdint.h>

#define DEMO_DATA_FRAME_LIMIT 3600
#define DEMO_DATA_CAPTION_TEXT_SIZE 66

typedef struct {
    int from;
    int to;
    char text[DEMO_DATA_CAPTION_TEXT_SIZE];
} demo_data_caption_t;

extern const uint8_t demo_data_record[];
extern const int demo_data_record_count;
extern const uint16_t demo_data_pad_buttons[];
extern const int demo_data_frame_count;
extern const demo_data_caption_t demo_data_captions[];
extern const int demo_data_caption_count;

#endif
