/*
 * F1 overlay (Dear ImGui): display, audio and port settings. While it is open the game
 * keeps running but receives no pad input. Settings are applied immediately and saved
 * to <save_dir>/recvx.ini.
 */
#include "../ps2_platform.h"
#include "ashford_panel.h"
extern "C" {
#include "../audio/audio.h"
}
#include "prof.h"
#include "cheat.h"
#include "actors.h"
#include "cast.h"
#include "../gs/stream.h"
#include "../gs/gs.h"
#include "../render/gl_loader.h"
#include "tps.h"
#include "menu_mouse.h"
#include "bindings.h"
#include "settings.h"
#include "enemies.h"
#include "quicksave.h"
#include "lang.h"
#include "../import/import.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <ctype.h>
#include <float.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

extern "C" void pad_set_game_input_enabled(int on);
extern "C" float gs_hor_scale(void);

static SDL_Window* win;
static bool ready, visible;
static GLuint hud_tex;
static int hud_tw, hud_th;

/* 10-second Keep/Revert confirmation when changing Display mode or Fullscreen resolution. */
#define OVERLAY_KEEP_MS 10000
static int keep_idx = -1, keep_old = 0;
static uint64_t keep_t = 0;

static bool is_display_setting(const Setting* s)
{
    return s && (!strcmp(s->label, "Display mode") || !strcmp(s->label, "Fullscreen resolution"));
}

static void maybe_start_keep(const Setting* s, int idx, int old_val)
{
    if (is_display_setting(s) && settings_get(s) != old_val) {
        keep_idx = idx;
        keep_old = old_val;
        keep_t = SDL_GetTicks();
    }
}

static void hud_upload_icons(void)
{
    if (hud_tex) return;
    const unsigned int* px = hud_icons(&hud_tw, &hud_th);
    if (!px) return;
    glGenTextures(1, &hud_tex);
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, hud_tw, hud_th, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void overlay_init(void* window, void* gl_context)
{
    win = (SDL_Window*)window;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;   /* our settings live in recvx.ini */
    ashford::panel_style();
    ImGui_ImplSDL3_InitForOpenGL(win, (SDL_GLContext)gl_context);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    ready = true;
    const char* show = SDL_getenv("RECVX_SHOW_OVERLAY");
    visible = show && strcmp(show, "0");
    if (show && !strcmp(show, "2")) ashford::panel_select_page(SP_CHEATS);
}

int overlay_event(const void* e)
{
    const SDL_Event* ev = (const SDL_Event*)e;
    if (!ready) return 0;
    if (bind_hotkey(ev, B_SETTINGS)) {
        visible = !visible;
        return 1;
    }
    if (bind_hotkey(ev, B_CHEATS)) {
        visible = true;
        ashford::panel_select_page(SP_CHEATS);
        return 1;
    }
    ImGui_ImplSDL3_ProcessEvent(ev);
    if (!visible && keep_idx < 0) return 0;
    return (ev->type == SDL_EVENT_KEY_DOWN || ev->type == SDL_EVENT_KEY_UP || ev->type == SDL_EVENT_TEXT_INPUT ||
            ev->type == SDL_EVENT_MOUSE_BUTTON_DOWN || ev->type == SDL_EVENT_MOUSE_BUTTON_UP || ev->type == SDL_EVENT_MOUSE_WHEEL);
}

/* Every shared setting is rendered in F1 and the native option menu. */
/* the picture calibration test pattern (settings_calib), in the colours the game's picture gets
   (the overlay itself is drawn without the calibration) */
static void calib_pattern(void)
{
    float sq = ImGui::GetFontSize() * 1.5f, gap = sq * 0.5f, w = CALIB_STEPS * (sq + gap) + gap;
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    auto grey = [](int v) { int g = (int)(gs_picture(v / 255.0f) * 255.0f + 0.5f); return IM_COL32(g, g, g, 255); };
    for (int k = 0; k < 2; k++) {
        float x = p.x + k * (w + gap);
        dl->AddRectFilled(ImVec2(x, p.y), ImVec2(x + w, p.y + sq + 2 * gap), grey(k ? 255 : 0));
        for (int i = 0; i < CALIB_STEPS; i++)
            dl->AddRectFilled(ImVec2(x + gap + i * (sq + gap), p.y + gap), ImVec2(x + gap + i * (sq + gap) + sq, p.y + gap + sq), grey(settings_calib[k][i]));
    }
    ImGui::Dummy(ImVec2(2 * w + gap, sq + 2 * gap));
}

/* Cutscene cast / enemy custom model checklist (cast.h): shown below Cast model pool or Cutscene enemy models when Custom. */
static bool cast_custom_imgui(int list)
{
    static short pos[CAST_CUSTOM_MAX];
    static int mdl[CAST_CUSTOM_MAX];
    unsigned char* off = list == 2 ? g_enemy_opts.cast_enemy_off : cast_off;
    int imp = !list && g_enemy_opts.cast_imports ? __atomic_load_n(&actor_import_n, __ATOMIC_ACQUIRE) : 0;
    int n = cast_custom_list(list, imp, pos, mdl, CAST_CUSTOM_MAX), on = 0, set = -1;
    bool changed = false;
    char head[96];
    for (int k = 0; k < n; k++) on += !cast_off_at(off, pos[k]);
    SDL_snprintf(head, sizeof head, list == 2 ? "Custom cutscene enemies (%d of %d checked)###castenemies" : "Custom cast models (%d of %d checked)###castcustom", on, n);
    if (!ImGui::TreeNodeEx(head, ImGuiTreeNodeFlags_DefaultOpen)) return false;
    ImGui::PushID(list);
    if (ImGui::Button("Select all")) set = 0;
    ImGui::SameLine();
    if (ImGui::Button("Deselect all")) set = 1;
    if (set >= 0) { for (int k = 0; k < n; k++) cast_off_put(off, pos[k], set); changed = true; }
    if (!on) { ImGui::SameLine(); ImGui::TextDisabled("%s", list == 2 ? "None checked: monsters keep their own looks." : "None checked: characters keep their own looks."); }
    if (ImGui::BeginChild("##castcustom", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * (n < 10 ? n + 1 : 10)), ImGuiChildFlags_Borders)) {
        const char* grp = nullptr;
        for (int k = 0; k < n; k++) {
            const char* g = cast_entry_group(mdl[k]);
            char label[128];
            bool b = !cast_off_at(off, pos[k]);
            if (!grp || strcmp(g, grp)) ImGui::SeparatorText(g);
            grp = g;
            SDL_snprintf(label, sizeof label, "%s##cc%d", cast_entry_name(mdl[k]), k);
            if (ImGui::Checkbox(label, &b)) { cast_off_put(off, pos[k], !b); changed = true; }
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
    ImGui::TreePop();
    return changed;
}

/* Custom enemy randomizer rates (enemies.h): shown below Randomizer variety when set to Custom. */
static bool enemy_rates_imgui(void)
{
    bool changed = false;
    int total = 0;
    for (int k = 0; k < enemy_rate_count(); k++) total += g_enemy_opts.rate[enemy_rate_at(k)];
    if (!ImGui::TreeNodeEx("Enemy rates###enemyrates", ImGuiTreeNodeFlags_DefaultOpen)) return false;
    for (int k = 0; k < enemy_rate_count(); k++) {
        int i = enemy_rate_at(k), v = g_enemy_opts.rate[i];
        char label[96], fmt[32];
        SDL_snprintf(label, sizeof label, "%s##er%d", enemy_rate_name(i), k);
        if (v) SDL_snprintf(fmt, sizeof fmt, "%%d (%d%%%%)", total ? (v * 100 + total / 2) / total : 0);
        else SDL_snprintf(fmt, sizeof fmt, "Off (0%%%%)");
        if (ImGui::SliderInt(label, &v, 0, ENEMY_RATE_MAX, fmt)) { g_enemy_opts.rate[i] = (unsigned char)v; changed = true; }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Relative weight for this enemy type. Some rooms can only hold certain enemy types.");
    }
    ImGui::TreePop();
    return changed;
}

/* Visual Give Item picker in Field Dossier > Cheats (shares the HUD RGBA icon atlas). */
static void visual_give_imgui(void)
{
    static int cat = 0;
    static char filter[64] = "";
    static const char* const cats[] = { "All", "Weapons", "Ammo", "Healing", "Other", "Key" };
    if (!ImGui::TreeNodeEx("Visual item picker###visual_give", ImGuiTreeNodeFlags_DefaultOpen)) return;
    if (!pc_in_game() || !g_cheat.names[1][0]) {
        ImGui::TextDisabled("Enter a game to browse items with real icons and add them immediately.");
        ImGui::TreePop();
        return;
    }
    hud_upload_icons();
    for (int i = 0; i < 6; i++) {
        if (i > 0) ImGui::SameLine();
        bool active = (cat == i);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.43f, 0.21f, 0.85f));
        if (ImGui::SmallButton(cats[i])) cat = i;
        if (active) ImGui::PopStyleColor();
    }
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##give_filter", "Filter items by name...", filter, sizeof(filter));

    auto matches = [](const char* name, const char* q) -> bool {
        if (!q || !q[0]) return true;
        for (const char* p = name; *p; p++) {
            const char *a = p, *b = q;
            while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) { a++; b++; }
            if (!*b) return true;
        }
        return false;
    };

    if (ImGui::BeginChild("##visual_give_list", ImVec2(0, 220), ImGuiChildFlags_Borders)) {
        for (int id = 1; id < CHEAT_ITEMS; id++) {
            int grp = 0, two = 0, cnt = 0, is_pct = 0, def_amt = 1, min_amt = 1, max_amt = 1;
            float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
            if (!hud_item_info(id, &grp, &two, &cnt, &is_pct, &def_amt, &min_amt, &max_amt, &u0, &v0, &u1, &v1)) continue;
            if (cat > 0 && grp != cat - 1) continue;
            if (!matches(g_cheat.names[id], filter)) continue;
            bool sel = (g_cheat.give_id == id);
            ImGui::PushID(id);
            float row_h = 28.0f;
            ImVec2 pos = ImGui::GetCursorPos();
            if (ImGui::Selectable("##pick", sel, ImGuiSelectableFlags_AllowOverlap, ImVec2(0, row_h))) {
                g_cheat.give_id = id;
                g_cheat.give_count = def_amt;
            }
            ImGui::SetCursorPos(ImVec2(pos.x + 4, pos.y + 2));
            if (hud_tex) {
                float iw = two ? 62.0f : 31.0f, ih = 22.0f;
                ImGui::Image((ImTextureID)(intptr_t)hud_tex, ImVec2(iw, ih), ImVec2(u0, v0), ImVec2(u1, v1));
                ImGui::SameLine(74.0f);
            }
            ImGui::AlignTextToFramePadding();
            ImGui::Text("%s", g_cheat.names[id]);
            ImGui::SameLine();
            ImGui::TextDisabled("#%d", id);
            ImGui::PopID();
        }
    }
    ImGui::EndChild();

    int cur = g_cheat.give_id;
    int grp = 0, two = 0, cnt = 0, is_pct = 0, def_amt = 1, min_amt = 1, max_amt = 1;
    float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
    if (hud_item_info(cur, &grp, &two, &cnt, &is_pct, &def_amt, &min_amt, &max_amt, &u0, &v0, &u1, &v1)) {
        if (hud_tex) {
            ImGui::Image((ImTextureID)(intptr_t)hud_tex, ImVec2(two ? 92.0f : 46.0f, 32.0f), ImVec2(u0, v0), ImVec2(u1, v1));
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::TextUnformatted(g_cheat.names[cur]);
        if (cnt) {
            if (g_cheat.give_count < min_amt) g_cheat.give_count = min_amt;
            if (g_cheat.give_count > max_amt) g_cheat.give_count = max_amt;
            ImGui::SetNextItemWidth(130.0f);
            ImGui::SliderInt("Qty##vgqty", &g_cheat.give_count, min_amt, max_amt, is_pct ? "%d" : "%d");
            ImGui::SameLine();
        } else {
            g_cheat.give_count = 1;
        }
        if (ImGui::Button("Add now##vgadd"))
            __atomic_or_fetch(&g_cheat.request, CHEAT_GIVE, __ATOMIC_ACQ_REL);
        ImGui::EndGroup();
        ImGui::TextDisabled("Adds to inventory immediately (falls back to item box if full).");
    }
    ImGui::TreePop();
}

static bool settings_imgui(int page)
{
    bool changed = false;
    for (int i = 0; i < g_settings_n; i++) {
        const Setting* s = &g_settings[i];
        if (s->page != page || (s->shown && !s->shown())) continue;
        if (!ashford::panel_matches(s->label, s->help, s->group)) continue;
        if (s->group) {
            ImGui::SeparatorText(s->group);
            if (page == SP_CHEATS && !strcmp(s->group, "ITEMS")) visual_give_imgui();
        }
        const char* lock = settings_locked(s);
        char label[96], val[96];
        int v = settings_get(s), hi = settings_hi(s), st = s->step > 0 ? s->step : 1;
        SDL_snprintf(label, sizeof label, "%s##set%d", s->label, i);
        ImGui::BeginDisabled(lock != nullptr);
        if (s->flags & SET_ACTION) {
            if (ImGui::Button(label)) settings_act(s);
        } else if (settings_is_bool(s)) {
            bool b = v != 0;
            if (ImGui::Checkbox(label, &b)) { settings_set(s, b); changed = true; }
        } else if (s->names || (s->name && (s->count || (hi - s->lo) / st <= 10))) {   /* a list (tenths: a slider) */
            if (ImGui::BeginCombo(label, settings_value(s, val, sizeof val), ImGuiComboFlags_HeightLarge)) {
                for (int k = s->lo; k <= hi; k += st) {
                    char nm[96];
                    if (ImGui::Selectable(s->name ? s->name(k, nm, sizeof nm) : s->names[k - s->lo], k == v)) {
                        int old_v = v;
                        settings_set(s, k);
                        maybe_start_keep(s, i, old_v);
                        changed = true;
                    }
                    if (k == v) ImGui::SetItemDefaultFocus();   /* the list opens at the current value (a long one: "You appear as") */
                }
                ImGui::EndCombo();
            }
            if ((s->val == &g_enemy_opts.variety && settings_get(s) == 2) ||
                (s->val == &g_enemy_opts.cast_all && settings_get(s) == 2) ||
                (s->val == &g_enemy_opts.cast_enemy && settings_get(s) == CAST_ENEMY_CUSTOM)) {
                if ((ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) && (s->help || lock)) {
                    ImGui::BeginTooltip();
                    ImGui::Text("%s%s%s", lock ? lock : "", lock && s->help ? "\n" : "", s->help ? s->help : "");
                    ImGui::EndTooltip();
                }
                ImGui::EndDisabled();
                if (s->flags & (SET_RESTART | SET_NEXT_ROOM)) {
                    ImGui::SameLine();
                    if (s->flags & SET_RESTART) ImGui::TextColored(ImVec4(0.95f, 0.62f, 0.28f, 1.0f), "[Restart]");
                    else ImGui::TextColored(ImVec4(0.84f, 0.71f, 0.41f, 1.0f), "[Next room]");
                }
                if (s->val == &g_enemy_opts.variety) { if (enemy_rates_imgui()) changed = true; }
                else if (cast_custom_imgui(s->val == &g_enemy_opts.cast_enemy ? 2 : 0)) changed = true;
                continue;
            }
        } else {   /* a number; a value name (tenths) is shown as the slider's text */
            char fmt[96];
            const char* f = s->fmt ? s->fmt : "%d";
            if (s->name) {
                char* o = fmt;
                for (const char* c = s->name(v, val, sizeof val); *c && o < fmt + sizeof fmt - 2; c++) { if (*c == '%') *o++ = '%'; *o++ = *c; }
                *o = 0;
                f = fmt;
            }
            if (ImGui::SliderInt(label, &v, s->lo, hi, f)) {
                settings_set(s, st > 1 ? (v + st / 2) / st * st : v);
                changed = true;
            }
        }
        ImGui::EndDisabled();
        if (((ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) || ImGui::IsItemFocused()) || ((s->flags & SET_CALIB) && ImGui::IsItemActive())) && (s->help || lock)) {
            ImGui::BeginTooltip();
            ImGui::Text("%s%s%s", lock ? lock : "", lock && s->help ? "\n" : "", s->help ? s->help : "");
            if (s->flags & SET_CALIB) calib_pattern();
            ImGui::EndTooltip();
        }
        if (s->flags & (SET_RESTART | SET_NEXT_ROOM)) {
            ImGui::SameLine();
            if (s->flags & SET_RESTART) {
                ImGui::TextColored(ImVec4(0.95f, 0.62f, 0.28f, 1.0f), "[Restart]");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Takes effect on next launch.");
            } else {
                ImGui::TextColored(ImVec4(0.84f, 0.71f, 0.41f, 1.0f), "[Next room]");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Applies when entering the next room.");
            }
        }
    }
    return changed;
}

/* Controls: the bindings of both sets; a click on a binding waits for a key (bindings.c) */
static void controls_section(void)
{
    static int dev, cap_row = -1, cap_slot;
    static char msg[128];
    char label[128], nm[64];
    if (cap_row >= 0) {
        int c = bind_capture_poll();
        if (c != -1) {
            if (c != -2) {
                bind_set(cap_row, dev, cap_slot, c == -3 ? 0 : c);
                if (bind_swapped()) SDL_snprintf(msg, sizeof msg, "Swapped with %s", bind_swapped());
                else if (bind_combo(cap_row, dev, cap_slot)) SDL_snprintf(msg, sizeof msg, "Shared: hold aim and press it to reload");
                else msg[0] = 0;
                settings_save_later();
            }
            cap_row = -1;
        }
    }
    settings_imgui(SP_CONTROLS);   /* input source, which gamepad */
    if (ImGui::BeginTabBar("##dev")) {
        if (ImGui::BeginTabItem("Keyboard / mouse")) { if (dev != BIND_KB) cap_row = -1; dev = BIND_KB; ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Gamepad")) { if (dev != BIND_PAD) cap_row = -1; dev = BIND_PAD; ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    char pad_name[96];
    if (dev == BIND_PAD) ImGui::TextDisabled("%s; the left stick also moves, as on the DS2. The third-person rows play in third\n"
                                             "person with Modern gamepad controls, instead of the game rows of the same buttons.", bind_pad_desc(pad_name, sizeof pad_name));
    ImGui::TextDisabled("Click a binding, then press the key/button (Esc cancels, Delete clears); right click clears.\n"
                        "Reload can share a button: then Aim + button reloads.");
    if (dev == BIND_KB) ImGui::TextDisabled("Mouse buttons work on any row, also with the fixed cameras.");
    if (msg[0]) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "%s", msg);
    if (ImGui::BeginTable("##binds", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("action", ImGuiTableColumnFlags_WidthStretch, 2.2f);
        ImGui::TableSetupColumn("1", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("2", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        int group = -1;
        for (int i = 0; i < B_COUNT; i++) {
            int r = bind_order[i];
            if (!bind_row_shown(r, dev) || !ashford::panel_matches(bind_row_label(r, label, sizeof label))) continue;
            if (bind_rows[r].group != group) {
                group = bind_rows[r].group;
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (group == BG_GAME) ImGui::SeparatorText("Game (as the KEY CONFIG type sets them)");
                else ImGui::SeparatorText(group == BG_TPS ? (dev ? "Third person (gamepad, Modern)" : "Third person (keyboard and mouse)") : "Hotkeys");
                if (group == BG_GAME) { ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("type %c", 'A' + pc_keytype()); }
            }
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            int conflict = bind_conflict(r, dev);
            if (conflict) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", bind_row_label(r, label, sizeof label));
            else ImGui::TextUnformatted(bind_row_label(r, label, sizeof label));
            if (conflict && ImGui::IsItemHovered()) ImGui::SetTooltip("Also bound to: %s", bind_rows[conflict - 1].name);
            for (int k = 0; k < BIND_SLOTS; k++) {
                ImGui::TableSetColumnIndex(1 + k);
                char b[96];
                if (cap_row == r && cap_slot == k) SDL_snprintf(b, sizeof b, "press... %ds##%d_%d", (bind_capture_left_ms() + 999) / 1000, r, k);
                else SDL_snprintf(b, sizeof b, "%s%s##%d_%d", bind_combo(r, dev, k) ? "Aim + " : "", bind_code_name(dev, bind_get(r, dev, k), nm, sizeof nm), r, k);
                if (ImGui::Button(b, ImVec2(-FLT_MIN, 0))) { cap_row = r; cap_slot = k; msg[0] = 0; bind_capture_start(dev); }
                if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) { bind_set(r, dev, k, 0); settings_save_later(); }
            }
        }
        ImGui::EndTable();
    }
    if (ImGui::Button(dev ? "Reset gamepad to defaults" : "Reset keyboard / mouse to defaults")) { bind_reset(dev); settings_save_later(); msg[0] = 0; }
    ImGui::TextDisabled("Also in the game's OPTION menu: ENHANCED OPTIONS -> CONTROLS.");
}

static char import_picked_path[1024];
static SDL_AtomicInt import_picked_src = { -1 };
static int import_active_src = -1;

static void SDLCALL import_iso_picked(void* ud, const char* const* files, int filter)
{
    (void)filter;
    if (files && files[0]) {
        SDL_strlcpy(import_picked_path, files[0], sizeof(import_picked_path));
        SDL_SetAtomicInt(&import_picked_src, (int)(intptr_t)ud);
    }
}

static void import_poll_picker(void)
{
    int ps = SDL_GetAtomicInt(&import_picked_src);
    if (ps >= 0 && ps < IMPORT_SOURCES) {
        SDL_strlcpy(g_import.iso[ps], import_picked_path, sizeof(g_import.iso[ps]));
        SDL_SetAtomicInt(&import_picked_src, -1);
        settings_save_later();
    }
}

/* Offline Outbreak File #2 character importer inside Field Dossier > Appearance. */
static void outbreak_import_section(void)
{
    static ImportItem* items;
    static int item_n, loaded, was_running;
    static int what = IMPORT_PLAYERS;
    static char when[64];
    const int s = IMPORT_OUTBREAK2;
    const ImportSource* src = &import_sources[s];
    if (!ashford::panel_matches("Outbreak File #2 character import", "SLUS-20984", "IMPORTED CHARACTERS")) return;
    import_poll_picker();
    if (!items && !(items = (ImportItem*)calloc(1024, sizeof(*items)))) return;
    ImportProgress* pg = &g_import_progress;
    int running = __atomic_load_n(&pg->running, __ATOMIC_ACQUIRE);
    if (!loaded || (was_running && !running)) {
        item_n = import_results(s, items, 1024, when, sizeof(when));
        if (loaded) import_registry_load();
        loaded = 1;
    }
    was_running = running;
    ImGui::SeparatorText("IMPORTED CHARACTERS");
    char head[128], dir[1100];
    int imp_n = __atomic_load_n(&actor_import_n, __ATOMIC_ACQUIRE);
    SDL_snprintf(head, sizeof head, "%s (%s) - %d loaded###imp_ob2", src->title, src->serial, imp_n);
    if (!ImGui::CollapsingHeader(head, imp_n > 0 ? 0 : ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::PushID(s);
    ImGui::TextWrapped("Extract playable character models from your own US Resident Evil Outbreak File #2 disc image (SLUS-20984). Extracted .cvxm models appear in \"You appear as\" and the Cutscene Cast randomizer.");
    ImGui::TextWrapped("Disc image: %s", g_import.iso[s][0] ? g_import.iso[s] : "(none chosen)");
    import_source_dir(s, dir, sizeof(dir));
    ImGui::TextDisabled("Output: %s", dir);
    ImGui::BeginDisabled(running != 0);
    if (ImGui::Button("Choose Disc Image...")) {
        static const SDL_DialogFileFilter filters[] = { { "PS2 disc image (*.iso)", "iso" }, { "All files", "*" } };
        SDL_ShowOpenFileDialog(import_iso_picked, (void*)(intptr_t)s, win, filters, 2, g_import.iso[s][0] ? g_import.iso[s] : nullptr, false);
    }
    what &= IMPORT_OFFERED;
    ImGui::CheckboxFlags("Playable Characters", &what, IMPORT_PLAYERS);
    ImGui::SameLine();
    ImGui::CheckboxFlags("Other Characters (NPCs)", &what, IMPORT_NPCS);
    ImGui::BeginDisabled(!g_import.iso[s][0] || !what);
    if (ImGui::Button("Import Characters")) {
        import_active_src = s;
        import_start(s, what);
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (import_active_src == s && (running || pg->total || pg->phase[0])) {
        char ov[64];
        SDL_snprintf(ov, sizeof(ov), "%d / %d", pg->done, pg->total);
        ImGui::ProgressBar(pg->total ? (float)pg->done / pg->total : 0.0f, ImVec2(-1, 0), ov);
        if (pg->phase[0]) ImGui::TextWrapped("%s", pg->phase);
    }
    int ok = 0, players = 0, failed = 0;
    for (int i = 0; i < item_n; i++) {
        if (items[i].group < 0 || !(IMPORT_OFFERED & (1 << items[i].group))) continue;
        ok += items[i].ok; players += items[i].player; failed += !items[i].ok;
    }
    if (!item_n) {
        ImGui::TextDisabled("Nothing extracted yet.");
    } else {
        ImGui::Text("Extracted %s: %d models, %d usable as player models, %d failed", when, ok, players, failed);
        if (ImGui::TreeNode("Extracted models###ob2_manifest")) {
            if (ImGui::BeginTable("##ob2_items", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV,
                                  ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 10))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Character", ImGuiTableColumnFlags_WidthStretch, 1.4f);
                ImGui::TableSetupColumn("Variant", ImGuiTableColumnFlags_WidthStretch, 1.0f);
                ImGui::TableSetupColumn("File", ImGuiTableColumnFlags_WidthStretch, 0.7f);
                ImGui::TableSetupColumn("Playable", ImGuiTableColumnFlags_WidthStretch, 0.5f);
                ImGui::TableSetupColumn("Notes", ImGuiTableColumnFlags_WidthStretch, 1.6f);
                ImGui::TableHeadersRow();
                for (int i = 0; i < item_n; i++) {
                    const ImportItem* x = &items[i];
                    if (x->group < 0 || !(IMPORT_OFFERED & (1 << x->group))) continue;
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(x->name);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(x->variant);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(x->id);
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(x->player ? "Yes" : "No");
                    ImGui::TableNextColumn(); ImGui::TextUnformatted(x->note);
                }
                ImGui::EndTable();
            }
            ImGui::TreePop();
        }
    }
    ImGui::PopID();
}

/* Offline Language Disc importer inside Field Dossier > Gameplay. */
static void language_import_section(void)
{
    static int was_running;
    if (!ashford::panel_matches("Language disc import", "Subtitles", "LANGUAGE")) return;
    import_poll_picker();
    ImportProgress* pg = &g_import_progress;
    int running = __atomic_load_n(&pg->running, __ATOMIC_ACQUIRE);
    if (was_running && !running) lang_scan();
    was_running = running;
    int inst = 0;
    for (int l = 1; l < LANG_COUNT; l++) inst += lang_installed(l);
    ImGui::SeparatorText("LANGUAGE DISC IMPORT");
    char head[128];
    SDL_snprintf(head, sizeof head, "Import Game Languages & Subtitles - %d installed###imp_lang", inst);
    if (!ImGui::CollapsingHeader(head, 0)) return;
    ImGui::TextWrapped("Extract text, fonts, cutscene subtitles, and optional translated pictures from your own regional Code: Veronica X PS2 disc images (offline).");
    if (inst > 0) {
        ImGui::Spacing();
        for (int l = 1; l < LANG_COUNT; l++) {
            if (!lang_installed(l)) continue;
            ImGui::BulletText("%s (%s)%s%s", lang_names[l], lang_codes[l],
                              lang_has_subtitles(l) ? " [subtitles]" : "",
                              lang_old_import(l) ? " [re-import recommended]" : "");
        }
    }
    ImGui::Spacing();
    for (int s = 0; s < IMPORT_SOURCES; s++) {
        if (import_sources[s].cat != IMPORT_CAT_LANGUAGES) continue;
        const ImportSource* src = &import_sources[s];
        ImGui::PushID(s);
        ImGui::Separator();
        ImGui::Text("%s (%s)", src->title, src->serial);
        ImGui::TextDisabled("ISO: %s", g_import.iso[s][0] ? g_import.iso[s] : "(none chosen)");
        ImGui::BeginDisabled(running != 0);
        if (ImGui::Button("Choose ISO...")) {
            static const SDL_DialogFileFilter filters[] = { { "PS2 disc image (*.iso)", "iso" }, { "All files", "*" } };
            SDL_ShowOpenFileDialog(import_iso_picked, (void*)(intptr_t)s, win, filters, 2, g_import.iso[s][0] ? g_import.iso[s] : nullptr, false);
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!g_import.iso[s][0]);
        if (ImGui::Button("Import")) {
            import_active_src = s;
            import_start(s, 0);
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (import_active_src == s && (running || pg->total || pg->phase[0])) {
            char ov[64];
            SDL_snprintf(ov, sizeof(ov), "%d / %d", pg->done, pg->total);
            ImGui::ProgressBar(pg->total ? (float)pg->done / pg->total : 0.0f, ImVec2(-1, 0), ov);
            if (pg->phase[0]) ImGui::TextWrapped("%s", pg->phase);
        }
        ImGui::PopID();
    }
}

static void dossier_page(int page)
{
    bool changed = false;
    if (page == SP_CONTROLS) controls_section();
    else if (page < SP_COUNT) {
        changed = settings_imgui(page);
        if (page == SP_GAMEPLAY) language_import_section();
        if (page == SP_APPEAR) outbreak_import_section();
        if (page == SP_TPS) ImGui::TextDisabled("Camera toggle binding: Input & bindings > Hotkeys.");
        if (page == SP_CHEATS && g_cheat.result) ImGui::TextWrapped("Last action: %s", g_cheat.result);
    } else if (page == SP_COUNT) {
        static ProfFrame h[240];
        bool on = prof_enabled != 0;
        if (ImGui::Checkbox("Measure (small overhead; always on with --profile)", &on)) prof_enabled = on;
        int n = prof_history(h, 240);
        if (n > 0) {
            float total[240], avg[PROF_COUNT] = { 0 }, avg_total = 0, worst = 0;
            int m = n < 60 ? n : 60;
            for (int i = 0; i < n; i++) total[i] = h[i].total;
            for (int i = n - m; i < n; i++) {
                avg_total += h[i].total / m;
                if (h[i].total > worst) worst = h[i].total;
                for (int c = 0; c < PROF_COUNT; c++) avg[c] += h[i].ms[c] / m;
            }
            ImGui::Text("game frame %.2f ms avg, %.2f ms worst (target 33.37 ms = 29.97 Hz)", avg_total, worst);
            ImGui::PlotLines("##ft", total, n, 0, "frame time (ms)", 0.0f, 66.7f, ImVec2(-1, 60));
            for (int c = 0; c < PROF_COUNT; c++) {
                ImGui::Text("%-8s %6.2f ms", prof_names[c], avg[c]);
                ImGui::SameLine(160);
                ImGui::ProgressBar(avg_total > 0 ? avg[c] / avg_total : 0, ImVec2(-1, 0), "");
            }
            ImGui::TextDisabled("'wait' is idle time; run with --profile for a CSV.");
        }
    
    } else {
        ImGui::TextWrapped("Ashford's Halberd / campaign backend");
        ImGui::Separator();
        ImGui::TextWrapped("Engine adapted from recvx-enhanced-port 0.6.8-0.8.0 (single-player). Copyright (c) 2026 aikatore and contributors. MIT License.");
        ImGui::TextWrapped("Game decompilation: AshfordFamily and contributors. PC-adapted fork by aikatore and contributors.");
        ImGui::TextWrapped("This interface is original to Ashford's Halberd. Campaign graphics are read from your own SLUS-20184 disc image.");
        ImGui::TextWrapped("Third-party source and license notices are included with the distribution.");
    }
    if (changed) settings_save_later();
}

static void settings_window(void)
{
    ashford::settings_panel(&visible, dossier_page);
}

/* the game's picture in the window as gs_gl.c presents it: its top left x0, y0 and size vw, vh */
static float picture(float* x0, float* y0, float* vw, float* vh)
{
    ImVec2 ds = ImGui::GetIO().DisplaySize;
    float hs = gs_hor_scale(), a = 4.0f / 3.0f * hs;
    *vw = ds.y * a; *vh = ds.y;
    if (*vw > ds.x) { *vw = ds.x; *vh = ds.x / a; }
    if (g_config.aspect == ASPECT_STRETCH && hs == 1.0f) { *vw = ds.x; *vh = ds.y; }
    *x0 = (ds.x - *vw) * 0.5f; *y0 = (ds.y - *vh) * 0.5f;
    return hs;
}

/* Ashford-styled HUD: obsidian/ink backing, antique brass rim, RE condition colours, weapon icons,
   and ammo-type tinting across both Weapon Circle (hud_style 0) and Minimal (hud_style 1). */
#define HUD_SWEEP (200.0f * 3.14159265f / 180.0f)
static float hud_a;
static int hud_left;

static void hud_text(ImDrawList* dl, float size, float x, float y, ImU32 col, const char* s)
{
    float o = size * 0.06f > 1.0f ? size * 0.06f : 1.0f;
    dl->AddText(ImGui::GetFont(), size, ImVec2(x + o, y + o), (col >> 25 << 24), s);
    dl->AddText(ImGui::GetFont(), size, ImVec2(x, y), col, s);
}

static void hud_count(ImDrawList* dl, float size, float x, float y, int n, int pct, ImU32 col)
{
    char b[16];
    if (n == HUD_INF) {
        float r = size * 0.17f;
        dl->AddCircle(ImVec2(x - r, y + size * 0.5f), r, col, 16, size * 0.08f);
        dl->AddCircle(ImVec2(x + r, y + size * 0.5f), r, col, 16, size * 0.08f);
        return;
    }
    snprintf(b, sizeof b, pct ? "%d%%" : "%d", n);
    hud_text(dl, size, x - ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0, b).x * 0.5f, y, col, b);
}

static void hud_draw(void)
{
    static const ImU32 cond_col[] = {
        IM_COL32(76, 214, 104, 255), IM_COL32(228, 206, 56, 255), IM_COL32(236, 126, 36, 255),
        IM_COL32(218, 46, 38, 255),  IM_COL32(182, 74, 198, 255)
    };
    float dt = ImGui::GetIO().DeltaTime;
    float target = g_hud.on && g_hud.left == hud_left ? 1.0f - g_hud.fade : 0.0f;
    float step = dt * 6.0f;
    hud_a = target > hud_a ? (hud_a + step < target ? hud_a + step : target)
                           : (hud_a - step > target ? hud_a - step : target);
    if (hud_a <= 0.0f) { hud_left = g_hud.left; return; }
    hud_upload_icons();
    float x0, y0, vw, vh;
    picture(&x0, &y0, &vw, &vh);
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    float al = hud_a, dir = hud_left ? -1.0f : 1.0f, o = vh / 720.0f > 1.0f ? vh / 720.0f : 1.0f;
    auto A = [al](ImU32 col, float k = 1.0f) { return (col & 0xFFFFFF) | (ImU32)((col >> 24) * al * k) << 24; };
    float f = g_hud.hp_max > 0 ? (float)g_hud.hp / g_hud.hp_max : 0.0f;
    f = f < 0 ? 0 : f > 1 ? 1 : f;
    ImU32 hp_col = A(cond_col[g_hud.cond < 0 || g_hud.cond > HUD_POISON ? 0 : g_hud.cond],
                     g_hud.cond == HUD_DANGER ? 0.7f + 0.3f * cosf((float)ImGui::GetTime() * 6.0f) : 1.0f);
    const ImU32 brass   = A(IM_COL32(188, 152, 86, 155));
    const ImU32 rim     = A(IM_COL32(188, 152, 86, 95));
    const ImU32 track   = A(IM_COL32(28, 22, 18, 200));
    const ImU32 surface = A(IM_COL32(14, 11, 9, 175));
    const ImU32 ivory   = A(IM_COL32(236, 224, 198, 240));
    const ImU32 red     = A(IM_COL32(235, 84, 72, 255));
    auto icon = [&](float x, float y, float k) {
        float iw = (float)g_hud.icon_w, ih = (float)g_hud.icon_h;
        float u0 = (g_hud.icon_x + 0.5f) / hud_tw, v0 = (g_hud.icon_y + 0.5f) / hud_th;
        float u1 = (g_hud.icon_x + iw - 0.5f) / hud_tw, v1 = (g_hud.icon_y + ih - 0.5f) / hud_th;
        dl->AddImage((ImTextureID)(intptr_t)hud_tex, ImVec2(x, y), ImVec2(x + iw * k, y + ih * k),
                     ImVec2(u0, v0), ImVec2(u1, v1), A(IM_COL32(255, 255, 255, 255)));
    };
    /* Plain ivory, acid gold, flame orange, gas emerald, gunpowder amber */
    static const ImU32 round_col[] = {
        IM_COL32(236, 224, 198, 245), IM_COL32(255, 210, 60, 245), IM_COL32(255, 140, 30, 245),
        IM_COL32(70, 230, 150, 245),  IM_COL32(255, 150, 40, 245)
    };
    int rk = g_hud.round_kind >= 0 && g_hud.round_kind <= 4 ? g_hud.round_kind : 0;
    const ImU32 tint = A(round_col[rk]);
    if (g_tps.hud_style == 1) {
        float m = vh * 0.05f, bw = vh * 0.26f, bh = vh * 0.016f, fs = vh * 0.042f, ss = fs * 0.72f;
        float ox = hud_left ? x0 + m : x0 + vw - m;
        float bx = hud_left ? ox : ox - bw, by = y0 + vh - m - bh, rr = bh * 0.5f;
        dl->AddRectFilled(ImVec2(bx - o, by - o), ImVec2(bx + bw + o, by + bh + o), brass, rr + o);
        dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), track, rr);
        if (f > 0) dl->AddRectFilled(ImVec2(hud_left ? bx : bx + bw * (1 - f), by),
                                     ImVec2(hud_left ? bx + bw * f : bx + bw, by + bh), hp_col, rr);
        ImFont* font = ImGui::GetFont();
        char ls[16] = "", sp[16] = "";
        if (g_hud.rounds >= 0) snprintf(ls, sizeof ls, g_hud.rounds_pct ? "%d%%" : "%d", g_hud.rounds);
        if (g_hud.rounds != HUD_NONE && g_hud.spare != HUD_NONE) snprintf(sp, sizeof sp, g_hud.spare_pct ? "/%d%%" : "/%d", g_hud.spare);
        float wl = g_hud.rounds == HUD_INF ? fs * 0.75f : g_hud.rounds == HUD_NONE ? 0.0f : font->CalcTextSizeA(fs, FLT_MAX, 0, ls).x;
        float wsp = *sp ? font->CalcTextSizeA(ss, FLT_MAX, 0, sp).x : 0.0f, gap = fs * 0.25f;
        float ik = hud_tex && g_hud.icon_w > 0 ? fs * 1.1f / g_hud.icon_h : 0.0f, wi = g_hud.icon_w * ik;
        float tw = wi + (wi > 0 && wl > 0 ? gap : 0.0f) + wl + wsp, x = hud_left ? ox : ox - tw, base = by - vh * 0.012f;
        float asc = font->GetFontBaked(fs)->Ascent, sasc = font->GetFontBaked(ss)->Ascent;
        if (wi > 0) { icon(x, base - asc * 0.5f - g_hud.icon_h * ik * 0.5f, ik); x += wi + gap; }
        if (g_hud.rounds != HUD_NONE) hud_count(dl, fs, x + wl * 0.5f, base - asc, g_hud.rounds, g_hud.rounds_pct, g_hud.rounds == 0 ? red : tint);
        if (*sp) hud_text(dl, ss, x + wl, base - sasc, !g_hud.spare ? red : rk ? tint : ivory, sp);
        return;
    }
    float R = vh * 0.07f, th = R * 0.17f, ra = R + th * 1.1f, m = vh * 0.05f + ra + th * 0.5f;
    ImVec2 c(hud_left ? x0 + m : x0 + vw - m, y0 + vh - m);
    dl->AddCircleFilled(c, R, surface, 48);
    dl->AddCircle(c, R, rim, 48, 1.5f);
    float a0 = 3.14159265f * 0.5f, a1 = a0 - dir * HUD_SWEEP, e = o / ra;
    dl->PathArcTo(c, ra, a0 + dir * e, a1 - dir * e, 40);
    dl->PathStroke(brass, 0, th + 2.0f * o);
    dl->PathArcTo(c, ra, a0, a1, 40);
    dl->PathStroke(track, 0, th);
    if (f > 0) {
        dl->PathArcTo(c, ra, a0, a0 + (a1 - a0) * f, 2 + (int)(38 * f));
        dl->PathStroke(hp_col, 0, th);
    }
    float fs = R * 0.5f, iy = g_hud.rounds != HUD_NONE ? c.y - R * 0.2f : c.y;
    if (hud_tex && g_hud.icon_w > 0) {
        float iw = (float)g_hud.icon_w, ih = (float)g_hud.icon_h;
        float k = 1.5f * R / iw < 0.75f * R / ih ? 1.5f * R / iw : 0.75f * R / ih;
        icon(c.x - iw * k * 0.5f, iy - ih * k * 0.5f, k);
    }
    if (g_hud.rounds != HUD_NONE)
        hud_count(dl, fs, c.x, c.y + R * 0.2f, g_hud.rounds, g_hud.rounds_pct, g_hud.rounds == 0 ? red : tint);
    if (g_hud.spare != HUD_NONE) {
        char b[16];
        float ss = R * 0.42f, pad = ss * 0.25f;
        snprintf(b, sizeof b, g_hud.spare_pct ? "%d%%" : "%d", g_hud.spare);
        float w = ImGui::GetFont()->CalcTextSizeA(ss, FLT_MAX, 0, b).x;
        float ex = c.x - dir * (R + pad * 2), x = hud_left ? ex : ex - w, y = c.y + R * 0.15f;
        dl->AddRectFilled(ImVec2(x - pad, y - pad * 0.4f), ImVec2(x + w + pad, y + ss + pad * 0.4f), surface, ss * 0.3f);
        dl->AddRect(ImVec2(x - pad, y - pad * 0.4f), ImVec2(x + w + pad, y + ss + pad * 0.4f), rim, ss * 0.3f, 0, 1.0f);
        hud_text(dl, ss, x, y, !g_hud.spare ? red : rk ? tint : ivory, b);
    }
}

void overlay_render(void)
{
    if (!ready) return;
    bool windows = visible || keep_idx >= 0;
    pad_set_game_input_enabled(!windows);
    tps_update_mouse_mode(win, windows);
    menu_mouse_update(win, windows);
    /* without a window ImGui leaves the cursor alone (menu_mouse_update shows/hides it) */
    if (windows) ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
    else ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    bool crosshair = g_tps.on && g_tps.crosshair && g_tps.cam_active && g_tps.aiming;
    const char* quick_notice = quicksave_notice();
    if (g_config.show_fps == 2 && !prof_enabled) prof_enabled = 1;
    if (!visible && keep_idx < 0 && !g_config.show_fps && !crosshair && !quick_notice && !g_hud.on && hud_a <= 0.0f) return;
    {   /* readable on large displays: scale text with the window height (1080p = 1x) */
        int ww, wh;
        SDL_GetWindowSizeInPixels(win, &ww, &wh);
        ImGui::GetStyle().FontScaleDpi = wh > 1080 ? wh / 1080.0f : 1.0f;
    }
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
    hud_draw();
    if (visible) settings_window();
    if (keep_idx >= 0 && keep_idx < g_settings_n) {
        int64_t left_ms = OVERLAY_KEEP_MS - (int64_t)(SDL_GetTicks() - keep_t);
        if (left_ms <= 0) {
            settings_set(&g_settings[keep_idx], keep_old);
            settings_save_later();
            keep_idx = -1;
        } else {
            int sec = (int)((left_ms + 999) / 1000);
            ImVec2 center = ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
            ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(340, 0), ImGuiCond_Always);
            if (ImGui::Begin("Keep display settings?###keep_display", nullptr,
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings)) {
                ImGui::TextWrapped("Keep the new %s?", g_settings[keep_idx].label);
                ImGui::Text("Reverting automatically in %d s.", sec);
                ImGui::ProgressBar((float)left_ms / (float)OVERLAY_KEEP_MS, ImVec2(-1, 6), "");
                ImGui::Spacing();
                if (ImGui::Button("Keep", ImVec2(140, 0))) {
                    settings_save_later();
                    keep_idx = -1;
                }
                ImGui::SameLine();
                if (ImGui::Button("Revert", ImVec2(140, 0)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    settings_set(&g_settings[keep_idx], keep_old);
                    settings_save_later();
                    keep_idx = -1;
                }
            }
            ImGui::End();
        }
    }
    if (quick_notice) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y - 24), ImGuiCond_Always, ImVec2(0.5f, 1));
        ImGui::SetNextWindowBgAlpha(0.92f);
        ImGui::Begin("##quick_status", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
        ImGui::TextUnformatted(quick_notice);
        ImGui::End();
    }
    if (crosshair) {   /* third person: the screen centre is where the shot goes (tps_glue.c) */
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        ImVec2 c(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
        float s = ImGui::GetIO().DisplaySize.y / 120.0f, g = s * 0.6f, t = s > 8 ? 2.0f : 1.5f;
        ImU32 col = IM_COL32(255, 60, 40, 220), shadow = IM_COL32(0, 0, 0, 160);
        for (int pass = 0; pass < 2; pass++) {
            ImU32 k = pass ? col : shadow;
            float o = pass ? 0.0f : 1.0f;
            dl->AddLine(ImVec2(c.x - g - s + o, c.y + o), ImVec2(c.x - g + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + g + o, c.y + o), ImVec2(c.x + g + s + o, c.y + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y - g - s + o), ImVec2(c.x + o, c.y - g + o), k, t);
            dl->AddLine(ImVec2(c.x + o, c.y + g + o), ImVec2(c.x + o, c.y + g + s + o), k, t);
            dl->AddCircleFilled(ImVec2(c.x + o, c.y + o), t, k);
        }
    }
    if (g_config.show_fps) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 10, 10), ImGuiCond_Always, ImVec2(1, 0));
        ImGui::SetNextWindowBgAlpha(g_config.show_fps == 2 ? 0.78f : 0.45f);
        ImGui::Begin("##fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs |
                                       ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoFocusOnAppearing);
        if (g_config.show_fps == 1) {
            ImGui::Text("%.1f fps", ImGui::GetIO().Framerate);
        } else {
            static ProfFrame h[120];
            float total[120], avg = 0.0f, worst = 0.0f;
            int n = prof_history(h, 120);
            int m = n < 60 ? n : 60;
            for (int i = 0; i < n; i++) total[i] = h[i].total;
            for (int i = n - m; i < n; i++) {
                avg += h[i].total / (float)m;
                if (h[i].total > worst) worst = h[i].total;
            }
            ImGui::TextColored(ImVec4(0.84f, 0.71f, 0.41f, 1.0f), "%.1f fps", ImGui::GetIO().Framerate);
            if (n > 0) {
                ImGui::SameLine();
                ImGui::TextDisabled("| %.1f ms (max %.1f)", avg, worst);
                ImGui::PlotLines("##fps_graph", total, n, 0, nullptr, 0.0f, 50.0f, ImVec2(200, 42));
            }
        }
        ImGui::End();
    }
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
