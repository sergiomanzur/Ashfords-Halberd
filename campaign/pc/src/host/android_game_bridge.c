/* Android arm64 runtime game-symbol bridge for libmain.so / test_window_load.
 *
 * All 16 data pointer slots, 8 static-table sentinel objects, and 45 forwarding
 * thunks carry hidden visibility so NONE of them appear in libmain.so's .dynsym.
 * librecvx_game.so (linked with -Bsymbolic) always binds game symbols to its own
 * fixed-window definitions in [0x30000000, 0x50000000).
 */
#if defined(__ANDROID__) && defined(ASHFORD_ANDROID_GAME_BRIDGE)
#define _GNU_SOURCE
#include "android_game_bridge.h"
#include "window_load.h"
#include "../ps2_platform.h"
#include <dlfcn.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "actors.h"
#include "biorand.h"
#include "cheat.h"
#include "cheat_room_snapshot.h"
#include "enemies.h"
#include "lang.h"
#include "room_snapshot.h"
#include "script_rules.h"
#include "settings.h"
#include "tps.h"
#include "bindings.h"
#include "../gs/stream.h"

/* ---------------------------------------------------------------- 16 data slots */
__attribute__((visibility("hidden"))) int* ashford_ptr_g_appear_chr;
__attribute__((visibility("hidden"))) CheatState* ashford_ptr_g_cheat;
__attribute__((visibility("hidden"))) EnemyOpts* ashford_ptr_g_enemy_opts;
__attribute__((visibility("hidden"))) int* ashford_ptr_g_lang;
__attribute__((visibility("hidden"))) int* ashford_ptr_g_subtitles;
__attribute__((visibility("hidden"))) int* ashford_ptr_g_lang_pictures;
__attribute__((visibility("hidden"))) int* ashford_ptr_g_lang_voices;
__attribute__((visibility("hidden"))) TpsConfig* ashford_ptr_g_tps;
__attribute__((visibility("hidden"))) TpsHud* ashford_ptr_g_hud;
__attribute__((visibility("hidden"))) int* ashford_ptr_tps_cam_active;
__attribute__((visibility("hidden"))) char* (*ashford_ptr_rdx_files)[205];
__attribute__((visibility("hidden"))) const unsigned char (*ashford_ptr_FontSz)[126];
__attribute__((visibility("hidden"))) unsigned int (*ashford_ptr_fstbl)[24];
__attribute__((visibility("hidden"))) unsigned int (*ashford_ptr_fsheader)[24];
__attribute__((visibility("hidden"))) unsigned int (*ashford_ptr_wallpaper)[24];
__attribute__((visibility("hidden"))) void* (*ashford_ptr_comevt_message_tab)[8];

/* Hidden sentinel storage referenced only by compile-time table initializers in
 * config.c (keys[]) and settings.c (g_settings[]), rebased in-place before use. */
__attribute__((visibility("hidden"))) int ashford_sent_g_appear_chr;
__attribute__((visibility("hidden"))) CheatState ashford_sent_g_cheat;
__attribute__((visibility("hidden"))) EnemyOpts ashford_sent_g_enemy_opts;
__attribute__((visibility("hidden"))) int ashford_sent_g_lang;
__attribute__((visibility("hidden"))) int ashford_sent_g_lang_pictures;
__attribute__((visibility("hidden"))) int ashford_sent_g_lang_voices;
__attribute__((visibility("hidden"))) int ashford_sent_g_subtitles;
__attribute__((visibility("hidden"))) TpsConfig ashford_sent_g_tps;

/* ---------------------------------------------------------------- 45 function pointers */
static void (*fn_appear_frame)(void);
static void (*fn_biorand_frame)(void);
static void (*fn_cheat_frame)(void);
static int  (*fn_cheat_room_snapshot_capture)(CheatRoomSnapshot* out);
static void (*fn_cheat_room_snapshot_restore)(const CheatRoomSnapshot* in);
static int  (*fn_cheat_room_snapshot_validate)(const CheatRoomSnapshot* in);
static const EnemyOpts* (*fn_enemy_opts)(void);
static int  (*fn_enemy_rate_at)(int k);
static int  (*fn_enemy_rate_count)(void);
static const char* (*fn_enemy_rate_name)(int i);
static void (*fn_enemy_rate_preset)(unsigned char* rate, int variety);
static void (*fn_enemy_rates_ini)(const char* text);
static void (*fn_enemy_rates_load)(void);
static const char* (*fn_enemy_rates_text)(void);
static void (*fn_glue_debug_frame)(uint64_t frame);
static const unsigned int* (*fn_hud_icons)(int* w, int* h);
static int  (*fn_hud_item_info)(int id, int* group, int* two_slot, int* counted, int* pct,
                                int* def_amt, int* min_amt, int* max_amt,
                                float* u0, float* v0, float* u1, float* v1);
static void (*fn_lang_frame)(void);
static int  (*fn_lang_has_subtitles)(int l);
static int  (*fn_lang_has_voices)(int l);
static int  (*fn_lang_installed)(int l);
static int  (*fn_lang_old_import)(int l);
static void (*fn_lang_scan)(void);
static int  (*fn_lang_sub)(void);
static int  (*fn_pc_frame_fade)(void);
static uint64_t (*fn_pc_frame_view)(float view[6], float cam[STREAM_CAM]);
static int  (*fn_pc_in_game)(void);
static int  (*fn_pc_insn_len_raw)(const uint8_t* p);
static int  (*fn_pc_keytype)(void);
static int  (*fn_pc_keytype_mask)(int vbit);
static int  (*fn_pc_lang_disc_parts)(const char* name, FILE* f, uint32_t size, uint32_t lsn,
                                     uint32_t dsize, struct PcPart* out, int max);
static const char* (*fn_pc_lang_file_dir)(const char* name);
static void (*fn_pc_lang_file_opened)(const char* name, int src);
static int  (*fn_pc_menu_smooth)(void);
static void (*fn_pc_rifle_stack)(int on);
static void (*fn_pc_tex_invalidate)(const void* dst, unsigned int bytes);
static void (*fn_pc_view_changed)(void);
static int  (*fn_recvx_game_main)(int argc, char* argv[]);
static int  (*fn_reload_combo_ok)(void);
static int  (*fn_room_snapshot_busy)(void);
static unsigned int (*fn_RoomNameSwitch)(unsigned int name);
static int  (*fn_spawn_count)(void);
static const char* (*fn_spawn_name)(int i);
static void (*fn_spoiler_key)(int k);
static void (*fn_tps_frame)(void);

/* ---------------------------------------------------------------- 45 hidden forwarding thunks */
void appear_frame(void) { fn_appear_frame(); }
void biorand_frame(void) { fn_biorand_frame(); }
void cheat_frame(void) { fn_cheat_frame(); }
int  cheat_room_snapshot_capture(CheatRoomSnapshot* out) { return fn_cheat_room_snapshot_capture(out); }
void cheat_room_snapshot_restore(const CheatRoomSnapshot* in) { fn_cheat_room_snapshot_restore(in); }
int  cheat_room_snapshot_validate(const CheatRoomSnapshot* in) { return fn_cheat_room_snapshot_validate(in); }
const EnemyOpts* enemy_opts(void) { return fn_enemy_opts(); }
int  enemy_rate_at(int k) { return fn_enemy_rate_at(k); }
int  enemy_rate_count(void) { return fn_enemy_rate_count(); }
const char* enemy_rate_name(int i) { return fn_enemy_rate_name(i); }
void enemy_rate_preset(unsigned char* rate, int variety) { fn_enemy_rate_preset(rate, variety); }
void enemy_rates_ini(const char* text) { fn_enemy_rates_ini(text); }
void enemy_rates_load(void) { fn_enemy_rates_load(); }
const char* enemy_rates_text(void) { return fn_enemy_rates_text(); }
void glue_debug_frame(uint64_t frame) { fn_glue_debug_frame(frame); }
const unsigned int* hud_icons(int* w, int* h) { return fn_hud_icons(w, h); }
int  hud_item_info(int id, int* group, int* two_slot, int* counted, int* pct,
                   int* def_amt, int* min_amt, int* max_amt,
                   float* u0, float* v0, float* u1, float* v1) {
    return fn_hud_item_info(id, group, two_slot, counted, pct, def_amt, min_amt, max_amt, u0, v0, u1, v1);
}
void lang_frame(void) { fn_lang_frame(); }
int  lang_has_subtitles(int l) { return fn_lang_has_subtitles(l); }
int  lang_has_voices(int l) { return fn_lang_has_voices(l); }
int  lang_installed(int l) { return fn_lang_installed(l); }
int  lang_old_import(int l) { return fn_lang_old_import(l); }
void lang_scan(void) { fn_lang_scan(); }
int  lang_sub(void) { return fn_lang_sub(); }
int  pc_frame_fade(void) { return fn_pc_frame_fade(); }
uint64_t pc_frame_view(float view[6], float cam[STREAM_CAM]) { return fn_pc_frame_view(view, cam); }
int  pc_in_game(void) { return fn_pc_in_game(); }
int  pc_insn_len_raw(const uint8_t* p) { return fn_pc_insn_len_raw(p); }
int  pc_keytype(void) { return fn_pc_keytype(); }
int  pc_keytype_mask(int vbit) { return fn_pc_keytype_mask(vbit); }
int  pc_lang_disc_parts(const char* name, FILE* f, uint32_t size, uint32_t lsn,
                        uint32_t dsize, struct PcPart* out, int max) {
    return fn_pc_lang_disc_parts(name, f, size, lsn, dsize, out, max);
}
const char* pc_lang_file_dir(const char* name) { return fn_pc_lang_file_dir(name); }
void pc_lang_file_opened(const char* name, int src) { fn_pc_lang_file_opened(name, src); }
int  pc_menu_smooth(void) { return fn_pc_menu_smooth(); }
void pc_rifle_stack(int on) { fn_pc_rifle_stack(on); }
void pc_tex_invalidate(const void* dst, unsigned int bytes) { fn_pc_tex_invalidate(dst, bytes); }
void pc_view_changed(void) { fn_pc_view_changed(); }
int  recvx_game_main(int argc, char* argv[]) { return fn_recvx_game_main(argc, argv); }
int  reload_combo_ok(void) { return fn_reload_combo_ok(); }
int  room_snapshot_busy(void) { return fn_room_snapshot_busy(); }
unsigned int RoomNameSwitch(unsigned int name) { return fn_RoomNameSwitch(name); }
int  spawn_count(void) { return fn_spawn_count(); }
const char* spawn_name(int i) { return fn_spawn_name(i); }
void spoiler_key(int k) { fn_spoiler_key(k); }
void tps_frame(void) { fn_tps_frame(); }

typedef struct {
    const char* name;
    void** slot;
} BridgeEntry;

static const BridgeEntry k_data_entries[] = {
    { "g_appear_chr",       (void**)&ashford_ptr_g_appear_chr },
    { "g_cheat",            (void**)&ashford_ptr_g_cheat },
    { "g_enemy_opts",       (void**)&ashford_ptr_g_enemy_opts },
    { "g_lang",             (void**)&ashford_ptr_g_lang },
    { "g_subtitles",        (void**)&ashford_ptr_g_subtitles },
    { "g_lang_pictures",    (void**)&ashford_ptr_g_lang_pictures },
    { "g_lang_voices",      (void**)&ashford_ptr_g_lang_voices },
    { "g_tps",              (void**)&ashford_ptr_g_tps },
    { "g_hud",              (void**)&ashford_ptr_g_hud },
    { "tps_cam_active",     (void**)&ashford_ptr_tps_cam_active },
    { "rdx_files",          (void**)&ashford_ptr_rdx_files },
    { "FontSz",             (void**)&ashford_ptr_FontSz },
    { "fstbl",              (void**)&ashford_ptr_fstbl },
    { "fsheader",           (void**)&ashford_ptr_fsheader },
    { "wallpaper",          (void**)&ashford_ptr_wallpaper },
    { "comevt_message_tab", (void**)&ashford_ptr_comevt_message_tab },
};

static const BridgeEntry k_func_entries[] = {
    { "appear_frame",                 (void**)&fn_appear_frame },
    { "biorand_frame",                (void**)&fn_biorand_frame },
    { "cheat_frame",                  (void**)&fn_cheat_frame },
    { "cheat_room_snapshot_capture",  (void**)&fn_cheat_room_snapshot_capture },
    { "cheat_room_snapshot_restore",  (void**)&fn_cheat_room_snapshot_restore },
    { "cheat_room_snapshot_validate", (void**)&fn_cheat_room_snapshot_validate },
    { "enemy_opts",                   (void**)&fn_enemy_opts },
    { "enemy_rate_at",                (void**)&fn_enemy_rate_at },
    { "enemy_rate_count",             (void**)&fn_enemy_rate_count },
    { "enemy_rate_name",              (void**)&fn_enemy_rate_name },
    { "enemy_rate_preset",            (void**)&fn_enemy_rate_preset },
    { "enemy_rates_ini",              (void**)&fn_enemy_rates_ini },
    { "enemy_rates_load",             (void**)&fn_enemy_rates_load },
    { "enemy_rates_text",             (void**)&fn_enemy_rates_text },
    { "glue_debug_frame",             (void**)&fn_glue_debug_frame },
    { "hud_icons",                    (void**)&fn_hud_icons },
    { "hud_item_info",                (void**)&fn_hud_item_info },
    { "lang_frame",                   (void**)&fn_lang_frame },
    { "lang_has_subtitles",           (void**)&fn_lang_has_subtitles },
    { "lang_has_voices",              (void**)&fn_lang_has_voices },
    { "lang_installed",               (void**)&fn_lang_installed },
    { "lang_old_import",              (void**)&fn_lang_old_import },
    { "lang_scan",                    (void**)&fn_lang_scan },
    { "lang_sub",                     (void**)&fn_lang_sub },
    { "pc_frame_fade",                (void**)&fn_pc_frame_fade },
    { "pc_frame_view",                (void**)&fn_pc_frame_view },
    { "pc_in_game",                   (void**)&fn_pc_in_game },
    { "pc_insn_len_raw",              (void**)&fn_pc_insn_len_raw },
    { "pc_keytype",                   (void**)&fn_pc_keytype },
    { "pc_keytype_mask",              (void**)&fn_pc_keytype_mask },
    { "pc_lang_disc_parts",           (void**)&fn_pc_lang_disc_parts },
    { "pc_lang_file_dir",             (void**)&fn_pc_lang_file_dir },
    { "pc_lang_file_opened",          (void**)&fn_pc_lang_file_opened },
    { "pc_menu_smooth",               (void**)&fn_pc_menu_smooth },
    { "pc_rifle_stack",               (void**)&fn_pc_rifle_stack },
    { "pc_tex_invalidate",            (void**)&fn_pc_tex_invalidate },
    { "pc_view_changed",              (void**)&fn_pc_view_changed },
    { "recvx_game_main",              (void**)&fn_recvx_game_main },
    { "reload_combo_ok",              (void**)&fn_reload_combo_ok },
    { "room_snapshot_busy",           (void**)&fn_room_snapshot_busy },
    { "RoomNameSwitch",               (void**)&fn_RoomNameSwitch },
    { "spawn_count",                  (void**)&fn_spawn_count },
    { "spawn_name",                   (void**)&fn_spawn_name },
    { "spoiler_key",                  (void**)&fn_spoiler_key },
    { "tps_frame",                    (void**)&fn_tps_frame },
};

typedef struct {
    const void* sentinel_base;
    size_t sentinel_size;
    void* const* live_ptr;
} SentinelMap;

static const SentinelMap k_sentinels[] = {
    { &ashford_sent_g_appear_chr,    sizeof(ashford_sent_g_appear_chr),    (void* const*)&ashford_ptr_g_appear_chr },
    { &ashford_sent_g_cheat,         sizeof(ashford_sent_g_cheat),         (void* const*)&ashford_ptr_g_cheat },
    { &ashford_sent_g_enemy_opts,    sizeof(ashford_sent_g_enemy_opts),    (void* const*)&ashford_ptr_g_enemy_opts },
    { &ashford_sent_g_lang,          sizeof(ashford_sent_g_lang),          (void* const*)&ashford_ptr_g_lang },
    { &ashford_sent_g_lang_pictures, sizeof(ashford_sent_g_lang_pictures), (void* const*)&ashford_ptr_g_lang_pictures },
    { &ashford_sent_g_lang_voices,   sizeof(ashford_sent_g_lang_voices),   (void* const*)&ashford_ptr_g_lang_voices },
    { &ashford_sent_g_subtitles,     sizeof(ashford_sent_g_subtitles),     (void* const*)&ashford_ptr_g_subtitles },
    { &ashford_sent_g_tps,           sizeof(ashford_sent_g_tps),           (void* const*)&ashford_ptr_g_tps },
};

static int g_bridge_ready;

void* ashford_game_bridge_rebase_ptr(void* p)
{
    if (!p)
        return NULL;
    uintptr_t addr = (uintptr_t)p;
    for (size_t i = 0; i < sizeof(k_sentinels) / sizeof(k_sentinels[0]); i++) {
        uintptr_t lo = (uintptr_t)k_sentinels[i].sentinel_base;
        uintptr_t hi = lo + k_sentinels[i].sentinel_size;
        if (addr >= lo && addr < hi) {
            void* live = *k_sentinels[i].live_ptr;
            if (!live)
                host_fatal("game_bridge: sentinel %zu rebased before live symbol resolution", i);
            return (void*)((uintptr_t)live + (addr - lo));
        }
    }
    return p;
}

void ashford_game_bridge_init(void* game_handle)
{
    if (g_bridge_ready)
        return;
    if (!game_handle)
        host_fatal("game_bridge: null librecvx_game.so handle");

    for (size_t i = 0; i < sizeof(k_data_entries) / sizeof(k_data_entries[0]); i++) {
        void* sym = dlsym(game_handle, k_data_entries[i].name);
        if (!sym) {
            host_fatal("game_bridge: failed to resolve data symbol '%s': %s",
                       k_data_entries[i].name, dlerror());
        }
        uintptr_t a = (uintptr_t)sym;
        if (a < ASHFORD_WINDOW_BASE || a >= ASHFORD_WINDOW_LIMIT) {
            host_fatal("game_bridge: data symbol '%s' at 0x%lx outside window [0x%08X, 0x%08X)",
                       k_data_entries[i].name, (unsigned long)a,
                       ASHFORD_WINDOW_BASE, ASHFORD_WINDOW_LIMIT);
        }
        *k_data_entries[i].slot = sym;
    }

    for (size_t i = 0; i < sizeof(k_func_entries) / sizeof(k_func_entries[0]); i++) {
        void* sym = dlsym(game_handle, k_func_entries[i].name);
        if (!sym) {
            host_fatal("game_bridge: failed to resolve function symbol '%s': %s",
                       k_func_entries[i].name, dlerror());
        }
        uintptr_t a = (uintptr_t)sym;
        if (a < ASHFORD_CODE_BASE || a >= ASHFORD_CODE_LIMIT) {
            host_fatal("game_bridge: function symbol '%s' at 0x%lx outside code range [0x%08X, 0x%08X)",
                       k_func_entries[i].name, (unsigned long)a,
                       ASHFORD_CODE_BASE, ASHFORD_CODE_LIMIT);
        }
        *k_func_entries[i].slot = sym;
    }

    config_android_bridge_rebase(ashford_game_bridge_rebase_ptr);
    settings_android_bridge_rebase(ashford_game_bridge_rebase_ptr);
    g_bridge_ready = 1;
}

int ashford_game_bridge_ready(void)
{
    return g_bridge_ready;
}

void ashford_game_bridge_audit(AshfordBridgeAudit* out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    out->ready = g_bridge_ready;
    out->data_count = (int)(sizeof(k_data_entries) / sizeof(k_data_entries[0]));
    out->func_count = (int)(sizeof(k_func_entries) / sizeof(k_func_entries[0]));
    for (size_t i = 0; i < sizeof(k_data_entries) / sizeof(k_data_entries[0]); i++) {
        uintptr_t a = (uintptr_t)(*k_data_entries[i].slot);
        if (a >= ASHFORD_WINDOW_BASE && a < ASHFORD_WINDOW_LIMIT)
            out->data_in_window++;
    }
    for (size_t i = 0; i < sizeof(k_func_entries) / sizeof(k_func_entries[0]); i++) {
        uintptr_t a = (uintptr_t)(*k_func_entries[i].slot);
        if (a >= ASHFORD_CODE_BASE && a < ASHFORD_CODE_LIMIT)
            out->func_in_code++;
    }
}

int ashford_game_bridge_self_test(void* game_handle)
{
    if (!g_bridge_ready || !game_handle)
        return 1;

    TpsConfig* raw_tps = (TpsConfig*)dlsym(game_handle, "g_tps");
    EnemyOpts* raw_eo = (EnemyOpts*)dlsym(game_handle, "g_enemy_opts");
    CheatState* raw_cheat = (CheatState*)dlsym(game_handle, "g_cheat");
    if (!raw_tps || !raw_eo || !raw_cheat)
        return 2;
    if (&g_tps != raw_tps || &g_enemy_opts != raw_eo || &g_cheat != raw_cheat)
        return 3;

    int saved_fov = g_tps.fov;
    g_tps.fov = 93;
    int alias_ok = (raw_tps->fov == 93);
    g_tps.fov = saved_fov;
    if (!alias_ok)
        return 4;

    if (enemy_opts() != raw_eo) return 5;
    if (spawn_count() <= 0) return 6;
    const char* sn = spawn_name(0);
    if (!sn || !sn[0]) return 7;
    lang_scan();
    if (lang_installed(LANG_EN) != 1) return 8;
    if (pc_in_game() != 0) return 9;
    if (room_snapshot_busy() != 0) return 10;
    if (RoomNameSwitch(0) != 0) return 11;

    int checked_variety = 0, checked_fov = 0, checked_inf_hp = 0;
    for (int i = 0; i < g_settings_n; i++) {
        if (!strcmp(g_settings[i].label, "Randomizer variety")) {
            if (g_settings[i].val != &raw_eo->variety) return 12;
            checked_variety = 1;
        } else if (!strcmp(g_settings[i].label, "Field of view")) {
            if (g_settings[i].val != &raw_tps->fov) return 13;
            checked_fov = 1;
        } else if (!strcmp(g_settings[i].label, "Infinite health")) {
            if (g_settings[i].val != (int*)&raw_cheat->inf_health) return 14;
            checked_inf_hp = 1;
        }
    }
    if (!checked_variety || !checked_fov || !checked_inf_hp)
        return 15;
    return 0;
}
#endif /* defined(__ANDROID__) && defined(ASHFORD_ANDROID_GAME_BRIDGE) */
