// Exercise the actual F1 composition without the game or proprietary headers.
#include "host/ashford_panel.h"
#include "host/settings.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
#include <cstdlib>

static int visited, expected_page;
static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static void page(int selected) {
    require(selected >= 0 && selected < SP_COUNT + 2, "invalid settings section");
    require(selected == expected_page, "section selection did not reach the requested page");
    ++visited;
    // A long section must scroll inside the panel rather than expanding it.
    for (int i=0;i<80;++i) ImGui::Text("Setting %d: retained engine callback",i);
}
int main() {
    ImGui::CreateContext();
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;
    unsigned char* pixels; int width,height;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
    ashford::panel_style();
    require(ashford::panel_matches("Anything"), "empty filter hides settings");
    const ImVec2 sizes[]={{1280,960},{640,480},{320,240},{1920,1080}};
    for (auto size:sizes) {
        io.DisplaySize=size; io.DeltaTime=1.0f/60;
        bool open=true;
        for (int section=0;section<SP_COUNT+2;++section) {
            expected_page=section;
            ashford::panel_select_page(section);
            for (int frame=0;frame<3;++frame) {
            ImGui::NewFrame();
            ashford::settings_panel(&open,page);
            ImGui::Render();
            auto* window=ImGui::FindWindowByName("Ashford's Halberd / Field dossier###ashford-settings");
            require(window!=nullptr,"settings window missing");
            require(window->Size.x<=size.x && window->Size.y<=size.y,"panel overflows resized window");
            require(ImGui::GetDrawData()->TotalVtxCount>0,"panel produces no geometry");
            }
        }
    }
    require(visited==12*(SP_COUNT+2),"settings content callback not retained");
    ImGui::DestroyContext();
    std::puts("Ashford F1 layout: wide, narrow, small and resized windows passed");
}
