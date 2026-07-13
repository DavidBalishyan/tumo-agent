#ifndef UI_H
#define UI_H

#include "raylib.h"

// Layout constants. The font size is dynamic (see ui_font_size / ui_zoom).
#define PAD 16
#define INPUT_H 52

// Load/unload the UI font. Call ui_load_font after InitWindow.
void ui_load_font(void);
void ui_unload_font(void);

// Font zoom.
int ui_font_size(void);  // current size in pixels
void ui_zoom(int steps); // grow (+) or shrink (-) the font, clamped

// Draw / measure a line of text in the UI font.
void ui_draw_text(const char *t, float x, float y, Color c);
float ui_measure_text(const char *t);

// Lay out every chat message starting at origin_y. When draw is 0 it only
// measures (used to size the scroll region). Returns the total content height.
float ui_layout(float origin_y, int screen_w, int draw);

#endif
