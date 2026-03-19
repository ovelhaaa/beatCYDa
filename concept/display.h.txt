#pragma once
/*  display.h — C-compatible interface to the LovyanGFX display layer.
    Include this from .c files; implemented in display.cpp (C++).
*/
#ifdef __cplusplus
extern "C" {
#endif

#include "sequencer.h"

/* Call once from setup() */
void display_init(void);

/* Full screen redraw (slow, use at boot or after mode change) */
void display_draw_all(const SeqState *s, UiMode mode);

/* Partial redraws — call only the zone that changed */
void display_draw_header  (const SeqState *s);
void display_draw_tracks  (const SeqState *s);
void display_draw_ring    (const SeqState *s);          /* uses sprite, no flicker */
void display_draw_params  (const SeqState *s, UiMode mode);
void display_draw_footer  (const SeqState *s, UiMode mode);
void display_draw_status  (const char *msg, bool highlight);

#ifdef __cplusplus
}
#endif
