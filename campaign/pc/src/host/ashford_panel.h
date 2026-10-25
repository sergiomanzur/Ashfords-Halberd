// Original Ashford's Halberd settings presentation. No graphical assets.
#pragma once

namespace ashford {
void panel_style();
void panel_select_page(int page);
bool panel_matches(const char* label, const char* help = nullptr, const char* group = nullptr);
const char* panel_query();
void settings_panel(bool* open, void (*draw_page)(int));
}
