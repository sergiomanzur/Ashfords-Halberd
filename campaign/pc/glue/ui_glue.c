/* Shared single-player menu drawing and mouse input helpers. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/adv.h>
#include <ps2/veronica/prog/message.h>
#include <ps2/veronica/prog/ps2_NaDraw.h>
#include "../src/host/menu_mouse.h"
#include <string.h>
void bhDispFontEx(NJS_POINT2* pos, int code, unsigned int argb, float pri);
extern float FontScaleY;
#define PRI_TEXT -1.0f
/* 2D depth (njDrawPolygon / njDrawQuadTexture z: the GS depth is -1/z, as a 3D point at view
   distance 1/z): the title picture is at 0.01 (100 units), the models stand 34 units away */
#define Z_DIM 0.012f                    /* behind the models */
#define Z_FRONT 0.3f                    /* in front of them */

static int code_of(char c) { return c > 32 && c < 127 ? c - 32 : 0; }
static float adv(int c) { return c ? FontSz[c] : 11.0f; }   /* the font's space is a full cell */

#define TRACK 0.88f                     /* a little tighter than the game's messages */

static float text_w(const char* s, float sc)
{
    float w = 0;
    for (; *s; s++) w += adv(code_of(*s)) * sc * TRACK;
    return w;
}

/* the game's font, left-aligned at x (bhDispMessage's advance), with a drop shadow */
static int font_ok;                     /* the game's font is loaded (font_ready) */

static void text(float x, float y, const char* s, float sc, unsigned int argb)
{
    float kx = FontScaleX, ky = FontScaleY, kc = FontScaleCR, x0 = x;
    const char* p;
    int pass;
    if (!font_ok) return;
    bhFontScaleSet(sc, sc, sc);
    for (pass = 0; pass < 2; pass++)
        for (p = s, x = x0; *p; p++) {
            int c = code_of(*p);
            NJS_POINT2 pos = { x + (pass ? 0 : 2), y + (pass ? 0 : 2) };
            if (c) bhDispFontEx(&pos, c, pass ? argb : (argb & 0xFF000000) >> 1 & 0xFF000000, PRI_TEXT);
            x += adv(c) * sc * TRACK;
        }
    bhFontScaleSet(kx, ky, kc);
}

static void text_c(float cx, float y, const char* s, float sc, unsigned int argb) { text(cx - text_w(s, sc) / 2, y, s, sc, argb); }

/* at most `w` wide: the words that fit, then the next line (returns the lines drawn) */
static int text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    char line[160];
    int n = 0;
    while (*s && n < max_lines) {
        int len = 0, fit = 0;
        while (s[len] && len < (int)sizeof(line) - 1) {
            memcpy(line, s, len + 1); line[len + 1] = 0;
            if (text_w(line, sc) > w) break;
            len++;
            if (!s[len] || s[len] == ' ') fit = len;
        }
        if (!s[len]) fit = len;
        if (!fit) fit = len ? len : 1;
        memcpy(line, s, fit); line[fit] = 0;
        text(x, y + n * 28 * sc * 1.05f, line, sc, argb);
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
float pc_ui_text_w(const char* s, float sc) { return text_w(s, sc); }
int pc_ui_text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines)
{
    font_ok = (sys->ss_flg & 0x40) != 0;
    return text_wrap(x, y, w, s, sc, argb, max_lines);
}
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z) { rect(x0, y0, x1, y1, argb, z); }
void pc_ui_arrow(float x, float y, int dir, unsigned int argb) { arrow(x, y, dir, argb); }
