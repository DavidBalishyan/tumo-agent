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

// Message hit-testing: after ui_layout(..., 1) is called, get the index of the
// message at screen Y coordinate y (relative to the window). Returns -1 if none.
int ui_msg_at_y(float screen_y);

// The current selection index (-1 = none). Set this to select a message.
extern int ui_selected_msg;

// ---- Theme / palette ----

typedef enum { THEME_DARK = 0, THEME_LIGHT, THEME_SEPIA, THEME_TOKYONIGHT, THEME_GRUVBOX, THEME_COUNT } Theme;

typedef struct {
    Color bg;         // window background
    Color input_bg;   // input bar background
    Color input_text; // typed text in the input bar
    Color cursor;     // input cursor
    Color placeholder;// input placeholder / hint text
    Color scrollbar;  // scrollbar color
    Color tag_you, tag_agent, tag_tool, tag_thinking;
    Color body_user, body_assistant, body_tool, body_thinking;
    Color code_bg;    // fenced-code background band
    Color code_fg;    // fenced-code text
    Color link;       // link underline / text
    Color inline_code;// inline `code` text color
    Color heading_shade; // heading accent
    Color blockquote_bar; // blockquote vertical bar
    Color bullet;       // list bullet
    Color hr;           // horizontal rule
    Color math_placeholder_bg; // placeholder rect for unloaded math
    Color math_placeholder_fg; // placeholder "∑" text
} Palette;

extern Palette ui_palette(void);
Theme ui_current_theme(void);
void ui_toggle_theme(void);
Color ui_shade(Color c, int d); // lighten/darken a color by d
void ui_set_theme(Theme t);     // set theme directly

// Persist settings (theme + font size) to ~/.config/tumo-gui/settings.json.
void ui_load_settings(void);
void ui_save_settings(void);

// ---- Settings overlay ----
//
// ui_settings_active indicates whether the settings panel is open. The caller
// (main.c) should set it to 0 when the user closes the panel (ESC or click out).
extern int ui_settings_active;

// Draw the settings overlay over the full window. Returns 1 while the overlay
// should remain open (0 to close it). Handles its own mouse / keyboard input.
// 'sw' and 'sh' are the screen dimensions.
int ui_draw_settings(int sw, int sh);

#endif
