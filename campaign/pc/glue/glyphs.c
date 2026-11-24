/* Procedural vector controller & keyboard button glyphs for native in-game menus
 * (Ashford Pause, Enhanced Options, Visual Give Item picker). Drawn with njDrawPolygon
 * and the game's font at the display's resolution — no external icon assets needed. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/adv.h>
#include <ps2/veronica/prog/ps2_NaDraw.h>
#include "../src/host/bindings.h"
#include "glyphs.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb);
void pc_ui_text_flat(float x, float y, const char* s, float sc, unsigned int argb);
float pc_ui_text_w(const char* s, float sc);
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z);

#define GL_PAD   '\x01'
#define GL_MOUSE '\x02'

enum {
    P_L1 = 0x4, P_R1 = 0x8, P_TRI = 0x10, P_CIRCLE = 0x20, P_CROSS = 0x40, P_SQUARE = 0x80, P_START = 0x800
};
enum {
    GP_SOUTH = 1, GP_EAST, GP_WEST, GP_NORTH, GP_BACK, GP_GUIDE, GP_START, GP_LSTICK, GP_RSTICK, GP_LB, GP_RB,
    GP_UP, GP_DOWN, GP_LEFT, GP_RIGHT, GP_AXIS = BIND_AXIS, GP_LT = BIND_AXIS + 8, GP_RT = BIND_AXIS + 10, GP_DPAD_LR = 0x70
};

#define C_EDGE 0xFF9C907Au
#define C_BODY 0xFF222024u
#define C_MARK 0xFFF2ECE0u
#define C_GOLD 0xFFD6B668u
#define C_HINT 0xFFC8C2B4u
#define Z_IC   0.45f
#define Z_ICD  0.002f

static int using_pad;

static void poly(const float* p, int n, unsigned int col, float z)
{
    NJS_POLYGON_VTX v[20];
    if (n > 20) {
        float b[42];
        int h = n / 2;
        poly(p, h + 1, col, z);
        memcpy(b, p + 2 * h, sizeof(float) * 2 * (n - h));
        b[2 * (n - h)] = p[0];
        b[2 * (n - h) + 1] = p[1];
        poly(b, n - h + 1, col, z);
        return;
    }
    for (int k = 0, lo = 1, hi = n - 1; k < n; k++) {
        int i = !k ? 0 : (k & 1) ? lo++ : hi--;
        v[k] = (NJS_POLYGON_VTX){ p[2 * i], p[2 * i + 1], z, col };
    }
    njDrawPolygon(v, n, 1);
}

static void disc(float cx, float cy, float r, unsigned int col, float z)
{
    float p[56];
    for (int i = 0; i < 28; i++) {
        p[2 * i]     = cx + r * cosf(i * 6.2831853f / 28);
        p[2 * i + 1] = cy + r * sinf(i * 6.2831853f / 28);
    }
    poly(p, 28, col, z);
}

static void rrect(float x0, float y0, float x1, float y1, float rt, float rb, unsigned int col, float z)
{
    static const float c[5] = { 0, 0.38268343f, 0.70710678f, 0.92387953f, 1 };
    const float cx[4] = { x1 - rt, x1 - rb, x0 + rb, x0 + rt };
    const float cy[4] = { y0 + rt, y1 - rb, y1 - rb, y0 + rt };
    const float r[4]  = { rt, rb, rb, rt };
    float p[40];
    for (int q = 0, n = 0; q < 4; q++) {
        for (int i = 0; i < 5; i++, n++) {
            float s = c[i], k = c[4 - i];
            float dx[4] = { s, k, -s, -k }, dy[4] = { -k, s, k, -s };
            p[2 * n]     = cx[q] + r[q] * dx[q];
            p[2 * n + 1] = cy[q] + r[q] * dy[q];
        }
    }
    poly(p, 20, col, z);
}

static void bar(float x0, float y0, float x1, float y1, float w, unsigned int col, float z)
{
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * w / 2, ny = dx / l * w / 2;
    float p[8] = { x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny };
    poly(p, 4, col, z);
}

static void tri(float cx, float cy, float r, int dir, unsigned int col, float z)
{
    static const float u[4][6] = {
        { 0, -1,  0.9f,  0.6f, -0.9f,  0.6f },
        { 1,  0, -0.6f,  0.9f, -0.6f, -0.9f },
        { 0,  1, -0.9f, -0.6f,  0.9f, -0.6f },
        { -1, 0,  0.6f, -0.9f,  0.6f,  0.9f }
    };
    float p[6];
    for (int i = 0; i < 6; i++) p[i] = (i & 1 ? cy : cx) + r * u[dir & 3][i];
    poly(p, 3, col, z);
}

static void ic_text(float cx, float y, const char* s, float sc, unsigned int col)
{
    pc_ui_text_flat(cx - pc_ui_text_w(s, sc) / 2, y, s, sc, col);
}

static void ring(float cx, float cy, float r, unsigned int body)
{
    disc(cx, cy, r, C_EDGE, Z_IC);
    disc(cx, cy, r - 0.9f, body, Z_IC + Z_ICD);
}

static float ic_face(int code, float x, float y, int draw)
{
    static const char xb[] = "ABXY", sw[] = "BAYX";
    static const unsigned int xc[] = { 0xFF5BAE3Cu, 0xFFD5443Au, 0xFF3478CFu, 0xFFE4B024u };
    int pos = code - GP_SOUTH, style = bind_pad_style();
    float cx = x + 7.5f, cy = y + 7.5f, z = Z_IC + 2 * Z_ICD;
    char l[2] = { 0, 0 };
    if (!draw) return 15;
    if (style == BPS_PS) {
        ring(cx, cy, 7.5f, 0xFF222222u);
        switch (pos) {
        case 0:
            bar(cx - 3.3f, cy - 3.3f, cx + 3.3f, cy + 3.3f, 1.5f, 0xFF8DB2F0u, z);
            bar(cx - 3.3f, cy + 3.3f, cx + 3.3f, cy - 3.3f, 1.5f, 0xFF8DB2F0u, z);
            break;
        case 1:
            disc(cx, cy, 4.0f, 0xFFF07A78u, z);
            disc(cx, cy, 2.6f, 0xFF222222u, z + Z_ICD);
            break;
        case 2:
            rrect(cx - 3.4f, cy - 3.4f, cx + 3.4f, cy + 3.4f, 0, 0, 0xFFE89CD0u, z);
            rrect(cx - 2.0f, cy - 2.0f, cx + 2.0f, cy + 2.0f, 0, 0, 0xFF222222u, z + Z_ICD);
            break;
        default: {
            float o[6] = { cx, cy - 4.4f, cx + 4.3f, cy + 3.0f, cx - 4.3f, cy + 3.0f }, in[6];
            for (int i = 0; i < 6; i++)
                in[i] = (i & 1 ? cy + 0.53f : cx) + (o[i] - (i & 1 ? cy + 0.53f : cx)) * 0.45f;
            poly(o, 3, 0xFF5CD0A8u, z);
            poly(in, 3, 0xFF222222u, z + Z_ICD);
            break;
        }
        }
        return 15;
    }
    if (style == BPS_SWITCH) {
        ring(cx, cy, 7.5f, C_BODY);
        l[0] = sw[pos];
        ic_text(cx - 0.4f, y + 2.5f, l, 0.42f, 0xFFFFFFFFu);
        return 15;
    }
    disc(cx, cy, 7.5f, 0xFF161616u, Z_IC);
    disc(cx, cy, 6.7f, xc[pos], Z_IC + Z_ICD);
    l[0] = xb[pos];
    ic_text(cx - 0.4f, y + 2.5f, l, 0.42f, 0xFFFFFFFFu);
    return 15;
}

static float ic_shoulder(int code, float x, float y, int draw)
{
    static const char* lab[3][4] = {
        { "LB", "RB", "LT", "RT" },
        { "L1", "R1", "L2", "R2" },
        { "L",  "R",  "ZL", "ZR" }
    };
    int trig = code == GP_LT || code == GP_RT, right = code == GP_RB || code == GP_RT;
    const char* s = lab[bind_pad_style()][trig * 2 + right];
    float w = pc_ui_text_w(s, 0.36f) + 9;
    if (w < 18) w = 18;
    if (!draw) return w;
    if (trig) {
        rrect(x, y, x + w, y + 15, 6.5f, 1.5f, C_EDGE, Z_IC);
        rrect(x + 0.9f, y + 0.9f, x + w - 0.9f, y + 14.1f, 5.6f, 0.8f, C_BODY, Z_IC + Z_ICD);
    } else {
        rrect(x, y + 2, x + w, y + 13.5f, 4.5f, 1.5f, C_EDGE, Z_IC);
        rrect(x + 0.9f, y + 2.9f, x + w - 0.9f, y + 12.6f, 3.6f, 0.8f, C_BODY, Z_IC + Z_ICD);
    }
    ic_text(x + w / 2, y + (trig ? 2.6f : 2.2f), s, 0.36f, 0xFFFFFFFFu);
    return w;
}

static float ic_menu(int code, float x, float y, int draw)
{
    int style = bind_pad_style(), start = code == GP_START;
    float z = Z_IC + 2 * Z_ICD, cx, cy = y + 7.5f;
    if (style == BPS_PS) {
        if (!draw) return 11;
        cx = x + 5.5f;
        rrect(x, y + 0.5f, x + 11, y + 14.5f, 3.5f, 3.5f, C_EDGE, Z_IC);
        rrect(x + 0.9f, y + 1.4f, x + 10.1f, y + 13.6f, 2.6f, 2.6f, C_BODY, Z_IC + Z_ICD);
        for (int i = -1; i <= 1; i++) {
            if (start) bar(cx - 2.6f, cy + i * 2.6f, cx + 2.6f, cy + i * 2.6f, 1.1f, C_MARK, z);
            else bar(cx + i * 2.2f, cy - 2.8f, cx + i * 2.2f, cy + 2.8f, 1.1f, C_MARK, z);
        }
        return 11;
    }
    if (!draw) return 15;
    cx = x + 7.5f;
    ring(cx, cy, 7.5f, C_BODY);
    if (style == BPS_SWITCH) {
        bar(cx - 3.5f, cy, cx + 3.5f, cy, 1.5f, C_MARK, z);
        if (start) bar(cx, cy - 3.5f, cx, cy + 3.5f, 1.5f, C_MARK, z);
    } else if (start) {
        for (int i = -1; i <= 1; i++) bar(cx - 3.2f, cy + i * 2.4f, cx + 3.2f, cy + i * 2.4f, 1.1f, C_MARK, z);
    } else {
        rrect(cx - 3.6f, cy - 3.0f, cx + 1.4f, cy + 1.2f, 0, 0, C_MARK, z);
        rrect(cx - 2.7f, cy - 2.1f, cx + 0.5f, cy + 0.3f, 0, 0, C_BODY, z + Z_ICD);
        rrect(cx - 1.4f, cy - 1.2f, cx + 3.6f, cy + 3.0f, 0, 0, C_MARK, z + 2 * Z_ICD);
        rrect(cx - 0.5f, cy - 0.3f, cx + 2.7f, cy + 2.1f, 0, 0, C_BODY, z + 3 * Z_ICD);
    }
    return 15;
}

static float ic_dpad(int code, float x, float y, int draw)
{
    float cx = x + 7.5f, cy = y + 7.5f, z = Z_IC + 2 * Z_ICD;
    if (!draw) return 15;
    rrect(cx - 2.8f, y, cx + 2.8f, y + 15, 1, 1, C_EDGE, Z_IC);
    rrect(x, cy - 2.8f, x + 15, cy + 2.8f, 1, 1, C_EDGE, Z_IC);
    rrect(cx - 1.9f, y + 0.9f, cx + 1.9f, y + 14.1f, 0.6f, 0.6f, C_BODY, Z_IC + Z_ICD);
    rrect(x + 0.9f, cy - 1.9f, x + 14.1f, cy + 1.9f, 0.6f, 0.6f, C_BODY, Z_IC + Z_ICD);
    if (code == GP_UP) tri(cx, y + 3.6f, 2.0f, 0, C_MARK, z);
    if (code == GP_DOWN) tri(cx, y + 11.4f, 2.0f, 2, C_MARK, z);
    if (code == GP_LEFT || code == GP_DPAD_LR) tri(x + 3.6f, cy, 2.0f, 3, C_MARK, z);
    if (code == GP_RIGHT || code == GP_DPAD_LR) tri(x + 11.4f, cy, 2.0f, 1, C_MARK, z);
    return 15;
}

static float ic_stick(int code, float x, float y, int draw)
{
    int a = (code - GP_AXIS) / 2, left = code == GP_LSTICK || (code >= GP_AXIS && a < 2);
    float cx = x + 7.5f, cy = y + 7.5f, z = Z_IC + 2 * Z_ICD;
    if (!draw) return 15;
    ring(cx, cy, 7.5f, C_BODY);
    if (code < GP_AXIS) {
        disc(cx, cy, 5.2f, C_EDGE, z);
        disc(cx, cy, 4.4f, C_BODY, z + Z_ICD);
    } else {
        static const int dir[4][2] = { { 1, 3 }, { 2, 0 }, { 1, 3 }, { 2, 0 } };
        int d = dir[a & 3][(code - GP_AXIS) & 1];
        tri(cx + (d == 1 ? 5.0f : d == 3 ? -5.0f : 0), cy + (d == 2 ? 5.0f : d == 0 ? -5.0f : 0), 1.7f, d, C_MARK, z);
    }
    ic_text(cx, y + 2.2f, left ? "L" : "R", 0.36f, 0xFFFFFFFFu);
    return 15;
}

static float ic_mouse(int n, float x, float y, int draw)
{
    float z = Z_IC + 2 * Z_ICD;
    if (!draw) return 11;
    rrect(x, y, x + 11, y + 15, 5.5f, 4.5f, C_EDGE, Z_IC);
    rrect(x + 0.9f, y + 0.9f, x + 10.1f, y + 14.1f, 4.6f, 3.6f, C_BODY, Z_IC + Z_ICD);
    bar(x + 1, y + 6.2f, x + 10, y + 6.2f, 0.8f, C_EDGE, z);
    bar(x + 5.5f, y + 1, x + 5.5f, y + 6.2f, 0.8f, C_EDGE, z);
    if (n == 1) rrect(x + 1.6f, y + 1.7f, x + 4.8f, y + 5.5f, 1.6f, 0.3f, C_MARK, z + Z_ICD);
    if (n == 3) rrect(x + 6.2f, y + 1.7f, x + 9.4f, y + 5.5f, 1.6f, 0.3f, C_MARK, z + Z_ICD);
    if (n == 2) rrect(x + 4.6f, y + 2.2f, x + 6.4f, y + 5.6f, 0.9f, 0.9f, C_MARK, z + Z_ICD);
    if (n > 3) bar(x + 0.3f, y + (n == 4 ? 9.5f : 6.5f), x + 0.3f, y + (n == 4 ? 12.5f : 9.5f), 1.4f, C_MARK, z + Z_ICD);
    return 11;
}

static float ic_key(const char* s, float x, float y, int draw)
{
    static const char* const arrows[4] = { "Up", "Right", "Down", "Left" };
    int arrow = -1;
    float w;
    for (int i = 0; i < 4; i++) if (!strcmp(s, arrows[i])) arrow = i;
    w = arrow >= 0 ? 15 : pc_ui_text_w(s, 0.40f) + 10;
    if (w < 15) w = 15;
    if (!draw) return w;
    rrect(x, y, x + w, y + 15, 2.5f, 2.5f, C_EDGE, Z_IC);
    rrect(x + 0.9f, y + 0.8f, x + w - 0.9f, y + 12.8f, 1.8f, 1.8f, 0xFF3E3B39u, Z_IC + Z_ICD);
    if (arrow >= 0) tri(x + 7.5f, y + 6.8f, 3.0f, arrow, 0xFFFFFFFFu, Z_IC + 2 * Z_ICD);
    else ic_text(x + w / 2, y + 0.4f, s, 0.40f, 0xFFFFFFFFu);
    return w;
}

static float ic_pad(int code, float x, float y, int draw)
{
    char name[40];
    if (code >= GP_SOUTH && code <= GP_NORTH) return ic_face(code, x, y, draw);
    if (code == GP_LB || code == GP_RB || code == GP_LT || code == GP_RT || code == GP_LT + 1 || code == GP_RT + 1)
        return ic_shoulder(code == GP_LT + 1 ? GP_LT : code == GP_RT + 1 ? GP_RT : code, x, y, draw);
    if (code == GP_BACK || code == GP_START) return ic_menu(code, x, y, draw);
    if ((code >= GP_UP && code <= GP_RIGHT) || code == GP_DPAD_LR) return ic_dpad(code, x, y, draw);
    if (code == GP_LSTICK || code == GP_RSTICK || (code >= GP_AXIS && code < GP_LT)) return ic_stick(code, x, y, draw);
    return ic_key(bind_code_name(BIND_PAD, code, name, sizeof(name)), x, y, draw);
}

static float glyph_run(const char* g, float x, float y, int draw)
{
    float w = 0, gap = 0;
    while (*g) {
        float iw;
        if (*g == '\t') { g++; continue; }
        if ((*g == GL_PAD || *g == GL_MOUSE) && g[1]) {
            int pad = *g == GL_PAD, code = (unsigned char)g[1];
            iw = pad ? ic_pad(code, 0, y, 0) : ic_mouse(code, 0, y, 0);
            if (draw && pad) ic_pad(code, x + w + gap, y, 1);
            if (draw && !pad) ic_mouse(code, x + w + gap, y, 1);
            g += 2;
        } else {
            char t[40];
            int n = 0;
            while (g[n] && g[n] != '\t' && g[n] != GL_PAD && g[n] != GL_MOUSE) n++;
            snprintf(t, sizeof(t), "%.*s", n, g);
            g += n;
            iw = ic_key(t, 0, y, 0);
            if (draw) ic_key(t, x + w + gap, y, 1);
        }
        w += gap + iw;
        gap = 3;
    }
    return w;
}

static const char* pad_glyph(unsigned int mask, char* buf, int n)
{
    static const struct { unsigned int bit; int row; } m[] = {
        { P_CROSS, B_CROSS }, { P_CIRCLE, B_CIRCLE }, { P_SQUARE, B_SQUARE }, { P_TRI, B_TRIANGLE },
        { P_L1, B_L1 }, { P_R1, B_R1 }, { P_START, B_START }
    };
    for (int i = 0; i < (int)(sizeof(m) / sizeof(m[0])); i++) {
        if (mask & m[i].bit) {
            int c = bind_get(m[i].row, BIND_PAD, 0);
            if (!c) c = bind_get(m[i].row, BIND_PAD, 1);
            if (c <= 0 || c > 0x7F || n < 3) return "-";
            buf[0] = GL_PAD; buf[1] = (char)c; buf[2] = 0;
            return buf;
        }
    }
    return "?";
}

static const char* kb_glyph(int row, char* buf, int n)
{
    int c = bind_get(row, BIND_KB, 0);
    if (!c) c = bind_get(row, BIND_KB, 1);
    const char* s;
    if (c > BIND_MOUSE && c < BIND_MOUSE + 6 && n >= 3) {
        buf[0] = GL_MOUSE; buf[1] = (char)(c - BIND_MOUSE); buf[2] = 0;
        return buf;
    }
    s = bind_code_name(BIND_KB, c, buf, n);
    if (!strcmp(s, "Return")) return "Enter";
    if (!strcmp(s, "Escape")) return "Esc";
    if (!strcmp(s, "Delete")) return "Del";
    return s;
}

void ui_glyph_device(unsigned int press, int kb_used)
{
    if (press) using_pad = pad_last_pad();
    if (g_button_icons == BI_KBM) using_pad = 0;
    else if (g_button_icons >= BI_XBOX && g_button_icons <= BI_SWITCH) using_pad = 1;
    else if (kb_used) using_pad = 0;
}

int ui_glyph_using_pad(void)
{
    if (g_button_icons == BI_KBM) return 0;
    if (g_button_icons >= BI_XBOX && g_button_icons <= BI_SWITCH) return 1;
    return using_pad;
}

const char* ui_glyph_str(int which, char* buf, int n)
{
    char a[32], b[32];
    int pad = ui_glyph_using_pad();
    unsigned int ok = (sys && (sys->keytype & 3) < 3) ? AdvGetOkButton() : 0xC0u;
    unsigned int cancel = (sys && (sys->keytype & 3) < 3) ? AdvGetCancelButton() : 0x20u;
    switch (which) {
    case UG_OK:
        return pad ? pad_glyph(ok, buf, n) : kb_glyph(B_START, buf, n);
    case UG_BACK:
        return pad ? pad_glyph(cancel, buf, n) : "Esc";
    case UG_START:
        return pad ? pad_glyph(P_START, buf, n) : kb_glyph(B_START, buf, n);
    case UG_CAT:
        if (pad) snprintf(buf, n, "%s%s", pad_glyph(P_L1, a, sizeof(a)), pad_glyph(P_R1, b, sizeof(b)));
        else snprintf(buf, n, "%s\t%s", kb_glyph(B_L1, a, sizeof(a)), kb_glyph(B_R1, b, sizeof(b)));
        return buf;
    case UG_CHANGE:
        if (pad) { snprintf(buf, n, "%c%c", GL_PAD, GP_DPAD_LR); return buf; }
        snprintf(buf, n, "%s\t%s", kb_glyph(B_LEFT, a, sizeof(a)), kb_glyph(B_RIGHT, b, sizeof(b)));
        return buf;
    }
    return "";
}

float ui_glyph_hint_w(const char* glyph, const char* word)
{
    return glyph_run(glyph, 0, 0, 0) + 5 + pc_ui_text_w(word, 0.46f);
}

float ui_glyph_hint(float x, float y, const char* glyph, const char* word)
{
    float gw = glyph_run(glyph, x, y, 0), ww = pc_ui_text_w(word, 0.46f);
    glyph_run(glyph, x, y, 1);
    pc_ui_text(x + gw + 5, y, word, 0.46f, C_HINT);
    return x + gw + 5 + ww + 16;
}

void ui_glyph_bar(float x, float y, const int* actions, const char* const* words, int count)
{
    char g[40];
    for (int i = 0; i < count; i++)
        x = ui_glyph_hint(x, y, ui_glyph_str(actions[i], g, sizeof(g)), words[i]);
}

void ui_glyph_mark(float x, float y, int kind)
{
    if (kind == 1) {
        pc_ui_rect(x, y + 1, x + 12, y + 14, 0xFFB06A28u, 0.34f);
        pc_ui_text_flat(x + 2.5f, y, "R", 0.40f, 0xFFFFFFFFu);
        return;
    }
    if (kind == 2) {
        float cx = x + 6, cy = y + 8;
        float hx = cx + 4.8f * cosf(4.19f), hy = cy + 4.8f * sinf(4.19f);
        for (int i = 0; i <= 10; i++) {
            float a = 5.24f + i * 0.524f;
            float px = cx + 4.8f * cosf(a), py = cy + 4.8f * sinf(a);
            pc_ui_rect(px - 0.9f, py - 0.9f, px + 0.9f, py + 0.9f, C_GOLD, 0.34f);
        }
        {
            NJS_POLYGON_VTX v[3] = {
                { hx + 3.2f, hy, 0.35f, C_GOLD },
                { hx - 1.4f, hy - 3.2f, 0.35f, C_GOLD },
                { hx - 1.4f, hy + 3.2f, 0.35f, C_GOLD }
            };
            njDrawPolygon(v, 3, 1);
        }
    }
}
