// Original presentation for Ashford's Halberd; engine callbacks retain their licenses.
#include "ashford_panel.h"
#include "settings.h"
#include <imgui.h>
#include <cctype>
#include <cstring>

namespace ashford {
namespace {
char query[128] = {};
int selected_page = SP_DISPLAY;
constexpr const char* titles[] = {"Picture & display", "Play preferences", "Input & bindings",
    "Camera & aiming", "Sound", "Character", "Cheats", "Frame analysis", "About"};
constexpr const char* descriptions[] = {
    "Framing, resolution, effects and picture calibration.",
    "Loading, transitions, saves and optional gameplay changes.",
    "Choose your devices and make every action comfortable.",
    "Original viewpoints or an adjustable over-the-shoulder camera.",
    "Balance music, voices and effects.",
    "Optional model and animation preferences.",
    "Health, ammunition, title unlocks, inventory and developer tools.",
    "Inspect frame times without changing the game's simulation rate.",
    "Source credits and the campaign backend."};

static_assert(IM_ARRAYSIZE(titles) == SP_COUNT + 2, "Every shared settings page needs a section");
static_assert(IM_ARRAYSIZE(descriptions) == IM_ARRAYSIZE(titles), "Every section needs its help text");

bool contains(const char* value, const char* needle) {
    if (!value) return false;
    for (; *value; ++value) {
        const char *a = value, *b = needle;
        while (*a && *b && std::tolower(static_cast<unsigned char>(*a)) ==
                               std::tolower(static_cast<unsigned char>(*b))) { ++a; ++b; }
        if (!*b) return true;
    }
    return false;
}
}

const char* panel_query() { return query; }
bool panel_matches(const char* label, const char* help, const char* group) {
    return !query[0] || contains(label, query) || contains(help, query) || contains(group, query);
}

void panel_style() {
    ImGui::StyleColorsDark();
    auto& s = ImGui::GetStyle();
    s.WindowRounding = 8; s.ChildRounding = 5; s.FrameRounding = 3;
    s.WindowPadding = ImVec2(20, 18); s.FramePadding = ImVec2(9, 6);
    s.ItemSpacing = ImVec2(12, 10); s.WindowBorderSize = 1;
    s.Colors[ImGuiCol_WindowBg] = ImVec4(.045f,.063f,.072f,.98f);
    s.Colors[ImGuiCol_TitleBg] = ImVec4(.07f,.09f,.09f,1);
    s.Colors[ImGuiCol_TitleBgActive] = ImVec4(.17f,.18f,.13f,1);
    s.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(.07f,.09f,.09f,1);
    s.Colors[ImGuiCol_NavCursor] = ImVec4(.82f,.68f,.40f,1);
    s.Colors[ImGuiCol_ChildBg] = ImVec4(.06f,.08f,.09f,1);
    s.Colors[ImGuiCol_Text] = ImVec4(.9f,.89f,.84f,1);
    s.Colors[ImGuiCol_TextDisabled] = ImVec4(.56f,.62f,.62f,1);
    s.Colors[ImGuiCol_Border] = ImVec4(.28f,.30f,.27f,.7f);
    s.Colors[ImGuiCol_FrameBg] = ImVec4(.10f,.14f,.15f,1);
    s.Colors[ImGuiCol_FrameBgHovered] = ImVec4(.19f,.24f,.23f,1);
    s.Colors[ImGuiCol_FrameBgActive] = ImVec4(.24f,.29f,.26f,1);
    s.Colors[ImGuiCol_Button] = ImVec4(.15f,.20f,.20f,1);
    s.Colors[ImGuiCol_ButtonHovered] = ImVec4(.29f,.31f,.24f,1);
    s.Colors[ImGuiCol_ButtonActive] = ImVec4(.40f,.36f,.23f,1);
    s.Colors[ImGuiCol_Header] = ImVec4(.29f,.27f,.18f,1);
    s.Colors[ImGuiCol_HeaderHovered] = ImVec4(.35f,.32f,.21f,1);
    s.Colors[ImGuiCol_HeaderActive] = ImVec4(.43f,.37f,.22f,1);
    s.Colors[ImGuiCol_CheckMark] = ImVec4(.82f,.68f,.40f,1);
    s.Colors[ImGuiCol_SliderGrab] = ImVec4(.72f,.59f,.35f,1);
    s.Colors[ImGuiCol_SliderGrabActive] = ImVec4(.91f,.77f,.48f,1);
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
}

void panel_select_page(int page) {
    if (page >= 0 && page < SP_COUNT + 2) { selected_page = page; query[0] = 0; }
}

void settings_panel(bool* open, void (*draw_page)(int)) {
    int& page = selected_page;
    const auto screen = ImGui::GetIO().DisplaySize;
    const float scale = ImGui::GetStyle().FontScaleDpi;
    const float margin = 12 * scale;
    const float max_w = screen.x > margin * 2 ? screen.x - margin * 2 : screen.x;
    const float max_h = screen.y > margin * 2 ? screen.y - margin * 2 : screen.y;
    ImGui::SetNextWindowPos(ImVec2(screen.x*.5f,screen.y*.5f), ImGuiCond_Appearing, ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(max_w < 980*scale ? max_w : 980*scale,
                                   max_h < 740*scale ? max_h : 740*scale), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(max_w < 540*scale ? max_w : 540*scale,
                                             max_h < 360*scale ? max_h : 360*scale), ImVec2(max_w,max_h));
    if (!ImGui::Begin("Ashford's Halberd / Field dossier###ashford-settings", open,
                      ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    ImGui::TextColored(ImVec4(.82f,.68f,.40f,1), "A S H F O R D ' S   H A L B E R D");
    ImGui::TextDisabled("FIELD DOSSIER  /  PREFERENCES");
    ImGui::Separator();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##settings-search", "Find a setting in the selected section...", query, sizeof query);

    const float footer = ImGui::GetFrameHeightWithSpacing() * 2;
    // A narrow window uses a section picker instead of squeezing the settings.
    const bool narrow = ImGui::GetContentRegionAvail().x < 680*scale;
    if (narrow) {
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##section", &page, titles, IM_ARRAYSIZE(titles));
    } else {
        ImGui::BeginChild("##sections", ImVec2(198*scale, -footer), ImGuiChildFlags_Borders);
        ImGui::TextDisabled("SECTIONS");
        for (int i=0;i<IM_ARRAYSIZE(titles);++i) {
            ImGui::PushID(i);
            if (ImGui::Selectable(titles[i], page==i, 0, ImVec2(0,31*scale))) page=i;
            ImGui::PopID();
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }
    ImGui::BeginChild("##preferences", ImVec2(0,-footer), ImGuiChildFlags_Borders);
    ImGui::TextUnformatted(titles[page]);
    ImGui::PushTextWrapPos();
    ImGui::TextDisabled("%s", descriptions[page]);
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x*.48f);
    draw_page(page);
    ImGui::EndChild();
    ImGui::Separator();
    if (ImGui::Button("Return to game")) *open=false;
    ImGui::SameLine();
    ImGui::TextDisabled("F1 close  |  Preferences save automatically");
    ImGui::TextDisabled("F2 Cheats  |  OPTION > Enhanced Options  |  Cheat toggles last this session");
    ImGui::End();
}
}
