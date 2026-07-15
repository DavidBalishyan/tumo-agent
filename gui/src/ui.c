#define _POSIX_C_SOURCE 200809L
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>

#include "chat.h"
#include "paths.h"
#include "bridge.h"

// ---- Theme / palette ---------------------------------------------------

static Theme current_theme = THEME_DARK;

static Palette palettes[] = {
    // THEME_DARK
    { .bg = {24,24,27,255}, .input_bg = {38,38,44,255}, .input_text = {255,255,255,255},
      .cursor = {255,255,255,255}, .placeholder = {120,120,130,255},
      .scrollbar = {80,80,100,255},
      .tag_you = {120,170,255,255}, .tag_agent = {130,220,150,255},
      .tag_tool = {150,150,160,255}, .tag_thinking = {130,130,145,255},
      .body_user = {235,235,240,255}, .body_assistant = {224,224,230,255},
      .body_tool = {150,165,180,255}, .body_thinking = {130,130,145,255},
      .code_bg = {36,36,46,255}, .code_fg = {210,210,222,255},
      .link = {110,170,255,255}, .inline_code = {230,185,130,255},
      .heading_shade = {40,40,40,255}, .blockquote_bar = {110,110,130,255},
      .bullet = {224,224,230,255}, .hr = {80,80,92,255},
      .math_placeholder_bg = {60,60,80,255}, .math_placeholder_fg = {120,120,140,255},
    },
    // THEME_LIGHT
    { .bg = {242,242,247,255}, .input_bg = {229,229,234,255}, .input_text = {0,0,0,255},
      .cursor = {0,0,0,255}, .placeholder = {142,142,147,255},
      .scrollbar = {180,180,190,255},
      .tag_you = {0,105,225,255}, .tag_agent = {34,135,60,255},
      .tag_tool = {100,100,120,255}, .tag_thinking = {120,120,140,255},
      .body_user = {10,10,15,255}, .body_assistant = {20,20,30,255},
      .body_tool = {70,85,100,255}, .body_thinking = {100,100,120,255},
      .code_bg = {230,230,240,255}, .code_fg = {40,40,50,255},
      .link = {0,122,255,255}, .inline_code = {150,80,20,255},
      .heading_shade = {220,220,220,255}, .blockquote_bar = {190,190,210,255},
      .bullet = {20,20,30,255}, .hr = {190,190,200,255},
      .math_placeholder_bg = {200,200,220,255}, .math_placeholder_fg = {120,120,140,255},
    },
    // THEME_SEPIA
    { .bg = {251,242,229,255}, .input_bg = {240,229,212,255}, .input_text = {50,40,30,255},
      .cursor = {140,90,40,255}, .placeholder = {150,135,115,255},
      .scrollbar = {180,160,140,255},
      .tag_you = {80,120,200,255}, .tag_agent = {100,160,110,255},
      .tag_tool = {130,110,100,255}, .tag_thinking = {140,125,110,255},
      .body_user = {40,35,25,255}, .body_assistant = {60,50,40,255},
      .body_tool = {100,90,80,255}, .body_thinking = {130,120,110,255},
      .code_bg = {235,220,200,255}, .code_fg = {40,35,30,255},
      .link = {0,90,180,255}, .inline_code = {160,100,40,255},
      .heading_shade = {220,200,180,255}, .blockquote_bar = {180,160,140,255},
      .bullet = {60,50,40,255}, .hr = {200,190,170,255},
      .math_placeholder_bg = {210,190,170,255}, .math_placeholder_fg = {150,140,120,255},
    },
    // THEME_TOKYONIGHT — based on the tokyonight.nvim storm palette
    { .bg = {36,40,59,255}, .input_bg = {46,50,70,255}, .input_text = {192,202,245,255},
      .cursor = {192,202,245,255}, .placeholder = {100,110,140,255},
      .scrollbar = {75,85,110,255},
      .tag_you = {125,170,255,255}, .tag_agent = {158,206,106,255},
      .tag_tool = {140,150,170,255}, .tag_thinking = {120,130,155,255},
      .body_user = {192,202,245,255}, .body_assistant = {169,177,214,255},
      .body_tool = {140,150,180,255}, .body_thinking = {130,140,165,255},
      .code_bg = {46,50,70,255}, .code_fg = {192,202,245,255},
      .link = {130,170,255,255}, .inline_code = {247,180,100,255},
      .heading_shade = {60,66,90,255}, .blockquote_bar = {100,120,180,255},
      .bullet = {169,177,214,255}, .hr = {80,90,120,255},
      .math_placeholder_bg = {56,62,85,255}, .math_placeholder_fg = {140,150,180,255},
    },
    // THEME_GRUVBOX — dark variant (gruvbox material dark)
    { .bg = {40,40,40,255}, .input_bg = {50,50,50,255}, .input_text = {235,219,178,255},
      .cursor = {235,219,178,255}, .placeholder = {124,111,100,255},
      .scrollbar = {80,75,65,255},
      .tag_you = {131,165,152,255}, .tag_agent = {184,187,38,255},
      .tag_tool = {146,131,116,255}, .tag_thinking = {124,111,100,255},
      .body_user = {235,219,178,255}, .body_assistant = {213,196,161,255},
      .body_tool = {168,153,132,255}, .body_thinking = {146,131,116,255},
      .code_bg = {50,50,50,255}, .code_fg = {235,219,178,255},
      .link = {131,165,152,255}, .inline_code = {254,128,25,255},
      .heading_shade = {60,60,55,255}, .blockquote_bar = {146,131,116,255},
      .bullet = {213,196,161,255}, .hr = {80,75,65,255},
      .math_placeholder_bg = {60,58,55,255}, .math_placeholder_fg = {146,131,116,255},
    },
};

Palette ui_palette(void) { return palettes[current_theme]; }
Theme ui_current_theme(void) { return current_theme; }
void ui_set_theme(Theme t) { current_theme = t; }
void ui_toggle_theme(void) {
    current_theme = (current_theme + 1) % THEME_COUNT;
}

#define FONT_BASE 72 // rasterization size; we scale down when drawing
#define FONT_MIN 12
#define FONT_MAX 40

// ---- message selection ----
#define MAX_MSG_POS 1024
static float msg_y0[MAX_MSG_POS], msg_y1[MAX_MSG_POS];
static int msg_pos_count = 0;
int ui_selected_msg = -1;

int ui_msg_at_y(float screen_y) {
    for (int i = 0; i < msg_pos_count && i < chat_count(); i++) {
        if (screen_y >= msg_y0[i] && screen_y < msg_y1[i])
            return i;
    }
    return -1;
}
// ----

// ---- inline style bits ----
#define ST_BOLD 1
#define ST_ITALIC 2
#define ST_CODE 4
#define ST_LINK 8
#define ST_MATH 16

// Text weights (JetBrainsMono) plus a monochrome emoji fallback (Noto Emoji).
// Glyphs are loaded on demand: we scan the chat for the codepoints that
// actually appear and (re)build the atlases to include them. That way any
// character a font supports renders, without a giant up-front atlas.
static Font f_reg, f_bold, f_ital, f_bital, f_emoji;
static int text_ok = 0, emoji_ok = 0;
static int font_size = 20; // body size, changed by ui_zoom
static char font_dir[PATH_MAX];

// ---- Settings persistence (JSON) ----------------------------------------
// We use cJSON to read/write a small settings file at
// ~/.config/tumo-gui/settings.json so the user's theme and font size
// survive restarts.

#include "cJSON.h"

static const char *settings_path(void) {
    static char buf[PATH_MAX];
    if (!buf[0]) {
        const char *home = getenv("HOME");
        if (!home) home = ".";
        snprintf(buf, sizeof(buf), "%s/.config/tumo-gui/settings.json", home);
        char dir[PATH_MAX];
        snprintf(dir, sizeof(dir), "%s/.config/tumo-gui", home);
        mkdir(dir, 0755);
    }
    return buf;
}

void ui_save_settings(void) {
    cJSON *o = cJSON_CreateObject();
    cJSON_AddNumberToObject(o, "theme", (int)current_theme);
    cJSON_AddNumberToObject(o, "font_size", font_size);
    char *s = cJSON_Print(o);
    if (s) {
        FILE *f = fopen(settings_path(), "w");
        if (f) { fputs(s, f); fclose(f); }
        free(s);
    }
    cJSON_Delete(o);
}

void ui_load_settings(void) {
    const char *path = settings_path();
    FILE *f = fopen(path, "r");
    if (!f) return;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0 || sz > 65536) { fclose(f); return; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return; }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = 0;
    cJSON *o = cJSON_Parse(buf);
    free(buf);
    if (!o) return;
    cJSON *t = cJSON_GetObjectItem(o, "theme");
    if (cJSON_IsNumber(t) && t->valueint >= 0 && t->valueint < THEME_COUNT)
        current_theme = (Theme)t->valueint;
    cJSON *fs = cJSON_GetObjectItem(o, "font_size");
    if (cJSON_IsNumber(fs)) {
        int s = fs->valueint;
        if (s < FONT_MIN) s = FONT_MIN;
        if (s > FONT_MAX) s = FONT_MAX;
        font_size = s;
    }
    cJSON_Delete(o);
}
// -----------------------------------------------------------

// Text codepoints are loaded on demand: we scan the chat for the ones that
// appear and rebuild the text atlas to include them. Emoji are handled
// differently (see below): the emoji font is loaded once with a broad fixed
// set, because raylib's atlas generator crashes on a tiny (1-glyph) atlas, and
// incremental loads would keep hitting that.
static unsigned char cp_seen[0x110000 / 8]; // membership bitset (text only)
static int *cp_text = NULL, cp_text_n = 0, cp_text_cap = 0;
static int fonts_dirty = 1;
static int want_emoji = 0;  // an emoji codepoint has appeared
static int emoji_tried = 0; // we've attempted the one-time emoji font load

static int line_height(float size) { return (int)(size * 1.4f); }

// Codepoints we route to the emoji font rather than the text font.
static int is_emoji_cp(int cp) {
    return (cp >= 0x1F000 && cp <= 0x1FAFF) || // emoji, symbols & pictographs
           (cp >= 0x2600 && cp <= 0x27BF) ||   // misc symbols + dingbats
           (cp >= 0x2B00 && cp <= 0x2BFF);     // more symbols (stars, arrows)
}

// Cap the text atlas at a size the GPU can hold, well above a normal chat.
#define MAX_TEXT_CP 3000

static void cp_add(int cp) {
    if (cp < 0x20 || cp > 0x10FFFF) return;
    if (cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D) return; // VS / ZWJ: zero-width
    if (is_emoji_cp(cp)) { want_emoji = 1; return; } // emoji font covers these
    if (cp_seen[cp >> 3] & (1 << (cp & 7))) return;  // already handled
    cp_seen[cp >> 3] |= (1 << (cp & 7));
    if (cp_text_n >= MAX_TEXT_CP) return; // atlas full: leave as fallback box
    if (cp_text_n == cp_text_cap) {
        cp_text_cap = cp_text_cap ? cp_text_cap * 2 : 1024;
        cp_text = realloc(cp_text, cp_text_cap * sizeof(int));
    }
    cp_text[cp_text_n++] = cp;
    fonts_dirty = 1;
}

static void cp_scan(const char *s) {
    while (*s) {
        int b = 0;
        int cp = GetCodepointNext(s, &b);
        cp_add(cp);
        s += (b > 0 ? b : 1);
    }
}

// Ownership flags: 1 only when we hold a real LoadFontEx result. On failure
// LoadFontEx returns the shared default font, which we must NEVER unload (doing
// so frees raylib's internal font and corrupts the heap).
static int own_reg, own_bold, own_ital, own_bital, own_emoji;

static int is_default_font(Font f) {
    return f.texture.id == GetFontDefault().texture.id;
}

// Load a weight; returns 1 (and sets *owned) only if a real font came back.
static int load_one(Font *out, int *owned, const char *file, int *cps, int count) {
    char path[PATH_MAX * 2];
    snprintf(path, sizeof(path), "%s/%s", font_dir, file);
    Font f = LoadFontEx(path, FONT_BASE, cps, count);
    if (f.texture.id != 0 && !is_default_font(f)) {
        SetTextureFilter(f.texture, TEXTURE_FILTER_BILINEAR);
        *out = f;
        *owned = 1;
        return 1;
    }
    *owned = 0; // load failed (or gave us the default) -- don't take ownership
    return 0;
}

static void unload_owned(void) {
    if (own_reg) { UnloadFont(f_reg); own_reg = 0; }
    if (own_bold) { UnloadFont(f_bold); own_bold = 0; }
    if (own_ital) { UnloadFont(f_ital); own_ital = 0; }
    if (own_bital) { UnloadFont(f_bital); own_bital = 0; }
    if (own_emoji) { UnloadFont(f_emoji); own_emoji = 0; }
}

// Rebuild the four text weights for the current text codepoint set. The set
// always starts at 700+ (ASCII + Latin-1), so the atlas is never tiny.
static void rebuild_text_fonts(void) {
    if (own_reg) { UnloadFont(f_reg); own_reg = 0; }
    if (own_bold) { UnloadFont(f_bold); own_bold = 0; }
    if (own_ital) { UnloadFont(f_ital); own_ital = 0; }
    if (own_bital) { UnloadFont(f_bital); own_bital = 0; }
    text_ok = 0;

    text_ok = load_one(&f_reg, &own_reg, "JetBrainsMonoNerdFontMono-Regular.ttf", cp_text, cp_text_n);
    load_one(&f_bold, &own_bold, "JetBrainsMonoNerdFontMono-Bold.ttf", cp_text, cp_text_n);
    load_one(&f_ital, &own_ital, "JetBrainsMonoNerdFontMono-Italic.ttf", cp_text, cp_text_n);
    load_one(&f_bital, &own_bital, "JetBrainsMonoNerdFontMono-BoldItalic.ttf", cp_text, cp_text_n);
    if (!text_ok) f_reg = GetFontDefault();
    fonts_dirty = 0;
}

// Load the emoji font once, with a broad fixed codepoint set. Loading a large
// set (vs. one glyph at a time) is what keeps raylib's atlas generator happy.
static void load_emoji_font(void) {
    emoji_tried = 1;
    struct { int lo, hi; } ranges[] = {
        {0x2600, 0x27BF}, {0x2B00, 0x2BFF}, {0x1F300, 0x1FAFF},
    };
    int count = 0;
    for (int r = 0; r < (int)(sizeof(ranges) / sizeof(ranges[0])); r++)
        count += ranges[r].hi - ranges[r].lo + 1;
    int *cps = malloc(count * sizeof(int));
    int k = 0;
    for (int r = 0; r < (int)(sizeof(ranges) / sizeof(ranges[0])); r++)
        for (int c = ranges[r].lo; c <= ranges[r].hi; c++) cps[k++] = c;
    emoji_ok = load_one(&f_emoji, &own_emoji, "NotoEmoji-Regular.ttf", cps, count);
    free(cps);
}

// Pick the text weight for a style (code stays regular monospace).
static Font text_font(int style) {
    if (!text_ok) return GetFontDefault();
    if ((style & ST_BOLD) && (style & ST_ITALIC)) return own_bital ? f_bital : f_reg;
    if (style & ST_BOLD) return own_bold ? f_bold : f_reg;
    if (style & ST_ITALIC) return own_ital ? f_ital : f_reg;
    return f_reg;
}

// Draw or measure a UTF-8 string one codepoint at a time, picking the emoji
// font for emoji codepoints and the text font for everything else. Returns the
// advance width. This is DrawTextEx with per-glyph font fallback.
static float run_glyphs(const char *s, float x, float y, float size, int style,
                        Color color, int do_draw) {
    float start = x;
    while (*s) {
        int b = 0;
        int cp = GetCodepointNext(s, &b);
        s += (b > 0 ? b : 1);
        if (cp == 0xFE0F || cp == 0xFE0E || cp == 0x200D) continue; // zero-width
        Font f = (is_emoji_cp(cp) && emoji_ok) ? f_emoji : text_font(style);
        float scale = size / (float)f.baseSize;
        int gi = GetGlyphIndex(f, cp);
        float adv = (f.glyphs[gi].advanceX == 0) ? f.recs[gi].width * scale
                                                 : f.glyphs[gi].advanceX * scale;
        if (do_draw) DrawTextCodepoint(f, cp, (Vector2){x, y}, size, color);
        x += adv;
    }
    return x - start;
}

static void draw_glyphs(const char *s, float x, float y, float size, int style, Color c) {
    run_glyphs(s, x, y, size, style, c, 1);
}
static float measure_glyphs(const char *s, float size, int style) {
    return run_glyphs(s, 0, 0, size, style, WHITE, 0);
}

// ---- LaTeX → Unicode translation for simple math expressions ----
// This lets $E = mc^2$ render as Unicode without any bitmap rendering.
// JetBrainsMono Nerd Font has excellent coverage of math Unicode codepoints.

typedef struct { const char *cmd; const char *repl; int cp; } LatexMap;

#define L(cmd, cp) {cmd, NULL, cp}
#define S(cmd, str) {cmd, str, 0}

static const LatexMap latex_map[] = {
    // Greek lowercase
    S("\\alpha", "\xCE\xB1"), S("\\beta", "\xCE\xB2"), S("\\gamma", "\xCE\xB3"),
    S("\\delta", "\xCE\xB4"), S("\\epsilon", "\xCE\xB5"), S("\\zeta", "\xCE\xB6"),
    S("\\eta", "\xCE\xB7"), S("\\theta", "\xCE\xB8"), S("\\iota", "\xCE\xB9"),
    S("\\kappa", "\xCE\xBA"), S("\\lambda", "\xCE\xBB"), S("\\mu", "\xCE\xBC"),
    S("\\nu", "\xCE\xBD"), S("\\xi", "\xCE\xBE"), S("\\omicron", "\xCE\xBF"),
    S("\\pi", "\xCF\x80"), S("\\rho", "\xCF\x81"), S("\\sigma", "\xCF\x83"),
    S("\\tau", "\xCF\x84"), S("\\upsilon", "\xCF\x85"), S("\\phi", "\xCF\x86"),
    S("\\chi", "\xCF\x87"), S("\\psi", "\xCF\x88"), S("\\omega", "\xCF\x89"),
    // Greek uppercase
    S("\\Gamma", "\xCE\x93"), S("\\Delta", "\xCE\x94"), S("\\Theta", "\xCE\x98"),
    S("\\Lambda", "\xCE\x9B"), S("\\Xi", "\xCE\x9E"), S("\\Pi", "\xCE\xA0"),
    S("\\Sigma", "\xCE\xA3"), S("\\Phi", "\xCE\xA6"), S("\\Psi", "\xCE\xA8"),
    S("\\Omega", "\xCE\xA9"),
    // Operators and symbols
    S("\\sum", "\xE2\x88\x91"), S("\\int", "\xE2\x88\xAB"), S("\\prod", "\xE2\x88\x8F"),
    S("\\infty", "\xE2\x88\x9E"), S("\\partial", "\xE2\x88\x82"), S("\\nabla", "\xE2\x88\x87"),
    S("\\sqrt", "\xE2\x88\x9A"), S("\\forall", "\xE2\x88\x80"), S("\\exists", "\xE2\x88\x83"),
    S("\\nexists", "\xE2\x88\x84"), S("\\emptyset", "\xE2\x88\x85"), S("\\in", "\xE2\x88\x88"),
    S("\\notin", "\xE2\x88\x89"), S("\\ni", "\xE2\x88\x8B"), S("\\subset", "\xE2\x8A\x82"),
    S("\\supset", "\xE2\x8A\x83"), S("\\subseteq", "\xE2\x8A\x86"), S("\\supseteq", "\xE2\x8A\x87"),
    S("\\cup", "\xE2\x88\xAA"), S("\\cap", "\xE2\x88\xA9"), S("\\wedge", "\xE2\x88\xA7"),
    S("\\vee", "\xE2\x88\xA8"), S("\\oplus", "\xE2\x8A\x95"), S("\\otimes", "\xE2\x8A\x97"),
    S("\\pm", "\xC2\xB1"), S("\\mp", "\xE2\x88\x93"), S("\\times", "\xC3\x97"),
    S("\\div", "\xC3\xB7"), S("\\cdot", "\xC2\xB7"), S("\\circ", "\xE2\x88\x98"),
    S("\\bullet", "\xE2\x80\xA2"), S("\\neq", "\xE2\x89\xA0"), S("\\equiv", "\xE2\x89\xA1"),
    S("\\approx", "\xE2\x89\x88"), S("\\sim", "\xE2\x88\xBC"), S("\\propto", "\xE2\x88\x9D"),
    S("\\leq", "\xE2\x89\xA4"), S("\\geq", "\xE2\x89\xA5"), S("\\ll", "\xE2\x89\xAA"),
    S("\\gg", "\xE2\x89\xAB"), S("\\leftarrow", "\xE2\x86\x90"), S("\\rightarrow", "\xE2\x86\x92"),
    S("\\Leftarrow", "\xE2\x87\x90"), S("\\Rightarrow", "\xE2\x87\x92"), S("\\leftrightarrow", "\xE2\x86\x94"),
    S("\\mapsto", "\xE2\x86\xA6"), S("\\to", "\xE2\x86\x92"), S("\\gets", "\xE2\x86\x90"),
    S("\\angle", "\xE2\x88\xA0"), S("\\perp", "\xE2\x8A\xA5"), S("\\triangle", "\xE2\x96\xB3"),
    S("\\therefore", "\xE2\x88\xB4"), S("\\because", "\xE2\x88\xB5"),
    S("\\aleph", "\xE2\x84\xB5"), S("\\hbar", "\xE2\x84\x8F"), S("\\ell", "\xE2\x84\x93"),
    S("\\Re", "\xE2\x84\x9C"), S("\\Im", "\xE2\x84\x91"),
    S("\\nabla", "\xE2\x88\x87"), S("\\Box", "\xE2\x96\xA1"),
    // Spacing
    S("\\;", " "), S("\\:", " "), S("\\,", " "), S("\\!", ""),
    {NULL, NULL, 0}
};

#undef L
#undef S

static const char *translate_latex_cmd(const char **pp) {
    const char *p = *pp;
    if (*p != '\\') return NULL;
    for (int i = 0; latex_map[i].cmd; i++) {
        size_t len = strlen(latex_map[i].cmd);
        if (strncmp(p, latex_map[i].cmd, len) == 0 &&
            !((unsigned char)p[len] >= 'a' && (unsigned char)p[len] <= 'z') &&
            !((unsigned char)p[len] >= 'A' && (unsigned char)p[len] <= 'Z')) {
            *pp = p + len;
            return latex_map[i].repl;
        }
    }
    return NULL;
}

// Translate a simple LaTeX math expression to Unicode. Output is written to
// 'out' (max out_size bytes). Returns the number of bytes written (excluding
// null), safe for truncation. Handles: superscript ^{...}, subscript _{...},
// \cmd commands, plain text.
static int latex_to_unicode(const char *src, char *out, int out_size) {
    if (out_size <= 0) return 0;
    char *op = out;
    char *end = out + out_size - 1; // reserve space for null
    const char *p = src;

    // Superscript/subscript Unicode codepoint tables
    // Maps digits and some letters to their superscript/subscript versions
    static const char *sup_chars[256] = {0};
    static const char *sub_chars[256] = {0};
    static int tables_init = 0;
    if (!tables_init) {
        // superscripts: ᵃᵇᶜᵈᵉᶠᵍʰⁱʲᵏˡᵐⁿᵒᵖʳˢᵗᵘᵛʷˣʸᶻ
        const char *sup_letters =
            "\xE1\xB5\x83\xE1\xB5\x87\xE1\xB6\x9C\xE1\xB5\x88\xE1\xB5\x89"
            "\xE1\xB6\xA0\xE1\xB5\x8D\xE1\xB5\x8A\xE1\xB5\x89\xCA\xBF"
            "\xE1\xB5\x8F\xCB\xA1\xE1\xB5\x90\xE1\xB5\x91\xE1\xB5\x92"
            "\xCA\x80\xE1\xB5\x93\xE1\xB5\xA1\xE1\xB5\x94\xE1\xB5\x95"
            "\xE1\xB5\x96\xE1\xB5\x97\xCA\xA2\xE1\xB5\x98\xCA\xB3\xE1\xB5\x9A";
        for (int c = 'a'; c <= 'z'; c++) {
            sup_chars[c] = sup_letters + (c - 'a') * 3;
        }
        sup_chars['0'] = "\xE2\x81\xB0";
        sup_chars['1'] = "\xC2\xB9";
        sup_chars['2'] = "\xC2\xB2";
        sup_chars['3'] = "\xC2\xB3";
        sup_chars['4'] = "\xE2\x81\xB4";
        sup_chars['5'] = "\xE2\x81\xB5";
        sup_chars['6'] = "\xE2\x81\xB6";
        sup_chars['7'] = "\xE2\x81\xB7";
        sup_chars['8'] = "\xE2\x81\xB8";
        sup_chars['9'] = "\xE2\x81\xB9";
        sup_chars['+'] = "\xE2\x81\xBA";
        sup_chars['-'] = "\xE2\x81\xBB";
        sup_chars['='] = "\xE2\x81\xBC";
        sup_chars['('] = "\xE2\x81\xBD";
        sup_chars[')'] = "\xE2\x81\xBE";
        // subscripts: ₐₑₕᵢⱼₖₗₘₙₒₚᵣₛₜᵤᵥₓ
        sub_chars['a'] = "\xE2\x82\x90";
        sub_chars['e'] = "\xE2\x82\x91";
        sub_chars['h'] = "\xE2\x82\x95";
        sub_chars['i'] = "\xE2\x82\xA2";
        sub_chars['j'] = "\xE2\x82\xA3";
        sub_chars['k'] = "\xE2\x82\xA4";
        sub_chars['l'] = "\xE2\x82\xA5";
        sub_chars['m'] = "\xE2\x82\xA6";
        sub_chars['n'] = "\xE2\x82\xA7";
        sub_chars['o'] = "\xE2\x82\xA8";
        sub_chars['p'] = "\xE2\x82\xA9";
        sub_chars['r'] = "\xE2\x82\xAB";
        sub_chars['s'] = "\xE2\x82\xAC";
        sub_chars['t'] = "\xE2\x82\xAD";
        sub_chars['u'] = "\xE2\x82\xAE";
        sub_chars['v'] = "\xE2\x82\xAF";
        sub_chars['x'] = "\xE2\x82\x93";
        sub_chars['0'] = "\xE2\x82\x80";
        sub_chars['1'] = "\xE2\x82\x81";
        sub_chars['2'] = "\xE2\x82\x82";
        sub_chars['3'] = "\xE2\x82\x83";
        sub_chars['4'] = "\xE2\x82\x84";
        sub_chars['5'] = "\xE2\x82\x85";
        sub_chars['6'] = "\xE2\x82\x86";
        sub_chars['7'] = "\xE2\x82\x87";
        sub_chars['8'] = "\xE2\x82\x88";
        sub_chars['9'] = "\xE2\x82\x89";
        tables_init = 1;
    }

    while (*p && op < end) {
        if (*p == '\\') {
            const char *repl = translate_latex_cmd(&p);
            if (repl) {
                size_t rlen = strlen(repl);
                size_t room = (size_t)(end - op);
                if (rlen > room) rlen = room;
                memcpy(op, repl, rlen);
                op += rlen;
                continue;
            }
            // Unknown command -- skip the backslash, emit the next char
            p++;
            if (*p && op < end) *op++ = *p++;
            continue;
        }
        if (*p == '^' && p[1] == '{') {
            // ^{...} superscript group
            p += 2; // skip ^{
            while (*p && *p != '}' && op < end) {
                unsigned char c = (unsigned char)*p;
                if (sup_chars[c]) {
                    size_t rlen = strlen(sup_chars[c]);
                    size_t room = (size_t)(end - op);
                    if (rlen > room) rlen = room;
                    memcpy(op, sup_chars[c], rlen);
                    op += rlen;
                } else if (op < end) {
                    *op++ = *p;
                }
                p++;
            }
            if (*p == '}') p++;
            continue;
        }
        if (*p == '^') {
            // single-char superscript
            p++;
            if (*p) {
                unsigned char c = (unsigned char)*p;
                if (sup_chars[c]) {
                    size_t rlen = strlen(sup_chars[c]);
                    size_t room = (size_t)(end - op);
                    if (rlen > room) rlen = room;
                    memcpy(op, sup_chars[c], rlen);
                    op += rlen;
                } else if (op < end) {
                    *op++ = c;
                }
                p++;
            }
            continue;
        }
        if (*p == '_' && p[1] == '{') {
            // _{...} subscript group
            p += 2;
            while (*p && *p != '}' && op < end) {
                unsigned char c = (unsigned char)*p;
                if (sub_chars[c]) {
                    size_t rlen = strlen(sub_chars[c]);
                    size_t room = (size_t)(end - op);
                    if (rlen > room) rlen = room;
                    memcpy(op, sub_chars[c], rlen);
                    op += rlen;
                } else if (op < end) {
                    *op++ = *p;
                }
                p++;
            }
            if (*p == '}') p++;
            continue;
        }
        if (*p == '_') {
            p++;
            if (*p) {
                unsigned char c = (unsigned char)*p;
                if (sub_chars[c]) {
                    size_t rlen = strlen(sub_chars[c]);
                    size_t room = (size_t)(end - op);
                    if (rlen > room) rlen = room;
                    memcpy(op, sub_chars[c], rlen);
                    op += rlen;
                } else if (op < end) {
                    *op++ = c;
                }
                p++;
            }
            continue;
        }
        // Plain character -- skip braces that are part of LaTeX grouping
        if (*p != '{' && *p != '}') {
            *op++ = *p;
        }
        p++;
    }
    *op = 0;
    return (int)(op - out);
}

// ---- Math texture cache (for rendered LaTeX bitmaps from the Node process) ----

// Public helpers (used by the input bar in main.c): regular weight, body size.
void ui_draw_text(const char *t, float x, float y, Color c) {
    draw_glyphs(t, x, y, (float)font_size, 0, c);
}
float ui_measure_text(const char *t) {
    return measure_glyphs(t, (float)font_size, 0);
}
int ui_font_size(void) { return font_size; }

// Make sure every codepoint currently in the chat has a glyph loaded.
static void ensure_glyphs(void) {
    for (int i = 0; i < chat_count(); i++) cp_scan(chat_get(i)->text);
    if (fonts_dirty) rebuild_text_fonts();
    if (want_emoji && !emoji_tried) load_emoji_font();
}

void ui_load_font(void) {
    const char *dir = gui_dir()[0] ? gui_dir() : "gui";
    snprintf(font_dir, sizeof(font_dir), "%s/vendor/fonts", dir);

    memset(cp_seen, 0, sizeof(cp_seen));
    cp_text_n = 0;
    // Seed with ASCII + Latin-1 + a little punctuation so basic text always works.
    for (int c = 0x20; c <= 0x2FF; c++) cp_add(c);
    int extra[] = {0x2018, 0x2019, 0x201C, 0x201D, 0x2013, 0x2014, 0x2026};
    for (int i = 0; i < (int)(sizeof(extra) / sizeof(extra[0])); i++) cp_add(extra[i]);
    // Seed math Unicode ranges (Greek, operators, arrows) for the Unicode math fallback
    for (int c = 0x393; c <= 0x3C9; c++) cp_add(c); // Greek
    for (int c = 0x2020; c <= 0x2064; c++) cp_add(c); // general punctuation (superscript area)
    for (int c = 0x2070; c <= 0x209F; c++) cp_add(c); // superscripts & subscripts
    for (int c = 0x20AC; c <= 0x20BF; c++) cp_add(c); // currency (includes some)
    for (int c = 0x2100; c <= 0x214F; c++) cp_add(c); // letterlike symbols
    for (int c = 0x2190; c <= 0x21FF; c++) cp_add(c); // arrows
    for (int c = 0x2200; c <= 0x22FF; c++) cp_add(c); // math operators
    for (int c = 0x25A0; c <= 0x25FF; c++) cp_add(c); // geometric shapes
    for (int c = 0x27C0; c <= 0x27EF; c++) cp_add(c); // misc math symbols A
    for (int c = 0x2980; c <= 0x29FF; c++) cp_add(c); // misc math symbols B
    for (int c = 0x2A00; c <= 0x2AFF; c++) cp_add(c); // supplemental math operators
    for (int c = 0x2080; c <= 0x208E; c++) cp_add(c); // subscript again (failsafe)
    rebuild_text_fonts();
}

static void unload_math_textures(void); // defined after the math cache section

void ui_unload_font(void) {
    unload_owned();
    free(cp_text);
    unload_math_textures();
}

void ui_zoom(int steps) {
    int s = font_size + steps;
    if (s < FONT_MIN) s = FONT_MIN;
    if (s > FONT_MAX) s = FONT_MAX;
    font_size = s; // scaling is per-draw, so no reload needed
}

// ---- Markdown rendering + word/character wrapping --------------

static int clamp8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return v;
}

static Color shade(Color c, int d) {
    return (Color){(unsigned char)clamp8(c.r + d), (unsigned char)clamp8(c.g + d),
                   (unsigned char)clamp8(c.b + d), c.a};
}

Color ui_shade(Color c, int d) { return shade(c, d); }

static Color style_color(Color base, int style) {
    Palette p = ui_palette();
    if (style & ST_LINK) return p.link;
    if (style & ST_CODE) return p.inline_code;
    return base;
}

// A wrapping cursor.
typedef struct {
    float x0, maxw, cx, y, size;
    int lh;
    float space_w;
    Color base;
    int draw;
} Wrap;

// ---- Math texture cache (for rendered LaTeX bitmaps from the Node process) ----

#define MATH_TEX_CACHE_SIZE 256

typedef struct {
    char *latex_key;
    int display;
    int req_id;
    int loaded;
    Texture2D texture;
    int w, h;
} MathTexEntry;

static MathTexEntry math_tex_cache[MATH_TEX_CACHE_SIZE];
static int math_tex_cache_count = 0;

static MathTexEntry *math_cache_find(const char *latex, int display) {
    for (int i = 0; i < math_tex_cache_count; i++) {
        if (math_tex_cache[i].latex_key &&
            math_tex_cache[i].display == display &&
            strcmp(math_tex_cache[i].latex_key, latex) == 0) {
            return &math_tex_cache[i];
        }
    }
    return NULL;
}

static MathTexEntry *math_cache_add(const char *latex, int display) {
    if (math_tex_cache_count >= MATH_TEX_CACHE_SIZE) return NULL;
    MathTexEntry *e = &math_tex_cache[math_tex_cache_count++];
    e->latex_key = strdup(latex);
    e->display = display;
    e->req_id = 0;
    e->loaded = 0;
    e->texture.id = 0;
    e->w = e->h = 0;
    return e;
}

static void math_poll_renders(void) {
    for (int i = 0; i < math_tex_cache_count; i++) {
        MathTexEntry *e = &math_tex_cache[i];
        if (e->loaded || e->req_id == 0) continue;
        const MathRenderResult *rr = bridge_math_result(e->req_id);
        if (rr && rr->loaded) {
            // rr->pixels holds raw PNG file data.
            // raylib can decode it from memory using LoadImageFromMemory.
            Image img = LoadImageFromMemory(".png", rr->pixels, rr->data_size);
            if (img.data) {
                e->texture = LoadTextureFromImage(img);
                e->w = img.width;
                e->h = img.height;
                e->loaded = 1;
                UnloadImage(img);
            }
            e->w = rr->w;
            e->h = rr->h;
            e->loaded = 1;
        }
    }
}

static void math_request_render(MathTexEntry *e) {
    if (e->req_id != 0) return;
    Palette p = ui_palette();
    // Use assistant body color for math text so it adapts to the theme.
    e->req_id = bridge_render_math(e->latex_key, e->display, p.body_assistant);
}

static float draw_math_block(const char *latex, int display, Wrap *w) {
    float scale = w->size / (float)FONT_BASE;
    MathTexEntry *e = math_cache_find(latex, display);
    if (!e) {
        e = math_cache_add(latex, display);
        if (!e) return 0;
    }
    if (!e->loaded && e->req_id == 0) {
        math_request_render(e);
    }
    if (e->loaded && e->texture.id != 0) {
        float tw = e->w * scale;
        float th = e->h * scale;
        if (display && w->cx > w->x0) {
            w->y += w->lh;
            w->cx = w->x0;
            if (tw < w->maxw) w->cx = w->x0 + (w->maxw - tw) * 0.5f;
        }
        float ty = w->y;
        if (!display) {
            float line_mid = w->y + w->size * 0.5f;
            ty = line_mid - th * 0.5f;
        }
        if (w->draw) {
            DrawTextureEx(e->texture, (Vector2){w->cx, ty}, 0, scale, WHITE);
        }
        w->cx += tw;
        if (display) {
            w->y = ty + th + w->lh / 2;
            w->cx = w->x0;
        }
        return tw;
    } else {
        float pw = w->size * (display ? 8 : 3);
        float ph = w->size * (display ? 2 : 1);
        if (display && w->cx > w->x0) {
            w->y += w->lh;
            w->cx = w->x0;
        }
        if (w->draw) {
            Palette p = ui_palette();
            DrawRectangle((int)w->cx, (int)(w->y + 2), (int)pw, (int)ph,
                          p.math_placeholder_bg);
            draw_glyphs("∑", w->cx + 2, w->y + 3, w->size * 0.6f, 0, p.math_placeholder_fg);
        }
        w->cx += pw;
        if (display) {
            w->y += ph + w->lh / 2;
            w->cx = w->x0;
        }
        return pw;
    }
}

static void unload_math_textures(void) {
    for (int i = 0; i < math_tex_cache_count; i++) {
        free(math_tex_cache[i].latex_key);
        if (math_tex_cache[i].texture.id != 0)
            UnloadTexture(math_tex_cache[i].texture);
    }
    math_tex_cache_count = 0;
}

// Draw one run at the cursor, with a code background / link underline.
static void paint(Wrap *w, const char *s, int style) {
    float tw = measure_glyphs(s, w->size, style);
    if (w->draw) {
        if (style & ST_CODE) {
            Palette pal = ui_palette();
            DrawRectangle((int)(w->cx - 2), (int)(w->y - 1), (int)(tw + 4),
                          (int)(w->size + 3), pal.code_bg);
        }
        draw_glyphs(s, w->cx, w->y, w->size, style, style_color(w->base, style));
        if (style & ST_LINK) {
            int uy = (int)(w->y + w->size);
            DrawLine((int)w->cx, uy, (int)(w->cx + tw), uy, style_color(w->base, style));
        }
    }
    w->cx += tw;
}

// A token wider than the line: break it character by character (on codepoint
// boundaries, so multibyte characters stay intact).
static void place_long(Wrap *w, const char *s, int style) {
    int i = 0;
    while (s[i]) {
        char seg[1024];
        int sl = 0;
        while (s[i]) {
            int b = 0;
            GetCodepointNext(&s[i], &b);
            if (b <= 0) b = 1;
            if (sl + b >= (int)sizeof(seg) - 1) break;
            char cand[1024];
            memcpy(cand, seg, sl);
            memcpy(cand + sl, &s[i], b);
            cand[sl + b] = 0;
            if (w->cx + measure_glyphs(cand, w->size, style) > w->x0 + w->maxw && sl > 0) break;
            memcpy(seg + sl, &s[i], b);
            sl += b;
            seg[sl] = 0;
            i += b;
        }
        paint(w, seg, style);
        if (s[i]) { w->y += w->lh; w->cx = w->x0; }
    }
}

// Place one word, wrapping at the space before it when needed.
static void place_word(Wrap *w, const char *s, int style, int space_before) {
    if (!*s) return;
    float ww = measure_glyphs(s, w->size, style);
    if (ww > w->maxw) {
        if (space_before && w->cx > w->x0) { w->y += w->lh; w->cx = w->x0; }
        place_long(w, s, style);
        return;
    }
    float lead = (space_before && w->cx > w->x0) ? w->space_w : 0;
    if (w->cx > w->x0 && w->cx + lead + ww > w->x0 + w->maxw) { w->y += w->lh; w->cx = w->x0; }
    else w->cx += lead;
    paint(w, s, style);
}

// Render one line (no '\n') with inline markdown, wrapping. base_style seeds the
// style (e.g. bold for headings); parse_md off treats markers as literal text.
static float draw_inline(const char *text, float x0, float y, float maxw,
                         Color base, float size, int base_style, int parse_md, int draw) {
    Wrap w = {x0, maxw, x0, y, size, line_height(size), measure_glyphs(" ", size, 0), base, draw};
    int bold = (base_style & ST_BOLD) ? 1 : 0;
    int italic = (base_style & ST_ITALIC) ? 1 : 0;
    int code = 0, link = 0, math = 0, math_display = 0;
    char word[1024];
    int wl = 0, pending = 0;

#define STYLE() ((bold ? ST_BOLD : 0) | (italic ? ST_ITALIC : 0) | (code ? ST_CODE : 0) | (link ? ST_LINK : 0) | (math ? ST_MATH : 0))
#define FLUSH() do { if (wl) { word[wl] = 0; place_word(&w, word, STYLE(), pending); wl = 0; pending = 0; } } while (0)

    const char *p = text;
    while (*p) {
        char ch = *p;
        if (parse_md && !code && !math && ch == '[') {
            // link: [label](url) -> draw the label, drop the url
            const char *close = strchr(p, ']');
            if (close && close[1] == '(') {
                const char *ue = strchr(close + 2, ')');
                if (ue) {
                    FLUSH();
                    link = 1;
                    int lpend = pending;
                    pending = 0;
                    char wbuf[1024];
                    int wb = 0;
                    for (const char *q = p + 1; q < close; q++) {
                        if (*q == ' ') { if (wb) { wbuf[wb] = 0; place_word(&w, wbuf, STYLE(), lpend); wb = 0; lpend = 1; } }
                        else if (wb < 1023) wbuf[wb++] = *q;
                    }
                    if (wb) { wbuf[wb] = 0; place_word(&w, wbuf, STYLE(), lpend); }
                    link = 0;
                    p = ue + 1;
                    pending = 0;
                    continue;
                }
            }
            if (wl < 1023) word[wl++] = '[';
            p++;
            continue;
        }
        if (parse_md && !code && !math && ch == '*' && p[1] == '*') {
            FLUSH();
            if (!(base_style & ST_BOLD)) bold = !bold;
            p += 2;
            continue;
        }
        if (parse_md && !code && !math && ch == '*') {
            // italic marker. Opening must hug the following text (so "2 * 3"
            // and stray stars stay literal); a close is always allowed.
            int opening = !italic;
            if (opening ? (p[1] && p[1] != ' ') : 1) {
                FLUSH();
                italic = !italic;
                p++;
                continue;
            }
            if (wl < 1023) word[wl++] = '*';
            p++;
            continue;
        }
        if (parse_md && !code && !math && ch == '`') { FLUSH(); code = !code; p++; continue; }
        // Math mode: $...$ (inline) and $$...$$ (display)
        if (parse_md && !code && ch == '$') {
            if (!math) {
                // Opening math delimiter. Check for $$ first.
                math_display = (p[1] == '$');
                FLUSH();
                math = 1;
                p += math_display ? 2 : 1;
                // Reset word buffer to collect the LaTeX source
                wl = 0;
                continue;
            } else {
                // Closing math delimiter. Check for $$ first.
                int is_dd = (p[1] == '$');
                if (is_dd == math_display) {
                    word[wl] = 0;
                    wl = 0;
                    if (math_display) {
                        draw_math_block(word, math_display, &w);
                        pending = 0;
                    } else {
                        char math_buf[2048];
                        latex_to_unicode(word, math_buf, sizeof(math_buf));
                        if (*math_buf) {
                            int mp = pending;
                            pending = 0;
                            place_word(&w, math_buf, 0, mp);
                        }
                    }
                    math = 0;
                    p += is_dd ? 2 : 1;
                    continue;
                }
            }
        }
        if (!math && (ch == ' ' || ch == '\t')) { FLUSH(); pending = 1; p++; continue; }
        if (wl < 1023) word[wl++] = ch;
        p++;
    }
    FLUSH();
#undef STYLE
#undef FLUSH
    return w.y + w.lh;
}

// A fenced-code line: full-width background band, monospace, character-wrapped.
static void draw_code_line(Wrap *w, const char *s) {
    Palette p = ui_palette();
    Color band = p.code_bg;
    Color fg = p.code_fg;
    int i = 0;
    do {
        if (w->draw)
            DrawRectangle((int)(w->x0 - 6), (int)(w->y - 2), (int)(w->maxw + 12), w->lh, band);
        char seg[1024];
        int sl = 0;
        while (s[i] && sl < (int)sizeof(seg) - 1) {
            seg[sl] = s[i];
            seg[sl + 1] = 0;
            if (measure_glyphs(seg, w->size, 0) > w->maxw && sl > 0) { seg[sl] = 0; break; }
            sl++;
            i++;
        }
        if (w->draw) draw_glyphs(seg, w->x0, w->y, w->size, 0, fg);
        if (s[i]) w->y += w->lh;
    } while (s[i]);
}

static int is_hrule(const char *t) {
    char c = t[0];
    if (c != '-' && c != '*' && c != '_') return 0;
    int n = 0;
    for (const char *q = t; *q; q++) {
        if (*q == c) n++;
        else if (*q != ' ') return 0;
    }
    return n >= 3;
}

static float heading_size(int lvl) {
    float s = (float)font_size;
    if (lvl <= 1) return s * 1.7f;
    if (lvl == 2) return s * 1.4f;
    if (lvl == 3) return s * 1.2f;
    return s * 1.1f;
}

// Markdown block renderer (assistant messages).
static float draw_markdown(const char *text, float x, float y, float maxw, Color base, int draw) {
    float fs = (float)font_size;
    int in_code = 0, in_math = 0;
    char math_src[4096];
    int math_src_len = 0;
    char line[4096];
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = 0;

        const char *t = line;
        while (*t == ' ') t++;

        // Display math fence: a line that is purely $$ with optional whitespace.
        // Content between fences is collected and rendered as a single block.
        if (!in_code && t[0] == '$' && t[1] == '$') {
            const char *rest = t + 2;
            while (*rest == ' ') rest++;
            int is_fence = (*rest == 0 || *rest == '\r');
            if (is_fence) {
                if (!in_math) {
                    in_math = 1;
                    math_src_len = 0;
                } else {
                    if (math_src_len > 0) {
                        Wrap w = {x, maxw, x, y, fs, line_height(fs), measure_glyphs(" ", fs, 0), base, draw};
                        draw_math_block(math_src, 1, &w);
                        y = w.y;
                    }
                    in_math = 0;
                }
            } else {
                // `$$...$$` on a single line — let draw_inline handle it
                if (in_math) {
                    // Collect this line as LaTeX content (inside a multi-line block)
                    if (math_src_len > 0 && math_src_len < (int)sizeof(math_src) - 1) {
                        math_src[math_src_len++] = ' ';
                    }
                    for (const char *src = t; *src && math_src_len < (int)sizeof(math_src) - 1; src++)
                        math_src[math_src_len++] = *src;
                    math_src[math_src_len] = 0;
                } else {
                    y = draw_inline(line, x, y, maxw, base, fs, 0, 1, draw);
                }
            }
        } else if (in_math) {
            // Collect a line of LaTeX source inside a multi-line display math block
            if (math_src_len > 0 && math_src_len < (int)sizeof(math_src) - 1) {
                math_src[math_src_len++] = ' ';
            }
            const char *src = t;
            while (*src && math_src_len < (int)sizeof(math_src) - 1) {
                math_src[math_src_len++] = *src++;
            }
            math_src[math_src_len] = 0;
        } else if (t[0] == '`' && t[1] == '`' && t[2] == '`') {
            in_code = !in_code; // fence line: not drawn
        } else if (in_code) {
            Wrap w = {x, maxw, x, y, fs, line_height(fs), measure_glyphs(" ", fs, 0), base, draw};
            draw_code_line(&w, line);
            y = w.y + w.lh;
        } else if (t[0] == '#') {
            int lvl = 0;
            const char *h = t;
            while (*h == '#') { lvl++; h++; }
            if (*h == ' ') {
                float hs = heading_size(lvl);
                y += (int)(hs * 0.25f); // breathing room above headings
                y = draw_inline(h + 1, x, y, maxw, shade(base, 40), hs, ST_BOLD, 1, draw);
            } else {
                y = draw_inline(line, x, y, maxw, base, fs, 0, 1, draw);
            }
        } else if (is_hrule(t)) {
            if (draw) {
                Palette pal = ui_palette();
                DrawRectangle((int)x, (int)(y + fs * 0.5f), (int)maxw, 2, pal.hr);
            }
            y += line_height(fs);
        } else if ((t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ') {
            float ind = fs * 1.2f;
                if (draw) {
                    Palette pal = ui_palette();
                    DrawCircle((int)(x + fs * 0.4f), (int)(y + fs * 0.55f), fs * 0.14f, pal.bullet);
                }
            y = draw_inline(t + 2, x + ind, y, maxw - ind, base, fs, 0, 1, draw);
        } else if (t[0] >= '0' && t[0] <= '9') {
            const char *d = t;
            while (*d >= '0' && *d <= '9') d++;
            if ((*d == '.' || *d == ')') && d[1] == ' ') {
                float ind = fs * 1.8f;
                char num[16];
                int k = (int)(d - t);
                if (k > 13) k = 13;
                memcpy(num, t, k);
                num[k] = *d;
                num[k + 1] = 0;
                if (draw) {
                    Palette pal = ui_palette();
                    draw_glyphs(num, x, y, fs, 0, shade(pal.bullet, 0));
                }
                y = draw_inline(d + 2, x + ind, y, maxw - ind, base, fs, 0, 1, draw);
            } else {
                y = draw_inline(line, x, y, maxw, base, fs, 0, 1, draw);
            }
        } else if (t[0] == '>') {
            const char *q = t + 1;
            if (*q == ' ') q++;
            float ind = fs * 0.8f;
            Color qc = shade(base, -30);
            float end = draw_inline(q, x + ind, y, maxw - ind, qc, fs, 0, 1, 0); // measure height
            if (draw) {
                Palette pal = ui_palette();
                DrawRectangle((int)x, (int)y, 3, (int)(end - y), pal.blockquote_bar);
            }
            y = draw_inline(q, x + ind, y, maxw - ind, qc, fs, 0, 1, draw);
        } else {
            y = draw_inline(line, x, y, maxw, base, fs, 0, 1, draw);
        }

        if (!nl) break;
        p = nl + 1;
    }
    return y;
}

// Plain block (user / tool / system): wraps, no markdown styling.
static float draw_plain(const char *text, float x, float y, float maxw, Color col, int draw) {
    char line[4096];
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = 0;
        y = draw_inline(line, x, y, maxw, col, (float)font_size, 0, 0, draw);
        if (!nl) break;
        p = nl + 1;
    }
    return y;
}

// Claude's reasoning, shown the same way Claude Code shows it: italic, no
// markdown styling (thinking text isn't meant to be read as a formatted reply).
static float draw_thinking(const char *text, float x, float y, float maxw, Color col, int draw) {
    char line[4096];
    const char *p = text;
    for (;;) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        if (len > sizeof(line) - 1) len = sizeof(line) - 1;
        memcpy(line, p, len);
        line[len] = 0;
        y = draw_inline(line, x, y, maxw, col, (float)font_size, ST_ITALIC, 0, draw);
        if (!nl) break;
        p = nl + 1;
    }
    return y;
}

float ui_layout(float origin_y, int screen_w, int draw) {
    math_poll_renders(); // check for completed math render results
    ensure_glyphs(); // load glyphs for any new characters before drawing
    int lh = line_height((float)font_size);
    float tag_w = ui_measure_text("agent") + 12; // label column, scales with font
    float y = origin_y;
    float x = PAD;
    float maxw = screen_w - 2 * PAD - tag_w;
    Palette p = ui_palette();
    if (draw) msg_pos_count = 0;
    for (int i = 0; i < chat_count(); i++) {
        const Msg *m = chat_get(i);
        const char *tag;
        Color tagcol, bodycol;
        switch (m->role) {
            case ROLE_USER:
                tag = "you"; tagcol = p.tag_you;
                bodycol = p.body_user; break;
            case ROLE_ASSISTANT:
                tag = "agent"; tagcol = p.tag_agent;
                bodycol = p.body_assistant; break;
            case ROLE_TOOL:
                tag = "tool"; tagcol = p.tag_tool;
                bodycol = p.body_tool; break;
            case ROLE_THINKING:
                tag = "thinking"; tagcol = p.tag_thinking;
                bodycol = p.body_thinking; break;
            default:
                tag = ""; tagcol = (Color){0, 0, 0, 0};
                bodycol = (Color){210, 150, 150, 255}; break;
        }
        if (draw) {
            if (msg_pos_count < MAX_MSG_POS) msg_y0[msg_pos_count] = y;
            if (i == ui_selected_msg && ui_selected_msg >= 0) {
                DrawRectangle((int)x - 4, (int)y - 2, (int)(tag_w + maxw + 8), (int)(lh + 14),
                              (Color){255, 255, 255, 20});
            }
        }
        if (draw && tag[0]) ui_draw_text(tag, x, y, tagcol);
        // The agent's replies are Markdown; thinking is italic plain text;
        // everything else is plain text.
        float ty = (m->role == ROLE_ASSISTANT)
                       ? draw_markdown(m->text, x + tag_w, y, maxw, bodycol, draw)
                   : (m->role == ROLE_THINKING)
                       ? draw_thinking(m->text, x + tag_w, y, maxw, bodycol, draw)
                       : draw_plain(m->text, x + tag_w, y, maxw, bodycol, draw);
        float used = ty - y;
        if (used < lh) used = lh;
        y += used + 10;
        if (draw && msg_pos_count < MAX_MSG_POS) {
            msg_y1[msg_pos_count++] = y;
        }
    }
    return y - origin_y;
}

// ---- Settings overlay ---------------------------------------------------

int ui_settings_active = 0;

int ui_draw_settings(int sw, int sh) {
    Palette p = ui_palette();
    Vector2 mp = GetMousePosition();

    // Dim background
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 120});

    // Panel rectangle
    int pw = 340, ph = 280;
    int px = (sw - pw) / 2, py = (sh - ph) / 2;
    Rectangle panel = {(float)px, (float)py, (float)pw, (float)ph};
    DrawRectangleRounded(panel, 0.1f, 8, p.input_bg);
    DrawRectangleRoundedLines(panel, 0.1f, 8, ui_shade(p.input_bg, 20));

    int cy = py + 20;
    int lh = 30;

    // Title
    ui_draw_text("Settings", (float)px + 16, (float)cy, p.input_text);
    cy += lh + 10;

    // ---- Theme row ----
    const char *theme_names[] = {"Dark", "Light", "Sepia", "Tokyo", "Gruv"};
    int sel = (int)ui_current_theme();
    int n_themes = sizeof(theme_names) / sizeof(theme_names[0]);

    int label_x = px + 16;
    ui_draw_text("Theme:", (float)label_x, (float)cy, p.placeholder);
    int btn_x = px + 100;
    for (int t = 0; t < n_themes; t++) {
        int bw = 54, bh = 28;
        Rectangle br = {(float)(btn_x + t * (bw + 4)), (float)cy, (float)bw, (float)bh};
        int hover = CheckCollisionPointRec(mp, br);
        Color bc = (t == sel) ? p.tag_agent : ui_shade(p.input_bg, 15);
        if (hover) bc = ui_shade(bc, 20);
        DrawRectangleRounded(br, 0.2f, 6, bc);
        float tw = ui_measure_text(theme_names[t]);
        float tx = br.x + (bw - tw) / 2;
        float ty2 = br.y + (bh - ui_font_size()) / 2;
        ui_draw_text(theme_names[t], tx, ty2, (t == sel) ? p.bg : p.input_text);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ui_set_theme((Theme)t);
        }
    }
    cy += lh + 6;

    // ---- Font size row ----
    ui_draw_text("Font size:", (float)label_x, (float)cy, p.placeholder);
    char size_str[16];
    snprintf(size_str, sizeof(size_str), "%d px", font_size);
    ui_draw_text(size_str, (float)px + 240, (float)cy, p.input_text);
    // minus button
    {
        int bw = 28;
        Rectangle br = {(float)(px + 120), (float)cy, (float)bw, (float)bw};
        int hover = CheckCollisionPointRec(mp, br);
        Color bc = hover ? ui_shade(p.input_bg, 20) : ui_shade(p.input_bg, 15);
        DrawRectangleRounded(br, 0.2f, 6, bc);
        ui_draw_text("-", br.x + 8, br.y + 4, p.input_text);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui_zoom(-1);
    }
    // plus button
    {
        int bw = 28;
        Rectangle br = {(float)(px + 152), (float)cy, (float)bw, (float)bw};
        int hover = CheckCollisionPointRec(mp, br);
        Color bc = hover ? ui_shade(p.input_bg, 20) : ui_shade(p.input_bg, 15);
        DrawRectangleRounded(br, 0.2f, 6, bc);
        ui_draw_text("+", br.x + 8, br.y + 4, p.input_text);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) ui_zoom(+1);
    }
    cy += lh + 6;

    // ---- Clear chat button ----
    {
        int bw = 140, bh = 32;
        Rectangle br = {(float)(px + 16), (float)cy, (float)bw, (float)bh};
        int hover = CheckCollisionPointRec(mp, br);
        Color bc = hover ? (Color){200, 60, 60, 255} : (Color){180, 50, 50, 255};
        DrawRectangleRounded(br, 0.2f, 6, bc);
        ui_draw_text("Clear chat", br.x + 12, br.y + 6, WHITE);
        if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            chat_clear();
            bridge_clear_math();
        }
    }

    // Close on ESC. Click-outside uses "just released" so the same press that
    // opened the panel doesn't immediately close it.
    if (IsKeyPressed(KEY_ESCAPE)) { ui_save_settings(); return 0; }
    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT) && !CheckCollisionPointRec(mp, panel))
        { ui_save_settings(); return 0; }

    return 1;
}
