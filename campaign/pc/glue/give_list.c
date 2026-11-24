/* Native Visual Give Item picker (ENHANCED OPTIONS > CHEATS > Visual item picker):
 * every named item grouped by category (WEAPONS, AMMUNITION, HEALING, OTHER ITEMS, KEY ITEMS)
 * with real inventory icons loaded from ITEM AFS file 145 (textures 3 & 4), quantity stepper,
 * live player inventory preview, and immediate addition (with item-box fallback when full).
 * Strictly offline single-player. Game thread. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/item.h>
#include <ps2/veronica/prog/sdfunc.h>
#include <ps2/veronica/prog/ps2_texture.h>
#include <ps2/veronica/prog/ps2_NaTextureFunction.h>
#include <ps2/veronica/prog/ps2_NaSprite.h>
#include <ps2/veronica/prog/ps2_NinjaCnk.h>
#include <ps2/veronica/prog/ps2_NaSystem.h>
#include <ps2/veronica/prog/ps2_NaDraw.h>
#include "../src/host/cheat.h"
#include "../src/host/menu_mouse.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void host_log(const char* fmt, ...);
void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb);
float pc_ui_text_w(const char* s, float sc);
int pc_ui_text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines);
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z);
void pc_ui_arrow(float x, float y, int dir, unsigned int argb);
void options_say(const char* m);
Sint32 ADXF_ReadNw32(ADXF adxf, Sint32 nsct, void* buf);

enum { BTN_L1 = 0x4, BTN_R1 = 0x8, BTN_UP = 0x1000, BTN_RIGHT = 0x2000, BTN_DOWN = 0x4000, BTN_LEFT = 0x8000 };

#define BAR     7.0f
#define GAP     5.0f
#define LIST_W  294.0f
#define ICON_SC 0.40f
#define NAME_DX 62.0f
#define Z_SEL   0.30f
#define Z_ICON  0.36f
#define ZR(base, i) ((base) + ((gl.top & 1) * 16 + (i)) * 0.0004f)
#define C_GOLD  0xFFD6B668u

enum { G_WEAPON, G_AMMO, G_HEAL, G_OTHER, G_KEY, G_N };
static const char* const group_name[G_N] = { "WEAPONS", "AMMUNITION", "HEALING", "OTHER ITEMS", "KEY ITEMS" };

/* ---------------------------------------------------------------- item icon textures (ITEM AFS 145) */
static unsigned char* icon_alloc;
static NJS_TEXNAME icon_names[2];
static NJS_TEXLIST icons = { icon_names, 2 };
static int icon_state;   /* 0 unloaded, 1 loaded, -1 failed */
static int icon_users;   /* bitmask of active icon users (1 give_list, 2 spoiler_menu) */

static int item_icon_file(unsigned char* buf, int cap, unsigned char** chunk)
{
    ADXF h;
    unsigned char *p = buf, *end;
    int n = 0, ok, sz;
    if (!sys || sys->itm_partid < 0) return 0;
    h = ADXF_OpenAfs(sys->itm_partid, 145);
    if (!h) return 0;
    ok = ADXF_GetFsizeSct(h) * 2048 <= cap;
    if (ok) { ADXF_ReadNw32(h, ADXF_GetFsizeSct(h), buf); ok = ADXF_GetStat(h) == 3; }
    sz = ADXF_GetFsizeByte(h);
    ADXF_Close(h);
    if (!ok) return 0;
    end = buf + sz;
    while (p + 32 <= end && *(int*)p != -1 && n < 5) {
        int csz = ((int*)p)[1];
        if (csz < 0 || p + 32 + csz > end) return 0;
        if (!memcmp(p, "TIM2", 4)) chunk[n++] = p;
        p += 32 + csz;
    }
    return n == 5;
}

static int icon_load(void)
{
    enum { CAP = 1100 * 1024 };
    unsigned char *raw, *out, *chunk[5];
    if (!icon_alloc) icon_alloc = malloc(CAP + 64);
    if (!icon_alloc) return 0;
    raw = (unsigned char*)(((uintptr_t)icon_alloc + 63) & ~(uintptr_t)63);
    out = raw;
    if (!item_icon_file(raw, CAP, chunk)) return 0;
    for (int n = 3; n < 5; n++) {
        int csz = 32 + ((int*)chunk[n])[1];
        TIM2_PICTUREHEADER_EX* t;
        memmove(out, chunk[n], (size_t)csz);
        t = (TIM2_PICTUREHEADER_EX*)(out + 32);
        if (t->ImageType != 5 || t->ImageWidth != 512) return 0;
        t->PictFormat = 0;
        t->Gindex = 0x03C70E00 + n;
        out += csz;
    }
    ((int*)out)[0] = -1;
    ((int*)out)[1] = 0;
    bhGarbageTexture(NULL, 0);
    return bhSetMemPvpTexture(&icons, raw, 0) == 2;
}

static void icon_unload(void)
{
    if (icon_state == 1) { njReleaseTexture(&icons); bhGarbageTexture(NULL, 0); }
    icon_state = 0;
    free(icon_alloc);
    icon_alloc = NULL;
}

/* ps2_NaTextureFunction.c njReleaseTextureAll freed every texture */
void give_list_textures_released(void)
{
    icon_state = 0;
    icon_users = 0;
    free(icon_alloc);
    icon_alloc = NULL;
}

int pc_item_icons(int user, int on)
{
    if (on) {
        icon_users |= user;
        if (!icon_state) {
            icon_state = icon_load() ? 1 : -1;
            host_log("items: item icons %s\n", icon_state > 0 ? "loaded" : "not loaded");
        }
    } else {
        icon_users &= ~user;
        if (!icon_users && icon_state) icon_unload();
    }
    return icon_state == 1;
}

static void item_icon_cell(int id, int* tex, int* u, int* v)
{
    static const struct { unsigned char from, to; } remap[] = {
        { 30, 13 }, { 63, 41 }, { 128, 41 }, { 129, 41 }, { 130, 35 }, { 136, 47 },
        { 138, 48 }, { 142, 128 }, { 143, 49 }, { 144, 102 }, { 145, 133 }
    };
    int no = id, item;
    for (int i = 0; i < (int)(sizeof(remap) / sizeof(remap[0])); i++)
        if (remap[i].from == id) no = remap[i].to;
    item = no;
    if (item > 63) no -= 64;
    else if (no && no < 4) no = no * 2 - 1;
    else if (no < 33) { if (no) no += 4; }
    else if (no == 33 || no == 34) no += no - 33 + 4;
    else no += 6;
    *u = (no % 7) * 34;
    *v = (no / 7) * 24;
    *tex = item < 64 ? 1 : 0;
}

static void draw_item_icon(unsigned int w, float x, float y, float sc, unsigned int argb, float z)
{
    int id = (w >> 16) & 0xFF, two = (itemdata[id].type & 0x100) != 0, tex, u, v;
    NJS_TEXANIM a;
    NJS_SPRITE s;
    NJS_ARGB c = {
        (argb >> 24) / 255.0f,
        ((argb >> 16) & 0xFF) / 255.0f,
        ((argb >> 8) & 0xFF) / 255.0f,
        (argb & 0xFF) / 255.0f
    };
    if (icon_state != 1 || !id) return;
    item_icon_cell(id, &tex, &u, &v);
    memset(&a, 0, sizeof(a));
    a.sx = two ? 136 : 68;
    a.sy = 48;
    a.u1 = u;
    a.v1 = v;
    a.u2 = a.u1 + (two ? 68 : 34);
    a.v2 = a.v1 + 24;
    a.texid = tex;
    memset(&s, 0, sizeof(s));
    s.p.x = x; s.p.y = y; s.p.z = -1.0f;
    s.sx = s.sy = sc;
    s.tlist = &icons;
    s.tanim = &a;
    njSetConstantMaterial(&c);
    njSetTexture(&icons);
    njDrawSprite2D(&s, 0, z < 1.0f ? -1.0f / z : -1.0f, 0x20 | 0x2);
}

void pc_item_icon(unsigned int w, float x, float y, float sc, unsigned int argb, float z)
{
    draw_item_icon(w, x, y, sc, argb, z);
}

/* ---------------------------------------------------------------- list state */
static struct {
    short row[CHEAT_ITEMS + G_N];
    int n, cur, top, rows;
    int amount;
    float x0, x1, y0, h;
    int icons;
} gl;

static int group_of(int id)
{
    int t = itemdata[id].type;
    if (id >= 152 || !g_cheat.names[id][0] || !strcmp(g_cheat.names[id], "?")) return -1;
    if (t & 0x40) return G_KEY;
    if (t & 0x1) return G_WEAPON;
    if ((t & 0x2) && (t & 0x1800)) return G_AMMO;
    if (t & 0x8) return G_HEAL;
    return G_OTHER;
}

static int heading(int r) { return r < gl.n && gl.row[r] < 0; }
static int item_at_row(int r) { return r >= 0 && r < gl.n && gl.row[r] > 0 ? gl.row[r] : 0; }
static int counted(int id) { return (itemdata[id].type & 0x1800) != 0; }
static int pct(int id) { return (itemdata[id].type & 0x1000) && itemdata[id].max > 0; }
static int amount_min(int id) { return (itemdata[id].type & 0x1) ? 0 : 1; }
static int amount_max(int id)
{
    int m = itemdata[id].max;
    if (!counted(id)) return 1;
    if (itemdata[id].type & 0x200) return 999;
    return m > 0 ? m : 1;
}
static int amount_default(int id) { int m = itemdata[id].max; return !counted(id) ? 1 : m > 0 ? (m > 999 ? 999 : m) : 1; }
static const char* amount_text(int id, int v, char* b, int n)
{
    if (pct(id)) snprintf(b, n, "%d%%", (v * 100 + itemdata[id].max / 2) / itemdata[id].max);
    else snprintf(b, n, "%d", v);
    return b;
}

static void build(void)
{
    gl.n = 0;
    for (int g = 0; g < G_N; g++) {
        int head = gl.n;
        gl.row[gl.n++] = (short)(-1 - g);
        for (int id = 1; id < CHEAT_ITEMS; id++)
            if (group_of(id) == g) gl.row[gl.n++] = (short)id;
        if (gl.n == head + 1) gl.n--;
    }
}

static int max_top(void) { return gl.n > gl.rows ? gl.n - gl.rows : 0; }
static float back_y(void) { return gl.y0 + gl.rows * gl.h + GAP; }
static float list_x1(void) { return gl.x0 + LIST_W; }
static float panel_x0(void) { return list_x1() + BAR + 10; }
static float amount_y(void) { return gl.y0 + 56; }

static void follow(void)
{
    int want = gl.cur > 0 && heading(gl.cur - 1) ? gl.cur - 1 : gl.cur;
    if (gl.cur < gl.n && want < gl.top) gl.top = want;
    if (gl.cur < gl.n && gl.cur >= gl.top + gl.rows) gl.top = gl.cur - gl.rows + 1;
    if (gl.top > max_top()) gl.top = max_top();
    if (gl.top < 0) gl.top = 0;
}

static void to(int c, int dir, int wrap)
{
    int was = gl.cur, id;
    if (!gl.n) { gl.cur = 0; return; }
    if (wrap) c = (c % (gl.n + 1) + gl.n + 1) % (gl.n + 1);
    else c = c < 0 ? 0 : c >= gl.n ? gl.n - 1 : c;
    while (c < gl.n && heading(c)) {
        c += dir < 0 ? -1 : 1;
        if (c < 0) c = wrap ? gl.n : 1;
    }
    if (c > gl.n) c = gl.n;
    gl.cur = c;
    if (c != was) {
        CallSystemSe(0, 2);
        if ((id = item_at_row(c))) gl.amount = amount_default(id);
    }
    follow();
}

static void group_jump(int dir)
{
    int c = gl.cur >= gl.n ? gl.n - 1 : gl.cur, h, k;
    if (gl.n < 2) return;
    for (h = c; h > 0 && !heading(h); h--) ;
    if (dir > 0) {
        for (k = c + 1; k < gl.n && !heading(k); k++) ;
        if (k >= gl.n) k = 0;
    } else if (c > h + 1) {
        k = h;
    } else {
        for (k = h - 1; k > 0 && !heading(k); k--) ;
        if (h == 0) for (k = gl.n - 1; k > 0 && !heading(k); k--) ;
    }
    to(k + 1, 1, 0);
    gl.top = k;
    follow();
}

static void amount_step(int dir)
{
    int id = item_at_row(gl.cur), v, step;
    if (!id || amount_min(id) == amount_max(id)) { CallSystemSe(0, 1); return; }
    step = pct(id) ? (itemdata[id].max + 9) / 10 : 1;
    v = gl.amount + dir * step;
    v = v < amount_min(id) ? amount_min(id) : v > amount_max(id) ? amount_max(id) : v;
    if (v == gl.amount) return;
    gl.amount = v;
    CallSystemSe(0, 2);
}

void give_list_open(float x0, float x1, float y0, float h, int rows)
{
    int keep = gl.cur, keep_top = gl.top;
    gl.x0 = x0; gl.x1 = x1; gl.y0 = y0; gl.h = h; gl.rows = rows;
    build();
    gl.cur = gl.top = 0;
    if (keep > 0 && keep < gl.n && !heading(keep)) { gl.cur = keep; gl.top = keep_top; }
    else to(0, 1, 0);
    if (item_at_row(gl.cur)) gl.amount = amount_default(item_at_row(gl.cur));
    follow();
    gl.icons = pc_item_icons(1, 1);
}

void give_list_release(void)
{
    pc_item_icons(1, 0);
    gl.icons = 0;
}

static void give(void)
{
    int id = item_at_row(gl.cur), r;
    char b[160], a[16];
    if (!id) return;
    r = cheat_give(id, gl.amount, 1);
    if (r == CHEAT_GIVE_FULL) {
        CallSystemSe(0, 1);
        options_say("Inventory and item box are full.");
        return;
    }
    if (r == CHEAT_GIVE_BOX) snprintf(b, sizeof(b), "Inventory full: %s went into the item box.", g_cheat.names[id]);
    else if (counted(id)) snprintf(b, sizeof(b), "Added: %s x%s", g_cheat.names[id], amount_text(id, gl.amount, a, sizeof(a)));
    else snprintf(b, sizeof(b), "Added: %s", g_cheat.names[id]);
    CallSystemSe(0, 3);
    options_say(b);
    host_log("give: %d (%s) x%d: %s\n", id, g_cheat.names[id], gl.amount, r == CHEAT_GIVE_BOX ? "item box" : "inventory");
}

static int at(float x, float y)
{
    int i;
    if (item_at_row(gl.cur) && y >= amount_y() - 4 && y < amount_y() + 18) {
        float px = panel_x0(), w = gl.x1 - px;
        if (x >= px && x < px + 22) return -2;
        if (x >= px + w - 22 && x < gl.x1 + 4) return -3;
    }
    if (x < gl.x0 - 8 || x > list_x1()) return -1;
    if (y >= back_y() - 3 && y < back_y() + gl.h - 3) return gl.n;
    if (y < gl.y0 - 3) return -1;
    i = (int)((y - gl.y0 + 3) / gl.h);
    return i < gl.rows && gl.top + i < gl.n && !heading(gl.top + i) ? gl.top + i : -1;
}

int give_list_input(const MouseTake* m, unsigned int press, unsigned int rept, unsigned int ok, unsigned int cancel)
{
    int hit;
    if (m->wheel) {
        gl.top -= m->wheel * 3;
        gl.top = gl.top < 0 ? 0 : gl.top > max_top() ? max_top() : gl.top;
        if (gl.cur < gl.n && gl.cur < gl.top) to(gl.top, 1, 0);
        else if (gl.cur < gl.n && gl.cur >= gl.top + gl.rows) to(gl.top + gl.rows - 1, -1, 0);
        return 0;
    }
    hit = at(m->x, m->y);
    if (hit >= 0 && (m->moved || m->left) && hit != gl.cur) {
        int t = gl.top;
        to(hit, 1, 0);
        gl.top = t;
    }
    if (m->left && hit == gl.n) { CallSystemSe(0, 0); return 1; }
    if (m->left && hit == -2) { amount_step(-1); return 0; }
    if (m->left && hit == -3) { amount_step(1); return 0; }
    if (m->left && hit >= 0) { give(); return 0; }
    if (m->right || (press & cancel)) { CallSystemSe(0, 0); return 1; }
    if (rept & BTN_UP) to(gl.cur - 1, -1, 1);
    else if (rept & BTN_DOWN) to(gl.cur + 1, 1, 1);
    else if (press & BTN_L1) group_jump(-1);
    else if (press & BTN_R1) group_jump(1);
    else if (rept & BTN_LEFT) amount_step(-1);
    else if (rept & BTN_RIGHT) amount_step(1);
    else if (press & ok) {
        if (gl.cur >= gl.n) { CallSystemSe(0, 0); return 1; }
        give();
    }
    return 0;
}

static void text_fit(float x, float y, const char* s, float sc, float w, unsigned int col)
{
    float tw = pc_ui_text_w(s, sc), k = tw > w && tw > 0 ? w / tw : 1.0f;
    pc_ui_text(x, y + (1.0f - k) * 9.0f, s, sc * k, col);
}

static void inventory(float x, float y)
{
    const unsigned int* inv = &sys->itm[(sys->ply_id > 2 ? 0 : sys->ply_id) * 16];
    int slots = (sys->gm_flg & 0x8000000) ? 10 : 8, c = 0;
    float cw = 25.0f, ch = 17.0f;
    char b[16];
    for (int k = 0; k < slots; k++) {
        float rx = x + (k & 1) * (cw + 2), ry = y + (k >> 1) * (ch + 2);
        pc_ui_rect(rx, ry, rx + cw, ry + ch, 0xC0141A28u, 0.33f);
    }
    for (int i = 2; i < 2 + slots && c < slots; i++) {
        int id = (inv[i] >> 16) & 0xFF, two;
        float cx, cy;
        if (!id || id >= CHEAT_ITEMS) continue;
        two = (itemdata[id].type & 0x100) != 0;
        if (two && (c & 1)) c++;
        cx = x + (c & 1) * (cw + 2);
        cy = y + (c >> 1) * (ch + 2);
        if (gl.icons) draw_item_icon(inv[i], cx, cy, 0.35f, 0xFFFFFFFFu, 0.37f + c * 0.0004f);
        if (counted(id)) {
            amount_text(id, inv[i] & 0xFFFF, b, sizeof(b));
            pc_ui_text(cx + (two ? 2 * cw + 2 : cw) - pc_ui_text_w(b, 0.30f) - 1, cy + 7, b, 0.30f, 0xFF80FF80u);
        }
        c += two ? 2 : 1;
    }
}

void give_list_draw(void)
{
    int id = item_at_row(gl.cur);
    float y, px = panel_x0(), pw = gl.x1 - px;
    char b[64];
    for (int k = gl.top; k < gl.n && k < gl.top + gl.rows; k++) {
        int i = k - gl.top, rid = gl.row[k], sel = gl.cur == k;
        y = gl.y0 + i * gl.h;
        if (rid < 0) {
            pc_ui_text(gl.x0 - 4, y + 3, group_name[-1 - rid], 0.45f, C_GOLD);
            pc_ui_rect(gl.x0 - 4, y + gl.h - 4, list_x1(), y + gl.h - 3, 0x40D6B668u, ZR(Z_SEL, i));
            continue;
        }
        if (sel) pc_ui_rect(gl.x0 - 8, y - 2, list_x1(), y + gl.h - 3, 0x50FFFFFFu, ZR(Z_SEL, i));
        if (gl.icons) draw_item_icon((unsigned int)rid << 16, gl.x0, y - 1, ICON_SC, 0xFFFFFFFFu, ZR(Z_ICON, i));
        else pc_ui_rect(gl.x0, y, gl.x0 + 26, y + gl.h - 5, 0x60303038u, ZR(Z_ICON, i));
        text_fit(gl.x0 + NAME_DX, y + 2, g_cheat.names[rid], 0.50f, LIST_W - NAME_DX - 42, sel ? 0xFFFFFFFFu : 0xFFC8C8C8u);
        if (counted(rid)) {
            amount_text(rid, sel ? gl.amount : amount_default(rid), b, sizeof(b));
            pc_ui_text(list_x1() - 6 - pc_ui_text_w(b, 0.42f), y + 4, b, 0.42f, sel ? 0xFF80FF80u : 0xFF70A070u);
        }
    }
    if (gl.n > gl.rows) {
        float y0 = gl.y0 - 2, h = gl.rows * gl.h - 1, th = h * gl.rows / gl.n, ty;
        if (th < 12) th = 12;
        ty = y0 + (h - th) * gl.top / max_top();
        pc_ui_rect(list_x1() + 2, y0, list_x1() + 7, y0 + h, 0x40FFFFFFu, Z_SEL);
        pc_ui_rect(list_x1() + 2, ty, list_x1() + 7, ty + th, 0xC0FFFFFFu, Z_SEL + 0.001f);
    }
    y = back_y();
    if (gl.cur >= gl.n) pc_ui_rect(gl.x0 - 8, y - 2, list_x1(), y + gl.h - 3, 0x50FFFFFFu, Z_SEL + 0.02f);
    pc_ui_text(gl.x0, y + 1, "BACK", 0.52f, gl.cur >= gl.n ? 0xFFFFFFFFu : 0xFFB0B0B0u);

    /* detail & live inventory panel */
    pc_ui_rect(px - 4, gl.y0 - 4, gl.x1 + 2, back_y() + gl.h - 3, 0x600E0D10u, 0.25f);
    if (id) {
        int two = (itemdata[id].type & 0x100) != 0;
        float sc = 0.66f, iw = (two ? 136.0f : 68.0f) * sc;
        if (gl.icons) draw_item_icon((unsigned int)id << 16, px + (pw - iw) / 2, gl.y0, sc, 0xFFFFFFFFu, 0.36f);
        pc_ui_text_wrap(px, gl.y0 + 33, pw - 4, g_cheat.names[id], 0.42f, 0xFFFFFFFFu, 2);
        y = amount_y();
        if (counted(id)) {
            int lo = gl.amount > amount_min(id), hi = gl.amount < amount_max(id);
            amount_text(id, gl.amount, b, sizeof(b));
            pc_ui_arrow(px + 8, y + 8, -1, lo ? 0xFFFFFFFFu : 0xFF505050u);
            pc_ui_arrow(gl.x1 - 8, y + 8, 1, hi ? 0xFFFFFFFFu : 0xFF505050u);
            pc_ui_text(px + (pw - pc_ui_text_w(b, 0.52f)) / 2, y, b, 0.52f, 0xFF80FF80u);
        }
    }
    y = amount_y() + 20;
    pc_ui_text(px, y, "INVENTORY", 0.40f, C_GOLD);
    inventory(px + (pw - 52) / 2, y + 13);
}

const char* give_list_help(void)
{
    if (gl.cur >= gl.n) return "Back to Cheats.";
    if (!gl.n) return "Item names load after entering a game.";
    return counted(item_at_row(gl.cur))
         ? "Confirm adds the item immediately (item box if full). Left/right: amount. L1/R1: next category."
         : "Confirm adds the item immediately (item box if full). L1/R1: next category.";
}
