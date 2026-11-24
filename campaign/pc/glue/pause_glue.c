/* Ashford In-Game Pause Menu (level 0 inside Adv_GameOptionScreen when g_config.pause_menu
 * is enabled in-game): controller-reachable obsidian & antique-brass pause screen with
 * one-tap Camera mode cycling (Original / Third person / First person), direct shortcuts to
 * Enhanced Options, Appearance & Cast, Cheats & Tools, Classic OPTION, and Return to Title,
 * plus an active campaign modifiers summary and live button glyphs. Strictly offline. */
#include <ps2/veronica/prog/types.h>
#include <ps2/veronica/prog/main.h>
#include <ps2/veronica/prog/adv.h>
#include <ps2/veronica/prog/padman.h>
#include <ps2/veronica/prog/sdfunc.h>
#include "../src/host/settings.h"
#include "../src/host/bindings.h"
#include "../src/host/menu_mouse.h"
#include "../src/host/tps.h"
#include "../src/host/cheat.h"
#include "../src/host/enemies.h"
#include "../src/host/cast.h"
#include "../src/host/biorand.h"
#include "../src/ps2_platform.h"
#include "glyphs.h"
#include <stdio.h>
#include <string.h>

extern ADV_WORK AdvWork;
void host_log(const char* fmt, ...);
void pc_ui_text(float x, float y, const char* s, float sc, unsigned int argb);
float pc_ui_text_w(const char* s, float sc);
int pc_ui_text_wrap(float x, float y, float w, const char* s, float sc, unsigned int argb, int max_lines);
void pc_ui_rect(float x0, float y0, float x1, float y1, unsigned int argb, float z);
void pc_ui_arrow(float x, float y, int dir, unsigned int argb);
int pc_options_quit(int port);
void pc_options_quit_open(void);
void pc_options_quit_draw(void);
int pc_options_quit_up(void);
void pc_options_open_page(int page);

enum { P_UP = 0x1000, P_RIGHT = 0x2000, P_DOWN = 0x4000, P_LEFT = 0x8000, P_START = 0x800, P_SELECT = 0x100 };
enum { SC_ESC = 41 };

static int classic_opt;    /* 1 while user dropped into Classic PS2 OPTION from Pause */
static int after_spoiler;  /* 1 to open Spoiler Log as soon as OPTION exits */
static int opt_held = 1;
static int esc_prev = 1;
static int pause_sel;
static MouseTake mt;

int pc_new_menu_on(void) { return g_config.pause_menu && pc_in_game() && !classic_opt; }

int pc_new_menu_entry(void)
{
    classic_opt = 0;
    after_spoiler = 0;
    opt_held = 1;
    esc_prev = 1;
    pause_sel = 0;
    ui_glyph_device(1, 0);
    if (pc_new_menu_on()) host_log("menu: ashford pause\n");
    return 0;
}

int pc_options_back_level(void) { return 0; }

void pc_pause_closed(void)
{
    classic_opt = 0;
    opt_held = 1;
    if (after_spoiler) { after_spoiler = 0; spoiler_key(1); }
}

static int options_button(void)
{
    int opt = bind_pad_raw(B_TPS_OPTIONS);
    int r = opt && !opt_held && g_tps.on && g_tps.pad_modern && pc_in_game();
    opt_held = opt;
    return r;
}

static int esc_edge(void)
{
    const bool* k = bind_keys();
    int down = k && k[SC_ESC], r = down && !esc_prev;
    esc_prev = down;
    return r;
}

enum { I_RESUME, I_CAMERA, I_OPTIONS, I_APPEAR, I_CHEATS, I_SPOILER, I_CLASSIC, I_TITLE, I_N };
static const char* const items[I_N] = {
    "RESUME",
    "Camera",
    "ENHANCED OPTIONS",
    "APPEARANCE AND CAST",
    "CHEATS AND TOOLS",
    "SPOILER LOG",
    "CLASSIC OPTION",
    "RETURN TO TITLE"
};
static const char* const item_help[I_N] = {
    "Return to the game.",
    "Cycle camera view: Original fixed cameras, over-the-shoulder Third person, or First person.",
    "Display, widescreen, gameplay, controls, camera tuning, and audio settings.",
    "Player model swaps, per-character outfits, Cutscene Cast, and Cutscene Voice randomizers.",
    "Visual item picker, health/ammo cheats, item box anywhere, enemy spawner, and room teleport.",
    "Browse the active BioRand pack's spoiler log by area, segment, or key items (also bound to L).",
    "Open the original PlayStation 2 OPTION menu (Audio Setup, Key Config, Screen Setup, Vibration).",
    "Leave the current playthrough and return to the title screen."
};

#define PX       38.0f
#define PY0      100.0f
#define PDY      28.0f
#define PSC      0.60f
#define PANEL_X0 320.0f
#define PANEL_X1 614.0f
#define C_ROW    0xFFA49E94u
#define C_GOLD   0xFFD6B668u
#define C_PARCH  0xFFE8E0C0u
#define C_HELP   0xFFC8C2B4u

static float item_y(int i)
{
    return PY0 + i * PDY + (i == I_TITLE ? 14.0f : 0.0f);
}

static const Setting* find_setting(const char* label)
{
    for (int i = 0; i < g_settings_n; i++)
        if (!strcmp(g_settings[i].label, label)) return &g_settings[i];
    return NULL;
}

static void camera_cycle(int dir)
{
    const Setting* s = find_setting("Camera mode");
    if (!s || settings_locked(s)) { CallSystemSe(0, 1); return; }
    settings_step(s, dir);
    CallSystemSe(0, 2);
    host_log("pause: camera mode = %d\n", (int)g_tps.on);
}

static const char* item_text(int i, char* buf, int n)
{
    static const char* const cam_names[3] = { "Original", "Third person", "First person" };
    if (i != I_CAMERA) return items[i];
    int m = g_tps.on >= 0 && g_tps.on <= 2 ? (int)g_tps.on : 0;
    snprintf(buf, n, "Camera: %s", cam_names[m]);
    return buf;
}

static void move_sel(int dir)
{
    pause_sel = (pause_sel + dir + I_N) % I_N;
    CallSystemSe(0, 2);
}

static int pick(int i)
{
    ADV_WORK* ap = &AdvWork;
    ap->OptFadeType = 0;
    switch (i) {
    case I_RESUME:  CallSystemSe(0, 0); return -1;
    case I_CAMERA:  camera_cycle(1); return 0;
    case I_OPTIONS: CallSystemSe(0, 3); pc_options_open_page(SP_DISPLAY); return 4;
    case I_APPEAR:  CallSystemSe(0, 3); pc_options_open_page(SP_APPEAR); return 4;
    case I_CHEATS:  CallSystemSe(0, 3); pc_options_open_page(SP_CHEATS); return 4;
    case I_SPOILER: CallSystemSe(0, 3); after_spoiler = 1; return -1;
    case I_CLASSIC: CallSystemSe(0, 3); classic_opt = 1; ap->OptIndex = 0; return 0;
    case I_TITLE:   CallSystemSe(0, 3); pc_options_quit_open(); return 0;
    }
    return 0;
}

static int item_at(float x, float y)
{
    if (x < PX - 10 || x > PANEL_X0 - 24) return -1;
    for (int i = 0; i < I_N; i++)
        if (y >= item_y(i) - 3 && y < item_y(i) + PDY - 3) return i;
    return -1;
}

static int pause_input(int port)
{
    unsigned int press = Pad[port].press, rept = Pad[port].Rept;
    unsigned int ok = (sys->keytype & 3) < 3 ? AdvGetOkButton() : 0xC0u;
    unsigned int cancel = (sys->keytype & 3) < 3 ? AdvGetCancelButton() : 0x20u;
    int esc = esc_edge(), hit;
    menu_mouse_take(&mt);
    ui_glyph_device(press, mt.moved || mt.left || mt.right || esc);
    if (options_button()) { CallSystemSe(0, 0); AdvWork.OptFadeType = 0; return -1; }
    hit = item_at(mt.x, mt.y);
    if (hit >= 0 && (mt.moved || mt.left) && hit != pause_sel) { pause_sel = hit; CallSystemSe(0, 2); }
    if (mt.left && hit >= 0) return pick(hit);
    if (mt.right || esc || (press & (cancel | P_SELECT)) || ((press & P_START) && !pad_kb_start())) return pick(I_RESUME);
    if (rept & P_UP) move_sel(-1);
    else if (rept & P_DOWN) move_sel(1);
    else if ((press & P_LEFT) && pause_sel == I_CAMERA) camera_cycle(-1);
    else if ((press & P_RIGHT) && pause_sel == I_CAMERA) camera_cycle(1);
    else if (press & (ok | P_START)) return pick(pause_sel);
    return 0;
}

static void draw_campaign_card(float y)
{
    static const char* const diff_names[] = { "Normal", "Easy", "Very Easy", "Battle Game" };
    static const char* const cam_names[]  = { "Original", "Third person", "First person" };
    static const char* const var_names[]  = { "Mild", "Wild", "Custom" };
    static const char* const cast_names[] = { "Cutscene models", "All models", "Custom" };
    char t[160];
    int gm = sys->gm_mode >= 0 && sys->gm_mode <= 3 ? sys->gm_mode : 0;
    int cm = g_tps.on >= 0 && g_tps.on <= 2 ? (int)g_tps.on : 0;
    int mods = 0;

    pc_ui_text(PANEL_X0 + 12, y, "THIS CAMPAIGN", 0.52f, C_GOLD);
    y += 20;
    snprintf(t, sizeof(t), "Mode: %s", diff_names[gm]);
    pc_ui_text(PANEL_X0 + 16, y, t, 0.46f, 0xFFFFFFFFu); y += 16;
    snprintf(t, sizeof(t), "Camera: %s", cam_names[cm]);
    pc_ui_text(PANEL_X0 + 16, y, t, 0.46f, 0xFFFFFFFFu); y += 20;

    pc_ui_text(PANEL_X0 + 12, y, "ACTIVE MODIFIERS", 0.48f, C_GOLD);
    y += 18;
    if (g_enemy_opts.rand && mods < 6) {
        int v = g_enemy_opts.variety >= 0 && g_enemy_opts.variety <= 2 ? g_enemy_opts.variety : 0;
        snprintf(t, sizeof(t), "Enemy randomizer: %s (seed %d)", var_names[v], g_enemy_opts.seed);
        pc_ui_text(PANEL_X0 + 16, y, t, 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_enemy_opts.extra > 0 && mods < 6) {
        snprintf(t, sizeof(t), "Extra enemies: up to %d / room", g_enemy_opts.extra);
        pc_ui_text(PANEL_X0 + 16, y, t, 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_enemy_opts.place && mods < 6) {
        pc_ui_text(PANEL_X0 + 16, y, "Rearranged enemies: On", 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_enemy_opts.cast && mods < 6) {
        int p = g_enemy_opts.cast_all >= 0 && g_enemy_opts.cast_all <= 2 ? g_enemy_opts.cast_all : 0;
        snprintf(t, sizeof(t), "Cutscene cast: %s", cast_names[p]);
        pc_ui_text(PANEL_X0 + 16, y, t, 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_enemy_opts.voices && mods < 6) {
        pc_ui_text(PANEL_X0 + 16, y, "Cutscene voice randomizer: On", 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_tps.headshots && mods < 6) {
        pc_ui_text(PANEL_X0 + 16, y, "Headshots and piercing: On", 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_config.no_ink_ribbon && mods < 6) {
        pc_ui_text(PANEL_X0 + 16, y, "Free typewriter saves: On", 0.42f, C_HELP); y += 14; mods++;
    }
    if (g_config.extra_objects && mods < 6) {
        pc_ui_text(PANEL_X0 + 16, y, "Extra item boxes and herbs: On", 0.42f, C_HELP); y += 14; mods++;
    }
    if (biorand_active() && mods < 6) {
        snprintf(t, sizeof(t), "BioRand pack: %s", biorand_seed_text()[0] ? biorand_seed_text() : "Active");
        pc_ui_text(PANEL_X0 + 16, y, t, 0.42f, C_HELP); y += 14; mods++;
    }
    if ((g_cheat.inf_health || g_cheat.inf_ammo || g_cheat.noclip || g_cheat.unlock) && mods < 6) {
        snprintf(t, sizeof(t), "Cheats:%s%s%s%s",
                 g_cheat.inf_health ? " HP" : "",
                 g_cheat.inf_ammo ? " Ammo" : "",
                 g_cheat.noclip ? " Noclip" : "",
                 g_cheat.unlock ? " Locks" : "");
        pc_ui_text(PANEL_X0 + 16, y, t, 0.42f, 0xFFFFC868u); y += 14; mods++;
    }
    if (!mods) {
        pc_ui_text(PANEL_X0 + 16, y, "Classic rules - no active modifiers.", 0.42f, C_HELP);
        y += 14;
    }

    y += 12;
    if (y < 332.0f) {
        static const struct { int row; const char* what; } keys[] = {
            { B_SETTINGS, "Dossier" }, { B_CHEATS, "Cheats" }, { B_TPS_TOGGLE, "Camera" },
            { B_QUICKSAVE, "Quicksave" }, { B_QUICKLOAD, "Quickload" }
        };
        float x = PANEL_X0 + 16;
        pc_ui_text(PANEL_X0 + 12, y, "SHORTCUTS", 0.48f, C_GOLD);
        y += 18;
        for (int i = 0; i < (int)(sizeof(keys) / sizeof(keys[0])); i++) {
            char k[32];
            bind_code_name(BIND_KB, bind_get(keys[i].row, BIND_KB, 0), k, sizeof(k));
            if (x + ui_glyph_hint_w(k, keys[i].what) > PANEL_X1 - 8) { x = PANEL_X0 + 16; y += 20; }
            if (y > 372.0f) break;
            x = ui_glyph_hint(x, y, k, keys[i].what);
        }
    }
}

static void pause_draw(void)
{
    char t[96];
    pc_ui_rect(0, 0, 640, 480, 0xCC0C0B0Eu, 0.015f);
    pc_ui_text(PX - 2, 24, "PAUSED", 1.25f, C_PARCH);
    pc_ui_text(PX, 62, "ASHFORD'S HALBERD", 0.42f, C_GOLD);
    pc_ui_rect(PX - 8, 82, PANEL_X1, 83, 0x70D6B668u, 0.021f);
    if (pc_options_quit_up()) { AdvEasySetTextureList(0); return; }

    for (int i = 0; i < I_N; i++) {
        float y = item_y(i);
        int sel = i == pause_sel;
        unsigned int col = sel ? 0xFFFFFFFFu : C_ROW;
        if (sel) {
            pc_ui_rect(PX - 10, y - 3, PANEL_X0 - 20, y + PDY - 5, 0x38FFFFFFu, 0.30f);
            pc_ui_rect(PX - 10, y - 3, PX - 7, y + PDY - 5, C_GOLD, 0.31f);
        }
        pc_ui_text(PX, y, item_text(i, t, sizeof(t)), PSC, col);
        if (i != I_RESUME && i != I_TITLE)
            pc_ui_arrow(PANEL_X0 - 32, y + 10, 1, sel ? C_GOLD : col);
    }

    pc_ui_rect(PANEL_X0, 96, PANEL_X1, 396, 0x90100E12u, 0.020f);
    pc_ui_rect(PANEL_X0, 96, PANEL_X1, 97, 0x55D6B668u, 0.022f);
    draw_campaign_card(106.0f);

    pc_ui_rect(24, 408, 616, 409, 0x60D6B668u, 0.021f);
    pc_ui_text_wrap(24, 415, 592, item_help[pause_sel], 0.43f, C_HELP, 2);
    {
        int actions[3] = { UG_OK, UG_BACK, UG_CHANGE };
        const char* words[3] = { "Select", "Resume", "Cycle" };
        ui_glyph_bar(24, 452, actions, words, pause_sel == I_CAMERA ? 3 : 2);
    }
    AdvEasySetTextureList(0);
}

int pc_pause(int port, int flag)
{
    int next = 0;
    if (flag) {
        int q = pc_options_quit(port);
        if (q < 0) { AdvWork.OptFadeType = 0; return -1; }
        if (q) flag = 0;
    }
    if (flag) next = pause_input(port);
    else menu_mouse_take(&mt);
    pause_draw();
    pc_options_quit_draw();
    FlushPlate();
    return next;
}
