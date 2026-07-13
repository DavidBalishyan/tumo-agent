#define _POSIX_C_SOURCE 200809L
#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "chat.h"
#include "paths.h"

#define FONT_BASE 72 // rasterization size; we scale down when drawing
#define FONT_MIN 12
#define FONT_MAX 40

// ---- inline style bits ----
#define ST_BOLD 1
#define ST_ITALIC 2
#define ST_CODE 4
#define ST_LINK 8

// Text weights (JetBrainsMono) plus a monochrome emoji fallback (Noto Emoji).
// Glyphs are loaded on demand: we scan the chat for the codepoints that
// actually appear and (re)build the atlases to include them. That way any
// character a font supports renders, without a giant up-front atlas.
static Font f_reg, f_bold, f_ital, f_bital, f_emoji;
static int text_ok = 0, emoji_ok = 0;
static int font_size = 20; // body size, changed by ui_zoom
static char font_dir[PATH_MAX];

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
    rebuild_text_fonts();
}

void ui_unload_font(void) {
    unload_owned();
    free(cp_text);
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

static Color style_color(Color base, int style) {
    if (style & ST_LINK) return (Color){110, 170, 255, 255};
    if (style & ST_CODE) return (Color){230, 185, 130, 255};
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

// Draw one run at the cursor, with a code background / link underline.
static void paint(Wrap *w, const char *s, int style) {
    float tw = measure_glyphs(s, w->size, style);
    if (w->draw) {
        if (style & ST_CODE)
            DrawRectangle((int)(w->cx - 2), (int)(w->y - 1), (int)(tw + 4),
                          (int)(w->size + 3), (Color){44, 44, 54, 255});
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
    int code = 0, link = 0;
    char word[1024];
    int wl = 0, pending = 0;

#define STYLE() ((bold ? ST_BOLD : 0) | (italic ? ST_ITALIC : 0) | (code ? ST_CODE : 0) | (link ? ST_LINK : 0))
#define FLUSH() do { if (wl) { word[wl] = 0; place_word(&w, word, STYLE(), pending); wl = 0; pending = 0; } } while (0)

    const char *p = text;
    while (*p) {
        char ch = *p;
        if (parse_md && !code && ch == '[') {
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
        if (parse_md && !code && ch == '*' && p[1] == '*') {
            FLUSH();
            if (!(base_style & ST_BOLD)) bold = !bold;
            p += 2;
            continue;
        }
        if (parse_md && !code && ch == '*') {
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
        if (parse_md && ch == '`') { FLUSH(); code = !code; p++; continue; }
        if (ch == ' ' || ch == '\t') { FLUSH(); pending = 1; p++; continue; }
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
    Color band = {36, 36, 46, 255};
    Color fg = {210, 210, 222, 255};
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
    int in_code = 0;
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

        if (t[0] == '`' && t[1] == '`' && t[2] == '`') {
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
            if (draw) DrawRectangle((int)x, (int)(y + fs * 0.5f), (int)maxw, 2, (Color){80, 80, 92, 255});
            y += line_height(fs);
        } else if ((t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ') {
            float ind = fs * 1.2f;
            if (draw) DrawCircle((int)(x + fs * 0.4f), (int)(y + fs * 0.55f), fs * 0.14f, base);
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
                if (draw) draw_glyphs(num, x, y, fs, 0, shade(base, 20));
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
            if (draw) DrawRectangle((int)x, (int)y, 3, (int)(end - y), (Color){110, 110, 130, 255});
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
    ensure_glyphs(); // load glyphs for any new characters before drawing
    int lh = line_height((float)font_size);
    float tag_w = ui_measure_text("agent") + 12; // label column, scales with font
    float y = origin_y;
    float x = PAD;
    float maxw = screen_w - 2 * PAD - tag_w;
    for (int i = 0; i < chat_count(); i++) {
        const Msg *m = chat_get(i);
        const char *tag;
        Color tagcol, bodycol;
        switch (m->role) {
            case ROLE_USER:
                tag = "you"; tagcol = (Color){120, 170, 255, 255};
                bodycol = (Color){235, 235, 240, 255}; break;
            case ROLE_ASSISTANT:
                tag = "agent"; tagcol = (Color){130, 220, 150, 255};
                bodycol = (Color){224, 224, 230, 255}; break;
            case ROLE_TOOL:
                tag = "tool"; tagcol = (Color){150, 150, 160, 255};
                bodycol = (Color){150, 165, 180, 255}; break;
            case ROLE_THINKING:
                tag = "thinking"; tagcol = (Color){130, 130, 145, 255};
                bodycol = (Color){130, 130, 145, 255}; break;
            default:
                tag = ""; tagcol = (Color){0, 0, 0, 0};
                bodycol = (Color){210, 150, 150, 255}; break;
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
    }
    return y - origin_y;
}
