/* Shared single-player menu drawing and mouse input helpers. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/adv.h>
#include <ps2/veronica/prog/message.h>
#include <ps2/veronica/prog/ps2_NaDraw.h>
#include "../src/host/lang.h"
#include "../src/host/menu_mouse.h"
#include <string.h>
void bhDispFontEx(NJS_POINT2* pos, int code, unsigned int argb, float pri);
extern float FontScaleY;
#define PRI_TEXT -1.0f
/* 2D depth (njDrawPolygon / njDrawQuadTexture z: the GS depth is -1/z, as a 3D point at view
   distance 1/z): the title picture is at 0.01 (100 units), the models stand 34 units away */
#define Z_DIM 0.012f                    /* behind the models */
#define Z_FRONT 0.3f                    /* in front of them */

/* UTF-8, the letters of the loaded font (lang.h pc_glyph_next: accents with a language's font) */
static float adv(int c) { return c ? pc_glyph_w(c) : 11.0f; }   /* the font's space is a full cell */

#define TRACK (lang_font() == LANG_RU || lang_font() == LANG_AR ? 1.0f : 0.88f)   /* a little tighter than the game's messages (the Russian widths are tight; Arabic letters join) */

static float text_w(const char* s, float sc)
{
    float w = 0;
    s = tr(s);   /* (our texts in the language: drawn and measured the same) */
    for (int c; (c = pc_glyph_next(&s)) >= 0;) w += adv(c) * sc * TRACK;
    return w;
}

/* the game's font, left-aligned at x (bhDispMessage's advance), with a drop shadow */
static int font_ok;                     /* the game's font is loaded (font_ready) */
static int text_flat;                   /* no drop shadow (pc_ui_text_flat) */

static void text(float x, float y, const char* s, float sc, unsigned int argb)
{
    float kx = FontScaleX, ky = FontScaleY, kc = FontScaleCR, x0 = x;
    const char* p;
    int pass;
    if (!font_ok) return;
    s = tr(s);
    bhFontScaleSet(sc, sc, sc);
    for (pass = text_flat; pass < 2; pass++)
        for (p = s, x = x0; *p;) {
            int c = pc_glyph_next(&p);
            NJS_POINT2 pos = { x + (pass ? 0 : 2), y + (pass ? 0 : 2) };
            if (c) bhDispFontEx(&pos, c, pass ? argb : (argb & 0xFF000000) >> 1 & 0xFF000000, PRI_TEXT);
            x += adv(c) * sc * TRACK;
        }
    bhFontScaleSet(kx, ky, kc);
}

static float rtl_w(const char* s, int n, void* sc)
{
    char line[400];
    if (n >= (int)sizeof(line)) return 1e9f;
    memcpy(line, s, (size_t)n);
    line[n] = 0;
    return text_w(line, *(float*)sc);
}

/* at most `w` wide: the words that fit, then the next line (returns the lines drawn). Arabic (in display order,
   lang.h lang_shape): its first line is the right end of the text, so lines are taken from there (lang_rtl_cut). */
static int wrap_right;
static int text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    char line[160], rl[400];
    int n = 0;
    s = tr(s);
    if (lang_rtl(s)) {
        for (int len = (int)strlen(s); len > 0 && n < max_lines;) {
            int cut = lang_rtl_cut(s, len, w, rtl_w, &sc);
            if (len - cut < (int)sizeof(rl)) {
                memcpy(rl, s + cut, (size_t)(len - cut));
                rl[len - cut] = 0;
                text(wrap_right ? x + w - text_w(rl, sc) : x, y + n * 28 * sc * 1.05f, rl, sc, argb);
            }
            n++;
            for (len = cut; len > 0 && s[len - 1] == ' '; len--) {}
        }
        return n;
    }
    while (*s && n < max_lines) {
        int len = 0, fit = 0;
        while (s[len] && len < (int)sizeof(line) - 4) {   /* (a whole UTF-8 letter at a time; a line may end after a kana or kanji) */
            unsigned char c = (unsigned char)s[len];
            int k = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;
            memcpy(line, s, len + k); line[len + k] = 0;
            if (text_w(line, sc) > w) break;
            len += k;
            if (!s[len] || s[len] == ' ' || lang_cjk_break(c, s + len)) fit = len;
        }
        if (!s[len]) fit = len;
        if (!fit) fit = len ? len : 1;
        memcpy(line, s, fit); line[fit] = 0;
        text(wrap_right ? x + w - text_w(line, sc) : x, y + n * 28 * sc * 1.05f, line, sc, argb);
        n++;
        s += fit;
        while (*s == ' ') s++;
    }
    return n;
}

static void rect(float x0, float y0, float x1, float y1, unsigned int argb, float z)
{
    NJS_POLYGON_VTX v[4] = { { x0, y0, z, argb }, { x0, y1, z, argb }, { x1, y0, z, argb }, { x1, y1, z, argb } };
    njDrawPolygon(v, 4, 1);
}

/* a small arrow head pointing left (dir -1) or right (1), its tip at x */
static void arrow(float x, float y, int dir, unsigned int argb)
{
    NJS_POLYGON_VTX v[3] = { { x, y, 0.3f, argb }, { x - dir * 7.0f, y - 6.0f, 0.3f, argb }, { x - dir * 7.0f, y + 6.0f, 0.3f, argb } };
    njDrawPolygon(v, 3, 1);
}

void menu_mouse_take(MouseTake* t)
{
    int x = g_menu_mouse.x, y = g_menu_mouse.y;
    t->left = __atomic_exchange_n(&g_menu_mouse.left, 0, __ATOMIC_RELAXED);
    t->right = __atomic_exchange_n(&g_menu_mouse.right, 0, __ATOMIC_RELAXED);
    t->wheel = __atomic_exchange_n(&g_menu_mouse.wheel, 0, __ATOMIC_RELAXED);
    __atomic_exchange_n(&g_menu_mouse.left_up, 0, __ATOMIC_RELAXED);
    t->moved = (x != t->px || y != t->py) && t->seen;
    t->px = x; t->py = y; t->seen = 1;
    t->x = x / 16.0f; t->y = y / 16.0f;
    if (!menu_mouse_on()) t->left = t->right = t->wheel = t->moved = 0;
}

void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb) { font_ok = (sys->ss_flg & 0x40) != 0; text(x, y, s, sc, argb); }
void pc_ui_text_flat(float x, float y, const char* s, float sc, unsigned int argb)
{
    font_ok = (sys->ss_flg & 0x40) != 0;
    text_flat = 1;
    text(x, y, s, sc, argb);
    text_flat = 0;
}
float pc_ui_text_w(const char* s, float sc) { return text_w(s, sc); }
int pc_ui_text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    font_ok = (sys->ss_flg & 0x40) != 0;
    return text_wrap(x, y, w, s, sc, argb, max_lines);
}
int pc_ui_text_wrap_r(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    int n;
    font_ok = (sys->ss_flg & 0x40) != 0;
    wrap_right = 1;
    n = text_wrap(x, y, w, s, sc, argb, max_lines);
    wrap_right = 0;
    return n;
}
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z) { rect(x0, y0, x1, y1, argb, z); }
void pc_ui_arrow(float x, float y, int dir, unsigned int argb) { arrow(x, y, dir, argb); }
