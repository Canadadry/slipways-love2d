#ifndef P8_INPUT_H
#define P8_INPUT_H

typedef struct {
    int mouse_x;
    int mouse_y;
    int mouse_buttons;
    int pad_buttons;
} p8_input_frame_t;

#define P8_PAD_BUTTON_1 1
#define P8_PAD_BUTTON_2 2
#define P8_PAD_BUTTON_3 4
#define P8_PAD_BUTTON_4 8
#define P8_PAD_BUTTON_5 16
#define P8_PAD_BUTTON_6 32
#define P8_PAD_BUTTON_7 64
#define P8_PAD_BUTTON_8 128
#define P8_PAD_BUTTON_9 256
#define P8_PAD_BUTTON_10 512
#define P8_PAD_BUTTON_11 1024
#define P8_PAD_BUTTON_12 2048
#define P8_PAD_BUTTON_COUNT 12
typedef char p8_pad_button_count_ends_at_button_12[(1 << (P8_PAD_BUTTON_COUNT - 1)) == P8_PAD_BUTTON_12 ? 1 : -1];

#define P8_MOUSE_LEFT 1

typedef struct {
    int x;
    int y;
    int buttons;
} p8_mouse_t;

typedef struct {
    p8_input_frame_t *frames;
    int frame_count;
    int current_frame;
} p8_input_t;

void p8_input_init(p8_input_t *input);

int p8_input_load(p8_input_t *input, const char *path);

int p8_input_save(const p8_input_frame_t *frames, int frame_count, const char *path, const char *header_members);

int p8_input_append(const p8_input_frame_t *frame, const char *path, long *tail_offset, const char *header_member);

void p8_input_free(p8_input_t *input);

void p8_input_step(p8_input_t *input);

p8_mouse_t p8_input_read_mouse(const p8_input_t *input);

#endif
