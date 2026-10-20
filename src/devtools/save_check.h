#ifndef DEVTOOLS_SAVE_CHECK_H
#define DEVTOOLS_SAVE_CHECK_H

#include "game/save_state.h"
#include "p8/vram.h"

#include <stddef.h>
#include <stdint.h>

enum { SAVE_CHECK_DUMP_CAP = 65536 };

/* dump is the state dump without its zone and cap lines, NUL-terminated.
   Tens of KiB: keep it in static storage. visual_random_state is not compared. */
typedef struct {
    uint8_t bytes[SAVE_STATE_MAX_BYTES];
    int byte_count;
    uint32_t visual_random_state;
    char dump[SAVE_CHECK_DUMP_CAP];
    int has_screen;
    unsigned char screen[P8_SCREEN_HEIGHT][P8_SCREEN_WIDTH];
} save_check_snapshot_t;

/* Asserts no wormhole is live and nothing is being placed. Fills everything but the screen and
   clears has_screen. Reads the game, changes nothing. */
void save_check_capture(save_check_snapshot_t *out);

/* Sets every field the save never holds back to the value the loader gives
   it (bursts, toasts, particles, shakes, transit dots, slipway fades, entities
   being placed or dead, the pad crosshair, selection and tab, the camera's
   drift from its map position); changes nothing the save holds. Leaves tooltips and UI boxes alone. */
void save_check_drop_unsaved_state(void);

/* Turn it on before the compared frame's cart_update60 and cart_draw, and
   off after save_check_capture_screen. Pixels only: see particle_hide_pixels. */
void save_check_hide_particles(int hidden);

/* 1 when no message box, panel, menu or pause menu is open: the screen
   capture's box conditions, without asserting. */
int save_check_screen_comparable(void);

/* Asserts save_check_screen_comparable, that particles are hidden and that
   no burst is live. */
void save_check_capture_screen(save_check_snapshot_t *out);

/* Asserts that saved holds valid bytes, then resumes them with
   cart_resume_game and sets the visual stream to saved's, which the bytes
   never hold: a vanishing planet's flicker draws from it. */
void save_check_resume(const save_check_snapshot_t *saved);

/* Returns 0 when equal; otherwise writes into report the first difference
   of each differing part (bytes, dump, screen), joined by "; ", truncated
   to size. Screens are compared only when both were taken. */
int save_check_compare(const save_check_snapshot_t *a, const save_check_snapshot_t *b, char *report, size_t size);

#endif
